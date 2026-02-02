#line 1 "c:\\Users\\JanShahid\\Desktop\\filemanagerpio\\MyFirebase.cpp"
#include "Global.h"

// ============ FIREBASE GLOBAL OBJECTS ============
UserAuth user_auth(FIREBASE_API_KEY, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD, 3000);
FirebaseApp app;
WiFiClientSecure ssl_client;
AsyncClientClass aClient(ssl_client);
RealtimeDatabase Database;
bool firebaseReady = false;

volatile bool completed = false;
  bool success = false;

// ============ TIMEOUT MANAGEMENT VARIABLES ============
int currentTimeout = 10000;  // Start with 10 seconds
int maxTimeout = 120000;      // Maximum 120 seconds
int timeoutIncrement = 5000; // Increase by 5 seconds each time
int consecutiveTimeouts = 0;
int maxConsecutiveTimeouts = 2;
long lastResetTime = 0;
bool connectionResetInProgress = false;
unsigned long nextReconnectAttempt = 0;

// ============ FIREBASE CALLBACK FUNCTIONS ============
void Firebase_PrintResult(AsyncResult &aResult) {

  if (aResult.isError()) {
    Serial.printf("❌ Firebase Error: %s (Code: %d)\n", aResult.error().message().c_str(), aResult.error().code());
    success = false;
  } else if (aResult.available()) {
    Serial.printf("✅ Firebase Success: %s\n", aResult.c_str());
    success = true;
  }
  completed = true;
  
}

// ============ VERIFY USER WITH GOOGLE IDENTITY TOOLKIT ============
bool Firebase_VerifyUser(const String &apiKey, const String &email, const String &password) {
  if (ssl_client.connected()) ssl_client.stop();

  String host = "www.googleapis.com";
  bool ret = false;

  if (ssl_client.connect(host.c_str(), 443) > 0) {
    String payload = "{\"email\":\"" + email + "\",\"password\":\"" + password + "\",\"returnSecureToken\":true}";
    String header = "POST /identitytoolkit/v3/relyingparty/verifyPassword?key=" + apiKey + " HTTP/1.1\r\n";
    header += "Host: " + host + "\r\n";
    header += "Content-Type: application/json\r\n";
    header += "Content-Length: " + String(payload.length()) + "\r\n\r\n";

    if (ssl_client.print(header) == header.length() && ssl_client.print(payload) == payload.length()) {
      unsigned long ms = millis();
      while (ssl_client.connected() && ssl_client.available() == 0 && millis() - ms < 5000) delay(1);

      ms = millis();
      while (ssl_client.connected() && ssl_client.available() && millis() - ms < 5000) {
        String line = ssl_client.readStringUntil('\n');
        if (line.indexOf("HTTP/1.1 200 OK") > -1) {
          ret = true;
          break;
        }
      }
      ssl_client.stop();
    }
  }
  return ret;
}

// ============ INITIALIZE FIREBASE ============
void Firebase_Init() {
  ssl_client.setInsecure();

  Serial.println("\n🔐 Initializing Firebase...");
  Serial.print("   - Verifying user credentials... ");
  
  if (!Firebase_VerifyUser(FIREBASE_API_KEY, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD)) {
    Serial.println("❌ FAILED!");
    Serial.println("   Firebase authentication failed. Check credentials in MyFirebase.h");
    return;
  }
  
  Serial.println("✅ SUCCESS");
  Serial.print("   - Initializing Firebase app... ");

  // Initialize Firebase App
  initializeApp(aClient, app, getAuth(user_auth), Firebase_PrintResult, "authTask");
  
  // Get Realtime Database instance
  app.getApp<RealtimeDatabase>(Database);
  Database.url(FIREBASE_DATABASE_URL);
  
  // Wait up to 10s for app to be ready, servicing loop
  unsigned long startWait = millis();
  while (!app.ready() && millis() - startWait < 10000) {
    app.loop();
    delay(10);
  }
  firebaseReady = app.ready();

  if (firebaseReady) {
    Serial.println("✅ SUCCESS");
    Serial.println("🚀 Firebase initialized and ready!\n");
  } else {
    Serial.println("❌ Firebase app not ready after init (10s)");
  }
}

