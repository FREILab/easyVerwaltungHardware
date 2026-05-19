#include "easy_api.h"
#include "../easy_api_example_config.h"
#include "../easy_api_example_http_transport.h"

#include <stdio.h>

int main(void) {
    char request_buffer[256];
    char response_buffer[1024];
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

    easy_api_health_response_t health;
    easy_api_status_t status = easy_api_init(&client, &config);
    if (status == EASY_API_OK) {
        status = easy_api_health_check(&client, &health);
    }

    if (status == EASY_API_OK) {
        printf("backend health OK: active=%u serviceId=%s serviceName=%s status=%s\n",
               health.active ? 1u : 0u,
               health.service_id,
               health.service_name,
               health.status);
        return 0;
    }

    printf("backend health failed: %s", easy_api_status_string(status));
    if (health.error.http_status != 0) {
        printf(" http=%d code=%s correlationId=%s", health.error.http_status, health.error.code, health.error.correlation_id);
    }
    printf("\n");
    return 1;
}
