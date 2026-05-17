#include "pico/stdlib.h"
#include "firmware_config.h"

#if defined(CYW43_WL_GPIO_LED_PIN)
#include "pico/cyw43_arch.h"
#endif

int main() {
    stdio_init_all();

#if defined(CYW43_WL_GPIO_LED_PIN)
    if (cyw43_arch_init()) {
        return 1;
    }

    const uint32_t blink_ms = OTA_ENABLED ? 200 : 500;

    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(blink_ms);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(blink_ms);
    }
#elif defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    const uint32_t blink_ms = OTA_ENABLED ? 200 : 500;

    while (true) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(blink_ms);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(blink_ms);
    }
#else
    while (true) {
        sleep_ms(1000);
    }
#endif
}
