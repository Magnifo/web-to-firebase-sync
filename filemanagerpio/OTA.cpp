#include "Global.h"
#include <Update.h>
#include <esp_task_wdt.h>

// ============ OTA STATE ============
bool otaInProgress = false;
size_t otaTotalSize = 0;
size_t otaReceivedSize = 0;

// ============ OTA FILE UPLOAD HANDLER ============
void OTA_HandleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  // Check authentication on first chunk only
  if (index == 0) {
    // IP-based validation for OTA (no tokens)
    String clientIP = String(request->client()->remoteIP());
    
    // Check if this IP is logged in
    if (!Auth_IsUserLoggedIn(clientIP)) {
      Serial.printf("❌ OTA: Unauthorized access from IP: %s\n", clientIP.c_str());
      request->send(401, "text/plain", "Unauthorized");
      return;
    }
    
    Auth_UpdateActivity(clientIP);
    
    // Only accept .bin files
    if (!filename.endsWith(".bin")) {
      request->send(400, "text/plain", "Only .bin files are allowed");
      return;
    }
    
    // Start OTA update
    otaInProgress = true;
    otaReceivedSize = 0;
    otaTotalSize = request->contentLength();
    
    Serial.println("\n\n=== OTA UPDATE START ===");
    Serial.print("Filename: ");
    Serial.println(filename);
    Serial.print("File Size: ");
    Serial.println(otaTotalSize);
    
    // Disable WiFi power saving for stable OTA
    WiFi.setSleep(false);
    
    // Initialize OTA Update
    if (!Update.begin(otaTotalSize, U_FLASH)) {
      Update.printError(Serial);
      request->send(500, "text/plain", "OTA Begin Failed");
      otaInProgress = false;
      return;
    }
    
    Serial.println("OTA Update Started");
  }
  
  // Write data to flash
  if (otaInProgress && len > 0) {
    if (Update.write(data, len) != len) {
      Update.printError(Serial);
      request->send(500, "text/plain", "OTA Write Failed");
      otaInProgress = false;
      Update.end();
      return;
    }
    
    otaReceivedSize += len;
    
    // Progress indicator
    if (otaReceivedSize % 1024 == 0) {
      int progress = (otaReceivedSize * 100) / otaTotalSize;
      Serial.print("OTA Progress: ");
      Serial.print(progress);
      Serial.println("%");
    }
  }
  
  // Finalize OTA Update
  if (final && otaInProgress) {
    if (!Update.end(true)) {
      Update.printError(Serial);
      Serial.println("OTA Update Failed!");
      request->send(500, "text/plain", "OTA Finalization Failed");
      otaInProgress = false;
      return;
    }
    
    Serial.println("=== OTA UPDATE COMPLETE ===");
    Serial.println("Device will restart in 5 seconds...");
    
    // Send success response with Connection: close header
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", 
      "✅ Firmware updated successfully!\nDevice restarting in 5 seconds...");
    response->addHeader("Connection", "close");
    request->send(response);
    
    // Give browser time to receive response before restart
    delay(500);
    
    // Re-enable WiFi power saving
    WiFi.setSleep(true);
    
    // Wait 4.5 more seconds while keeping watchdog happy
    for (int i = 0; i < 9; i++) {
      delay(500);
      esp_task_wdt_reset();  // Reset watchdog timer
    }
    
    // Schedule restart
    otaInProgress = false;
    delay(4500);  // Total 5 seconds (500ms for response + 4500ms wait)
    ESP.restart();
  }
}

// ============ OTA UPLOAD COMPLETE HANDLER ============
void OTA_HandleFileUploadComplete(AsyncWebServerRequest *request) {
  // Already handled in upload handler
  if (!otaInProgress) {
    request->send(200, "text/plain", "OTA process complete");
  }
}

// ============ GET OTA STATUS ============
String OTA_GetStatus() {
  if (otaInProgress) {
    int progress = (otaReceivedSize * 100) / otaTotalSize;
    String status = "{\"status\":\"in_progress\",\"progress\":";
    status += progress;
    status += ",\"received\":";
    status += otaReceivedSize;
    status += ",\"total\":";
    status += otaTotalSize;
    status += "}";
    return status;
  } else {
    return "{\"status\":\"idle\"}";
  }
}
