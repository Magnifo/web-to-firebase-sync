# ESP32 Real-Time Status Feature - COMPLETE GUIDE

## ✅ **What I've Done:**

1. ✅ Added HTML event monitor section to `filemanager.html`
2. ✅ Added CSS styling to `style.css`
3. ✅ Added `/api/esp32status` endpoint to `MyServer.cpp`
4. ✅ Added status variables to `Globals.cpp`

---

## **What You Need to Do:**

### **Step 1: Add Variable Declarations to Global.h**

Find the line `extern bool Start;` in `Global.h` and add these lines right after it:

```cpp
extern bool Start;

// ============ ESP32 STATUS VARIABLES ============
extern String esp32Status;
extern String esp32StatusIcon;
extern unsigned long esp32UptimeStart;
```

---

### **Step 2: Update ESP32 Status Throughout Your Code**

In `filemanagerpio.ino`, update status at key operations:

```cpp
void setup() {
  Serial.begin(DEFAULT_BAUD_RATE);
  delay(100);

  // UPDATE STATUS
  esp32Status = "Initializing LittleFS...";
  esp32StatusIcon = "💾";
  
  Serial.println("\n\n=== ESP32-S3 File Manager Starting ===\n");

  if (!LittleFS_Init()) {
    esp32Status = "ERROR: LittleFS failed";
    esp32StatusIcon = "❌";
    Serial.println("System halted - LittleFS initialization failed");
    return;
  }

  // UPDATE STATUS
  esp32Status = "Connecting to WiFi...";
  esp32StatusIcon = "📶";
  
  Auth_Init();
  WiFi_Init();
  NTP_Init();
  
  // UPDATE STATUS
  esp32Status = "Connecting to Firebase...";
  esp32StatusIcon = "🔥";
  
  Firebase_Init();
  
  // UPDATE STATUS
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

  // EMERGENCY SAFEGUARD
  static unsigned long emergencyStartTime = millis();
  if (millis() - emergencyStartTime > 600000) {
    esp32Status = "Emergency restart (timeout)";
    esp32StatusIcon = "⚠️";
    Serial.println("\n⚠️ EMERGENCY RESTART TRIGGERED (10 min timeout)");
    delay(1000);
    ESP.restart();
  }

  if ((millis() - lastFetchTime) >= FETCH_INTERVAL || Start) {
    emergencyStartTime = millis();
    allData.clear();
    Start = false;

    // UPDATE STATUS
    esp32Status = "Fetching departure flights...";
    esp32StatusIcon = "📥";

    Serial.printf("\nMemory before sync - Free: %d, PSRAM: %d\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());

    // Fetch data
    Serial.println("\n⚡ FETCHING DEPARTURE FLIGHTS (INT)");
    Fetch("configs/FREE/departure_int.cfg", "Departure", "INT");

    Serial.println("\n⚡ FETCHING DEPARTURE FLIGHTS (DOM)");
    Fetch("configs/FREE/departure_dom.cfg", "Departure", "DOM");

    // UPDATE STATUS
    esp32Status = "Loading check-in data...";
    esp32StatusIcon = "📋";
    
    Serial.println("\n⚡ FETCHING CHECKIN DATA");
    Fetch("configs/FREE/checkin.cfg", "CheckIn", "");

    // Wait for Firebase
    unsigned long fbWaitStart = millis();
    while (!Firebase_IsReady() && (millis() - fbWaitStart < 30000)) {
      app.loop();
      delay(10);
    }
    
    if (!Firebase_IsReady()) {
      esp32Status = "Firebase not ready, skipping upload";
      esp32StatusIcon = "⚠️";
      Serial.println("⚠️ Firebase not ready after 30s, skipping DEPARTURE upload");
    } else {
      // UPDATE STATUS
      esp32Status = "Uploading departure flights...";
      esp32StatusIcon = "📤";
      
      Serial.println("\nCompiling DEPARTURE flights for Firebase...");
      if (allData.containsKey("Departure")) {
        JsonObject departures = allData["Departure"].as<JsonObject>();
        int totalDepFlights = departures.size();

        const int CHUNK_SIZE = 5;
        DynamicJsonDocument depBatch(4096);
        JsonObject batchFlights = depBatch.to<JsonObject>();

        int processCount = 0;
        int successCount = 0;

        for (JsonPair p : departures) {
          String flightKey = String(p.key().c_str());
          JsonObject localFlight = p.value().as<JsonObject>();

          DynamicJsonDocument *filteredFlightDoc = CreateFilteredFlightDoc(localFlight);
          JsonObject filteredLocal = filteredFlightDoc->as<JsonObject>();

          batchFlights[flightKey].set(filteredLocal);
          delete filteredFlightDoc;

          processCount++;
          
          // UPDATE STATUS
          esp32Status = "Uploading departures " + String(processCount) + "/" + String(totalDepFlights);
          esp32StatusIcon = "📤";
          
          Serial.printf("%d/%d: [ADD] %s\n", processCount, totalDepFlights, flightKey.c_str());

          if (batchFlights.size() >= CHUNK_SIZE || processCount == totalDepFlights) {
            Serial.printf("\n📦 Uploading chunk of %d DEPARTURE flights to Firebase...\n",
                          batchFlights.size());
            Serial.printf("   Free heap before upload: %d bytes\n", ESP.getFreeHeap());

            if (UpdateFlightsBatch("Departure", batchFlights)) {
              successCount += batchFlights.size();
              Serial.printf("✅ Chunk uploaded! Total uploaded: %d/%d\n",
                            successCount, totalDepFlights);
            } else {
              Serial.printf("❌ Failed to upload chunk\n");
            }

            depBatch.clear();
            batchFlights = depBatch.to<JsonObject>();
            delay(100);
            app.loop();
          }
        }

        Serial.printf("\n✅ DEPARTURE complete: %d/%d flights uploaded\n",
                      successCount, totalDepFlights);
      }
    }

    allData.clear();

    // UPDATE STATUS
    esp32Status = "Fetching arrival flights...";
    esp32StatusIcon = "📥";

    Serial.println("\n⏳ Waiting 2 seconds before arrivals...");
    for (int i = 0; i < 20; i++) {
      app.loop();
      delay(100);
    }

    Serial.println("\n⚡ FETCHING ARRIVAL FLIGHTS (INT)");
    Fetch("configs/FREE/arrival_int.cfg", "Arrival", "INT");

    Serial.println("\n⚡ FETCHING ARRIVAL FLIGHTS (DOM)");
    Fetch("configs/FREE/arrival_dom.cfg", "Arrival", "DOM");

    // Wait for Firebase
    Serial.println("⏳ Checking Firebase status before ARRIVAL upload...");
    unsigned long fbWaitStart2 = millis();
    while (!Firebase_IsReady() && (millis() - fbWaitStart2 < 30000)) {
      Serial.println("🔄 Waiting for Firebase...");
      app.loop();
      delay(500);
    }
    
    if (!Firebase_IsReady()) {
      esp32Status = "Firebase timeout, skipping arrivals";
      esp32StatusIcon = "⚠️";
      Serial.println("⚠️ Firebase not ready after 30s, skipping ARRIVAL upload");
    } else {
      // UPDATE STATUS
      esp32Status = "Uploading arrival flights...";
      esp32StatusIcon = "📤";
      
      Serial.println("\nCompiling ARRIVAL flights for Firebase...");
      if (allData.containsKey("Arrival")) {
        JsonObject arrivals = allData["Arrival"].as<JsonObject>();
        int totalArrFlights = arrivals.size();

        const int CHUNK_SIZE = 5;
        DynamicJsonDocument arrBatch(4096);
        JsonObject batchFlights = arrBatch.to<JsonObject>();

        int processCount = 0;
        int successCount = 0;

        for (JsonPair p : arrivals) {
          String flightKey = String(p.key().c_str());
          JsonObject localFlight = p.value().as<JsonObject>();

          DynamicJsonDocument *filteredFlightDoc = CreateFilteredFlightDoc(localFlight);
          JsonObject filteredLocal = filteredFlightDoc->as<JsonObject>();

          batchFlights[flightKey].set(filteredLocal);
          delete filteredFlightDoc;

          processCount++;
          
          // UPDATE STATUS
          esp32Status = "Uploading arrivals " + String(processCount) + "/" + String(totalArrFlights);
          esp32StatusIcon = "📤";
          
          Serial.printf("%d/%d: [ADD] %s\n", processCount, totalArrFlights, flightKey.c_str());

          if (batchFlights.size() >= CHUNK_SIZE || processCount == totalArrFlights) {
            Serial.printf("\n📦 Uploading chunk of %d ARRIVAL flights to Firebase...\n",
                          batchFlights.size());

            if (UpdateFlightsBatch("Arrival", batchFlights)) {
              successCount += batchFlights.size();
              Serial.printf("✅ Chunk uploaded! Total uploaded: %d/%d\n",
                            successCount, totalArrFlights);
            } else {
              Serial.printf("❌ Failed to upload chunk\n");
            }

            arrBatch.clear();
            batchFlights = arrBatch.to<JsonObject>();
            delay(100);
            app.loop();
          }
        }

        Serial.printf("\n✅ ARRIVAL complete: %d/%d flights uploaded\n",
                      successCount, totalArrFlights);
      }
    }

    allData.clear();

    // UPDATE STATUS
    esp32Status = "Updating Firebase timestamp...";
    esp32StatusIcon = "📅";

    Serial.println("\n📅 Updating sync timestamp...");
    Firebase_UpdateLastSyncTime();

    // UPDATE STATUS - DONE
    esp32Status = "Sync complete, waiting to restart...";
    esp32StatusIcon = "✅";

    Serial.println("\n✅ Sync cycle complete (success or failure)\n");
    lastFetchTime = millis();

    Serial.println("⏳ Waiting 5 minutes before GUARANTEED restart...");
    Serial.println("   (Restart will happen even if WiFi/Firebase failed)");
    
    // UPDATE STATUS
    esp32Status = "Waiting for restart (5 min)";
    esp32StatusIcon = "⏳";
    
    delay(300000); // 5 minutes

    // UPDATE STATUS
    esp32Status = "Restarting now...";
    esp32StatusIcon = "🔁";
    
    Serial.println("\n🔄 RESTARTING ESP32 NOW...");
    delay(500);
    ESP.restart();
  }

  delay(10);
}
```

