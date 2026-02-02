#include "Global.h"

// ============ IP-BASED SESSION STORAGE (NO TOKENS) ============
Session sessions[5];  // Max 5 concurrent IPs
int activeSessionCount = 0;

// ============ INITIALIZE AUTH ============
void Auth_Init() {
  for (int i = 0; i < 5; i++) {
    sessions[i].active = false;
    sessions[i].lastActivity = 0;
    sessions[i].loginTime = 0;
    sessions[i].clientIP[0] = '\0';
  }
  Serial.println("✅ Auth system initialized (IP-based, no tokens)");
}

// ============ LOGIN - VERIFY CREDENTIALS AND STORE IP ============
bool Auth_Login(const String& username, const String& password, const String& clientIP) {
  // Load credentials from settings
  if (!LittleFS.exists("/settings.json")) {
    Serial.println("❌ Settings file not found");
    return false;
  }
  
  File settingsFile = LittleFS.open("/settings.json", "r");
  if (!settingsFile) {
    Serial.println("❌ Failed to open settings");
    return false;
  }
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, settingsFile);
  settingsFile.close();
  
  if (error) {
    Serial.println("❌ Failed to parse settings");
    return false;
  }
  
  // Verify credentials
  String savedUsername = doc["username"].as<String>();
  String savedPassword = doc["password"].as<String>();
  
  if (username != savedUsername || password != savedPassword) {
    Serial.printf("❌ Login FAILED - wrong credentials from IP: %s\n", clientIP.c_str());
    return false;
  }
  
  // Check if this IP is already logged in
  for (int i = 0; i < 5; i++) {
    if (sessions[i].active && String(sessions[i].clientIP) == clientIP) {
      // IP already has active session - just update activity
      sessions[i].lastActivity = millis();
      Serial.printf("✅ IP %s already logged in, session refreshed\n", clientIP.c_str());
      return true;
    }
  }
  
  // Find empty slot for new session
  for (int i = 0; i < 5; i++) {
    if (!sessions[i].active) {
      sessions[i].active = true;
      sessions[i].loginTime = millis();
      sessions[i].lastActivity = millis();
      strncpy(sessions[i].clientIP, clientIP.c_str(), 15);
      sessions[i].clientIP[15] = '\0';
      activeSessionCount++;
      
      Serial.printf("✅ Login SUCCESS for user: %s\n", username.c_str());
      Serial.printf("   IP: %s\n", clientIP.c_str());
      Serial.printf("   Slot: %d\n", i);
      Serial.printf("   Active sessions: %d/5\n", activeSessionCount);
      return true;
    }
  }
  
  // No slots available - remove oldest session
  unsigned long oldestTime = ULONG_MAX;
  int oldestSlot = -1;
  
  for (int i = 0; i < 5; i++) {
    if (sessions[i].active && sessions[i].lastActivity < oldestTime) {
      oldestTime = sessions[i].lastActivity;
      oldestSlot = i;
    }
  }
  
  if (oldestSlot != -1) {
    Serial.printf("⚠️ Max sessions reached, removing IP: %s\n", sessions[oldestSlot].clientIP);
    sessions[oldestSlot].active = false;
  }
  
  // Try again
  for (int i = 0; i < 5; i++) {
    if (!sessions[i].active) {
      sessions[i].active = true;
      sessions[i].loginTime = millis();
      sessions[i].lastActivity = millis();
      strncpy(sessions[i].clientIP, clientIP.c_str(), 15);
      sessions[i].clientIP[15] = '\0';
      activeSessionCount++;
      
      Serial.printf("✅ Login SUCCESS for user: %s\n", username.c_str());
      Serial.printf("   IP: %s\n", clientIP.c_str());
      Serial.printf("   Slot: %d\n", i);
      return true;
    }
  }
  
  return false;
}

// ============ CHECK IF IP IS LOGGED IN ============
bool Auth_IsUserLoggedIn(const String& clientIP) {
  Auth_CleanupExpiredSessions();
  
  for (int i = 0; i < 5; i++) {
    if (sessions[i].active && String(sessions[i].clientIP) == clientIP) {
      return true;
    }
  }
  
  return false;
}

// ============ UPDATE ACTIVITY TIMESTAMP ============
void Auth_UpdateActivity(const String& clientIP) {
  for (int i = 0; i < 5; i++) {
    if (sessions[i].active && String(sessions[i].clientIP) == clientIP) {
      sessions[i].lastActivity = millis();
      return;
    }
  }
}

// ============ LOGOUT - REMOVE IP SESSION ============
void Auth_Logout(const String& clientIP) {
  for (int i = 0; i < 5; i++) {
    if (sessions[i].active && String(sessions[i].clientIP) == clientIP) {
      unsigned long duration = millis() - sessions[i].loginTime;
      sessions[i].active = false;
      sessions[i].clientIP[0] = '\0';
      activeSessionCount--;
      
      Serial.printf("✅ IP %s logged out\n", clientIP.c_str());
      Serial.printf("   Session duration: %lu ms\n", duration);
      Serial.printf("   Active sessions: %d/5\n", activeSessionCount);
      return;
    }
  }
}

// ============ CLEANUP EXPIRED SESSIONS (5 MIN TIMEOUT) ============
void Auth_CleanupExpiredSessions() {
  unsigned long now = millis();
  int removedCount = 0;
  
  for (int i = 0; i < 5; i++) {
    if (sessions[i].active) {
      unsigned long idleTime = now - sessions[i].lastActivity;
      
      if (idleTime > SESSION_TIMEOUT) {
        Serial.printf("⏱️ Session EXPIRED - IP: %s (idle: %lu ms)\n", sessions[i].clientIP, idleTime);
        sessions[i].active = false;
        sessions[i].clientIP[0] = '\0';
        activeSessionCount--;
        removedCount++;
      }
    }
  }
  
  if (removedCount > 0) {
    Serial.printf("   Active sessions: %d/5\n", activeSessionCount);
  }
}
