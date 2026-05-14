#include "easy_api_service.h"

#include <string.h>

static bool has_text(const char *s) { return s != NULL && s[0] != '\0'; }

static void clear_memory(void *ptr, size_t len) {
    if (ptr != NULL && len > 0u) memset(ptr, 0, len);
}

static bool copy_bounded(char *dst, size_t cap, const char *src) {
    size_t len;
    if (dst == NULL || cap == 0u || src == NULL) return false;
    len = strlen(src);
    if (len + 1u > cap) return false;
    memcpy(dst, src, len + 1u);
    return true;
}

static bool elapsed_or_equal(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

static easy_api_status_t validate_service(const easy_api_service_t *service) {
    if (service == NULL) return EASY_API_ERR_INVALID_ARG;
    if (service->config.client == NULL) return EASY_API_ERR_INVALID_ARG;
    if (service->config.now_ms == NULL) return EASY_API_ERR_INVALID_ARG;
    if (service->config.executor_start == NULL || service->config.executor_poll == NULL) return EASY_API_ERR_INVALID_ARG;
    return EASY_API_OK;
}

static uint32_t effective_timeout(const easy_api_service_t *service, uint32_t timeout_ms) {
    if (timeout_ms != 0u) return timeout_ms;
    if (service->config.default_timeout_ms != 0u) return service->config.default_timeout_ms;
    return service->config.client->config.timeout_ms;
}

static void emit_event(easy_api_service_t *service, easy_api_service_event_type_t type, easy_api_status_t status) {
    if (service->config.event_handler != NULL) {
        easy_api_service_event_t event;
        event.type = type;
        event.status = status;
        event.result = &service->last_result;
        service->config.event_handler(&event, service->config.event_context);
    }
}

static easy_api_service_event_type_t map_result_event(const easy_api_service_result_t *result) {
    if (result->type == EASY_API_SERVICE_JOB_HEALTH) {
        return result->status == EASY_API_OK ? EASY_API_SERVICE_EVENT_HEALTH_OK : EASY_API_SERVICE_EVENT_HEALTH_FAILED;
    }
    if (result->type == EASY_API_SERVICE_JOB_LOGIN) {
        return result->status == EASY_API_OK ? EASY_API_SERVICE_EVENT_LOGIN_GRANTED : EASY_API_SERVICE_EVENT_LOGIN_DENIED;
    }
    if (result->type == EASY_API_SERVICE_JOB_HEARTBEAT) {
        return result->status == EASY_API_OK ? EASY_API_SERVICE_EVENT_HEARTBEAT_OK : EASY_API_SERVICE_EVENT_HEARTBEAT_REJECTED;
    }
    return EASY_API_SERVICE_EVENT_ERROR;
}

static easy_api_status_t submit_job(easy_api_service_t *service, easy_api_service_job_type_t type, uint32_t timeout_ms) {
    easy_api_status_t st = validate_service(service);
    if (st != EASY_API_OK) return st;
    if (service->state != EASY_API_SERVICE_IDLE) return EASY_API_ERR_INVALID_ARG;
    service->current_job.type = type;
    service->current_job.client = service->config.client;
    service->deadline_ms = service->config.now_ms(service->config.time_context) + effective_timeout(service, timeout_ms);
    service->state = EASY_API_SERVICE_QUEUED;
    return EASY_API_OK;
}

easy_api_status_t easy_api_service_init(easy_api_service_t *service, const easy_api_service_config_t *config) {
    easy_api_status_t st;
    if (service == NULL || config == NULL) return EASY_API_ERR_INVALID_ARG;
    clear_memory(service, sizeof(*service));
    service->config = *config;
    service->state = EASY_API_SERVICE_IDLE;
    st = validate_service(service);
    if (st != EASY_API_OK) clear_memory(service, sizeof(*service));
    return st;
}

easy_api_status_t easy_api_service_submit_health(easy_api_service_t *service, uint32_t timeout_ms) {
    easy_api_status_t st = validate_service(service);
    if (st != EASY_API_OK) return st;
    if (service->state != EASY_API_SERVICE_IDLE) return EASY_API_ERR_INVALID_ARG;
    clear_memory(&service->current_job, sizeof(service->current_job));
    return submit_job(service, EASY_API_SERVICE_JOB_HEALTH, timeout_ms);
}

easy_api_status_t easy_api_service_submit_login(easy_api_service_t *service, const char *rfid, easy_api_machine_mode_t mode, const char *pin, uint32_t timeout_ms) {
    easy_api_status_t st = validate_service(service);
    if (st != EASY_API_OK) return st;
    if (service->state != EASY_API_SERVICE_IDLE) return EASY_API_ERR_INVALID_ARG;
    if (!has_text(rfid)) return EASY_API_ERR_INVALID_ARG;
    if (mode != EASY_API_MODE_SINGLE && mode != EASY_API_MODE_HEARTBEAT) return EASY_API_ERR_INVALID_ARG;
    clear_memory(&service->current_job, sizeof(service->current_job));
    if (!copy_bounded(service->current_job.rfid, sizeof(service->current_job.rfid), rfid)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (pin != NULL && pin[0] != '\0' && !copy_bounded(service->current_job.pin, sizeof(service->current_job.pin), pin)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    service->current_job.mode = mode;
    st = submit_job(service, EASY_API_SERVICE_JOB_LOGIN, timeout_ms);
    if (st != EASY_API_OK) clear_memory(&service->current_job, sizeof(service->current_job));
    return st;
}

easy_api_status_t easy_api_service_submit_heartbeat(easy_api_service_t *service, const char *session_id, const char *rfid, uint32_t timeout_ms) {
    easy_api_status_t st = validate_service(service);
    if (st != EASY_API_OK) return st;
    if (service->state != EASY_API_SERVICE_IDLE) return EASY_API_ERR_INVALID_ARG;
    if (!has_text(session_id) || !has_text(rfid)) return EASY_API_ERR_INVALID_ARG;
    clear_memory(&service->current_job, sizeof(service->current_job));
    if (!copy_bounded(service->current_job.session_id, sizeof(service->current_job.session_id), session_id)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!copy_bounded(service->current_job.rfid, sizeof(service->current_job.rfid), rfid)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    st = submit_job(service, EASY_API_SERVICE_JOB_HEARTBEAT, timeout_ms);
    if (st != EASY_API_OK) clear_memory(&service->current_job, sizeof(service->current_job));
    return st;
}

easy_api_status_t easy_api_service_poll(easy_api_service_t *service) {
    easy_api_service_exec_state_t exec_state;
    easy_api_status_t st;
    uint32_t now;

    st = validate_service(service);
    if (st != EASY_API_OK) return st;
    if (service->state == EASY_API_SERVICE_IDLE) return EASY_API_OK;

    if (service->state == EASY_API_SERVICE_CANCELLING) {
        if (service->config.executor_cancel != NULL) service->config.executor_cancel(service->config.executor_context);
        clear_memory(&service->last_result, sizeof(service->last_result));
        service->last_result.type = service->current_job.type;
        service->last_result.status = EASY_API_ERR_NETWORK;
        clear_memory(&service->current_job, sizeof(service->current_job));
        service->state = EASY_API_SERVICE_IDLE;
        emit_event(service, EASY_API_SERVICE_EVENT_CANCELLED, EASY_API_ERR_NETWORK);
        return EASY_API_OK;
    }

    now = service->config.now_ms(service->config.time_context);
    if (elapsed_or_equal(now, service->deadline_ms)) {
        if (service->config.executor_cancel != NULL) service->config.executor_cancel(service->config.executor_context);
        clear_memory(&service->last_result, sizeof(service->last_result));
        service->last_result.type = service->current_job.type;
        service->last_result.status = EASY_API_ERR_TIMEOUT;
        clear_memory(&service->current_job, sizeof(service->current_job));
        service->state = EASY_API_SERVICE_IDLE;
        emit_event(service, EASY_API_SERVICE_EVENT_TIMEOUT, EASY_API_ERR_TIMEOUT);
        return EASY_API_OK;
    }

    if (service->state == EASY_API_SERVICE_QUEUED) {
        st = service->config.executor_start(&service->current_job, service->config.executor_context);
        if (st != EASY_API_OK) {
            clear_memory(&service->last_result, sizeof(service->last_result));
            service->last_result.type = service->current_job.type;
            service->last_result.status = st;
            clear_memory(&service->current_job, sizeof(service->current_job));
            service->state = EASY_API_SERVICE_IDLE;
            emit_event(service, EASY_API_SERVICE_EVENT_ERROR, st);
            return EASY_API_OK;
        }
        service->state = EASY_API_SERVICE_RUNNING;
        return EASY_API_OK;
    }

    exec_state = service->config.executor_poll(&service->last_result, service->config.executor_context);
    if (exec_state == EASY_API_SERVICE_EXEC_PENDING) return EASY_API_OK;

    if (exec_state == EASY_API_SERVICE_EXEC_FAILED && service->last_result.status == EASY_API_OK) {
        service->last_result.status = EASY_API_ERR_NETWORK;
    }

    if (service->last_result.type == EASY_API_SERVICE_JOB_NONE) service->last_result.type = service->current_job.type;
    clear_memory(&service->current_job, sizeof(service->current_job));
    service->state = EASY_API_SERVICE_IDLE;
    emit_event(service, map_result_event(&service->last_result), service->last_result.status);
    return EASY_API_OK;
}

easy_api_status_t easy_api_service_cancel(easy_api_service_t *service) {
    easy_api_status_t st = validate_service(service);
    if (st != EASY_API_OK) return st;
    if (service->state == EASY_API_SERVICE_IDLE) return EASY_API_OK;
    service->state = EASY_API_SERVICE_CANCELLING;
    return EASY_API_OK;
}

easy_api_service_state_t easy_api_service_state(const easy_api_service_t *service) {
    return service == NULL ? EASY_API_SERVICE_IDLE : service->state;
}

const char *easy_api_service_event_string(easy_api_service_event_type_t event_type) {
    switch (event_type) {
        case EASY_API_SERVICE_EVENT_NONE: return "NONE";
        case EASY_API_SERVICE_EVENT_HEALTH_OK: return "HEALTH_OK";
        case EASY_API_SERVICE_EVENT_HEALTH_FAILED: return "HEALTH_FAILED";
        case EASY_API_SERVICE_EVENT_LOGIN_GRANTED: return "LOGIN_GRANTED";
        case EASY_API_SERVICE_EVENT_LOGIN_DENIED: return "LOGIN_DENIED";
        case EASY_API_SERVICE_EVENT_HEARTBEAT_OK: return "HEARTBEAT_OK";
        case EASY_API_SERVICE_EVENT_HEARTBEAT_REJECTED: return "HEARTBEAT_REJECTED";
        case EASY_API_SERVICE_EVENT_TIMEOUT: return "TIMEOUT";
        case EASY_API_SERVICE_EVENT_CANCELLED: return "CANCELLED";
        case EASY_API_SERVICE_EVENT_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
