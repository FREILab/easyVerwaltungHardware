#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "config.h"
#include "wifi.h"
#include "ota.h"

/* Wie oft der OTA-Check im Betrieb ausgeführt wird (in ms) */
#define OTA_CHECK_INTERVAL_MS (60 * 60 * 1000)

int main(void) {
    stdio_init_all();
    sleep_ms(1000); /* Kurz warten damit USB-Serial-Monitor sich verbinden kann */

    printf("[BOOT] Machine Node " MACHINE_ID " (" MACHINE_NAME ")\n");
    printf("[BOOT] Active slot: %s\n", ota_get_active_slot() == OTA_SLOT_A ? "A" : "B");

    if (!wifi_connect(WIFI_SSID, WIFI_PASSWORD)) {
        printf("[WIFI] Verbindung fehlgeschlagen — Reboot in 5s\n");
        sleep_ms(5000);
        /* TODO: watchdog_reboot(0, 0, 0) nach watchdog_enable */
        return 1;
    }
    printf("[WIFI] Verbunden\n");

    /* OTA-Check direkt nach dem Start */
    ota_result_t result = ota_check_and_stage();
    if (result == OTA_RESULT_UPDATE_STAGED) {
        printf("[OTA] Update bereit — Reboot\n");
        sleep_ms(500);
        /* TODO: watchdog_reboot(0, 0, 0) */
        return 0;
    }

    /* Hauptschleife */
    uint32_t last_ota_check = to_ms_since_boot(get_absolute_time());
    while (true) {
        /* TODO: Gerätelogik (RFID-Scan, Relay, OLED, ...) */

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_ota_check >= OTA_CHECK_INTERVAL_MS) {
            last_ota_check = now;
            if (ota_check_and_stage() == OTA_RESULT_UPDATE_STAGED) {
                printf("[OTA] Update bereit — Reboot\n");
                sleep_ms(500);
                /* TODO: watchdog_reboot(0, 0, 0) */
                break;
            }
        }

        sleep_ms(10);
    }

    return 0;
}
