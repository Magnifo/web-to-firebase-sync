# Dual-write: Firebase RTDB + Supabase

GitHub Actions still pushes website flight fields to **Firebase RTDB** (release app).
In parallel it **bulk-upserts** the same managed fields into Supabase `public.flights`
(and patches weather onto `public.cities`).

## Secrets to add

Repo → Settings → Secrets and variables → Actions:

| Secret | Value |
|---|---|
| `FIREBASE_SERVICE_ACCOUNT` | (already set) |
| `SUPABASE_URL` | `https://tbifgskqeyreqrwhhtmf.supabase.co` |
| `SUPABASE_SERVICE_ROLE_KEY` | AirOps service_role key (never commit) |

If Supabase secrets are missing, the job still completes RTDB sync and logs a skip warning.

## Behavior

- RTDB: per-flight `.update()` of managed fields only (unchanged)
- Supabase: chunked PostgREST upsert (`city,direction,flight_key`), ~500 rows/request
- Managed columns only: `flnr`, `airline_code`, `sub_cat`, `city_lu`, `prem_lu`, `stm`, `att`, `est`, `last_updated`
- Ops columns (gates, loaders, `field_meta`, …) are **not** overwritten
- Airline stubs are upserted first so `airline_code` FK does not fail
- Weather: RTDB `/weather/{city}` + patch weather columns on `cities` by name
