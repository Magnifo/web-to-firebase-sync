
#include "Global.h"

unsigned long lastFetchTime = 0;
const unsigned long FETCH_INTERVAL = 300000; // 10 minutes in milliseconds
bool Start = true;

// ============ ESP32 STATUS VARIABLES ============
String esp32Status = "System starting...";
String esp32StatusIcon = "🚀";
unsigned long esp32UptimeStart = 0;

// ============ HELPER FUNCTIONS ============
// ESP32-MANAGED FIELDS: All fields that ESP32 is responsible for updating
const char *MANAGED_FIELDS[] = {
    "stm",     // Time
    "flnr",    // Flight No
    "ect",     // Estimated Time
    "att",     // Actual Time
    "blt1",    // Belt
    "bre1",    // Status
    "city_lu", // City
    "via1_lu", // Via
    "prem_lu", // Remark
    "fsta_lu", // FSTA
    "cro1",    // Zone
    "cctf",    // Check-In From
    "cctt",    // Check-In To
    "gat1",    // Gate
    "crow",    // Row
    "ckco",    // Counter
    "aopn",    // Open
    "aclo",    // Close
    "crem",    // Remark
    "crem_lu", // Remark
    "SubCat"   // Category
};
const int MANAGED_FIELDS_COUNT = 21;

// Helper function to check if a field is managed by ESP32
bool IsFieldManaged(const String &key) {
  for (int i = 0; i < MANAGED_FIELDS_COUNT; i++) {
    if (key == MANAGED_FIELDS[i]) {
      return true;
    }
  }
  return false;
}

// Helper function to check if a value is empty, null, or just whitespace
bool IsValueEmpty(JsonVariant value) {
  if (value.isNull()) {
    return true;
  }

  // For strings, check if empty or just whitespace
  if (value.is<String>()) {
    String strValue = value.as<String>();
    strValue.trim();
    if (strValue.length() == 0) {
      return true;
    }
  }

  // For objects (like ckco), check if it's empty
  if (value.is<JsonObject>()) {
    JsonObject obj = value.as<JsonObject>();
    if (obj.size() == 0) {
      return true;
    }
  }

  // For arrays, check if empty
  if (value.is<JsonArray>()) {
    JsonArray arr = value.as<JsonArray>();
    if (arr.size() == 0) {
      return true;
    }
  }

  return false;
}

// Create filtered flight document for Firebase upload
// IMPORTANT: Only includes fields that:
// 1. Are in MANAGED_FIELDS (ESP32 managed)
// 2. Have actual values (not empty, null, or whitespace)
// This ensures we don't overwrite existing data with empty values
DynamicJsonDocument *CreateFilteredFlightDoc(JsonObject source) {
  DynamicJsonDocument *filteredDoc = new DynamicJsonDocument(512);
  JsonObject destObj = filteredDoc->to<JsonObject>();

  int fieldCount = 0;
  int skippedCount = 0;
  String fieldsList = "";
  String skippedList = "";

  for (JsonPair p : source) {
    String key = String(p.key().c_str());

    // Only process managed fields
    if (IsFieldManaged(key)) {
      // Check if value is empty/null
      if (IsValueEmpty(p.value())) {
        skippedCount++;
        if (skippedList.length() > 0)
          skippedList += ", ";
        skippedList += key;
        continue; // Skip this field - don't send empty values
      }

      // Field has a valid value, include it
      destObj[key] = p.value();
      fieldCount++;
      if (fieldsList.length() > 0)
        fieldsList += ", ";
      fieldsList += key;
    }
  }

  // Log what fields we're including and skipping
  Serial.printf("  📋 Including %d fields: %s\n", fieldCount,
                fieldsList.c_str());
  if (skippedCount > 0) {
    Serial.printf("  ⏭️  Skipped %d empty fields: %s\n", skippedCount,
                  skippedList.c_str());
  }

  return filteredDoc;
}