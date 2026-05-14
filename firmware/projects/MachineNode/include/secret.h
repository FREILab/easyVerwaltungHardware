#pragma once

#define WIFI_SSID "FLIntern"
#define WIFI_PASSWORD "aGA2EEWbk8RTlkQgyoxX"

/**
 * Server-Konfiguration: Hostname statt Hardcoded-IP
 * 
 * SERVER_HOST wird per DNS/mDNS aufgelöst (z.B. "dashboard.intern").
 * Bei Server-Umzug muss nur der DNS-Eintrag geändert werden — kein Reflash nötig.
 * SERVER_IP_FALLBACK wird verwendet, wenn die DNS-Auflösung fehlschlägt.
 */
#define SERVER_HOST          "easyverwaltung.freilab.local" ///< Auflösbarer Hostname des Auth-Servers
#define SERVER_IP_FALLBACK   "192.168.178.79"               ///< Fallback-IP falls DNS nicht verfügbar
#define AUTHENTICATION_TOKEN "21042017freilab1337fooboardasgeht111" ///< Secure API token
