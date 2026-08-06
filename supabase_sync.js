/**
 * Parallel Supabase writer for the PAA flight/weather sync.
 * Uses PostgREST bulk upserts (chunked) — not field-by-field RTDB updates.
 *
 * Required env (GitHub Actions secrets):
 *   SUPABASE_URL
 *   SUPABASE_SERVICE_ROLE_KEY
 *
 * Safe for dual-write: never throws if env is missing; callers should
 * catch so Firebase RTDB sync stays independent.
 */

const axios = require('axios');

const CHUNK_SIZE = 500;

function getConfig() {
  const url = (process.env.SUPABASE_URL || '').trim().replace(/\/$/, '');
  const key = (process.env.SUPABASE_SERVICE_ROLE_KEY || '').trim();
  if (!url || !key) {
    console.warn(
      'Supabase sync skipped: set SUPABASE_URL and SUPABASE_SERVICE_ROLE_KEY'
    );
    return null;
  }
  return { url, key };
}

function restHeaders(key, prefer = 'resolution=merge-duplicates,return=minimal') {
  return {
    apikey: key,
    Authorization: `Bearer ${key}`,
    'Content-Type': 'application/json',
    Prefer: prefer,
  };
}

/** PostgREST needs identical keys on every object in a batch. */
function normalizeRows(rows) {
  const keys = new Set();
  for (const row of rows) {
    Object.keys(row).forEach((k) => keys.add(k));
  }
  return rows.map((row) => {
    const out = {};
    for (const k of keys) {
      out[k] = row[k] === undefined ? null : row[k];
    }
    return out;
  });
}

async function upsertTable(url, key, table, rows, onConflict) {
  if (!rows.length) return 0;
  const normalized = normalizeRows(rows);
  let written = 0;
  for (let i = 0; i < normalized.length; i += CHUNK_SIZE) {
    const chunk = normalized.slice(i, i + CHUNK_SIZE);
    const path = `${table}?on_conflict=${encodeURIComponent(onConflict)}`;
    await axios.post(`${url}/rest/v1/${path}`, chunk, {
      headers: restHeaders(key),
      timeout: 120000,
      validateStatus: (status) => status >= 200 && status < 300,
    });
    written += chunk.length;
    console.log(`  Supabase ${table}: batch ${Math.floor(i / CHUNK_SIZE) + 1} (${chunk.length})`);
  }
  return written;
}

/**
 * Bulk-upsert website-managed flight fields into public.flights.
 * Only touch: flnr, airline_code, sub_cat, city_lu, prem_lu, stm, att, est
 * (+ keys / last_updated). Ops fields (gates, loaders, field_meta, …) stay intact.
 *
 * @param {Record<string, { Arrival?: object, Departure?: object }>} structuredData
 * @param {string} [lastUpdateStr]
 */
async function syncFlightsToSupabase(structuredData, lastUpdateStr) {
  const cfg = getConfig();
  if (!cfg) return { skipped: true };

  const { url, key } = cfg;
  const flightRows = [];
  const airlinesByCode = new Map();

  for (const city of Object.keys(structuredData || {})) {
    const directions = structuredData[city] || {};
    for (const direction of Object.keys(directions)) {
      if (direction !== 'Arrival' && direction !== 'Departure') continue;
      const flights = directions[direction] || {};
      for (const flightKey of Object.keys(flights)) {
        const fl = flights[flightKey];
        if (!fl || typeof fl !== 'object') continue;

        const airline = fl.airline != null ? String(fl.airline).trim() : '';
        if (airline) {
          airlinesByCode.set(airline, { code: airline, name: airline });
        }

        flightRows.push({
          city,
          direction,
          flight_key: String(flightKey),
          airline_code: airline || null,
          flnr: fl.flnr ?? null,
          stm: fl.stm ?? null,
          sub_cat: fl.SubCat ?? null,
          city_lu: fl.city_lu ?? null,
          prem_lu: fl.prem_lu ?? null,
          att: fl.att ?? null,
          est: fl.est ?? null,
          last_updated: new Date().toISOString(),
        });
      }
    }
  }

  console.log(
    `Supabase: upserting ${airlinesByCode.size} airline stubs + ${flightRows.length} flights (bulk)...`
  );
  const started = Date.now();

  if (airlinesByCode.size > 0) {
    await upsertTable(url, key, 'airlines', [...airlinesByCode.values()], 'code');
  }

  const flightsWritten = await upsertTable(
    url,
    key,
    'flights',
    flightRows,
    'city,direction,flight_key'
  );

  if (lastUpdateStr) {
    await upsertTable(
      url,
      key,
      'sync_meta',
      [{ key: 'lastUpdate', value: String(lastUpdateStr) }],
      'key'
    );
  }

  console.log(
    `Supabase: flights upsert complete (${flightsWritten} rows) in ${Date.now() - started}ms`
  );
  return {
    skipped: false,
    airlines: airlinesByCode.size,
    flights: flightsWritten,
  };
}

/**
 * Patch weather columns on public.cities by name.
 * Does not insert missing cities and never changes enabled / logo / coords.
 *
 * @param {Record<string, object>} weatherByCity
 */
async function syncWeatherToSupabase(weatherByCity) {
  const cfg = getConfig();
  if (!cfg) return { skipped: true };

  const { url, key } = cfg;
  const names = Object.keys(weatherByCity || {});
  if (!names.length) return { skipped: false, synced: 0 };

  console.log(`Supabase: patching weather on cities (${names.length})...`);
  let synced = 0;

  // Small parallel batches — cities table is tiny.
  const concurrency = 4;
  for (let i = 0; i < names.length; i += concurrency) {
    const slice = names.slice(i, i + concurrency);
    await Promise.all(
      slice.map(async (name) => {
        const w = weatherByCity[name];
        if (!w) return;
        await axios.patch(
          `${url}/rest/v1/cities?name=eq.${encodeURIComponent(name)}`,
          {
            temp: w.temp ?? null,
            feels_like: w.feels_like ?? null,
            humidity: w.humidity ?? null,
            condition: w.condition ?? null,
            icon: w.icon ?? null,
            weathercode: w.weathercode ?? null,
            wind_speed: w.wind_speed ?? null,
            visibility: w.visibility ?? null,
            weather_updated_at: new Date().toISOString(),
          },
          {
            headers: restHeaders(key, 'return=minimal'),
            timeout: 60000,
            validateStatus: (status) => status >= 200 && status < 300,
          }
        );
        synced += 1;
      })
    );
  }

  console.log(`Supabase: weather patched for ${synced} cities`);
  return { skipped: false, synced };
}

module.exports = {
  syncFlightsToSupabase,
  syncWeatherToSupabase,
};
