#include "Global.h"

bool LittleFS_Init() {
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED, "", 5, LITTLEFS_PARTITION)) {
    Serial.println("LittleFS Mount Failed");
    return false;
  }
  Serial.println("LittleFS Mounted Successfully");
  return true;
}

size_t LittleFS_GetTotalBytes() {
  return LittleFS.totalBytes();
}

size_t LittleFS_GetUsedBytes() {
  return LittleFS.usedBytes();
}

size_t LittleFS_GetAvailableBytes() {
  return LittleFS.totalBytes() - LittleFS.usedBytes();
}

bool LittleFS_FileExists(String path) {
  return LittleFS.exists(path);
}

bool LittleFS_DeleteFile(String path) {
  if (LittleFS.exists(path)) {
    return LittleFS.remove(path);
  }
  return false;
}

File LittleFS_OpenFile(String path, String mode) {
  return LittleFS.open(path, mode.c_str());
}
