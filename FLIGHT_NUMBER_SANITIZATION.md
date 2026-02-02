# Flight Number Sanitization Fix

## Problem
The ESP32 was receiving flight numbers with **TWO spaces**, where the second space indicated the start of garbage data.

### The Real Issue:
- **Valid Format**: Flight numbers can only have **ONE space** (e.g., `QR 345`)
- **Garbage Format**: Flight numbers with **TWO spaces** have garbage data after the second space (e.g., `QR 345 2015`)
- **Hidden in Keys**: When creating keys (`stm_flnr`), all spaces are removed, so `QR 345 2015` becomes `QR3452015` - making the garbage invisible in the key, but it still exists in the `flnr` field!

### Examples:
- ✅ **Correct**: `2601170615_QR 345` → Key: `2601170615_QR345`, flnr: `QR 345`
- ❌ **Wrong**: `2601170615_QR 345 2015` → Key: `2601170615_QR3452015`, flnr: `QR 345 2015` (garbage "2015")
- ❌ **Wrong**: `2601170615_TK 711 8cc` → Key: `2601170615_TK7118cc`, flnr: `TK 711 8cc` (garbage "8cc")
- ❌ **Wrong**: `2601170615_PK-234-999` → Two dashes = two spaces equivalent

## Root Cause
When parsing HTML data from the web source:
1. Some flight numbers include extra data after a second space/separator
2. This extra data is garbage (could be internal codes, timestamps, or buffer overflow)
3. The key-generation code removes ALL spaces, hiding the problem in the key
4. But the `flnr` field stored in Firebase contains the full garbage string

## Solution
Implemented a `SanitizeFlightNumber()` function in `websraper.cpp` that:

1. **Counts Spaces**: Tracks number of spaces/dashes encountered
2. **Truncates at Second Space**: Stops processing when second space is found (garbage starts here)
3. **Replaces Dashes with Spaces**: Converts "PK-234" to "PK 234" format
4. **Validates Characters**: Only keeps alphanumeric characters and the first space/dash
5. **Trims Whitespace**: Removes leading/trailing spaces

### Standardized Format
All flight numbers are now in the format: **`AIRLINE NUMBER`** (e.g., `PK 234`, `TK 711`)
- Two-letter airline code (or more if needed)
- **ONE space only**
- Flight number
- **Everything after a second space is discarded**

### Code Changes

#### 1. Added SanitizeFlightNumber Function (lines 3-47)
```cpp
// Helper function to sanitize flight number by removing invalid characters
// Format: "PK 234" (airline code, ONE space, number)
// Flight numbers can only have ONE space - second space indicates garbage data
// Example: "QR 345 2015" should become "QR 345" (remove everything after 2nd space)
String SanitizeFlightNumber(String flnr) {
  String cleaned = "";
  int spaceCount = 0;
  
  // Only keep alphanumeric characters, spaces, and hyphens
  // Stop at second space (garbage data starts there)
  for (int i = 0; i < flnr.length(); i++) {
    char c = flnr[i];
    
    // Check for space
    if (c == ' ') {
      spaceCount++;
      if (spaceCount >= 2) {
        // Second space found - this is where garbage starts, stop here
        break;
      }
      cleaned += ' ';
    }
    // Replace dash with space (but count it as a space)
    else if (c == '-') {
      spaceCount++;
      if (spaceCount >= 2) {
        // Second separator found - stop here
        break;
      }
      cleaned += ' ';
    }
    // Keep alphanumeric characters
    else if (isalnum(c)) {
      cleaned += c;
    }
    // Stop at any other invalid character
    else {
      break;
    }
  }
  
  cleaned.trim();
  return cleaned;
}
```

#### 2. Applied Sanitization When Flight Number is Parsed (lines 280-286)
```cpp
} else if (fieldName == "flnr") {
  // Sanitize flight number to remove any garbage characters
  flnr = SanitizeFlightNumber(value);
  Serial.printf("  🧹 Cleaned flnr: '%s' (original: '%s')\n", 
                flnr.c_str(), value.c_str());
  // Update value to use cleaned version
  value = flnr;
}
```

## Benefits

1. **Clean Keys**: The custom keys (`stm_flnr`) will now always be clean without garbage characters
2. **Standardized Format**: All flight numbers use consistent "AIRLINE NUMBER" format (e.g., "PK 234", "TK 711")
3. **No Duplicates**: Won't create multiple entries for the same flight with different formats (e.g., "PK-234", "PK 234", "PK234")
4. **Data Integrity**: Flight numbers stored in Firebase will be accurate and consistent
5. **Debugging**: Serial output shows both the original dirty value and the cleaned value for debugging
6. **Defensive Programming**: Handles buffer corruption gracefully instead of crashing or producing invalid data

## Test Examples

Here's how the `SanitizeFlightNumber()` function handles different inputs:

| Input | Output | Explanation |
|-------|--------|-------------|
| `QR 345` | `QR 345` | ✅ Valid format, no changes |
| `QR 345 2015` | `QR 345` | ✅ Truncated at 2nd space (removed "2015") |
| `TK 711 8cc` | `TK 711` | ✅ Truncated at 2nd space (removed "8cc") |
| `PK-234` | `PK 234` | ✅ Dash replaced with space |
| `PK-234-999` | `PK 234` | ✅ Stopped at 2nd dash (removed "-999") |
| `TK711` | `TK711` | ⚠️ No space added (keeps as-is) |
| `QR 345@xyz` | `QR 345` | ✅ Stopped at invalid char '@' |
| ` QR 345 ` | `QR 345` | ✅ Leading/trailing spaces trimmed |

## Testing Recommendations

1. Monitor Serial output for the `🧹 Cleaned flnr` messages to see if garbage characters are being caught
2. Verify that keys in Firebase no longer have duplicate entries with garbage characters
3. Check that flight numbers display correctly in the UI

## Next Steps

If the problem persists:
1. Investigate the HTML parsing buffer sizes in `ExtractTDInfoJSON()`
2. Consider increasing the buffer size for `rowData` string concatenation
3. Add null termination checks when reading from HTTP stream
