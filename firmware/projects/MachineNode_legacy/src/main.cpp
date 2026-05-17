#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "firmware_config.h"
#include "ota.h"

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

void print_ota_help() {
    printf("\n[OTA] Available commands (via serial):\n");
    printf("  ota download <url> [crc32]\n");
    printf("    Download firmware from HTTP URL and activate\n");
    printf("    Example: ota download http://192.168.1.100:8000/firmware.bin 0x12345678\n");
    printf("  ota status\n");
    printf("    Print current OTA metadata\n");
    printf("  ota rollback\n");
    printf("    Switch back to previous firmware\n");
    printf("\n");
}

void handle_ota_command() {
    char cmd[256];
    char arg1[256];
    char arg2[256];
    
    printf("[CMD] Enter OTA command: ");
    fflush(stdout);
    
    if (scanf("%255s", cmd) != 1) return;
    
    if (strcmp(cmd, "download") == 0) {
        if (scanf("%255s %255s", arg1, arg2) < 1) {
            printf("[CMD] Usage: download <url> [crc32]\n");
            return;
        }
        
        uint32_t crc32 = 0;
        if (strlen(arg2) > 0) {
            crc32 = (uint32_t)strtol(arg2, NULL, 16);
        }
        
        printf("[CMD] Starting OTA download from: %s\n", arg1);
        if (ota_http_download(arg1, 0, crc32)) {
            printf("[CMD] Download successful! Activating firmware...\n");
            ota_activate_and_reboot(0, "1.0.0-ota");
        } else {
            printf("[CMD] Download failed. Try again.\n");
        }
    } 
    else if (strcmp(cmd, "status") == 0) {
        const boot_metadata_t *meta = ota_get_metadata();
        printf("[CMD] Active partition: 0x%08X\n", meta->active_partition == 0 ? 0x10000000 : 0x101A0000);
        printf("[CMD] Firmware size: %u\n", meta->firmware_size);
        printf("[CMD] Version: %s\n", meta->version);
    }
    else if (strcmp(cmd, "rollback") == 0) {
        printf("[CMD] Rolling back...\n");
        ota_rollback();
    }
    else if (strcmp(cmd, "help") == 0) {
        print_ota_help();
    }
    else {
        printf("[CMD] Unknown command: %s\n", cmd);
    }
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
    printf("[OTA] Type 'help' in serial console for OTA commands.\n");
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

    ota_init();
    bool wifi_connected = connect_wifi();
    run_ota_self_test(wifi_connected);
    
    if (OTA_ENABLED) {
        print_ota_help();
    }

    const uint32_t blink_ms = wifi_connected ? (OTA_ENABLED ? 150 : 300) : 800;
    absolute_time_t next_heartbeat = make_timeout_time_ms(5000);

    while (true) {
        /* Check for serial input (non-blocking) */
        int c = getchar_timeout_us(100000);  // 100ms timeout
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == 'h' || c == '?') {
                print_ota_help();
            } else if (c == 'c') {
                handle_ota_command();
            }
        }
        
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
