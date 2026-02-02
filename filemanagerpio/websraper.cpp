#include "Global.h"

// Helper function to sanitize flight number by removing invalid characters
// Format: "PK 234" (airline code, ONE space, number)
// Flight numbers can only have ONE space - second space indicates garbage data
// Example: "QR 345 2015" should become "QR 345" (remove everything after 2nd
// space)
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

// Helper function to normalize flight key to YYMMDDHHMM_FLNR format
String NormalizeFlightKey(String keySTM, String flnr) {
  // Handle both YYMMDDHHMM (10 digits) and YYYYMMDDHHMM (12 digits) formats
  if (keySTM.length() == 12) {
    // Convert YYYYMMDDHHMM to YYMMDDHHMM (remove first 2 digits of year)
    keySTM = keySTM.substring(2);
    Serial.printf("🔧 Converted 4-digit year format: %s -> %s\n", keySTM.c_str() + 2, keySTM.c_str());
  }
  else if (keySTM.length() != 10) {
    // Try to extract digits from the string
    String cleanSTM = "";
    for (int i = 0; i < keySTM.length() && cleanSTM.length() < 12; i++) {
      if (isdigit(keySTM[i])) {
        cleanSTM += keySTM[i];
      }
    }

    // Handle 12-digit format first
    if (cleanSTM.length() == 12) {
      keySTM = cleanSTM.substring(2); // Convert YYYYMMDDHHMM to YYMMDDHHMM
      Serial.printf("🔧 Extracted and converted 4-digit year: %s -> %s\n", cleanSTM.c_str(), keySTM.c_str());
    }
    else if (cleanSTM.length() == 10) {
      keySTM = cleanSTM;
    }
    else {
      Serial.printf(
          "⚠️ Invalid STM format: '%s' (length: %d), cannot normalize\n",
          keySTM.c_str(), keySTM.length());
      return ""; // Return empty to skip this flight
    }
  }

  // Clean flight number
  flnr.trim();

  String key = keySTM + "_" + flnr;
  key.replace(" ", "");

  // Final validation
  if (key.length() < 12) {
    Serial.printf("⚠️ Invalid key format: '%s' (too short)\n", key.c_str());
    return "";
  }

  return key;
}

// Helper function to convert keySTM format (YYMMDDHHmm) to Unix timestamp
time_t ParseKeySTMTime(String keySTM) {
  if (keySTM.length() != 10)
    return 0;

  struct tm timeinfo = {0};
  timeinfo.tm_year =
      (keySTM.substring(0, 2).toInt() + 2000) - 1900;   // Year since 1900
  timeinfo.tm_mon = keySTM.substring(2, 4).toInt() - 1; // Month (0-11)
  timeinfo.tm_mday = keySTM.substring(4, 6).toInt();    // Day
  timeinfo.tm_hour = keySTM.substring(6, 8).toInt();    // Hour
  timeinfo.tm_min = keySTM.substring(8, 10).toInt();    // Minute
  timeinfo.tm_sec = 0;

  return mktime(&timeinfo);
}

// Define the flight data state machine variable (declared extern in Global.h)
FlightDataState flightDataState = IDLE;

// Increased JSON document sizes for more flight data capacity
StaticJsonDocument<32768> allData; // 32KB - temporary scratchpad
const char *url = "http://172.17.16.4/isb/maxcs/WebDavis/index.php";
int Page = -1;
int Pages = -1;
String phpSessionID = ""; // Will store captured PHPSESSID
bool SessoinIdFound = false;

void Fetch(String line, String category, String subCategory) {

  Page = -1;
  Pages = -1;
  fetchPHPSESSID();
  if (phpSessionID.length() > 0) {
    do {
      postRequest(line, Page, Pages, category, subCategory);
      delay(8500); // ~8.5 second delay between pages (35 pages over 5 min =
                   // 300s/35 = ~8.5s per page)
    } while (Page < Pages);

    Serial.println("✅ Fetch complete.");
  }
}
void fetchPHPSESSID() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(10000); // 10 second timeout
    http.begin(url);

    int httpCode = http.GET();

    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
      WiFiClient *stream = http.getStreamPtr();
      bool found = false;

      while (http.connected() && stream->available() && !found) {
        String line = stream->readStringUntil('\n');

        found = CheckSession(line);
      }
    } else {
      Serial.printf("GET request failed, error: %s\n",
                    http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("WiFi not connected!");
  }
}
bool extractPageInfo(String line) {
  // Find "Page:"
  int p1 = line.indexOf("Page:");
  if (p1 < 0)
    return false;

  // Extract current page
  p1 += 5; // move past "Page:"
  while (p1 < line.length() && line[p1] == ' ')
    p1++; // skip spaces

  int pFrom = line.indexOf("from", p1);
  if (pFrom < 0)
    return false;

  String currentStr = line.substring(p1, pFrom);
  currentStr.trim();
  Page = currentStr.toInt();

  // Extract total pages
  pFrom += 4; // skip "from"
  while (pFrom < line.length() && line[pFrom] == ' ')
    pFrom++;

  int pParen = line.indexOf("(", pFrom);
  if (pParen < 0)
    return false;

  String totalStr = line.substring(pFrom, pParen);
  totalStr.trim();
  Pages = totalStr.toInt();
  return true;
}

