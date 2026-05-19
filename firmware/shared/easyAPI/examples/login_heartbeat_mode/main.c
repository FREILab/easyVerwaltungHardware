#include "easy_api.h"
#include "../easy_api_example_config.h"
#include "../easy_api_example_http_transport.h"

#include <stdio.h>

int main(void) {
    char request_buffer[512];
    char response_buffer[2048];
    easy_api_client_t client;

    const easy_api_config_t config = {
        .base_url = EASY_API_EXAMPLE_BASE_URL,
        .service_id = EASY_API_EXAMPLE_SERVICE_ID,
        .service_token = EASY_API_EXAMPLE_SERVICE_TOKEN,
        .timeout_ms = 5000u,
        .request_buffer = request_buffer,
        .request_buffer_size = sizeof(request_buffer),
        .response_buffer = response_buffer,
        .response_buffer_size = sizeof(response_buffer),
        .transport = easy_api_example_http_transport,
        .transport_context = NULL,
    };

    easy_api_login_response_t login;
    easy_api_status_t status = easy_api_init(&client, &config);
    if (status == EASY_API_OK) {
        status = easy_api_machine_login(&client, EASY_API_EXAMPLE_RFID, EASY_API_MODE_HEARTBEAT, EASY_API_EXAMPLE_PIN, &login);
    }

    if (status != EASY_API_OK) {
        printf("heartbeat login failed: %s", easy_api_status_string(status));
        if (login.reason[0] != '\0') {
            printf(" reason=%s", login.reason);
        }
        if (login.error.http_status != 0) {
            printf(" http=%d code=%s correlationId=%s", login.error.http_status, login.error.code, login.error.correlation_id);
        }
        printf("\n");
        return 1;
    }

    printf("heartbeat login granted: sessionId=%s interval=%u timeout=%u\n",
           login.session_id,
           login.heartbeat_interval_seconds,
           login.session_timeout_seconds);

    for (unsigned i = 0u; i < 3u; ++i) {
        easy_api_heartbeat_response_t heartbeat;
        status = easy_api_machine_heartbeat(&client, login.session_id, EASY_API_EXAMPLE_RFID, &heartbeat);
        if (status != EASY_API_OK) {
            printf("heartbeat %u rejected/failed: %s", i + 1u, easy_api_status_string(status));
            if (heartbeat.reason[0] != '\0') {
                printf(" reason=%s", heartbeat.reason);
            }
            if (heartbeat.error.http_status != 0) {
                printf(" http=%d code=%s correlationId=%s", heartbeat.error.http_status, heartbeat.error.code, heartbeat.error.correlation_id);
            }
            printf("\n");
            return 1;
        }
        printf("heartbeat %u accepted\n", i + 1u);
    }

    return 0;
}
