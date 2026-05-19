#pragma once

#include "easy_api.h"
#include <ArduinoLog.h>
#include <HTTPClient.h>
#include <string.h>

extern "C" easy_api_status_t easy_api_esp32_transport(
    const easy_api_http_request_t *request,
    easy_api_http_response_t *response,
    void *user_context)
{
    (void)user_context;
    HTTPClient http;
    WiFiClient client;

    Log.verbose("[transport] %s %s\n", request->method, request->url);
    if (request->body != NULL && request->body_len > 0) {
        Log.verbose("[transport] Body: %s\n", request->body);
    }

    if (!http.begin(client, String(request->url))) {
        return EASY_API_ERR_NETWORK;
    }

    for (size_t i = 0; i < request->header_count; i++) {
        http.addHeader(request->headers[i].name, request->headers[i].value);
    }
    http.setTimeout(request->timeout_ms);

    int httpCode;
    if (strcmp(request->method, "POST") == 0) {
        httpCode = http.POST((uint8_t *)request->body, (int)request->body_len);
    } else {
        httpCode = http.GET();
    }

    Log.verbose("[transport] HTTP %d\n", httpCode);

    if (httpCode < 0) {
        http.end();
        return EASY_API_ERR_NETWORK;
    }

    response->http_status = httpCode;
    response->body_len = 0;

    if (response->body != NULL && response->body_capacity > 0) {
        String payload = http.getString();
        size_t len = payload.length();
        if (len >= response->body_capacity) {
            len = response->body_capacity - 1;
        }
        memcpy(response->body, payload.c_str(), len);
        response->body[len] = '\0';
        response->body_len = len;
    }

    http.end();
    return EASY_API_OK;
}