// ============ CHECK IF FIREBASE IS READY ============
bool Firebase_IsReady() {
  return firebaseReady && app.ready();
}

// ============ RESET FIREBASE CONNECTION ============
bool Firebase_ResetConnection() {
  if (connectionResetInProgress) {
    Serial.println("🔄 Connection reset already in progress...");
    return false;
  }
  
  connectionResetInProgress = true;
  Serial.println("🔧 Resetting Firebase connection...");
  
  // Check available memory
  size_t freeHeap = ESP.getFreeHeap();
  Serial.printf("   - Free heap: %d bytes\n", freeHeap);
  
  // If memory is critically low, recommend restart instead
  if (freeHeap < 30000) {
    Serial.println("   ⚠️  Memory critically low! Consider ESP.restart()");
    Serial.println("   - Attempting to continue with reduced operations...");
  }

  // Ensure WiFi is connected before doing anything
  if (!WiFi_IsConnected()) {
    Serial.println("   - WiFi offline, waiting up to 10s to reconnect...");
    unsigned long wstart = millis();
    while (!WiFi_IsConnected() && millis() - wstart < 10000) {
      delay(50);
    }
    if (!WiFi_IsConnected()) {
      Serial.println("   ❌ WiFi still offline, aborting reset");
      connectionResetInProgress = false;
      return false;
    }
  }
  
  // Stop current SSL connection
  if (ssl_client.connected()) {
    ssl_client.stop();
    Serial.println("   - SSL connection stopped");
  }
  
  // No direct public stop/deinit; rely on re-init after TLS stop
  
  // Wait for connection to fully close and memory to settle
  delay(500);
  
  // Force garbage collection
  Serial.printf("   - Free heap after cleanup: %d bytes\n", ESP.getFreeHeap());
  
  // Reset SSL client
  ssl_client.setInsecure();
  Serial.println("   - SSL client reset");
  
  // Reinitialize Firebase with re-auth
  firebaseReady = false;
  Serial.println("   - Re-verifying credentials...");
  if (!Firebase_VerifyUser(FIREBASE_API_KEY, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD)) {
    Serial.println("   ❌ Re-authentication failed (possibly out of memory)");
    Serial.printf("   - Free heap: %d bytes\n", ESP.getFreeHeap());
    connectionResetInProgress = false;
    nextReconnectAttempt = millis() + 30000; // try again in 30s
    return false;
  }

  Serial.println("   - Reinitializing Firebase app...");
  initializeApp(aClient, app, getAuth(user_auth), Firebase_PrintResult, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(FIREBASE_DATABASE_URL);

  // Wait for app readiness up to 15s, servicing loop
  unsigned long startWait = millis();
  while (!app.ready() && millis() - startWait < 15000) {
    app.loop();
    delay(10);
  }

  firebaseReady = app.ready();
  connectionResetInProgress = false;
  
  if (firebaseReady) {
    Serial.println("\n✅ Firebase connection reset successful!");
    consecutiveTimeouts = 0;  // Reset timeout counter
    currentTimeout = 10000;   // Reset to initial timeout
    lastResetTime = millis();
    return true;
  } else {
    Serial.println("\n❌ Firebase connection reset failed!");
    nextReconnectAttempt = millis() + 30000; // avoid thrashing, schedule next attempt
    return false;
  }
}

// Ensure Firebase is ready; attempts re-init if not
bool Firebase_EnsureReady(unsigned long maxWaitMs) {
  if (Firebase_IsReady()) return true;

  // throttle re-init attempts
  if (millis() < nextReconnectAttempt) return false;

  Serial.println("🔎 Firebase not ready, attempting re-init...");
  Firebase_Init();

  unsigned long start = millis();
  while (!Firebase_IsReady() && millis() - start < maxWaitMs) {
    app.loop();
    delay(10);
  }

  if (!Firebase_IsReady()) {
    nextReconnectAttempt = millis() + 30000; // retry later
    Serial.println("❌ Firebase still not ready after re-init attempt");
    return false;
  }
  Serial.println("✅ Firebase ready after re-init");
  return true;
}

// // ============ UPLOAD FLIGHT DATA TO FIREBASE ============
// bool UpdateFlight(const String &category, const String &flightKey, const JsonDocument &fieldsToUpdate){
//   if (!Firebase_IsReady()) {
//     Serial.println("❌ Firebase not ready. Call Firebase_Init() first.");
//     return;
//   }
//   String path = "/" + category + "/" + flightKey;
//   Database.set<object_t>(aClient, path.c_str(), object_t(jsonStr), Firebase_PrintResult);
// }
bool UpdateFlight(String category, String flightKey, const JsonDocument &fieldsToUpdate){
  if (!Firebase_IsReady()) {
    // Attempt to recover automatically
    if (!Firebase_EnsureReady(10000)) {
      Serial.println("❌ Firebase not ready. Call Firebase_Init() first.");
      return false;
    }
  }

  // Reset flags before each call
  completed = false;
  success = false;

  String path = "/flights/Islamabad/" + category + "/" + flightKey;

  // Serialize JSON to string
  String jsonStr;
  serializeJson(fieldsToUpdate, jsonStr);

  // Async request
  Database.update<object_t>(aClient, path, object_t(jsonStr), Firebase_PrintResult);

  // Blocking wait with current timeout
  unsigned long start = millis();
  while (!completed && millis() - start < currentTimeout) {
    app.loop();
    delay(10);
  }

  if (!completed) {
    consecutiveTimeouts++;
    Serial.printf("⏳ Timeout while waiting for Firebase response! (Timeout #%d, %ds)\n", 
                  consecutiveTimeouts, currentTimeout/1000);
    
    // If we've had multiple consecutive timeouts, try to reset connection
    if (consecutiveTimeouts >= maxConsecutiveTimeouts) {
      Serial.println("🔧 Too many consecutive timeouts, attempting connection reset...");
      
      // DON'T clear allData here - it causes crashes when iterating
      // The reset itself will free SSL buffers which is sufficient
      Serial.printf("   - Free heap before reset: %d bytes\n", ESP.getFreeHeap());
      
      if (Firebase_ResetConnection()) {
        Serial.println("✅ Connection reset successful, retrying operation...");
        
        // Retry the operation once after reset
        completed = false;
        success = false;
        Database.update<object_t>(aClient, path, object_t(jsonStr), Firebase_PrintResult);
        
        start = millis();
        while (!completed && millis() - start < currentTimeout) {
          app.loop();
          delay(10);
        }
        
        if (!completed) {
          Serial.println("❌ Operation failed even after connection reset");
          // Increase timeout for next operations
          if (currentTimeout < maxTimeout) {
            currentTimeout += timeoutIncrement;
            Serial.printf("⏰ Increasing timeout to %d seconds\n", currentTimeout/1000);
          }
          nextReconnectAttempt = millis() + 30000;
          return false;
        } else {
          Serial.println("✅ Operation succeeded after connection reset");
          return success;
        }
      } else {
        Serial.println("❌ Connection reset failed, increasing timeout");
        if (currentTimeout < maxTimeout) {
          currentTimeout += timeoutIncrement;
          Serial.printf("⏰ Timeout increased to %d seconds\n", currentTimeout/1000);
        }
        nextReconnectAttempt = millis() + 30000;
        return false;
      }
    } else {
      // Just increase timeout gradually
      if (currentTimeout < maxTimeout) {
        currentTimeout += timeoutIncrement;
        Serial.printf("⏰ Increasing timeout to %d seconds for next operations\n", currentTimeout/1000);
      }
      return false;
    }
  }

  // Reset consecutive timeout counter on success
  if (success) {
    consecutiveTimeouts = 0;
    // Gradually reduce timeout back to normal if it was increased
    if (currentTimeout > 10000) {
      currentTimeout = max(10000, currentTimeout - 1000);
      Serial.printf("⏰ Reducing timeout to %d seconds\n", currentTimeout/1000);
    }
  }

  return success;
}


// ============ BATCH UPDATE MULTIPLE FLIGHTS AT ONCE ============
bool UpdateFlightsBatch(String category, JsonObject flightsToUpdate) {
  if (!Firebase_IsReady()) {
    // Attempt to recover automatically
    if (!Firebase_EnsureReady(10000)) {
      Serial.println("❌ Firebase not ready. Call Firebase_Init() first.");
      return false;
    }
  }

  // Reset flags before each call
  completed = false;
  success = false;

  // Build the batch update object with all flights
  String path = "/flights/Islamabad/" + category;
  
  // Serialize JSON to string
  String jsonStr;
  serializeJson(flightsToUpdate, jsonStr);

  Serial.printf("📦 Sending batch update for %d flights to %s\n", flightsToUpdate.size(), path.c_str());

  // Async request for batch update
  Database.update<object_t>(aClient, path.c_str(), object_t(jsonStr), Firebase_PrintResult);

  // Blocking wait with current timeout
  unsigned long start = millis();
  while (!completed && millis() - start < currentTimeout) {
    app.loop();
    delay(10);
  }

  if (!completed) {
    consecutiveTimeouts++;
    Serial.printf("⏳ Timeout while waiting for batch update! (Timeout #%d, %ds)\n", 
                  consecutiveTimeouts, currentTimeout/1000);
    
    // If we've had multiple consecutive timeouts, try to reset connection
    if (consecutiveTimeouts >= maxConsecutiveTimeouts) {
      Serial.println("🔧 Too many consecutive timeouts, attempting connection reset...");
      
      if (Firebase_ResetConnection()) {
        Serial.println("✅ Connection reset successful, retrying batch update...");
        
        // Retry the operation once after reset
        completed = false;
        success = false;
        Database.update<object_t>(aClient, path.c_str(), object_t(jsonStr), Firebase_PrintResult);
        
        start = millis();
        while (!completed && millis() - start < currentTimeout) {
          app.loop();
          delay(10);
        }
        
        if (!completed) {
          Serial.println("❌ Batch update failed even after connection reset");
          if (currentTimeout < maxTimeout) {
            currentTimeout += timeoutIncrement;
            Serial.printf("⏰ Increasing timeout to %d seconds\n", currentTimeout/1000);
          }
          nextReconnectAttempt = millis() + 30000;
          return false;
        } else {
          Serial.println("✅ Batch update succeeded after connection reset");
          return success;
        }
      } else {
        Serial.println("❌ Connection reset failed, increasing timeout");
        if (currentTimeout < maxTimeout) {
          currentTimeout += timeoutIncrement;
          Serial.printf("⏰ Timeout increased to %d seconds\n", currentTimeout/1000);
        }
        nextReconnectAttempt = millis() + 30000;
        return false;
      }
    } else {
      // Just increase timeout gradually
      if (currentTimeout < maxTimeout) {
        currentTimeout += timeoutIncrement;
        Serial.printf("⏰ Increasing timeout to %d seconds for next operations\n", currentTimeout/1000);
      }
      return false;
    }
  }

  // Reset consecutive timeout counter on success
  if (success) {
    consecutiveTimeouts = 0;
    // Gradually reduce timeout back to normal if it was increased
    if (currentTimeout > 10000) {
      currentTimeout = max(10000, currentTimeout - 1000);
      Serial.printf("⏰ Reducing timeout to %d seconds\n", currentTimeout/1000);
    }
    Serial.printf("✅ Successfully updated %d %s flights in batch!\n", flightsToUpdate.size(), category.c_str());
  }

  return success;
}

