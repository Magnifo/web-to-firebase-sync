/*
  ESP32-S3 Flight Manager

  BOARD SETTINGS REQUIRED:
  - Board: ESP32S3 Dev Module
  - Flash Size: 16MB
  - Partition Scheme: Custom (partitions.csv in sketch folder)
  - USB Mode: Hardware CDC and JTAG
  - USB CDC On Boot: Enabled
*/

#include "Global.h"
// Global server object
AsyncWebServer server(SERVER_PORT);

void setup() {
  Serial.begin(DEFAULT_BAUD_RATE);
  delay(100);

  // UPDATE STATUS
  esp32Status = "System starting...";
  esp32StatusIcon = "🚀";

  Serial.println("\n\n=== ESP32-S3 File Manager Starting ===\n");

  Serial.printf("Internal RAM: %d bytes free\n", ESP.getFreeHeap());
  Serial.printf("PSRAM: %d bytes total, %d bytes free\n", ESP.getPsramSize(),
                ESP.getFreePsram());

  esp32Status = "Initializing LittleFS...";
  esp32StatusIcon = "💾";

  if (!LittleFS_Init()) {
    esp32Status = "ERROR: LittleFS failed";
    esp32StatusIcon = "❌";
    Serial.println("System halted - LittleFS initialization failed");
    return;
  }

  Auth_Init();

  esp32Status = "Connecting to WiFi...";
  esp32StatusIcon = "📶";
  WiFi_Init();

  NTP_Init(); // Re-enabled for timestamp updates

  esp32Status = "Connecting to Firebase...";
  esp32StatusIcon = "🔥";
  Firebase_Init();

  esp32Status = "Starting web server...";
  esp32StatusIcon = "🌐";
  Server_Setup();
  server.begin();

  Serial.println("HTTP Server Started");
  Serial.print("Access at: http://");
  Serial.print(WiFi_GetLocalIP());
  Serial.println("/");

  // UPDATE STATUS - READY
  esp32Status = "System ready";
  esp32StatusIcon = "✅";
  esp32UptimeStart = millis();

  delay(2000);
}

