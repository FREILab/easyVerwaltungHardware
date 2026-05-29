#pragma once

#include "easy_api.h"

extern "C" easy_api_status_t easy_api_esp32_transport(
    const easy_api_http_request_t *request,
    easy_api_http_response_t *response,
    void *user_context);
