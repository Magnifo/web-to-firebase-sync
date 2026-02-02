#line 1 "c:\\Users\\JanShahid\\Desktop\\filemanagerpio\\MyWiFi.cpp"
#include "Global.h"

// ============ NTP TIME FUNCTIONS ============
void NTP_Init() {
  Serial.print("Initializing NTP time sync...");
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  // Wait up to 10 seconds for time to be set
  int attempts = 0;
  while (!NTP_IsTimeSet() && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (NTP_IsTimeSet()) {
    Serial.println(" SUCCESS");
    Serial.printf("Current time: %s\n", NTP_GetTimeString().c_str());
  } else {
    Serial.println(" FAILED");
    Serial.println("Could not sync with NTP server");
  }
}

bool NTP_IsTimeSet() {
  time_t now = time(nullptr);
  return (now > 1000000000); // Valid timestamp (after year 2001)
}

String NTP_GetTimeString() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  
  if (!localtime_r(&now, &timeinfo)) {
    return "Time not set";
  }
  
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// Load WiFi settings from LittleFS
void LoadWiFiSettings(String &staSSID, String &staPassword, String &apSSID, String &apPassword)
{
  if (LittleFS.exists("/settings.json"))
  {
    File settingsFile = LittleFS.open("/settings.json", "r");
    if (settingsFile)
    {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, settingsFile);
      settingsFile.close();

      if (!error)
      {
        staSSID = doc["staSSID"].as<String>();
        staPassword = doc["staPassword"].as<String>();
        apSSID = doc["apSSID"].as<String>();
        apPassword = doc["apPassword"].as<String>();

        Serial.println("✅ WiFi settings loaded from LittleFS");
        return;
      }
    }
  }

  // Default settings if file doesn't exist
  staSSID = "TP-LINK_7BAE";
  staPassword = "15885972";
  apSSID = "ESP";
  apPassword = "12345678";
  Serial.println("⚠️ Using default WiFi settings");
}

unsigned long lastSTAReconnect = 0;
const unsigned long STA_RECONNECT_INTERVAL = 3 * 60 * 1000; // 3 minutes
bool staConnectedBefore = false;
String staSSID, staPassword;
void WiFi_Init()
{
    String apSSID, apPassword;

    LoadWiFiSettings(staSSID, staPassword, apSSID, apPassword);

    Serial.print("📡 Starting AP: ");
    Serial.println(apSSID);

    // Start AP
    WiFi.AP.begin();
    WiFi.AP.create(apSSID.c_str(), apPassword.c_str());

    if (!WiFi.AP.waitStatusBits(ESP_NETIF_STARTED_BIT, 1000))
    {
        Serial.println("Failed to start AP!");
        return;
    }

    // Try STA connection once at startup
    Serial.print("📶 Connecting STA: ");
    Serial.println(staSSID);
    WiFi.mode(WIFI_AP_STA);  // Enable dual mode
    WiFi.begin(staSSID.c_str(), staPassword.c_str());
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        staConnectedBefore = true;
        Serial.println("\n✅ STA Connected!");
        Serial.print("   STA IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\n⚠️ STA Failed! AP active only.");
        WiFi.mode(WIFI_AP); // Keep AP stable if STA fails
    }

    lastSTAReconnect = millis();
}

// Call this inside loop()
void WiFi_STA_Reconnect_Handler()
{
    // Check every 3 minutes
    if (WiFi.status() != WL_CONNECTED && millis() - lastSTAReconnect > STA_RECONNECT_INTERVAL)
    {
        Serial.println("🔄 Trying STA reconnect...");
        WiFi.mode(WIFI_AP_STA); // Enable dual mode only during STA reconnect
        WiFi.begin(staSSID.c_str(), staPassword.c_str());
        lastSTAReconnect = millis();
    }

    // If STA just connected
    if (WiFi.status() == WL_CONNECTED && !staConnectedBefore)
    {
        staConnectedBefore = true;
        Serial.println("🎉 STA Connected!");
        Serial.print("   STA IP: ");
        Serial.println(WiFi.localIP());
        WiFi.mode(WIFI_AP_STA); // dual mode after STA connects
    }

    // If STA disconnects later
    if (staConnectedBefore && WiFi.status() != WL_CONNECTED)
    {
        staConnectedBefore = false;
        Serial.println("⚠️ STA Lost! AP remains active.");
        WiFi.mode(WIFI_AP); // keep AP stable
    }
}

bool WiFi_IsConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

String WiFi_GetLocalIP()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return WiFi.localIP().toString();
  }
  return WiFi.softAPIP().toString();
}
