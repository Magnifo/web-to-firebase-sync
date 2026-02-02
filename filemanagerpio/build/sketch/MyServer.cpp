#line 1 "c:\\Users\\JanShahid\\Desktop\\filemanagerpio\\MyServer.cpp"
#include "Global.h"

// Static variables for file upload
File fsUploadFile;
String uploadFilename;
size_t uploadTotalSize = 0;

// ============ MINIMAL BASIC HTML (Fallback) ============
String GetBasicPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3 File Manager</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #1a1a1a; color: #fff; }
        .container { max-width: 800px; margin: 0 auto; padding: 20px; }
        header { text-align: center; margin-bottom: 30px; }
        h1 { color: #4CAF50; margin-bottom: 10px; }
        .upload-section { background: #2a2a2a; padding: 20px; border-radius: 8px; margin-bottom: 20px; }
        .drop-zone { border: 2px dashed #4CAF50; padding: 40px; text-align: center; border-radius: 8px; cursor: pointer; transition: all 0.3s; }
        .drop-zone:hover { background: #333; }
        .drop-zone.dragover { background: #4CAF50; color: #000; }
        .file-input { display: none; }
        button { background: #4CAF50; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-size: 16px; }
        button:hover { background: #45a049; }
        .progress-bar { display: none; margin-top: 20px; height: 30px; background: #444; border-radius: 5px; overflow: hidden; }
        .progress-bar.active { display: block; }
        .progress-fill { height: 100%; background: #4CAF50; width: 0%; transition: width 0.3s; }
        .notification { position: fixed; top: 20px; right: 20px; padding: 15px 20px; border-radius: 5px; color: white; z-index: 1000; }
        .notification.success { background: #4CAF50; }
        .notification.error { background: #f44336; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>📁 File Manager</h1>
            <p>Upload filemanager.html to LittleFS for full features</p>
        </header>
        
        <div class="upload-section">
            <div class="drop-zone" id="dropZone">
                <h3>📤 Drop Files Here</h3>
                <p>or click to browse</p>
                <button class="upload-btn">Select Files</button>
                <input type="file" id="fileInput" class="file-input" multiple>
                <div class="progress-bar" id="progressBar">
                    <div class="progress-fill" id="progressFill"></div>
                </div>
            </div>
        </div>
    </div>

    <script>
        const dropZone = document.getElementById('dropZone');
        const fileInput = document.getElementById('fileInput');
        const progressBar = document.getElementById('progressBar');
        const progressFill = document.getElementById('progressFill');

        dropZone.addEventListener('click', () => fileInput.click());
        
        dropZone.addEventListener('dragover', (e) => {
            e.preventDefault();
            dropZone.classList.add('dragover');
        });
        
        dropZone.addEventListener('dragleave', () => {
            dropZone.classList.remove('dragover');
        });
        
        dropZone.addEventListener('drop', (e) => {
            e.preventDefault();
            dropZone.classList.remove('dragover');
            handleFiles(e.dataTransfer.files);
        });
        
        fileInput.addEventListener('change', (e) => {
            handleFiles(e.target.files);
        });

        async function handleFiles(files) {
            for (let file of files) {
                await uploadFile(file);
            }
        }

        async function uploadFile(file) {
            return new Promise((resolve, reject) => {
                const formData = new FormData();
                formData.append('file', file);
                
                progressBar.classList.add('active');
                const xhr = new XMLHttpRequest();
                
                xhr.upload.addEventListener('progress', (e) => {
                    if (e.lengthComputable) {
                        const percentComplete = (e.loaded / e.total) * 100;
                        progressFill.style.width = percentComplete + '%';
                    }
                });
                
                xhr.addEventListener('load', () => {
                    if (xhr.status === 200) {
                        showNotification(`"${file.name}" uploaded!`, 'success');
                        progressFill.style.width = '0%';
                        progressBar.classList.remove('active');
                        resolve();
                    } else {
                        showNotification(`Upload failed`, 'error');
                        reject();
                    }
                });
                
                xhr.addEventListener('error', () => {
                    showNotification(`Upload error`, 'error');
                    progressBar.classList.remove('active');
                    reject();
                });
                
                xhr.open('POST', '/api/upload');
                xhr.send(formData);
            });
        }

        function showNotification(message, type) {
            const notification = document.createElement('div');
            notification.className = `notification ${type}`;
            notification.textContent = message;
            document.body.appendChild(notification);
            setTimeout(() => notification.remove(), 3000);
        }
    </script>
</body>
</html>
  )rawliteral";
}

// ============ SERVER HANDLERS ============

// ============ IP-ONLY VALIDATION HELPER ============
bool ValidateIPFromRequest(AsyncWebServerRequest *request) {
  // Extract client IP from request
  String clientIP = String(request->client()->remoteIP());
  
  // Check if this IP is logged in (server-side only)
  if (!Auth_IsUserLoggedIn(clientIP)) {
    return false;
  }
  
  // Update activity to keep session alive
  Auth_UpdateActivity(clientIP);
  return true;
}

void Server_HandleRoot(AsyncWebServerRequest *request) {
  String clientIP = String(request->client()->remoteIP());
  
  // Check if IP is logged in (server-side only)
  if (Auth_IsUserLoggedIn(clientIP)) {
    // IP is authenticated - serve filemanager
    Auth_UpdateActivity(clientIP);  // Update activity timestamp
    
    if (LittleFS.exists("/filemanager.html")) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/filemanager.html", "text/html");
      request->send(response);
    } else {
      String html = GetBasicPage();
      request->send(200, "text/html", html);
    }
  } else {
    // IP not authenticated - serve login page
    
    if (LittleFS.exists("/login.html")) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/login.html", "text/html");
      request->send(response);
    } else {
      // Fallback minimal login
      String html = GetBasicPage();
      request->send(200, "text/html", html);
    }
  }
}

void Server_HandleListFiles(AsyncWebServerRequest *request) {
  // Check authentication (IP-only)
  if (!ValidateIPFromRequest(request)) {
    request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }
  
  String json = "[";
  
  File root = LittleFS.open("/");
  if (!root) {
    request->send(500, "application/json", "[]");
    return;
  }
  
  bool first = true;
  File file = root.openNextFile();
  
  while (file) {
    if (!file.isDirectory()) {
      if (!first) json += ",";
      
      json += "{\"name\":\"";
      json += file.name();
      json += "\",\"size\":";
      json += file.size();
      json += "}";
      
      first = false;
    }
    file = root.openNextFile();
  }
  
  json += "]";
  request->send(200, "application/json", json);
}

void Server_HandleStorageInfo(AsyncWebServerRequest *request) {
  // Check authentication (IP-only)
  if (!ValidateIPFromRequest(request)) {
    request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }
  
  size_t totalBytes = LittleFS_GetTotalBytes();
  size_t usedBytes = LittleFS_GetUsedBytes();
  size_t availableBytes = LittleFS_GetAvailableBytes();
  
  String json = "{";
  json += "\"total\":" + String(totalBytes);
  json += ",\"used\":" + String(usedBytes);
  json += ",\"available\":" + String(availableBytes);
  json += "}";
  
  request->send(200, "application/json", json);
}

void Server_HandleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  // Check authentication on first chunk only
  if (index == 0) {
    // if (!ValidateIPFromRequest(request)) {
    //   Serial.println("❌ Upload rejected - unauthorized IP");
    //   return;
    // }
    
    uploadFilename = "/" + filename;
    uploadTotalSize = 0;
    
    if (LittleFS.exists(uploadFilename)) {
      LittleFS.remove(uploadFilename);
    }
    
    fsUploadFile = LittleFS.open(uploadFilename, "w");
    if (!fsUploadFile) {
      Serial.println("Failed to create file");
      return;
    }
    Serial.print("Upload Start: ");
    Serial.println(uploadFilename);
  }
  
  if (fsUploadFile) {
    if (len > 0) {
      size_t written = fsUploadFile.write(data, len);
      uploadTotalSize += written;
    }
    
    if (final) {
      fsUploadFile.close();
      Serial.print("Upload Complete: ");
      Serial.print(uploadFilename);
      Serial.print(" Size: ");
      Serial.println(uploadTotalSize);
    }
  }
}

void Server_HandleFileUploadComplete(AsyncWebServerRequest *request) {
  // Check authentication (IP-only)
  if (!ValidateIPFromRequest(request)) {
    request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }
  
  request->send(200, "text/plain", "File uploaded successfully");
}

void Server_HandleDeleteFile(AsyncWebServerRequest *request) {
  // Check authentication (IP-only)
  if (!ValidateIPFromRequest(request)) {
    request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }
  
  if (request->hasParam("filename", true)) {
    String filename = request->getParam("filename", true)->value();
    String filepath = "/" + filename;
    
    if (LittleFS.exists(filepath)) {
      if (LittleFS.remove(filepath)) {
        request->send(200, "text/plain", "File deleted");
        Serial.print("Deleted: ");
        Serial.println(filepath);
      } else {
        request->send(500, "text/plain", "Failed to delete file");
      }
    } else {
      request->send(404, "text/plain", "File not found");
    }
  } else {
    request->send(400, "text/plain", "Missing filename parameter");
  }
}

void Server_HandleDownloadFile(AsyncWebServerRequest *request) {
  // Check authentication (IP-only)
  if (!ValidateIPFromRequest(request)) {
    request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }
  
  if (request->hasParam("filename")) {
    String filename = request->getParam("filename")->value();
    String filepath = "/" + filename;
    
    if (!LittleFS.exists(filepath)) {
      request->send(404, "text/plain", "File not found");
      return;
    }
    
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, filepath, "application/octet-stream");
    response->addHeader("Content-Disposition", "attachment; filename=" + filename);
    request->send(response);
  } else {
    request->send(400, "text/plain", "Missing filename parameter");
  }
}

// ============ STATIC FILE HANDLER (PROTECTED) ============
void Server_HandleStaticFile(AsyncWebServerRequest *request) {
  String clientIP = String(request->client()->remoteIP());
  
  // Check if IP is authenticated (CSS, JS, etc. require login)
  if (!ValidateIPFromRequest(request)) {
    request->send(401, "text/plain", "Unauthorized - Please login first");
    return;
  }
  
  String path = request->url();
  
  // Determine MIME type based on file extension
  String contentType = "text/plain";
  if (path.endsWith(".css")) {
    contentType = "text/css";
  } else if (path.endsWith(".js")) {
    contentType = "application/javascript";
  } else if (path.endsWith(".html")) {
    contentType = "text/html";
  } else if (path.endsWith(".json")) {
    contentType = "application/json";
  } else if (path.endsWith(".png")) {
    contentType = "image/png";
  } else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) {
    contentType = "image/jpeg";
  } else if (path.endsWith(".gif")) {
    contentType = "image/gif";
  } else if (path.endsWith(".svg")) {
    contentType = "image/svg+xml";
  }
  
  // Check if file exists in LittleFS
  if (LittleFS.exists(path)) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, contentType);
    request->send(response);
    //Serial.println(path);
  } else {
    request->send(404, "text/plain", "File not found");
    Serial.print("Static file not found: ");
    Serial.println(path);
  }
}

// ============ SETTINGS HANDLERS ============
void Server_HandleGetSettings(AsyncWebServerRequest *request) {
  // Check authentication (IP-only)
  if (!ValidateIPFromRequest(request)) {
    request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }
  
  // Check if settings file exists in LittleFS
  if (LittleFS.exists("/settings.json")) {
    File settingsFile = LittleFS.open("/settings.json", "r");
    if (settingsFile) {
      String jsonContent = settingsFile.readString();
      settingsFile.close();
      request->send(200, "application/json", jsonContent);
      return;
    }
  }
  
  // Return default settings if file doesn't exist
  String defaultSettings = R"({"username":"admin","password":"admin","apSSID":"ESP","apPassword":"12345678","staSSID":"Jan","staPassword":"Asdf1234!"})";
  request->send(200, "application/json", defaultSettings);
}

void Server_HandleSaveSettings(AsyncWebServerRequest *request) {
  // The actual saving is handled in the route body handler above
  // This function is kept for compatibility but not actively used
}

// ============ SERVER SETUP ============
void Server_Setup() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Server_HandleRoot(request);
  });
  server.on("/basic", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = GetBasicPage();
      request->send(200, "text/html", html);
  });
  // Serve CSS files
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    Server_HandleStaticFile(request);
  });
  
  // Serve JavaScript files
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    Server_HandleStaticFile(request);
  });
  
  // Generic static file handler (catch-all for other static files)
  server.onNotFound([](AsyncWebServerRequest *request) {
    String path = request->url();
    // Check if it's a static file request (CSS, JS, images, etc.)
    if (path.endsWith(".css") || path.endsWith(".js") || path.endsWith(".png") || 
        path.endsWith(".jpg") || path.endsWith(".jpeg") || path.endsWith(".gif") || 
        path.endsWith(".svg") || path.endsWith(".ico")) {
      Server_HandleStaticFile(request);
      return;
    }

    // If client is not authenticated, redirect to root (captive portal login)
    if (!ValidateIPFromRequest(request)) {
      // Redirect to root which serves `/login.html` when unauthenticated
      request->redirect("/");
      return;
    }

    // Authenticated but path not found
    request->send(404, "text/plain", "Not Found");
  });
  
  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    Server_HandleListFiles(request);
  });
  
  server.on("/api/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
    Server_HandleStorageInfo(request);
  });
  
  server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    Server_HandleFileUploadComplete(request);
  }, [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    Server_HandleFileUpload(request, filename, index, data, len, final);
  });
  
  // ============ OTA FIRMWARE UPDATE ENDPOINT ============
  server.on("/api/ota", HTTP_POST, [](AsyncWebServerRequest *request) {
    OTA_HandleFileUploadComplete(request);
  }, [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    OTA_HandleFileUpload(request, filename, index, data, len, final);
  });
  
  server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    Server_HandleDeleteFile(request);
  });
  
  server.on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    Server_HandleDownloadFile(request);
  });

  // ============ SETTINGS ENDPOINTS ============
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    Server_HandleGetSettings(request);
  });

  // POST endpoint for saving settings with body handler
  server.on("/api/settings", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
      // This will be called when the request is complete
      Server_HandleSaveSettings(request);
    },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      // Not used for JSON
    },
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      // This handler receives body data
      static String jsonBody = "";
      
      // Append data to jsonBody
      for (size_t i = 0; i < len; i++) {
        jsonBody += (char)data[i];
      }
      
      // When we have all data, process it
      if (index + len == total) {
        Serial.print("📥 Received JSON: ");
        Serial.println(jsonBody);
        
        // Validate and save
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, jsonBody);
        
        if (!error) {
          // Save to LittleFS
          File settingsFile = LittleFS.open("/settings.json", "w");
          if (settingsFile) {
            size_t bytesWritten = settingsFile.print(jsonBody);
            settingsFile.close();
            Serial.printf("✅ Settings saved (%d bytes)\n", bytesWritten);
            request->send(200, "text/plain", "Settings saved");
          } else {
            Serial.println("❌ Failed to open settings file");
            request->send(500, "text/plain", "File write error");
          }
        } else {
          Serial.println("❌ Invalid JSON");
          request->send(400, "text/plain", "Invalid JSON");
        }
        jsonBody = "";  // Clear for next request
      }
    }
  );

  // ============ LOGIN ENDPOINT (IP-ONLY) ============
  server.on("/api/login", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      // Called when request is complete
    },
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      // Extract client IP
      String clientIP = String(request->client()->remoteIP());
      
      // This handler receives body data
      static String jsonBody = "";
      
      // Append data to jsonBody
      for (size_t i = 0; i < len; i++) {
        jsonBody += (char)data[i];
      }
      
      // When we have all data, process it
      if (index + len == total) {
        Serial.printf("🔐 Login attempt from IP: %s\n", clientIP.c_str());
        
        // Parse JSON
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, jsonBody);
        
        if (!error && doc.containsKey("username") && doc.containsKey("password")) {
          String username = doc["username"].as<String>();
          String password = doc["password"].as<String>();
          
          // Attempt login (server-side only, no token sent)
          if (Auth_Login(username, password, clientIP)) {
            // Success - no token sent, just confirmation
            StaticJsonDocument<256> response;
            response["success"] = true;
            response["message"] = "Login successful";
            
            String responseStr;
            serializeJson(response, responseStr);
            request->send(200, "application/json", responseStr);
            Serial.printf("✅ Login successful for %s from IP: %s\n", username.c_str(), clientIP.c_str());
          } else {
            // Failed authentication
            StaticJsonDocument<256> response;
            response["success"] = false;
            response["message"] = "Invalid credentials";
            
            String responseStr;
            serializeJson(response, responseStr);
            request->send(401, "application/json", responseStr);
            Serial.printf("❌ Login failed - invalid credentials from IP: %s\n", clientIP.c_str());
          }
        } else {
          request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing username or password\"}");
          Serial.printf("❌ Login failed - missing fields from IP: %s\n", clientIP.c_str());
        }
        jsonBody = "";  // Clear for next request
      }
    }
  );

  // ============ LOGOUT ENDPOINT (IP-ONLY) ============
  server.on("/api/logout", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Extract client IP
    String clientIP = String(request->client()->remoteIP());
    
    // Check if IP is logged in
    if (Auth_IsUserLoggedIn(clientIP)) {
      Auth_Logout(clientIP);
      
      StaticJsonDocument<128> response;
      response["success"] = true;
      response["message"] = "Logged out successfully";
      
      String responseStr;
      serializeJson(response, responseStr);
      request->send(200, "application/json", responseStr);
      Serial.printf("✅ Logout from IP: %s\n", clientIP.c_str());
    } else {
      request->send(401, "application/json", "{\"success\":false,\"message\":\"Not logged in\"}");
    }
  });
}