---

### **Step 3: Update JavaScript to Fetch from ESP32**

Replace the content from `EVENT_MONITOR_CODE.js` with this **simplified version** that fetches from ESP32:

```javascript
// ============ ESP32 STATUS MONITORING (DIRECT FROM ESP32) ============

let statusMonitorInterval = null;

// Fetch and display ESP32 status directly from ESP32
async function fetchESP32Status() {
    try {
        const response = await fetch('/api/esp32status');
        
        if (!response.ok) {
            throw new Error('Failed to fetch status');
        }
        
        const data = await response.json();
        
        // Update event display
        updateEventDisplay({
            icon: data.icon || '⏳',
            message: data.message || 'No status',
            time: data.timestamp || '--',
            status: 'connected'
        });
        
        // Update stats
        document.getElementById('lastSyncTime').textContent = data.timestamp || '--';
        document.getElementById('uptimeValue').textContent = data.uptime || '--';
        document.getElementById('freeMemory').textContent = data.freeMemory || '--';
        
    } catch (error) {
        console.error('Error fetching ESP32 status:', error);
        updateEventDisplay({
            icon: '❌',
            message: 'Failed to connect to ESP32',
            time: new Date().toLocaleTimeString(),
            status: 'error'
        });
    }
}

// Update event display
function updateEventDisplay(data) {
    const eventStatus = document.getElementById('eventStatus');
    const eventIcon = document.getElementById('eventIcon');
    const eventMessage = document.getElementById('eventMessage');
    const eventTime = document.getElementById('eventTime');
    
    if (eventIcon) eventIcon.textContent = data.icon || '⏳';
    if (eventMessage) eventMessage.textContent = data.message || 'Waiting...';
    if (eventTime) eventTime.textContent = data.time || '--';
    
    // Update status badge
    if (eventStatus) {
        eventStatus.textContent = data.status === 'connected' ? '✅ Connected' : '❌ Error';
        eventStatus.className = 'event-status ' + (data.status || '');
    }
}

// Start monitoring when page loads
document.addEventListener('DOMContentLoaded', () => {
    setTimeout(() => {
        // Initial fetch
        fetchESP32Status();
        
        // Fetch every 10 seconds
        if (statusMonitorInterval) {
            clearInterval(statusMonitorInterval);
        }
        statusMonitorInterval = setInterval(fetchESP32Status, 10000);
    }, 1000);
});

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (statusMonitorInterval) {
        clearInterval(statusMonitorInterval);
    }
});
```

**Copy this code to the END of `filemanagerpio/littlefs/app.js`**

---

## **How It Works:**

1. **ESP32 updates `esp32Status` and `esp32StatusIcon`** at each operation:
   - "Fetching departure flights..."  📥
   - "Uploading departures 15/90" 📤
   - "Uploading arrivals 45/91" 📤
   - "Sync complete" ✅

2. **JavaScript fetches `/api/esp32status`** every 10 seconds

3. **File Manager UI updates** automatically showing:
   - Current operation with icon
   - Timestamp
   - Uptime
   - Free memory

---

## **Expected Output:**

When you open File Manager, you'll see:

```
┌─────────────────────────────────────────────────┐
│ 📡 ESP32 Status & Events    ✅ Connected        │
├─────────────────────────────────────────────────┤
│                                                 │
│   📤    Uploading departures 45/90             │
│         15 Jan 2026 01:45 AM                   │
│                                                 │
│  Last Sync: 15 Jan 2026 01:40 AM               │
│  Uptime: 2h 15m                                │
│  Free Memory: 128 KB                           │
└─────────────────────────────────────────────────┘
```

Updates every 10 seconds showing REAL-TIME progress!

---

**Your file manager now shows live ESP32 status directly from the device!** 🎉
