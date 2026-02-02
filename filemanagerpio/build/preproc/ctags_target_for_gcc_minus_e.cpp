# 1 "c:\\Users\\JanShahid\\Desktop\\filemanagerpio\\filemanagerpio.ino"
/*

  ESP32-S3 Flight Manager

  

  BOARD SETTINGS REQUIRED:

  - Board: ESP32S3 Dev Module

  - Flash Size: 16MB

  - Partition Scheme: Custom (partitions.csv in sketch folder)

  - USB Mode: Hardware CDC and JTAG

  - USB CDC On Boot: Enabled

*/
# 12 "c:\\Users\\JanShahid\\Desktop\\filemanagerpio\\filemanagerpio.ino"
# 13 "c:\\Users\\JanShahid\\Desktop\\filemanagerpio\\filemanagerpio.ino" 2
// Global server object
AsyncWebServer server(80);

void setup()
{
  Serial0.begin(115200);
  delay(100);

  Serial0.println("\n\n=== ESP32-S3 File Manager Starting ===\n");

  Serial0.printf("Internal RAM: %d bytes free\n", ESP.getFreeHeap());
  Serial0.printf("PSRAM: %d bytes total, %d bytes free\n", ESP.getPsramSize(), ESP.getFreePsram());

  if (!LittleFS_Init())
  {
    Serial0.println("System halted - LittleFS initialization failed");
    return;
  }

  Auth_Init();
  WiFi_Init();
  NTP_Init();
  Firebase_Init();
  Server_Setup();
  server.begin();

  Serial0.println("HTTP Server Started");
  Serial0.print("Access at: http://");
  Serial0.print(WiFi_GetLocalIP());
  Serial0.println("/");

  // Upload sample flight data to Firebase
  delay(2000);
}



void loop()
{
  WiFi_STA_Reconnect_Handler();
  app.loop();

  // Print current time every loop iteration
  static unsigned long lastTimePrint = 0;
  if (millis() - lastTimePrint >= 10000) { // Print every 10 seconds
    Serial0.printf("[TIME] %s\n", NTP_GetTimeString().c_str());
    lastTimePrint = millis();
  }

  if ((millis() - lastFetchTime) >= FETCH_INTERVAL || Start)
  {
    // Clear data and report memory before processing
    allData.clear();
    Start = false;

    Serial0.printf("\nMemory before sync - Free: %d, PSRAM: %d\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());

    // ============ PHASE 1: FETCH DEPARTURES & CHECKIN ============
    // Distribute fetches across the interval to mimic normal user behavior
    // 5 minute interval = 300,000ms, distributed across ~4 fetches = ~75 second spacing

    Serial0.println("\n⚡ FETCHING DEPARTURE FLIGHTS (INT)");
    Fetch("configs/FREE/departure_all_int.cfg", "Departure", "INT");

    Serial0.println("\n⚡ FETCHING DEPARTURE FLIGHTS (DOM)");
    Fetch("configs/FREE/departure_all_dom.cfg", "Departure", "DOM");

    Serial0.println("\n⚡ FETCHING CHECKIN DATA");
    Fetch("configs/FREE/checkin.cfg", "CheckIn", "");

    while (!Firebase_IsReady())
    {
      app.loop();
      delay(10);
    }

    Serial0.println("\nCompiling DEPARTURE flights for Firebase...");
    if (allData.containsKey("Departure")) {
      JsonObject departures = allData["Departure"].as<JsonObject>();
      int totalDepFlights = departures.size();

      // Prepare batch for all flights
      DynamicJsonDocument depBatch(8192);
      JsonObject batchFlights = depBatch.to<JsonObject>();

      int processCount = 0;
      for (JsonPair p : departures) {
        String flightKey = String(p.key().c_str());
        JsonObject localFlight = p.value().as<JsonObject>();

        // Create filtered flight document
        DynamicJsonDocument* filteredFlightDoc = CreateFilteredFlightDoc(localFlight);
        JsonObject filteredLocal = filteredFlightDoc->as<JsonObject>();

        // Add to batch
        batchFlights[flightKey].set(filteredLocal);

        Serial0.printf("%d/%d: [ADD] %s\n", processCount + 1, totalDepFlights, flightKey.c_str());

        delete filteredFlightDoc;
        processCount++;

        // Small delay every 5 flights
        if (processCount % 5 == 0) {
          delay(50);
        }
      }

      // Send all flights in one batch update
      Serial0.printf("\n📦 Uploading %d DEPARTURE flights to Firebase...\n", batchFlights.size());
      if (UpdateFlightsBatch("Departure", batchFlights)) {
        Serial0.println("✅ DEPARTURE flights uploaded successfully!");
      } else {
        Serial0.println("❌ Failed to upload DEPARTURE flights");
      }
    }

    allData.clear();

    // ============ PHASE 2: FETCH ARRIVALS ============
    Serial0.println("\n⚡ FETCHING ARRIVAL FLIGHTS (INT)");
    Fetch("configs/FREE/arrival_all_int.cfg", "Arrival", "INT");

    Serial0.println("\n⚡ FETCHING ARRIVAL FLIGHTS (DOM)");
    Fetch("configs/FREE/arrival_all_dom.cfg", "Arrival", "DOM");

    Serial0.println("\nCompiling ARRIVAL flights for Firebase...");
    if (allData.containsKey("Arrival")) {
      JsonObject arrivals = allData["Arrival"].as<JsonObject>();
      int totalArrFlights = arrivals.size();

      // Prepare batch for all flights
      DynamicJsonDocument arrBatch(8192);
      JsonObject batchFlights = arrBatch.to<JsonObject>();

      int processCount = 0;
      for (JsonPair p : arrivals) {
        String flightKey = String(p.key().c_str());
        JsonObject localFlight = p.value().as<JsonObject>();

        // Create filtered flight document
        DynamicJsonDocument* filteredFlightDoc = CreateFilteredFlightDoc(localFlight);
        JsonObject filteredLocal = filteredFlightDoc->as<JsonObject>();

        // Add to batch
        batchFlights[flightKey].set(filteredLocal);

        Serial0.printf("%d/%d: [ADD] %s\n", processCount + 1, totalArrFlights, flightKey.c_str());

        delete filteredFlightDoc;
        processCount++;

        // Small delay every 5 flights
        if (processCount % 5 == 0) {
          delay(50);
        }
      }

      // Send all flights in one batch update
      Serial0.printf("\n📦 Uploading %d ARRIVAL flights to Firebase...\n", batchFlights.size());
      if (UpdateFlightsBatch("Arrival", batchFlights)) {
        Serial0.println("✅ ARRIVAL flights uploaded successfully!");
      } else {
        Serial0.println("❌ Failed to upload ARRIVAL flights");
      }
    }

    allData.clear();

    Serial0.println("✅ Sync cycle complete - waiting for next cycle (5 minutes)\n");
    lastFetchTime = millis();
  }

  delay(10);
}
