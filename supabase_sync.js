/**
 * Supabase writer for the PAA flight/weather sync (Firebase RTDB disabled).
 * Uses PostgREST bulk upserts (chunked).
 *
 * Required env (GitHub Actions secrets):
 *   SUPABASE_URL
 *   SUPABASE_SERVICE_ROLE_KEY
 */

const axios = require('axios');

const CHUNK_SIZE = 500;

function getConfig() {
  const url = (process.env.SUPABASE_URL || '').trim().replace(/\/$/, '');
  const key = (process.env.SUPABASE_SERVICE_ROLE_KEY || '').trim();
  if (!url || !key) {
    throw new Error(
      'Missing SUPABASE_URL or SUPABASE_SERVICE_ROLE_KEY (set GitHub Actions secrets).'
    );
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

async function upsertTable(url, key, table, rows, onConflict, prefer = 'resolution=merge-duplicates,return=minimal') {
  if (!rows.length) return 0;
  const normalized = normalizeRows(rows);
  let written = 0;
  for (let i = 0; i < normalized.length; i += CHUNK_SIZE) {
    const chunk = normalized.slice(i, i + CHUNK_SIZE);
    const path = `${table}?on_conflict=${encodeURIComponent(onConflict)}`;
    await axios.post(`${url}/rest/v1/${path}`, chunk, {
      headers: restHeaders(key, prefer),
      timeout: 120000,
      validateStatus: (status) => status >= 200 && status < 300,
    });
    written += chunk.length;
    console.log(`  Supabase ${table}: batch ${Math.floor(i / CHUNK_SIZE) + 1} (${chunk.length})`);
  }
  return written;
}

async function getJson(url, key, path) {
  const res = await axios.get(`${url}/rest/v1/${path}`, {
    headers: restHeaders(key, 'return=representation'),
    timeout: 60000,
    validateStatus: (status) => status >= 200 && status < 300,
  });
  return res.data;
}

/** Load cities.name -> id. Creates missing cities as stubs so FK upserts succeed. */
async function resolveCityIds(url, key, cityNames) {
  const unique = [...new Set(cityNames.map((n) => String(n || '').trim()).filter(Boolean))];
  if (!unique.length) return new Map();

  const existing = await getJson(
    url,
    key,
    `cities?select=id,name&name=in.(${unique.map((n) => `"${n.replace(/"/g, '')}"`).join(',')})`
  );
  const byName = new Map();
  for (const row of existing || []) {
    if (row?.name != null && row?.id != null) byName.set(String(row.name), Number(row.id));
  }

  const missing = unique.filter((n) => !byName.has(n));
  if (missing.length) {
    console.log(`Supabase: creating ${missing.length} missing cities: ${missing.join(', ')}`);
    const stubs = missing.map((name, index) => ({
      name,
      enabled: true,
      latitude: 0,
      longitude: 0,
      timezone: 'Asia/Karachi',
      sort_order: 500 + index,
    }));
    await upsertTable(url, key, 'cities', stubs, 'name');
    const created = await getJson(
      url,
      key,
      `cities?select=id,name&name=in.(${missing.map((n) => `"${n.replace(/"/g, '')}"`).join(',')})`
    );
    for (const row of created || []) {
      if (row?.name != null && row?.id != null) byName.set(String(row.name), Number(row.id));
    }
  }

  return byName;
}

function compactFlnr(value) {
  return String(value || '').replace(/[^A-Za-z0-9]/g, '').toUpperCase();
}

function directionLetter(direction) {
  return direction === 'Arrival' ? 'A' : 'D';
}

function dateFromStm(stm) {
  const d = String(stm || '').replace(/\D/g, '');
  return d.slice(0, 6) || '000000';
}

/** `{cityId}_{D|A}_{FLNR}_{YYMMDD}` — no hour/minute. */
function buildFlightKey(cityId, direction, fl) {
  return `${cityId}_${directionLetter(direction)}_${compactFlnr(fl.flnr)}_${dateFromStm(fl.stm)}`;
}

function isFinishedStatus(premLu) {
  const s = String(premLu || '').toUpperCase();
  return s.includes('DEPARTED') || s.includes('LANDED') || s.includes('ARRIVED');
}