bool CheckSession(String line) {
  int idx = line.indexOf("name='PHPSESSID'");
  if (idx < 0)
    idx = line.indexOf("name=\"PHPSESSID\"");

  if (idx >= 0) {
    int valIdx = line.indexOf("value='", idx);
    if (valIdx < 0)
      valIdx = line.indexOf("value=\"", idx);
    if (valIdx >= 0) {
      valIdx += 7; // skip "value='"
      int endIdx = line.indexOf("'", valIdx);
      if (endIdx < 0)
        endIdx = line.indexOf("\"", valIdx);
      if (endIdx > valIdx) {
        phpSessionID = line.substring(valIdx, endIdx);
        SessoinIdFound = true;
      }
    }
  }

  return SessoinIdFound;
}

void ExtractTDInfoJSON(String trHtml, JsonDocument &doc, String Category,
                       String subCategory) {
  bool Checkin = false;
  if (Category == "CheckIn") {
    Checkin = true;
    Category = "Departure";
  }

  int pos = 0;
  String keySTM = "";
  String flnr = "";
  StaticJsonDocument<1024> rowTemp;
  String cremVal = "";
  String cremLuVal = "";
  if (!Checkin) {
    rowTemp["SubCat"] = subCategory;
  }

  while (true) {
    int tdStart = trHtml.indexOf("<td", pos);
    if (tdStart == -1)
      break;

    int tdEnd = trHtml.indexOf("</td>", tdStart);
    if (tdEnd == -1)
      break;

    int titleStart = trHtml.indexOf("title='", tdStart);
    String fieldName = "";

    if (titleStart >= 0 && titleStart < tdEnd) {
      titleStart += 7;
      int titleEnd = trHtml.indexOf("'", titleStart);
      if (titleEnd > titleStart) {
        String title = trHtml.substring(titleStart, titleEnd);
        int fIndex = title.indexOf("Field:");
        if (fIndex >= 0) {
          fieldName = title.substring(fIndex + 6);
          fieldName.trim();
          int sp = fieldName.indexOf(' ');
          if (sp > 0)
            fieldName = fieldName.substring(0, sp);
        }
      }
    }

    int valueIndex = trHtml.indexOf("Value:", tdStart);
    String value = "";

    if (valueIndex >= 0 && valueIndex < tdEnd) {
      valueIndex += 6;
      int valueEnd = trHtml.indexOf("<", valueIndex);
      if (valueEnd > valueIndex) {
        value = trHtml.substring(valueIndex, valueEnd);
        value.trim();
      }
    }

    if (fieldName.length() == 0 || value.length() == 0) {
      pos = tdEnd + 5;
      continue;
    }

    if (value.length() == 19 && value[4] == '-' && value[7] == '-' &&
        value[10] == ' ' && value[13] == ':' && value[16] == ':') {
      value = value.substring(2, 4) + value.substring(5, 7) +
              value.substring(8, 10) + value.substring(11, 13) +
              value.substring(14, 16);
    }

    if (fieldName == "stm") {
      // Ensure stm is in YYMMDDHHMM format (10 digits)
      // If it's in ISO format (YYYY-MM-DD HH:MM:SS), convert it
      if (value.length() == 19 && value[4] == '-' && value[7] == '-' &&
          value[10] == ' ' && value[13] == ':' && value[16] == ':') {
        // Extract: 2026-01-31 08:10:00 -> 2601310810
        value = value.substring(2, 4) + value.substring(5, 7) +
                value.substring(8, 10) + value.substring(11, 13) +
                value.substring(14, 16);
      }
      // If format has extra digits, handle 4-digit year (YYYYMMDDHHMM -> YYMMDDHHMM)
      else if (value.length() == 12 && value.substring(0, 4).toInt() > 1999) {
        // Extract last 10 digits from 4-digit year format
        value = value.substring(2);
        Serial.printf("🔧 Converted 4-digit year STM: %s\n", value.c_str());
      }
      // If format is wrong, try alternative patterns
      else if (value.length() > 10) {
        // Handle other date formats - extract numbers only
        String cleanValue = "";
        for (int i = 0; i < value.length() && cleanValue.length() < 12; i++) {
          if (isdigit(value[i])) {
            cleanValue += value[i];
          }
        }
        // Handle 12-digit format (YYYYMMDDHHMM)
        if (cleanValue.length() == 12) {
          value = cleanValue.substring(2); // Convert to YYMMDDHHMM
          Serial.printf("🔧 Extracted and converted STM: %s\n", value.c_str());
        }
        else if (cleanValue.length() == 10) {
          value = cleanValue;
        }
        else {
          Serial.printf("⚠️ Invalid STM format, skipping: %s\n", value.c_str());
          pos = tdEnd + 5;
          continue;
        }
      }
      keySTM = value;
    } else if (fieldName == "flnr") {
      // Sanitize flight number to remove any garbage characters
      flnr = SanitizeFlightNumber(value);
      Serial.printf("  🧹 Cleaned flnr: '%s' (original: '%s')\n", flnr.c_str(),
                    value.c_str());
      // Update value to use cleaned version
      value = flnr;
    }

    rowTemp[fieldName] = value;
    if (fieldName == "crem")
      cremVal = value;
    if (fieldName == "crem_lu")
      cremLuVal = value;

    pos = tdEnd + 5;
  }

  if (keySTM.isEmpty() || flnr.isEmpty())
    return;

  // Normalize and validate the flight key
  String key = NormalizeFlightKey(keySTM, flnr);
  if (key.isEmpty()) {
    Serial.printf(
        "[SKIP] Flight key normalization failed: stm='%s', flnr='%s'\n",
        keySTM.c_str(), flnr.c_str());
    return;
  }

  key.replace(" ", "");

  if (!allData.containsKey(Category))
    allData.createNestedObject(Category);

  JsonObject categoryObj = allData[Category].as<JsonObject>();

  JsonObject targetRow;
  if (categoryObj.containsKey(key))
    targetRow = categoryObj[key].as<JsonObject>();
  else
    targetRow = categoryObj.createNestedObject(key);

  for (JsonPair kv : rowTemp.as<JsonObject>()) {
    String k = kv.key().c_str();
    String v = kv.value().as<String>();

    if (k == "ckco") {

      if (!targetRow.containsKey("ckco"))
        targetRow.createNestedObject("ckco");

      if (!targetRow["ckco"].containsKey(v))
        targetRow["ckco"].createNestedArray(v);

      JsonArray arr = targetRow["ckco"][v].as<JsonArray>();
      // arr.clear();

      arr.add(cremVal);

      arr.add(cremLuVal);
    } else if (k != "crem" && k != "crem_lu") // ❗ block them here
    {
      targetRow[k] = kv.value();
    }
  }
}

