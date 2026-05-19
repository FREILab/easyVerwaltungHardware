#include "easy_api.h"
#include "../easy_api_example_config.h"
#include "../easy_api_example_http_transport.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    MACHINE_IDLE = 0,
    MACHINE_LOGIN_PENDING,
    MACHINE_ACTIVE,
    MACHINE_ENDED,
} machine_state_t;

typedef struct {
    char session_id[EASY_API_MAX_SESSION_ID_LEN];
    uint32_t heartbeat_interval_seconds;
    uint32_t session_timeout_seconds;
    bool output_enabled;
} machine_session_t;

static void machine_enable_output(machine_session_t *session) {
    session->output_enabled = true;
    printf("machine output enabled\n");
}

static void machine_end_session(machine_session_t *session, const char *reason) {
    session->output_enabled = false;
    session->session_id[0] = '\0';
    printf("machine session ended: %s\n", reason);
}

int main(void) {
    char request_buffer[512];
    char response_buffer[2048];
    easy_api_client_t client;
    machine_session_t session = {0};
    machine_state_t state = MACHINE_IDLE;
    easy_api_status_t status;
    unsigned heartbeat_attempts = 0u;

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

    status = easy_api_init(&client, &config);
    if (status != EASY_API_OK) {
        printf("easyAPI init failed: %s\n", easy_api_status_string(status));
        return 1;
    }

    while (state != MACHINE_ENDED) {
        switch (state) {
            case MACHINE_IDLE:
                printf("rfid detected: %s\n", EASY_API_EXAMPLE_RFID);
                state = MACHINE_LOGIN_PENDING;
                break;

            case MACHINE_LOGIN_PENDING: {
                easy_api_login_response_t login;
                status = easy_api_machine_login(&client, EASY_API_EXAMPLE_RFID, EASY_API_MODE_HEARTBEAT, EASY_API_EXAMPLE_PIN, &login);
                if (status != EASY_API_OK || !login.authorized) {
                    machine_end_session(&session, login.reason[0] != '\0' ? login.reason : easy_api_status_string(status));
                    state = MACHINE_ENDED;
                    break;
                }

                snprintf(session.session_id, sizeof(session.session_id), "%s", login.session_id);
                session.heartbeat_interval_seconds = login.heartbeat_interval_seconds;
                session.session_timeout_seconds = login.session_timeout_seconds;
                machine_enable_output(&session);
                state = MACHINE_ACTIVE;
                break;
            }

            case MACHINE_ACTIVE: {
                easy_api_heartbeat_response_t heartbeat;
                status = easy_api_machine_heartbeat(&client, session.session_id, EASY_API_EXAMPLE_RFID, &heartbeat);
                heartbeat_attempts++;
                if (status != EASY_API_OK) {
                    machine_end_session(&session, heartbeat.reason[0] != '\0' ? heartbeat.reason : easy_api_status_string(status));
                    state = MACHINE_ENDED;
                    break;
                }

                printf("heartbeat accepted\n");
                if (heartbeat_attempts >= 3u) {
                    machine_end_session(&session, "example completed after 3 heartbeats");
                    state = MACHINE_ENDED;
                }
                break;
            }

            case MACHINE_ENDED:
            default:
                break;
        }
    }

    return session.output_enabled ? 1 : 0;
}