function mergeFlightRows(a, b) {
  const aDone = isFinishedStatus(a.prem_lu);
  const bDone = isFinishedStatus(b.prem_lu);
  const keep = (!aDone && bDone) ? b : (aDone && !bDone) ? a : b;
  const other = keep === b ? a : b;
  const out = { ...other, ...keep };
  if (!out.att && other.att) out.att = other.att;
  if (!out.est && other.est) out.est = other.est;
  return out;
}

/**
 * Bulk-upsert website-managed flight fields into public.flights.
 * Uses cities.id FK (city_id). City name comes from public.cities via v_flights.
 * Only touch managed scrape fields; ops columns stay intact.
 *
 * @param {Record<string, { Arrival?: object, Departure?: object }>} structuredData
 * @param {string} [lastUpdateStr]
 */
async function syncFlightsToSupabase(structuredData, lastUpdateStr) {
  const { url, key } = getConfig();
  const cityNames = Object.keys(structuredData || {});
  const cityIdsByName = await resolveCityIds(url, key, cityNames);
  const flightByKey = new Map();

  for (const city of cityNames) {
    const cityId = cityIdsByName.get(city);
    if (!cityId) {
      console.error(`Supabase: skip city "${city}" — no cities.id`);
      continue;
    }
    const directions = structuredData[city] || {};
    for (const direction of Object.keys(directions)) {
      if (direction !== 'Arrival' && direction !== 'Departure') continue;
      const flights = directions[direction] || {};
      for (const scrapeKey of Object.keys(flights)) {
        const fl = flights[scrapeKey];
        if (!fl || typeof fl !== 'object') continue;

        const airline = fl.airline != null ? String(fl.airline).trim() : '';
        const flightKey = buildFlightKey(cityId, direction, fl);
        const row = {
          city_id: cityId,
          direction,
          flight_key: flightKey,
          airline_code: airline || null,
          flnr: fl.flnr ?? null,
          stm: fl.stm ?? null,
          sub_cat: fl.SubCat ?? null,
          city_lu: fl.city_lu ?? null,
          prem_lu: fl.prem_lu ?? null,
          att: fl.att ?? null,
          est: fl.est ?? null,
          last_updated: new Date().toISOString(),
        };
        const prev = flightByKey.get(flightKey);
        flightByKey.set(flightKey, prev ? mergeFlightRows(prev, row) : row);
      }
    }
  }

  const flightRows = [...flightByKey.values()];

  console.log(
    `Supabase: upserting ${flightRows.length} flights (bulk via city_id)...`
  );
  const started = Date.now();

  // Do not write airlines — that table is admin-curated majors only.

  const flightsWritten = await upsertTable(
    url,
    key,
    'flights',
    flightRows,
    'city_id,direction,flight_key'
  );

  if (lastUpdateStr) {
    await upsertTable(
      url,
      key,
      'sync_meta',
      [
        { key: 'lastUpdate', value: String(lastUpdateStr) },
        { key: 'last_update', value: String(lastUpdateStr) },
      ],
      'key'
    );
  }

  console.log(
    `Supabase: flights upsert complete (${flightsWritten} rows) in ${Date.now() - started}ms`
  );
  return {
    skipped: false,
    flights: flightsWritten,
    cities: cityIdsByName.size,
  };
}

/**
 * Patch weather on public.cities by name and upsert public.weather_cache.
 * Does not insert missing cities and never changes enabled / logo / coords.
 *
 * @param {Record<string, object>} weatherByCity
 */
async function syncWeatherToSupabase(weatherByCity) {
  const { url, key } = getConfig();
  const names = Object.keys(weatherByCity || {});
  if (!names.length) return { skipped: false, synced: 0 };

  console.log(`Supabase: writing weather (${names.length} cities)...`);
  let synced = 0;

  const cacheRows = [];
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
        cacheRows.push({
          city: name,
          temp: w.temp ?? null,
          feels_like: w.feels_like ?? null,
          humidity: w.humidity ?? null,
          condition: w.condition ?? null,
          icon: w.icon ?? null,
          weathercode: w.weathercode ?? null,
          wind_speed: w.wind_speed ?? null,
          visibility: w.visibility ?? null,
          last_updated: w.lastUpdated || w.last_updated || null,
        });
        synced += 1;
      })
    );
  }

  if (cacheRows.length) {
    await upsertTable(url, key, 'weather_cache', cacheRows, 'city');
  }

  console.log(`Supabase: weather written for ${synced} cities (cities + weather_cache)`);
  return { skipped: false, synced };
}

module.exports = {
  syncFlightsToSupabase,
  syncWeatherToSupabase,
};
