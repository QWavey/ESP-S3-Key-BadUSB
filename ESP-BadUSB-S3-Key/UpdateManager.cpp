#include "UpdateManager.h"
#include "FSManager.h"
#include <SD.h>
#include <Update.h>
#include <ArduinoJson.h>

static File g_pkgUploadFile;

// ---- Upload: stream the raw .espkg to the SD card --------------------------
void handleUpdatePackageUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    updateApplying = false;
    updateProgress = 0;
    updateStatus = "Receiving package...";
    if (SD.exists(ESPKG_TMP_PATH)) SD.remove(ESPKG_TMP_PATH);
    g_pkgUploadFile = SD.open(ESPKG_TMP_PATH, FILE_WRITE);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (g_pkgUploadFile) g_pkgUploadFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (g_pkgUploadFile) g_pkgUploadFile.close();
    updateStatus = "Package received";
  }
}

// ---- POST finalizer: arm the apply (runs from loop) ------------------------
void handleUpdatePackagePost() {
  if (!sdCardPresent || !SD.exists(ESPKG_TMP_PATH)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"no package received\"}");
    return;
  }
  updatePackageReady = true;   // applied on the next loop() so progress is pollable
  updateStatus = "Queued";
  updateProgress = 0;
  server.send(200, "application/json", "{\"ok\":true,\"status\":\"applying\"}");
}

// ---- GET /api/update-status ------------------------------------------------
void handleUpdateStatus() {
  DynamicJsonDocument doc(256);
  doc["progress"] = updateProgress;
  doc["status"] = updateStatus;
  doc["applying"] = updateApplying;
  String out; serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "application/json", out);
}

// Copy `len` bytes from the (already-positioned) package into `path` on SD.
// Writes to a temp file and only renames into place on full success, so a
// truncated upload can never leave a half-written /index.html behind.
static bool extractFile(File& src, const String& path, uint32_t len,
                        uint32_t totalBytes, uint32_t& doneBytes) {
  String tmp = path + ".new";
  if (SD.exists(tmp)) SD.remove(tmp);
  File dst = SD.open(tmp, FILE_WRITE);
  if (!dst) return false;
  uint8_t buf[512];
  uint32_t remaining = len;
  while (remaining > 0) {
    size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
    int r = src.read(buf, chunk);
    if (r <= 0) { dst.close(); SD.remove(tmp); return false; }
    if (dst.write(buf, r) != (size_t)r) { dst.close(); SD.remove(tmp); return false; }
    remaining -= r;
    doneBytes += r;
    updateProgress = (int)((uint64_t)doneBytes * 100ULL / (totalBytes ? totalBytes : 1));
    server.handleClient();   // keep the UI progress poll responsive
  }
  dst.close();
  // atomically swap into place
  if (SD.exists(path)) SD.remove(path);
  if (!SD.rename(tmp, path)) { SD.remove(tmp); return false; }
  return true;
}

// ---- Apply the armed package (called from loop) ----------------------------
void processPendingUpdate() {
  if (!updatePackageReady) return;
  updatePackageReady = false;
  updateApplying = true;
  updateProgress = 0;
  updateStatus = "Opening package...";

  File pkg = SD.open(ESPKG_TMP_PATH, FILE_READ);
  if (!pkg) { updateStatus = "Error: cannot open package"; updateApplying = false; return; }

  // magic: 'E','S','P','K','G',0x01
  uint8_t magic[6];
  if (pkg.read(magic, 6) != 6 || magic[0] != 'E' || magic[1] != 'S' || magic[2] != 'P' ||
      magic[3] != 'K' || magic[4] != 'G' || magic[5] != 0x01) {
    updateStatus = "Error: not a valid .espkg"; pkg.close(); updateApplying = false; return;
  }

  // manifest length (uint32 little-endian)
  uint8_t lenb[4];
  if (pkg.read(lenb, 4) != 4) { updateStatus = "Error: truncated header"; pkg.close(); updateApplying = false; return; }
  uint32_t manLen = (uint32_t)lenb[0] | ((uint32_t)lenb[1] << 8) |
                    ((uint32_t)lenb[2] << 16) | ((uint32_t)lenb[3] << 24);
  if (manLen == 0 || manLen > 8192) { updateStatus = "Error: bad manifest size"; pkg.close(); updateApplying = false; return; }

  // manifest JSON (heap — never put multi-KB on the 8KB loop stack)
  char* manBuf = (char*)malloc(manLen + 1);
  if (!manBuf) { updateStatus = "Error: out of memory"; pkg.close(); updateApplying = false; return; }
  if (pkg.read((uint8_t*)manBuf, manLen) != (int)manLen) {
    updateStatus = "Error: truncated manifest"; free(manBuf); pkg.close(); updateApplying = false; return;
  }
  manBuf[manLen] = 0;
  DynamicJsonDocument man(8192);
  DeserializationError jerr = deserializeJson(man, manBuf);
  free(manBuf);
  if (jerr) { updateStatus = "Error: bad manifest JSON"; pkg.close(); updateApplying = false; return; }

  // total bytes (for the progress bar)
  JsonArray sdArr = man["sd"].as<JsonArray>();
  uint64_t totalBytes = 0;
  for (JsonObject f : sdArr) totalBytes += (uint32_t)(f["size"] | 0);
  uint32_t fwSize = man["fw"]["size"] | 0;
  totalBytes += fwSize;
  uint32_t doneBytes = 0;

  // Payloads begin right after the manifest — the cursor is already there.
  // 1) website / SD files FIRST (safe, no reboot)
  for (JsonObject f : sdArr) {
    String path = f["path"].as<String>();
    if (!path.startsWith("/")) path = "/" + path;
    uint32_t sz = f["size"] | 0;
    updateStatus = "Writing " + path;
    server.handleClient();
    if (!extractFile(pkg, path, sz, (uint32_t)totalBytes, doneBytes)) {
      updateStatus = "Error writing " + path; pkg.close(); updateApplying = false; return;
    }
  }

  // 2) firmware LAST (OTA — reboots on success)
  if (fwSize > 0) {
    updateStatus = "Flashing firmware...";
    server.handleClient();
    if (!Update.begin(fwSize, U_FLASH)) {
      updateStatus = String("Error: OTA begin (") + Update.errorString() + ")";
      pkg.close(); updateApplying = false; return;
    }
    uint8_t buf[1024];
    uint32_t remaining = fwSize;
    while (remaining > 0) {
      size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
      int r = pkg.read(buf, chunk);
      if (r <= 0) { updateStatus = "Error: truncated firmware"; Update.abort(); pkg.close(); updateApplying = false; return; }
      if (Update.write(buf, r) != (size_t)r) { updateStatus = "Error: OTA write failed"; Update.abort(); pkg.close(); updateApplying = false; return; }
      remaining -= r;
      doneBytes += r;
      updateProgress = (int)((uint64_t)doneBytes * 100ULL / (totalBytes ? totalBytes : 1));
      if ((remaining & 0x3FFF) == 0) server.handleClient();  // ~every 16 KB
    }
    if (!Update.end(true)) {
      updateStatus = String("Error: OTA end (") + Update.errorString() + ")";
      pkg.close(); updateApplying = false; return;
    }
  }

  pkg.close();
  SD.remove(ESPKG_TMP_PATH);
  updateProgress = 100;
  updateStatus = fwSize > 0 ? "Done — rebooting..." : "Website updated";
  updateApplying = false;
  server.handleClient();   // flush the final status to any poller
  delay(800);
  if (fwSize > 0) ESP.restart();
}