void loop() {
  WiFi_STA_Reconnect_Handler();
  app.loop();

  // EMERGENCY SAFEGUARD: Force restart after 10 minutes no matter what
  // This prevents infinite loops or stuck states
  static unsigned long emergencyStartTime = millis();
  if (millis() - emergencyStartTime > 600000) { // 10 minutes
    Serial.println("\n⚠️ EMERGENCY RESTART TRIGGERED (10 min timeout)");
    Serial.println("🔄 Forcing restart to prevent stuck state...");
    delay(1000);
    ESP.restart();
  }

  if ((millis() - lastFetchTime) >= FETCH_INTERVAL || Start) {
    // Reset emergency timer when starting new cycle
    emergencyStartTime = millis();
    // Clear data and report memory before processing
    allData.clear();
    Start = false;

    esp32Status = "Starting data sync...";
    esp32StatusIcon = "🔄";

    Serial.printf("\nMemory before sync - Free: %d, PSRAM: %d\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());

    // ============ FETCH REAL FLIGHT DATA ============
    esp32Status = "Fetching departure flights...";
    esp32StatusIcon = "📥";
    Serial.println("\n⚡ FETCHING DEPARTURE FLIGHTS (INT)");
    Fetch("configs/FREE/departure_int.cfg", "Departure", "INT");

    Serial.println("\n⚡ FETCHING DEPARTURE FLIGHTS (DOM)");
    Fetch("configs/FREE/departure_dom.cfg", "Departure", "DOM");

    Serial.println("\n⚡ FETCHING CHECKIN DATA");
    Fetch("configs/FREE/checkin.cfg", "CheckIn", "");

    // Wait for Firebase with timeout (30 seconds max)
    unsigned long fbWaitStart = millis();
    while (!Firebase_IsReady() && (millis() - fbWaitStart < 30000)) {
      app.loop();
      delay(10);
    }

    if (!Firebase_IsReady()) {
      Serial.println(
          "⚠️ Firebase not ready after 30s, skipping DEPARTURE upload");
    }

    esp32Status = "Uploading departures...";
    esp32StatusIcon = "📤";
    Serial.println("\nCompiling DEPARTURE flights for Firebase...");
    if (allData.containsKey("Departure")) {
      JsonObject departures = allData["Departure"].as<JsonObject>();
      int totalDepFlights = departures.size();

      int processCount = 0;
      int successCount = 0;

      for (JsonPair p : departures) {
        String flightKey = String(p.key().c_str());
        JsonObject localFlight = p.value().as<JsonObject>();

        // Create filtered flight document (only non-empty managed fields)
        DynamicJsonDocument *filteredFlightDoc =
            CreateFilteredFlightDoc(localFlight);

        processCount++;

        // UPDATE STATUS with detailed upload progress
        esp32Status = "Uploading departures " + String(processCount) + "/" +
                      String(totalDepFlights);
        esp32StatusIcon = "📤";

        Serial.printf("%d/%d: Updating %s\n", processCount, totalDepFlights,
                      flightKey.c_str());

        // Update INDIVIDUAL flight - this ensures field-level merging
        if (UpdateFlight("Departure", flightKey, *filteredFlightDoc)) {
          successCount++;
          Serial.printf("✅ Success! Total uploaded: %d/%d\n", successCount,
                        totalDepFlights);
        } else {
          Serial.printf("❌ Failed to update %s\n", flightKey.c_str());
        }

        delete filteredFlightDoc;

        // Small delay between updates + keep Firebase active
        delay(100);
        app.loop();
      }

      Serial.printf("\n✅ DEPARTURE complete: %d/%d flights uploaded\n",
                    successCount, totalDepFlights);
    }

    allData.clear();

    esp32Status = "Waiting before arrivals...";
    esp32StatusIcon = "⏳";
    // Wait for Firebase connection to stabilize before next phase
    Serial.println("\n⏳ Waiting 2 seconds before arrivals...");
    for (int i = 0; i < 20; i++) {
      app.loop(); // Keep Firebase active
      delay(100);
    }

    // ============ PHASE 2: FETCH ARRIVALS ============
    esp32Status = "Fetching arrival flights...";
    esp32StatusIcon = "📥";
    Serial.println("\n⚡ FETCHING ARRIVAL FLIGHTS (INT)");
    Fetch("configs/FREE/arrival_int.cfg", "Arrival", "INT");

    Serial.println("\n⚡ FETCHING ARRIVAL FLIGHTS (DOM)");
    Fetch("configs/FREE/arrival_dom.cfg", "Arrival", "DOM");

    // Wait for Firebase with timeout (30 seconds max)
    Serial.println("⏳ Checking Firebase status before ARRIVAL upload...");
    unsigned long fbWaitStart2 = millis();
    while (!Firebase_IsReady() && (millis() - fbWaitStart2 < 30000)) {
      Serial.println("🔄 Waiting for Firebase...");
      app.loop();
      delay(500);
    }

    if (!Firebase_IsReady()) {
      Serial.println("⚠️ Firebase not ready after 30s, skipping ARRIVAL upload");
    }

    esp32Status = "Uploading arrivals...";
    esp32StatusIcon = "📤";
    Serial.println("\nCompiling ARRIVAL flights for Firebase...");
    if (allData.containsKey("Arrival")) {
      JsonObject arrivals = allData["Arrival"].as<JsonObject>();
      int totalArrFlights = arrivals.size();

      int processCount = 0;
      int successCount = 0;

      for (JsonPair p : arrivals) {
        String flightKey = String(p.key().c_str());
        JsonObject localFlight = p.value().as<JsonObject>();

        // Create filtered flight document (only non-empty managed fields)
        DynamicJsonDocument *filteredFlightDoc =
            CreateFilteredFlightDoc(localFlight);

        processCount++;

        // UPDATE STATUS with detailed upload progress
        esp32Status = "Uploading arrivals " + String(processCount) + "/" +
                      String(totalArrFlights);
        esp32StatusIcon = "📤";

        Serial.printf("%d/%d: Updating %s\n", processCount, totalArrFlights,
                      flightKey.c_str());

        // Update INDIVIDUAL flight - this ensures field-level merging
        if (UpdateFlight("Arrival", flightKey, *filteredFlightDoc)) {
          successCount++;
          Serial.printf("✅ Success! Total uploaded: %d/%d\n", successCount,
                        totalArrFlights);
        } else {
          Serial.printf("❌ Failed to update %s\n", flightKey.c_str());
        }

        delete filteredFlightDoc;

        // Small delay between updates + keep Firebase active
        delay(100);
        app.loop();
      }

      Serial.printf("\n✅ ARRIVAL complete: %d/%d flights uploaded\n",
                    successCount, totalArrFlights);
    }

    allData.clear();

    esp32Status = "Updating timestamp...";
    esp32StatusIcon = "📅";
    // Update last sync timestamp in Firebase
    Serial.println("\n📅 Updating sync timestamp...");
    Firebase_UpdateLastSyncTime();

    Serial.println("\n✅ Sync cycle complete (success or failure)\n");
    lastFetchTime = millis();

    // GUARANTEED RESTART: Wait 5 minutes then restart no matter what
    // This ensures ESP32 never gets stuck in a bad state
    Serial.println("⏳ Waiting 5 minutes before GUARANTEED restart...");
    Serial.println("   (Restart will happen even if WiFi/Firebase failed)");
    delay(300000); // 5 minutes

    Serial.println("\n🔄 RESTARTING ESP32 NOW...");
    delay(500);
    ESP.restart(); // ← UNCONDITIONAL RESTART
  }

  delay(10);
}
