# ESP32 Event Monitoring Feature - Implementation Guide

## What Was Added

### 1. HTML (filemanager.html) ✅
Added an "ESP32 Status & Events" section that displays:
- Current event with icon and message
- Event timestamp
- Connection status (Connected/Connecting/Error)
- Stats: Last Sync, Uptime, Free Memory

### 2. CSS (style.css) ✅
Added complete styling for the event monitor section with:
- Animated status indicators
- Color-coded event displays
- Responsive grid layout
- Pulse animations

### 3. JavaScript (EVENT_MONITOR_CODE.js) ✅
Created Firebase monitoring code that:
- Fetches `/Esp33Event` and `/Esp33Update` from Firebase every 10 seconds
- Updates the UI with event icon, message, and timestamp
- Shows connection status (green if updated < 10 min ago)
- Displays stats like uptime and free memory

---

## How to Complete the Setup

### Step 1: Add JavaScript to app.js

**Copy the content from `EVENT_MONITOR_CODE.js` and append it to the end of `app.js`**

Or manually add this import at the end of `filemanagerpio/littlefs/filemanager.html` before `</body>`:

```html
    <script src="app.js"></script>
    <script src="EVENT_MONITOR_CODE.js"></script> <!-- ADD THIS -->
</body>
```

### Step 2: Add ESP32 Event Update Function

Add this function to `MyFirebase.cpp`:

```cpp
// ============ UPDATE ESP32 EVENT IN FIREBASE ============
void Firebase_UpdateEvent(const char* eventType, const char* message, 
                         size_t freeMemory, unsigned long uptimeMs) {
  if (!Firebase_IsReady()) {
    Serial.println("⚠️ Firebase not ready, skipping event update");
    return;
  }

  // Get formatted timestamp
  String timestamp = NTP_GetFormattedTimestamp();
  
  // Calculate uptime
  unsigned long seconds = uptimeMs / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  char uptimeStr[32];
  sprintf(uptimeStr, "%luh %lumin", hours % 24, minutes % 60);

  // Format free memory
  char memoryStr[16];
  sprintf(memoryStr, "%d KB", freeMemory / 1024);

  // Create event JSON
  String eventJson = "{";
  eventJson += "\"type\":\"" + String(eventType) + "\",";
  eventJson += "\"message\":\"" + String(message) + "\",";
  eventJson += "\"timestamp\":\"" + timestamp + "\",";
  eventJson += "\"uptime\":\"" + String(uptimeStr) + "\",";
  eventJson += "\"freeMemory\":\"" + String(memoryStr) + "\"";
  eventJson += "}";

  Serial.printf("📡 Updating event: %s\n", message);

  // Reset flags
  completed = false;
  success = false;

  // Update the event at ROOT level: /Esp33Event
  String path = "/Esp33Event";
  object_t eventObj(eventJson);

  Database.set<object_t>(aClient, path.c_str(), eventObj, Firebase_PrintResult);

  // Wait for completion with 5 second timeout
  unsigned long start = millis();
  while (!completed && millis() - start < 5000) {
    app.loop();
    delay(10);
  }

  if (completed && success) {
    Serial.println("✅ Event updated");
  } else {
    Serial.println("❌ Event update failed");
  }
}
```

### Step 3: Add Function Declaration to Global.h

Add this line to `Global.h` in the Firebase functions section:

```cpp
void Firebase_UpdateEvent(const char* eventType, const char* message, 
                         size_t freeMemory, unsigned long uptimeMs);
```

### Step 4: Call Event Updates in Main Code

In `filemanagerpio.ino`, add event updates at key points:

```cpp
void setup() {
  // ... existing code ...
  
  Firebase_Init();
  
  // ADD THIS - Report boot event
  Firebase_UpdateEvent("boot", "ESP32 started successfully", 
                      ESP.getFreeHeap(), millis());
  
  Server_Setup();
  server.begin();
  
  // ... rest of code ...
}

void loop() {
  // ... existing code ...
  
  if ((millis() - lastFetchTime) >= FETCH_INTERVAL || Start) {
    // ADD THIS - Report sync start
    Firebase_UpdateEvent("sync_start", "Starting flight data sync", 
                        ESP.getFreeHeap(), millis());
    
    // ... fetch and upload code ...
    
    // ADD THIS - Report sync complete (before restart wait)
    Firebase_UpdateEvent("sync_complete", "Sync completed successfully", 
                        ESP.getFreeHeap(), millis());
    
    // Update timestamp (already exists)
    Firebase_UpdateLastSyncTime();
    
    // ... restart code ...
  }
}
```

---

## Firebase Data Structure

After implementation, your Firebase will look like this:

```json
{
  "Esp33Update": "15 Jan 2026 01:20 AM",
  "Esp33Event": {
    "type": "sync_complete",
    "message": "Sync completed successfully",
    "timestamp": "15 Jan 2026 01:20 AM",
    "uptime": "2h 15min",
    "freeMemory": "128 KB"
  },
  "flights": { ... }
}
```

---

## Event Types You Can Use

| Event Type | Icon | When to Use |
|------------|------|-------------|
| `boot` | 🚀 | ESP32 powered on |
| `wifi_connected` | 📶 | WiFi connection successful |
| `wifi_failed` | 📵 | WiFi connection failed |
| `firebase_ready` | 🔥 | Firebase authenticated |
| `firebase_failed` | ❌ | Firebase auth failed |
| `sync_start` | 🔄 | Starting data fetch |
 |`uploading` | 📤 | Uploading to Firebase |
| `sync_complete` | ✅ | Sync finished |
| `sync_failed` | ⚠️ | Sync had errors |
| `restart` | 🔁 | About to restart |
| `error` | ❌ | Generic error |

---

## How It Works

1. **ESP32** updates `/Esp33Event` in Firebase at major points
2. **Filemanager.html** fetches this data every 10 seconds via JavaScript
3. **UI updates** automatically with event icon, message, and stats
4. **Status indicator** shows green if updated < 10 min ago, red otherwise

---

## Benefits

✅ **See what ESP32 is doing** - Real-time status updates  
✅ **Monitor remotely** - No serial connection needed  
✅ **Track uptime & memory** - System health monitoring  
✅ **Beautiful UI** - Professional event display  
✅ **Auto-refresh** - Updates every 10 seconds  

---

## Testing

1. Upload ESP32 code with event updates
2. Open File Manager web interface
3. Look for "📡 ESP32 Status & Events" section
4. Should show current event and update every 10 seconds

**Your file manager now shows live ESP32 status!** 🎉
