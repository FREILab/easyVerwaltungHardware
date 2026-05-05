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
        status = easy_api_machine_login(&client, EASY_API_EXAMPLE_RFID, EASY_API_MODE_SINGLE, EASY_API_EXAMPLE_PIN, &login);
    }

    if (status == EASY_API_OK && login.authorized) {
        printf("single login granted: serviceId=%s sessionId=%s\n", login.service_id, login.session_id);
        if (login.user.first_name[0] != '\0' || login.user.last_name[0] != '\0' || login.user.email[0] != '\0') {
            printf("display user: %s %s <%s>\n", login.user.first_name, login.user.last_name, login.user.email);
        }
        return 0;
    }

    printf("single login denied/failed: %s", easy_api_status_string(status));
    if (login.reason[0] != '\0') {
        printf(" reason=%s", login.reason);
    }
    if (login.error.http_status != 0) {
        printf(" http=%d code=%s correlationId=%s", login.error.http_status, login.error.code, login.error.correlation_id);
    }
    printf("\n");
    return 1;
}
