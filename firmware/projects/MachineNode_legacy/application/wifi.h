#pragma once
#include <stdbool.h>

bool wifi_connect(const char *ssid, const char *password);
void wifi_disconnect(void);
bool wifi_is_connected(void);
