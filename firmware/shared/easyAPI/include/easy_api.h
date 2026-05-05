#ifndef EASY_API_H
#define EASY_API_H

/**
 * @file easy_api.h
 * @brief Public API for bounded HTTP/JSON backend communication from embedded firmware.
 *
 * easyAPI is transport-agnostic. Applications provide a transport callback that performs
 * the actual HTTP request. The library builds JSON payloads, validates inputs, maps HTTP
 * and transport errors, and parses the expected backend JSON responses.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef EASY_API_DEFAULT_TIMEOUT_MS
#define EASY_API_DEFAULT_TIMEOUT_MS 5000u
#endif

#define EASY_API_MAX_REASON_LEN 64u
#define EASY_API_MAX_SERVICE_ID_LEN 64u
#define EASY_API_MAX_SERVICE_NAME_LEN 96u
#define EASY_API_MAX_SESSION_ID_LEN 96u
#define EASY_API_MAX_USER_FIELD_LEN 96u
#define EASY_API_MAX_ERROR_CODE_LEN 64u
#define EASY_API_MAX_CORRELATION_ID_LEN 96u

    typedef enum
    {
        EASY_API_OK = 0,
        EASY_API_ERR_INVALID_ARG,
        EASY_API_ERR_TIMEOUT,
        EASY_API_ERR_NETWORK,
        EASY_API_ERR_HTTP_STATUS,
        EASY_API_ERR_JSON_PARSE,
        EASY_API_ERR_BUFFER_TOO_SMALL,
        EASY_API_ERR_BACKEND_DENIED,
        EASY_API_ERR_SERVICE_INACTIVE
    } easy_api_status_t;

    typedef enum
    {
        EASY_API_MODE_SINGLE = 0,
        EASY_API_MODE_HEARTBEAT = 1
    } easy_api_machine_mode_t;

    typedef struct
    {
        const char *name;
        const char *value;
    } easy_api_header_t;

    typedef struct
    {
        const char *method;
        const char *url;
        const easy_api_header_t *headers;
        size_t header_count;
        const char *body;
        size_t body_len;
        uint32_t timeout_ms;
    } easy_api_http_request_t;

    typedef struct
    {
        int http_status;
        char *body;
        size_t body_capacity;
        size_t body_len;
    } easy_api_http_response_t;

    /**
     * @brief Transport function implemented by the platform or application.
     *
     * The callback must block no longer than request->timeout_ms. On success it fills
     * response->http_status, response->body, and response->body_len. It must never write
     * more than response->body_capacity bytes including the trailing NUL byte when a body
     * is returned.
     */
    typedef easy_api_status_t (*easy_api_transport_fn)(
        const easy_api_http_request_t *request,
        easy_api_http_response_t *response,
        void *user_context);

    typedef struct
    {
        const char *base_url;
        const char *service_id;
        const char *service_token;
        uint32_t timeout_ms;
        char *request_buffer;
        size_t request_buffer_size;
        char *response_buffer;
        size_t response_buffer_size;
        easy_api_transport_fn transport;
        void *transport_context;
    } easy_api_config_t;

    typedef struct
    {
        easy_api_config_t config;
    } easy_api_client_t;

    typedef struct
    {
        char code[EASY_API_MAX_ERROR_CODE_LEN];
        char correlation_id[EASY_API_MAX_CORRELATION_ID_LEN];
        int http_status;
    } easy_api_error_info_t;

    typedef struct
    {
        bool active;
        char service_id[EASY_API_MAX_SERVICE_ID_LEN];
        char service_name[EASY_API_MAX_SERVICE_NAME_LEN];
        char status[EASY_API_MAX_REASON_LEN];
        easy_api_error_info_t error;
    } easy_api_health_response_t;

    typedef struct
    {
        bool valid;
        char member_id[EASY_API_MAX_USER_FIELD_LEN];
        char user_id[EASY_API_MAX_USER_FIELD_LEN];
        char first_name[EASY_API_MAX_USER_FIELD_LEN];
        char last_name[EASY_API_MAX_USER_FIELD_LEN];
        char email[EASY_API_MAX_USER_FIELD_LEN];
    } easy_api_user_t;

    typedef struct
    {
        bool authorized;
        easy_api_machine_mode_t mode;
        char session_id[EASY_API_MAX_SESSION_ID_LEN];
        uint32_t heartbeat_interval_seconds;
        uint32_t session_timeout_seconds;
        char service_id[EASY_API_MAX_SERVICE_ID_LEN];
        char service_name[EASY_API_MAX_SERVICE_NAME_LEN];
        char reason[EASY_API_MAX_REASON_LEN];
        easy_api_user_t user;
        easy_api_error_info_t error;
    } easy_api_login_response_t;

    typedef struct
    {
        bool valid;
        bool session_active;
        char reason[EASY_API_MAX_REASON_LEN];
        easy_api_error_info_t error;
    } easy_api_heartbeat_response_t;

    /**
     * @brief Initialize a client with explicit configuration.
     */
    easy_api_status_t easy_api_init(easy_api_client_t *client, const easy_api_config_t *config);

    /**
     * @brief Return a stable string for a status code.
     */
    const char *easy_api_status_string(easy_api_status_t status);

    /**
     * @brief Convert a machine mode enum to the backend wire value.
     */
    const char *easy_api_mode_string(easy_api_machine_mode_t mode);

    /**
     * @brief Call POST /api/service/health?serviceId=<serviceId>.
     *
     * Blocking behavior is controlled by config.timeout_ms. Non-2xx responses return
     * EASY_API_ERR_HTTP_STATUS and populate response->error when the backend error
     * envelope is present.
     */
    easy_api_status_t easy_api_health_check(easy_api_client_t *client, easy_api_health_response_t *response);

    /**
     * @brief Call POST /api/service/machine/login.
     *
     * The RFID and optional PIN are serialized into a bounded caller-provided request
     * buffer. Raw RFID and token values are never stored by the library after the call.
     */
    easy_api_status_t easy_api_machine_login(
        easy_api_client_t *client,
        const char *rfid,
        easy_api_machine_mode_t mode,
        const char *pin,
        easy_api_login_response_t *response);

    /**
     * @brief Call POST /api/service/machine/heartbeat.
     *
     * The function fails closed on timeout, network failure, non-2xx response, malformed
     * JSON, or an explicit backend rejection.
     */
    easy_api_status_t easy_api_machine_heartbeat(
        easy_api_client_t *client,
        const char *session_id,
        const char *rfid,
        easy_api_heartbeat_response_t *response);

#ifdef __cplusplus
}
#endif

#endif
