#line 1 "c:\\Users\\JanShahid\\Desktop\\filemanagerpio\\Globals.cpp"

#include "Global.h"

unsigned long lastFetchTime = 0;
const unsigned long FETCH_INTERVAL = 300000; // 10 minutes in milliseconds
bool Start = true;

// ============ HELPER FUNCTIONS ============
// List of allowed keys to keep in Firebase upload
const char* allowedKeys[] = {
  "stm",      // Time
  "flnr",     // Flight No
  "ect",      // Estimated Time
  "att",      // Actual Time
  "belt1",    // Belt
  "bre1",     // Status
  "city_lu",  // City
  "via1_lu",  // Via
  "prem_lu",  // Remark
  "fsta_lu",  // FSTA
  "cro1",     // Zone
  "cctf",     // Check-In From
  "cctt",     // Check-In To
  "gat1",     // Gate
  "crow",     // Row
  "ckco",     // Counter
  "aopn",     // Open
  "aclo",     // Close
  "crem",     // Remark
  "crem_lu",  // Remark
  "SubCat"
};
const int allowedKeysCount = 21;

// Create filtered flight document for Firebase upload
DynamicJsonDocument* CreateFilteredFlightDoc(JsonObject source) {
  DynamicJsonDocument* filteredDoc = new DynamicJsonDocument(512);
  JsonObject destObj = filteredDoc->to<JsonObject>();
  
  for (JsonPair p : source) {
    String key = String(p.key().c_str());
    
    // Check if key is in allowed list
    bool isAllowed = false;
    for (int i = 0; i < allowedKeysCount; i++) {
      if (key == allowedKeys[i]) {
        isAllowed = true;
        break;
      }
    }
    
    if (isAllowed) {
      destObj[key] = p.value();
    }
  }
  
  return filteredDoc;
}