#include "wifi.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#define WIFI_CONNECT_TIMEOUT_MS 10000

bool wifi_connect(const char *ssid, const char *password) {
    if (cyw43_arch_init()) return false;
    cyw43_arch_enable_sta_mode();

    int err = cyw43_arch_wifi_connect_timeout_ms(
        ssid, password, CYW43_AUTH_WPA2_AES_PSK, WIFI_CONNECT_TIMEOUT_MS
    );
    return err == 0;
}

void wifi_disconnect(void) {
    cyw43_arch_deinit();
}

bool wifi_is_connected(void) {
    return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
}
