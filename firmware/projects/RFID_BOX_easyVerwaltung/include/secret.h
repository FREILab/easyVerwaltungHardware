/**
 * @file secret.h
 * @brief Private credentials for network and backend communication.
 * @details Sensitive data excluded from version control.
 *          WiFi credentials are managed in platformio.secrets.ini.
 * @note MACHINE_NAME, MACHINE_ID, OTA_HOSTNAME are defined per environment in platformio.ini.
 */

#ifndef SECRET_H
#define SECRET_H

// Backend — auflösbarer Hostname des easyVerwaltung-Servers
#ifndef SERVER_HOST
  #define SERVER_HOST "dashboard.intern"
#endif
#ifndef SERVICE_TOKEN
  #define SERVICE_TOKEN "YOUR_SERVICE_TOKEN"
#endif

#endif /* SECRET_H */