void postRequest(String cnfg, int pg, int pgs, String category,
                 String subCategory) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(10000); // 10 second timeout
    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData = "PHPSESSID=" + phpSessionID;

    if (pg == -1 || pgs == -1) {
      postData += "&configs=" + cnfg;
    } else {
      postData += "&n=>";
    }

    int httpCode = http.POST(postData);
    SessoinIdFound = false;
    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
      WiFiClient *stream = http.getStreamPtr();
      bool inRow = false;
      String rowData = "";

      while (http.connected() && stream->available()) {
        String line = stream->readStringUntil('\n');
        if (extractPageInfo(line)) {
          // UPDATE STATUS with detailed page progress
          String pageStatus = category;
          if (subCategory.length() > 0) {
            pageStatus += " (" + subCategory + ")";
          }
          pageStatus += " - Page " + String(Page) + " of " + String(Pages);
          esp32Status = pageStatus;
          esp32StatusIcon = "📥";

          Serial.print(Page);
          Serial.print(" Out of ");
          Serial.println(Pages);
        }
        if (!SessoinIdFound) {
          CheckSession(line);
        }

        line.trim();
        if (line.length() == 0)
          continue;

        if (!inRow && (line.indexOf("<tr class='row_1'>") >= 0 ||
                       line.indexOf("<tr class='row_0'>") >= 0)) {
          inRow = true;
          rowData = line;
          continue;
        }

        if (inRow) {
          rowData += " " + line;
          if (line.indexOf("</tr>") >= 0) {
            inRow = false;
            ExtractTDInfoJSON(rowData, allData, category, subCategory);
            rowData = "";
          }
        }
      }
    } else {
      Serial.printf("POST request failed, error: %s\n",
                    http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("WiFi not connected!");
  }
}
