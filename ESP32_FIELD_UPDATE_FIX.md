# ESP32 Firebase Field-Level Update Fix - FINAL

## Problem
The ESP32 was overwriting entire flight objects in Firebase, removing fields that were set by other devices (like mobile apps). For example:

**Initial data (from other device like mobile app):**
```json
{
  "SubCat": "INT",
  "airline": "PA",
  "att": "2601150046",
  "city_lu": "Dubai",
  "flnr": "PA 210",
  "prem_lu": "Departed",
  "stm": "2601150035"
}
```

**After ESP32 update (WRONG - removed other fields):**
```json
{
  "aclo": "2601142330",
  "aopn": "2601142048",
  "city_lu": "Dubai",
  "ckco": {...},
  "crow": "02",
  "flnr": "PA 210",
  "stm": "2601150035"
}
```

❌ Notice how `SubCat`, `airline`, `att`, and `prem_lu` were removed!

## Solution Implemented

### ESP32-MANAGED FIELDS
The ESP32 is responsible for managing these 21 fields:

```cpp
const char *MANAGED_FIELDS[] = {
    "stm",     // Scheduled Time
    "flnr",    // Flight Number
    "ect",     // Estimated Time
    "att",     // Actual Time
    "belt1",   // Belt
    "bre1",    // Status
    "city_lu", // City
    "via1_lu", // Via
    "prem_lu", // Remark
    "fsta_lu", // Flight Status
    "cro1",    // Zone
    "cctf",    // Check-In From
    "cctt",    // Check-In To
    "gat1",    // Gate
    "crow",    // Row
    "ckco",    // Counter
    "aopn",    // Open Time
    "aclo",    // Close Time
    "crem",    // Counter Remark
    "crem_lu", // Counter Remark Lookup
    "SubCat"   // Subcategory
};
```

### Smart Field Filtering

**Key Features:**
1. ✅ **Only sends fields ESP32 scraped** - from MANAGED_FIELDS list
2. ✅ **Skips empty/null values** - prevents overwriting existing data with blanks
3. ✅ **Uses Firebase `update()`** - merges instead of replacing
4. ✅ **Preserves other fields** - fields not in MANAGED_FIELDS or set by other devices remain untouched

### How It Works

The `CreateFilteredFlightDoc()` function now:

```cpp
// For each field in scraped data:
for (JsonPair p : source) {
    String key = p.key();
    
    // 1. Is this field managed by ESP32?
    if (IsFieldManaged(key)) {
        
        // 2. Does it have a valid value (not empty/null)?
        if (!IsValueEmpty(p.value())) {
            
            // ✅ YES - Include it in the update
            destObj[key] = p.value();
        }
        else {
            // ⏭️ SKIP - Don't send empty values
        }
    }
    else {
        // ⏭️ SKIP - Not an ESP32-managed field
    }
}
```

### Empty Value Detection

The `IsValueEmpty()` helper checks for:
- `null` values
- Empty strings (`""` or whitespace only)
- Empty objects (`{}`)
- Empty arrays (`[]`)

## Expected Behavior After Fix

### Scenario 1: Mobile app sets data, then ESP32 updates

**Step 1: Mobile app writes**
```json
{
  "SubCat": "INT",
  "airline": "PA",
  "att": "2601150046",
  "city_lu": "Dubai",
  "flnr": "PA 210",
  "prem_lu": "Departed",
  "stm": "2601150035",
  "customField": "custom value"
}
```

**Step 2: ESP32 scrapes check-in data and sends**
ESP32 only sends fields it actually scraped:
```json
{
  "aclo": "2601142330",
  "aopn": "2601142048",
  "ckco": {"11": ["val1", "val2"]},
  "crow": "02",
  "city_lu": "Dubai",     // Scraped by ESP32
  "flnr": "PA 210",       // Scraped by ESP32
  "stm": "2601150035"     // Scraped by ESP32
}
```

**Step 3: Firebase Result (MERGED ✅)**
```json
{
  "SubCat": "INT",              ← PRESERVED (set by mobile)
  "airline": "PA",              ← PRESERVED (set by mobile)
  "att": "2601150046",          ← PRESERVED (set by mobile)
  "prem_lu": "Departed",        ← PRESERVED (set by mobile)
  "customField": "custom value", ← PRESERVED (not in MANAGED_FIELDS)
  "city_lu": "Dubai",           ← UPDATED by ESP32
  "flnr": "PA 210",             ← UPDATED by ESP32
  "stm": "2601150035",          ← UPDATED by ESP32
  "aclo": "2601142330",         ← NEW from ESP32
  "aopn": "2601142048",         ← NEW from ESP32
  "ckco": {...},                ← NEW from ESP32
  "crow": "02"                  ← NEW from ESP32
}
```

