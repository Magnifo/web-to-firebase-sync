#ifndef GLOBAL_H
#define GLOBAL_H

#define ENABLE_DATABASE
#define ENABLE_USER_AUTH

// Enable PSRAM usage for large allocations
#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_ENABLE_ALIGNMENT 0

#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FirebaseClient.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

// ============ CONFIGURATION ============
#define WIFI_SSID "TP-LINK_7BAE"
#define WIFI_PASSWORD "15885972"
#define SERVER_PORT 80
#define DEFAULT_BAUD_RATE 115200
#define LITTLEFS_PARTITION "littlefs"
#define FORMAT_LITTLEFS_IF_FAILED true
#define MAX_UPLOAD_SIZE 10485760 // 10 MB
#define FIREBASE_API_KEY "AIzaSyCiG3ZhMyPEz1cz0Gy61p3fqndTHy8fO0E"
#define FIREBASE_DATABASE_URL                                                  \
  "https://aseengrs-7f20c-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_USER_EMAIL "esp32s3@magnifo.tech"
#define FIREBASE_USER_PASSWORD "Asdf1234!"
// ============ NTP TIME CONFIGURATION ============
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 18000  // GMT+5 (Pakistan: 5 * 3600)
#define DAYLIGHT_OFFSET_SEC 0 // No daylight saving
// ============ GLOBAL SERVER OBJECT ============
extern AsyncWebServer server;
extern UserAuth user_auth;
extern FirebaseApp app;
extern WiFiClientSecure ssl_client;
extern AsyncClientClass aClient;
extern RealtimeDatabase Database;
extern bool firebaseReady;
// ============ SESSION CONFIGURATION ============
#define SESSION_TIMEOUT 300000 // 5 minutes of inactivity timeout

// ============ SESSION STRUCTURE - IP BASED ============
struct Session {
  char clientIP[16];          // Client IP address (e.g., "192.168.1.100")
  unsigned long loginTime;    // When user logged in
  unsigned long lastActivity; // Last request time
  bool active;                // Is session active?
};

// ============ FLIGHT DATA STATE MACHINE ===========
enum FlightDataState {
  WAITING,  // Waiting for next fetch interval
  FETCHING, // Currently fetching data from website
  PRINTING, // Printing flight data to serial
  IDLE      // Ready to fetch
};

extern FlightDataState flightDataState;
extern Session sessions[5];

// Increased sizes for more flight data capacity
extern StaticJsonDocument<32768> allData; // 32KB - temporary scratchpad

// Flight data state
extern const char *url;
extern int Page;
extern int Pages;
extern String phpSessionID; // Will store captured PHPSESSID
extern bool SessoinIdFound;
extern unsigned long lastFetchTime;
extern const unsigned long FETCH_INTERVAL;
// ============ HELPER FUNCTIONS ============
// ESP32-MANAGED FIELDS: All fields that ESP32 is responsible for updating
extern const char *MANAGED_FIELDS[];
extern const int MANAGED_FIELDS_COUNT;
extern bool Start;

// ============ ESP32 STATUS VARIABLES ============
extern String esp32Status;
extern String esp32StatusIcon;
extern unsigned long esp32UptimeStart;

// ============ NTP TIME FUNCTIONS ============
void NTP_Init();
String NTP_GetTimeString();
String NTP_GetFormattedTimestamp(); // Format: "15 Jan 2026 12:20 AM"
bool NTP_IsTimeSet();
// ============ WIFI FUNCTIONS ============
void WiFi_Init();
bool WiFi_IsConnected();
String WiFi_GetLocalIP();
void WiFi_Process();

// DNS server used for captive portal (redirect all DNS queries to the ESP AP
// IP)

// ============ LITTLEFS FUNCTIONS ============
bool LittleFS_Init();
size_t LittleFS_GetTotalBytes();
size_t LittleFS_GetUsedBytes();
size_t LittleFS_GetAvailableBytes();
bool LittleFS_FileExists(String path);
bool LittleFS_DeleteFile(String path);
File LittleFS_OpenFile(String path, String mode);

// ============ SERVER HANDLER FUNCTIONS ============
void Server_Setup();
void Server_HandleRoot(AsyncWebServerRequest *request);
void Server_HandleListFiles(AsyncWebServerRequest *request);
void Server_HandleStorageInfo(AsyncWebServerRequest *request);
void Server_HandleFileUpload(AsyncWebServerRequest *request,
                             const String &filename, size_t index,
                             uint8_t *data, size_t len, bool final);
void Server_HandleFileUploadComplete(AsyncWebServerRequest *request);
void Server_HandleDeleteFile(AsyncWebServerRequest *request);
void Server_HandleDownloadFile(AsyncWebServerRequest *request);
void Server_HandleGetSettings(AsyncWebServerRequest *request);
void Server_HandleSaveSettings(AsyncWebServerRequest *request);

// ============ OTA FIRMWARE UPDATE FUNCTIONS ============
void OTA_HandleFileUpload(AsyncWebServerRequest *request,
                          const String &filename, size_t index, uint8_t *data,
                          size_t len, bool final);
void OTA_HandleFileUploadComplete(AsyncWebServerRequest *request);
String OTA_GetStatus();

// ============ AUTHENTICATION FUNCTIONS ============
void Auth_Init();
bool Auth_Login(const String &username, const String &password,
                const String &clientIP);
bool Auth_IsUserLoggedIn(const String &clientIP);
void Auth_UpdateActivity(const String &clientIP);
void Auth_Logout(const String &clientIP);
void Auth_CleanupExpiredSessions();

// ============ FIREBASE FUNCTIONS ============

void Firebase_Init();
bool Firebase_IsReady();
bool Firebase_ResetConnection();
void Firebase_UploadFlightData(JsonObject flightData);
void Firebase_PrintResult(AsyncResult &aResult);
bool Firebase_VerifyUser(const String &apiKey, const String &email,
                         const String &password);
void Firebase_UpdateLastSyncTime(); // Update Esp33Update timestamp

// ============ WEB PAGE & JAVASCRIPT ============
String GetWebPage();
String GetAppScript();

// ============ STATIC FILE UPLOAD VARIABLES ============
extern File fsUploadFile;
extern String uploadFilename;
extern size_t uploadTotalSize;

//=============Web Parser Variables=============
// Web scraping helper functions (implemented in websraper.cpp)
void Fetch(String line, String category, String subCategory);
void fetchPHPSESSID();
bool extractPageInfo(String line);
bool CheckSession(String line);
void ExtractTDInfoJSON(String trHtml, JsonDocument &doc, String Category,
                       String subCategory);
void postRequest(String cnfg, int pg, int pgs, String category,
                 String subCategory);

// HTML Parsing & JSON Extraction
void parseFlightTableRow(String tableRowHtml, JsonDocument &doc,
                         String category, String subCategory);

void WiFi_STA_Reconnect_Handler();
bool UpdateFlight(String category, String flightKey,
                  const JsonDocument &fieldsToUpdate);
bool UpdateFlightsBatch(String category, JsonObject flightsToUpdate);
DynamicJsonDocument *CreateFilteredFlightDoc(JsonObject source);
#endif
