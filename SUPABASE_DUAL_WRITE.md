# Sync target: Supabase only

GitHub Actions **Sync Flight Data** fetches PAA flights + Open-Meteo weather and
writes to **Supabase** (`public.flights`, `public.cities` weather cols,
`public.weather_cache`, `sync_meta`).

Firebase RTDB writes are **commented out** in `index.js` / `weather_sync.js`
(code kept for reference — do not discard).

## Secrets (required)

Repo → Settings → Secrets and variables → Actions:

| Secret | Value |
|---|---|
| `SUPABASE_URL` | `https://tbifgskqeyreqrwhhtmf.supabase.co` |
| `SUPABASE_SERVICE_ROLE_KEY` | AirOps service_role key (never commit) |

If secrets are missing the job **fails** (no silent skip).

`FIREBASE_SERVICE_ACCOUNT` is no longer used by this workflow.

## Behavior

- Flights: chunked PostgREST upsert on `city_id,direction,flight_key`
- Managed columns only: `flnr`, `airline_code`, `sub_cat`, `city_lu`, `prem_lu`, `stm`, `att`, `est`, `last_updated`
- Ops columns (gates, loaders, `field_meta`, …) are **not** overwritten
- Airline stubs upserted first so `airline_code` FK does not fail
- Missing cities (e.g. Multan) are created as stubs so FK succeeds
- Weather: patch `cities` weather columns + upsert `weather_cache`
- Stamps `sync_meta.lastUpdate` and `sync_meta.last_update`
