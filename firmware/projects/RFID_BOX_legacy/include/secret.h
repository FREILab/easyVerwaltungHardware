/**
 * @file secret.h
 * @brief Private credentials for network and backend communication.
 * @details This file contains sensitive data excluded from version control.
 *          WiFi credentials (WIFI_SSID, WIFI_PASSWORD) are injected via
 *          platformio.secrets.ini build flags and must not be redefined here.
 * @note Machine configuration (MACHINE_NAME, MACHINE_ID, OTA_HOSTNAME) is defined
 * per environment in platformio.ini via [pair_*] sections.
 */

/**
 * @defgroup RFID_BOX_LEGACY RFID Box Legacy (ESP32)
 * @brief Software for the RFID-Box and Power-Switching-Units.
 * @{
 */

#ifndef SECRET_H
#define SECRET_H

/**
 * @name Backend Communication
 * @brief Connection parameters for the management server.
 *
 * SERVER_HOST wird per DNS/mDNS aufgelöst (z.B. "dashboard.intern").
 * Bei Server-Umzug muss nur der DNS-Eintrag geändert werden — kein Reflash nötig.
 * SERVER_IP_FALLBACK wird verwendet, wenn die DNS-Auflösung fehlschlägt.
 */
///@{
#define SERVER_HOST          "YOUR_SERVER_HOSTNAME"  ///< Auflösbarer Hostname des Auth-Servers
#define SERVER_IP_FALLBACK   "YOUR_SERVER_IP"        ///< Fallback-IP falls DNS nicht verfügbar
#define AUTHENTICATION_TOKEN "YOUR_AUTH_TOKEN"       ///< Secure API token
///@}

#endif /* SECRET_H */
