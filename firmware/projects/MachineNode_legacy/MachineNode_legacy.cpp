#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "firmware_config.h"

#if defined(CYW43_WL_GPIO_LED_PIN)
#include "pico/cyw43_arch.h"
#endif

namespace {

bool is_value_set(const char *value) {
    return value != nullptr && strcmp(value, "") != 0 && strcmp(value, "NotSet") != 0 &&
           strcmp(value, "NoToken") != 0 && strcmp(value, "localhost") != 0;
}

void print_boot_info() {
    printf("\n[BOOT] MachineNode_legacy start\n");
    printf("[BOOT] ENV_PROFILE=%s\n", FW_ENV_PROFILE);
    printf("[BOOT] MACHINE=%s (%s)\n", MACHINE_NAME, MACHINE_ID);
    printf("[BOOT] OTA_HOSTNAME=%s OTA_ENABLED=%d\n", OTA_HOSTNAME, OTA_ENABLED);
}

#if defined(CYW43_WL_GPIO_LED_PIN)
bool connect_wifi() {
    if (!is_value_set(WIFI_SSID) || !is_value_set(WIFI_PASSWORD)) {
        printf("[WiFi] Skipped: WIFI_SSID/WIFI_PASSWORD not configured.\n");
        return false;
    }

    printf("[WiFi] Connecting to SSID '%s' ...\n", WIFI_SSID);
    cyw43_arch_enable_sta_mode();

    int result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID,
        WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK,
        30000
    );

    if (result != 0) {
        printf("[WiFi] Connect failed: %d\n", result);
        return false;
    }

    printf("[WiFi] Connected successfully.\n");
    return true;
}

void run_ota_self_test(bool wifi_connected) {
    if (!OTA_ENABLED) {
        printf("[OTA] Disabled for this ENV profile.\n");
        return;
    }

    printf("[OTA] Self-test start.\n");

    if (!wifi_connected) {
        printf("[OTA] FAIL: WiFi not connected.\n");
        return;
    }

    if (!is_value_set(SERVER_HOST)) {
        printf("[OTA] FAIL: SERVER_HOST not configured.\n");
        return;
    }

    if (!is_value_set(AUTHENTICATION_TOKEN)) {
        printf("[OTA] FAIL: AUTHENTICATION_TOKEN not configured.\n");
        return;
    }

    printf("[OTA] PASS: Config and WiFi look valid for OTA workflow.\n");
    printf("[OTA] Next step: add image download + flash apply path.\n");
}
#endif

} // namespace

int main() {
    stdio_init_all();
    sleep_ms(1500);
    print_boot_info();

#if defined(CYW43_WL_GPIO_LED_PIN)
    if (cyw43_arch_init()) {
        printf("[WiFi] cyw43_arch_init failed.\n");
        return 1;
    }

    bool wifi_connected = connect_wifi();
    run_ota_self_test(wifi_connected);

    const uint32_t blink_ms = wifi_connected ? (OTA_ENABLED ? 150 : 300) : 800;
    absolute_time_t next_heartbeat = make_timeout_time_ms(5000);

    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(blink_ms);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(blink_ms);

        if (absolute_time_diff_us(get_absolute_time(), next_heartbeat) <= 0) {
            printf("[DBG] Alive. wifi_connected=%d, blink_ms=%lu\n", wifi_connected ? 1 : 0, (unsigned long)blink_ms);
            next_heartbeat = make_timeout_time_ms(5000);
        }
    }
#elif defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    const uint32_t blink_ms = OTA_ENABLED ? 200 : 500;
    absolute_time_t next_heartbeat = make_timeout_time_ms(5000);

    while (true) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(blink_ms);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(blink_ms);

        if (absolute_time_diff_us(get_absolute_time(), next_heartbeat) <= 0) {
            printf("[DBG] Alive (GPIO fallback). blink_ms=%lu\n", (unsigned long)blink_ms);
            next_heartbeat = make_timeout_time_ms(5000);
        }
    }
#else
    while (true) {
        printf("[DBG] Alive (no LED pin available).\n");
        sleep_ms(1000);
    }
#endif
}
