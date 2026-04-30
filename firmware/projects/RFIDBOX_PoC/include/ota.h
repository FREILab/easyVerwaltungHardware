/**
 * @file ota.h
 * @brief ESP32 Over-The-Air (OTA) Update Module
 * 
 * Provides OTA update capabilities using ElegantOTA library.
 * Enables wireless firmware updates over WiFi with authentication.
 * 
 * Credentials are read from secret.h (OTA_BUILD_USERNAME, OTA_BUILD_PASSWORD).
 * 
 * Usage:
 *   - Call initOTA() in setup() after WiFi connection
 *   - Call handleOTA() in the main loop
 * 
 * The OTA web UI is available at http://<device-ip>/update
 */

#ifndef __OTA_H__
#define __OTA_H__

#include <ElegantOTA.h>
#include <ArduinoLog.h>
#include <WebServer.h>

// --- OTA CREDENTIALS (from secret.h) ---
#if __has_include("secret.h")
  #include "secret.h"
#endif

// Fallback defaults if secret.h not available
#ifndef OTA_BUILD_USERNAME
  #define OTA_BUILD_USERNAME "admin"
#endif
#ifndef OTA_BUILD_PASSWORD
  #define OTA_BUILD_PASSWORD "password"
#endif

WebServer server(80);

/**
 * @brief Initialize the OTA update service with authentication
 *
 * Must be called after WiFi is connected.
 * Uses OTA_BUILD_USERNAME and OTA_BUILD_PASSWORD from secret.h (credentials baked into firmware).
 * OTA_USERNAME and OTA_PASSWORD are the current credentials used by the uploader.
 *
 * @param hostname Device hostname for mDNS discovery
 */
void initOTA(const char* hostname = "rfidbox") {
  // Start the web server
  server.begin();
  Log.notice("[OTA] Web server started on port 80\n");

  // Initialize ElegantOTA
  ElegantOTA.begin(&server);

  // Set authentication credentials baked into firmware
  ElegantOTA.setAuth(OTA_BUILD_USERNAME, OTA_BUILD_PASSWORD);

  Log.notice("[OTA] ElegantOTA initialized with authentication\n");
  Log.notice("[OTA] Access at http://%s.local/update (Username: %s)\n", hostname, OTA_BUILD_USERNAME);
}

/**
 * @brief Handle incoming OTA update requests
 * 
 * Must be called regularly in the main loop.
 * Non-blocking and safe to call frequently.
 */
void handleOTA() {
  server.handleClient();
}

#endif // __OTA_H__