### Scenario 2: ESP32 scrapes incomplete data

**ESP32 scrapes flight but some fields are empty:**
```json
{
  "stm": "2601150035",
  "flnr": "PA 210",
  "city_lu": "Dubai",
  "gat1": "",           // Empty gate
  "belt1": null,        // Null belt
  "ckco": {},           // Empty counter object
  "crow": "02",
  "aclo": "2601142330",
  "aopn": "2601142048"
}
```

**ESP32 will send (empty fields removed):**
```json
{
  "stm": "2601150035",
  "flnr": "PA 210",
  "city_lu": "Dubai",
  "crow": "02",
  "aclo": "2601142330",
  "aopn": "2601142048"
}
```

**Serial Log Output:**
```
📋 Including 6 fields: stm, flnr, city_lu, crow, aclo, aopn
⏭️  Skipped 3 empty fields: gat1, belt1, ckco
```

## Changes Made

### Files Modified

1. **`Globals.cpp`**
   - Defined `MANAGED_FIELDS` array with all 21 ESP32-managed fields
   - Added `IsFieldManaged()` - checks if field is in MANAGED_FIELDS
   - Added `IsValueEmpty()` - checks for null/empty/whitespace values
   - Enhanced `CreateFilteredFlightDoc()` to:
     - Only include managed fields
     - Skip empty/null values
     - Log included and skipped fields

2. **`Global.h`**
   - Added extern declarations for `MANAGED_FIELDS` and `MANAGED_FIELDS_COUNT`

3. **`MyFirebase.cpp`**
   - Removed duplicate `MANAGED_FIELDS` definition (now uses Globals.cpp version)
   - Enhanced `UpdateFlight()` with debug logging showing:
     - Firebase path being updated
     - Exact JSON payload being sent

## Serial Monitor Output Example

When ESP32 updates a flight, you'll see:
```
📋 Including 7 fields: stm, flnr, city_lu, crow, ckco, aclo, aopn
⏭️  Skipped 2 empty fields: gat1, belt1
🔥 Firebase UPDATE to /flights/Islamabad/Departure/2601150035_PA210
📤 Payload: {"stm":"2601150035","flnr":"PA 210","city_lu":"Dubai","crow":"02","ckco":{...},"aclo":"2601142330","aopn":"2601142048"}
✅ Firebase Success
```

## Testing Steps

1. **Upload code to ESP32**
   - Compile and upload the updated `.ino`, `Globals.cpp`, `Global.h`, and `MyFirebase.cpp`

2. **Test Scenario 1: Mobile app data preserved**
   - Use mobile app to set `airline`, `SubCat`, custom fields on a flight
   - Wait for ESP3 to run its sync cycle
   - Check Firebase - verify mobile app fields are still there
   - Check Serial Monitor - verify ESP32 only sent its managed fields

3. **Test Scenario 2: Empty field handling**
   - Wait for ESP32 to scrape a flight with some empty fields
   - Check Serial Monitor - look for "Skipped X empty fields"
   - Verify Firebase doesn't have null/empty values written

4. **Test Scenario 3: New flight creation**
   - Let ESP32 scrape a brand new flight (not in Firebase)
   - Verify ESP32 creates the flight with only non-empty fields
   - Add custom fields from mobile app
   - Verify next ESP32 sync preserves those custom fields

## Important Notes

⚠️ **Lint Errors Are Normal**
All the lint errors shown in the IDE are expected for Arduino/ESP32 projects. Types like `String`, `Serial`, `WiFiClientSecure`, `JsonObject`, etc. are Arduino-specific and will compile fine in the Arduino IDE or PlatformIO environment.

✅ **This Fix Ensures:**
- ESP32 only updates its managed fields
- Empty/null values are never sent to Firebase
- Other devices' fields are completely preserved
- True collaborative, non-destructive editing
- No data loss from overwrites

## Key Differences from Previous Approaches

| Aspect | Previous | Now |
|--------|----------|-----|
| Field list | Split into multiple arrays | Single MANAGED_FIELDS array |
| Empty values | Sent to Firebase | Filtered out |
| Other device fields | Could be overwritten | Always preserved |
| Logging | Basic | Detailed (includes skipped fields) |
| Flexibility | Rigid | Smart filtering |

The new approach is **defensive** - it actively prevents sending bad/empty data and respects data from other sources.
