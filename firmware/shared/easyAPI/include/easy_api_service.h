#ifndef EASY_API_SERVICE_H
#define EASY_API_SERVICE_H

/**
 * @file easy_api_service.h
 * @brief Non-blocking job/event layer for easyAPI machine-service workflows.
 *
 * The service layer does not perform network I/O itself. It owns request state,
 * timeout checks, event mapping, and fail-closed transitions. The application
 * provides an executor that starts and polls backend jobs without blocking the
 * UI or machine-control loop.
 */

#include "easy_api.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EASY_API_SERVICE_MAX_RFID_LEN
#define EASY_API_SERVICE_MAX_RFID_LEN 96u
#endif

#ifndef EASY_API_SERVICE_MAX_PIN_LEN
#define EASY_API_SERVICE_MAX_PIN_LEN 32u
#endif

#ifndef EASY_API_SERVICE_MAX_SESSION_ID_LEN
#define EASY_API_SERVICE_MAX_SESSION_ID_LEN EASY_API_MAX_SESSION_ID_LEN
#endif

typedef enum {
    EASY_API_SERVICE_IDLE = 0,
    EASY_API_SERVICE_QUEUED,
    EASY_API_SERVICE_RUNNING,
    EASY_API_SERVICE_CANCELLING
} easy_api_service_state_t;

typedef enum {
    EASY_API_SERVICE_JOB_NONE = 0,
    EASY_API_SERVICE_JOB_HEALTH,
    EASY_API_SERVICE_JOB_LOGIN,
    EASY_API_SERVICE_JOB_HEARTBEAT
} easy_api_service_job_type_t;

typedef enum {
    EASY_API_SERVICE_EXEC_PENDING = 0,
    EASY_API_SERVICE_EXEC_DONE,
    EASY_API_SERVICE_EXEC_FAILED
} easy_api_service_exec_state_t;

typedef enum {
    EASY_API_SERVICE_EVENT_NONE = 0,
    EASY_API_SERVICE_EVENT_HEALTH_OK,
    EASY_API_SERVICE_EVENT_HEALTH_FAILED,
    EASY_API_SERVICE_EVENT_LOGIN_GRANTED,
    EASY_API_SERVICE_EVENT_LOGIN_DENIED,
    EASY_API_SERVICE_EVENT_HEARTBEAT_OK,
    EASY_API_SERVICE_EVENT_HEARTBEAT_REJECTED,
    EASY_API_SERVICE_EVENT_TIMEOUT,
    EASY_API_SERVICE_EVENT_CANCELLED,
    EASY_API_SERVICE_EVENT_ERROR
} easy_api_service_event_type_t;

typedef struct {
    easy_api_service_job_type_t type;
    easy_api_client_t *client;
    easy_api_machine_mode_t mode;
    char rfid[EASY_API_SERVICE_MAX_RFID_LEN];
    char pin[EASY_API_SERVICE_MAX_PIN_LEN];
    char session_id[EASY_API_SERVICE_MAX_SESSION_ID_LEN];
} easy_api_service_job_t;

typedef struct {
    easy_api_service_job_type_t type;
    easy_api_status_t status;
    easy_api_health_response_t health;
    easy_api_login_response_t login;
    easy_api_heartbeat_response_t heartbeat;
} easy_api_service_result_t;

typedef struct {
    easy_api_service_event_type_t type;
    easy_api_status_t status;
    const easy_api_service_result_t *result;
} easy_api_service_event_t;

typedef uint32_t (*easy_api_service_now_ms_fn)(void *user_context);
typedef void (*easy_api_service_event_fn)(const easy_api_service_event_t *event, void *user_context);

typedef easy_api_status_t (*easy_api_service_executor_start_fn)(
    const easy_api_service_job_t *job,
    void *executor_context);

typedef easy_api_service_exec_state_t (*easy_api_service_executor_poll_fn)(
    easy_api_service_result_t *result,
    void *executor_context);

typedef void (*easy_api_service_executor_cancel_fn)(void *executor_context);

typedef struct {
    easy_api_client_t *client;
    easy_api_service_now_ms_fn now_ms;
    void *time_context;
    easy_api_service_event_fn event_handler;
    void *event_context;
    easy_api_service_executor_start_fn executor_start;
    easy_api_service_executor_poll_fn executor_poll;
    easy_api_service_executor_cancel_fn executor_cancel;
    void *executor_context;
    uint32_t default_timeout_ms;
} easy_api_service_config_t;

typedef struct {
    easy_api_service_config_t config;
    easy_api_service_state_t state;
    easy_api_service_job_t current_job;
    easy_api_service_result_t last_result;
    uint32_t deadline_ms;
} easy_api_service_t;

/**
 * @brief Initialize the service layer.
 *
 * The executor callbacks must be non-blocking. Use a worker task, IRQ-safe queue,
 * or cooperative transport below the executor boundary.
 */
easy_api_status_t easy_api_service_init(easy_api_service_t *service, const easy_api_service_config_t *config);

/** @brief Submit a health-check job. */
easy_api_status_t easy_api_service_submit_health(easy_api_service_t *service, uint32_t timeout_ms);

/** @brief Submit a machine-login job. RFID is copied into bounded volatile service state. */
easy_api_status_t easy_api_service_submit_login(
    easy_api_service_t *service,
    const char *rfid,
    easy_api_machine_mode_t mode,
    const char *pin,
    uint32_t timeout_ms);

/** @brief Submit a machine-heartbeat job. */
easy_api_status_t easy_api_service_submit_heartbeat(
    easy_api_service_t *service,
    const char *session_id,
    const char *rfid,
    uint32_t timeout_ms);

/**
 * @brief Progress the service state machine.
 *
 * This call is non-blocking when the configured executor is non-blocking. It may
 * emit at most one event per call through the configured event handler.
 */
easy_api_status_t easy_api_service_poll(easy_api_service_t *service);

/** @brief Cancel the active or queued job and emit CANCELLED on the next poll. */
easy_api_status_t easy_api_service_cancel(easy_api_service_t *service);

/** @brief Return the current state. */
easy_api_service_state_t easy_api_service_state(const easy_api_service_t *service);

/** @brief Return a stable string for a service event type. */
const char *easy_api_service_event_string(easy_api_service_event_type_t event_type);

#ifdef __cplusplus
}
#endif

#endif
