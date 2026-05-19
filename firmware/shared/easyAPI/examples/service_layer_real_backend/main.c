#include "easy_api_service.h"
#include "../easy_api_example_config.h"
#include "../easy_api_example_http_transport.h"

#include <stdio.h>
#include <string.h>

typedef struct { uint32_t now_ms; } app_clock_t;

typedef struct {
    easy_api_service_result_t result;
    bool active;
} blocking_executor_t;

static uint32_t app_now_ms(void *ctx) { return ((app_clock_t *)ctx)->now_ms; }

static void on_easy_api_event(const easy_api_service_event_t *event, void *ctx) {
    (void)ctx;
    printf("easyAPI event: %s (%s)\n", easy_api_service_event_string(event->type), easy_api_status_string(event->status));
    if (event->type == EASY_API_SERVICE_EVENT_LOGIN_GRANTED) {
        printf("session: %s\n", event->result->login.session_id);
    } else if (event->type == EASY_API_SERVICE_EVENT_LOGIN_DENIED && event->result->login.reason[0] != '\0') {
        printf("reason: %s\n", event->result->login.reason);
    }
}

static easy_api_status_t blocking_executor_start(const easy_api_service_job_t *job, void *ctx) {
    blocking_executor_t *exec = (blocking_executor_t *)ctx;
    memset(&exec->result, 0, sizeof(exec->result));
    exec->result.type = job->type;
    exec->active = true;

    if (job->type == EASY_API_SERVICE_JOB_LOGIN) {
        exec->result.status = easy_api_machine_login(job->client, job->rfid, job->mode, job->pin[0] != '\0' ? job->pin : NULL, &exec->result.login);
    } else if (job->type == EASY_API_SERVICE_JOB_HEARTBEAT) {
        exec->result.status = easy_api_machine_heartbeat(job->client, job->session_id, job->rfid, &exec->result.heartbeat);
    } else if (job->type == EASY_API_SERVICE_JOB_HEALTH) {
        exec->result.status = easy_api_health_check(job->client, &exec->result.health);
    } else {
        exec->result.status = EASY_API_ERR_INVALID_ARG;
    }

    return EASY_API_OK;
}

static easy_api_service_exec_state_t blocking_executor_poll(easy_api_service_result_t *result, void *ctx) {
    blocking_executor_t *exec = (blocking_executor_t *)ctx;
    if (!exec->active) {
        return EASY_API_SERVICE_EXEC_FAILED;
    }
    *result = exec->result;
    exec->active = false;
    return EASY_API_SERVICE_EXEC_DONE;
}

static void blocking_executor_cancel(void *ctx) { ((blocking_executor_t *)ctx)->active = false; }

int main(void) {
    char request_buffer[512];
    char response_buffer[2048];
    easy_api_client_t client;
    app_clock_t clock = {0};
    blocking_executor_t executor;
    easy_api_service_t service;

    const easy_api_config_t api_config = {
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

    memset(&executor, 0, sizeof(executor));
    if (easy_api_init(&client, &api_config) != EASY_API_OK) {
        return 1;
    }

    easy_api_service_config_t service_config = {
        .client = &client,
        .now_ms = app_now_ms,
        .time_context = &clock,
        .event_handler = on_easy_api_event,
        .executor_start = blocking_executor_start,
        .executor_poll = blocking_executor_poll,
        .executor_cancel = blocking_executor_cancel,
        .executor_context = &executor,
        .default_timeout_ms = 5000u
    };

    if (easy_api_service_init(&service, &service_config) != EASY_API_OK) {
        return 1;
    }
    if (easy_api_service_submit_login(&service, EASY_API_EXAMPLE_RFID, EASY_API_MODE_HEARTBEAT, EASY_API_EXAMPLE_PIN, 5000u) != EASY_API_OK) {
        return 1;
    }

    while (easy_api_service_state(&service) != EASY_API_SERVICE_IDLE) {
        easy_api_service_poll(&service);
        clock.now_ms += 10u;
    }

    return 0;
}
