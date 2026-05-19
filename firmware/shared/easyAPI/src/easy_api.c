#include "easy_api.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define EASY_API_CONTENT_TYPE "application/json"
#define EASY_API_ACCEPT "application/json"
#define EASY_API_HEALTH_PATH "/api/service/health?serviceId="
#define EASY_API_LOGIN_PATH "/api/service/machine/login"
#define EASY_API_HEARTBEAT_PATH "/api/service/machine/heartbeat"

static bool has_text(const char *s) { return s != NULL && s[0] != '\0'; }

static void clear_memory(void *ptr, size_t len) {
    if (ptr != NULL && len > 0u) memset(ptr, 0, len);
}

static bool append_char(char *out, size_t cap, size_t *pos, char c) {
    if (*pos + 1u >= cap) return false;
    out[(*pos)++] = c;
    out[*pos] = '\0';
    return true;
}

static bool append_str(char *out, size_t cap, size_t *pos, const char *s) {
    if (s == NULL) return true;
    while (*s != '\0') {
        if (!append_char(out, cap, pos, *s++)) return false;
    }
    return true;
}

/* Serialize only RFC 8259 string escapes required by the backend payloads. */
static bool append_json_string(char *out, size_t cap, size_t *pos, const char *s) {
    if (!append_char(out, cap, pos, '"')) return false;
    if (s != NULL) {
        for (; *s != '\0'; ++s) {
            unsigned char ch = (unsigned char)*s;
            switch (ch) {
                case '"': if (!append_str(out, cap, pos, "\\\"")) return false; break;
                case '\\': if (!append_str(out, cap, pos, "\\\\")) return false; break;
                case '\b': if (!append_str(out, cap, pos, "\\b")) return false; break;
                case '\f': if (!append_str(out, cap, pos, "\\f")) return false; break;
                case '\n': if (!append_str(out, cap, pos, "\\n")) return false; break;
                case '\r': if (!append_str(out, cap, pos, "\\r")) return false; break;
                case '\t': if (!append_str(out, cap, pos, "\\t")) return false; break;
                default:
                    if (ch < 0x20u) return false;
                    if (!append_char(out, cap, pos, (char)ch)) return false;
            }
        }
    }
    return append_char(out, cap, pos, '"');
}

static bool build_url(const easy_api_client_t *client, const char *path, char *out, size_t cap) {
    size_t pos = 0u;
    const char *base = client->config.base_url;
    size_t len = strlen(base);
    if (!append_str(out, cap, &pos, base)) return false;
    if (len > 0u && base[len - 1u] == '/' && path[0] == '/') {
        path++;
    }
    return append_str(out, cap, &pos, path);
}

static bool build_health_url(const easy_api_client_t *client, char *out, size_t cap) {
    size_t pos = 0u;
    const char *base = client->config.base_url;
    size_t len = strlen(base);
    if (!append_str(out, cap, &pos, base)) return false;
    if (len > 0u && base[len - 1u] == '/') {
        if (!append_str(out, cap, &pos, "api/service/health?serviceId=")) return false;
    } else {
        if (!append_str(out, cap, &pos, EASY_API_HEALTH_PATH)) return false;
    }
    return append_str(out, cap, &pos, client->config.service_id);
}

/* Bounded field extraction for known response schemas, not a general JSON parser. */
static const char *find_key_value(const char *json, const char *key) {
    char pattern[64];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (n <= 0 || (size_t)n >= sizeof(pattern)) return NULL;
    const char *p = strstr(json, pattern);
    if (p == NULL) return NULL;
    p += (size_t)n;
    while (*p != '\0' && isspace((unsigned char)*p)) p++;
    if (*p != ':') return NULL;
    p++;
    while (*p != '\0' && isspace((unsigned char)*p)) p++;
    return p;
}

static bool json_get_bool(const char *json, const char *key, bool *value) {
    const char *p = find_key_value(json, key);
    if (p == NULL) return false;
    if (strncmp(p, "true", 4u) == 0) { *value = true; return true; }
    if (strncmp(p, "false", 5u) == 0) { *value = false; return true; }
    return false;
}

static bool json_get_uint(const char *json, const char *key, uint32_t *value) {
    const char *p = find_key_value(json, key);
    uint32_t v = 0u;
    if (p == NULL || !isdigit((unsigned char)*p)) return false;
    while (isdigit((unsigned char)*p)) {
        uint32_t d = (uint32_t)(*p - '0');
        if (v > (UINT32_MAX - d) / 10u) return false;
        v = (v * 10u) + d;
        p++;
    }
    *value = v;
    return true;
}

static bool json_get_string(const char *json, const char *key, char *out, size_t cap) {
    const char *p = find_key_value(json, key);
    size_t pos = 0u;
    if (cap == 0u) return false;
    out[0] = '\0';
    if (p == NULL || *p != '"') return false;
    p++;
    while (*p != '\0' && *p != '"') {
        char c = *p++;
        if (c == '\\') {
            c = *p++;
            switch (c) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                default: return false;
            }
        }
        if (pos + 1u >= cap) return false;
        out[pos++] = c;
    }
    if (*p != '"') return false;
    out[pos] = '\0';
    return true;
}

static void update_user_valid(easy_api_user_t *user) {
    if (user == NULL) return;

    user->valid =
        has_text(user->member_id) ||
        has_text(user->user_id) ||
        has_text(user->first_name) ||
        has_text(user->last_name) ||
        has_text(user->email);
}

static void parse_login_user(const char *json, easy_api_user_t *user) {
    if (json == NULL || user == NULL) return;

    (void)json_get_string(json, "member_id", user->member_id, sizeof(user->member_id));
    (void)json_get_string(json, "user_id", user->user_id, sizeof(user->user_id));
    (void)json_get_string(json, "first_name", user->first_name, sizeof(user->first_name));
    (void)json_get_string(json, "last_name", user->last_name, sizeof(user->last_name));
    (void)json_get_string(json, "email", user->email, sizeof(user->email));

    update_user_valid(user);
}

static void parse_error_info(const char *json, int http_status, easy_api_error_info_t *error) {
    if (error == NULL) return;
    clear_memory(error, sizeof(*error));
    error->http_status = http_status;
    (void)json_get_string(json, "code", error->code, sizeof(error->code));
    (void)json_get_string(json, "correlationId", error->correlation_id, sizeof(error->correlation_id));
}

/* The transport owns sockets, TLS, and timeout enforcement. */
static easy_api_status_t execute_json(easy_api_client_t *client, const char *url, const char *body, easy_api_http_response_t *resp) {
    easy_api_header_t headers[3] = {
        { "X-Service-Token", client->config.service_token },
        { "Content-Type", EASY_API_CONTENT_TYPE },
        { "Accept", EASY_API_ACCEPT }
    };
    easy_api_http_request_t req = {
        .method = "POST",
        .url = url,
        .headers = headers,
        .header_count = 3u,
        .body = body,
        .body_len = body != NULL ? strlen(body) : 0u,
        .timeout_ms = client->config.timeout_ms
    };
    resp->http_status = 0;
    resp->body = client->config.response_buffer;
    resp->body_capacity = client->config.response_buffer_size;
    resp->body_len = 0u;
    if (resp->body_capacity > 0u) resp->body[0] = '\0';
    return client->config.transport(&req, resp, client->config.transport_context);
}

/* Keep validation centralized so every public entry point fails closed. */
static easy_api_status_t validate_client(const easy_api_client_t *client) {
    if (client == NULL) return EASY_API_ERR_INVALID_ARG;
    if (!has_text(client->config.base_url) || !has_text(client->config.service_id) || !has_text(client->config.service_token)) return EASY_API_ERR_INVALID_ARG;
    if (client->config.timeout_ms == 0u || client->config.transport == NULL) return EASY_API_ERR_INVALID_ARG;
    if (client->config.request_buffer == NULL || client->config.request_buffer_size < 2u) return EASY_API_ERR_INVALID_ARG;
    if (client->config.response_buffer == NULL || client->config.response_buffer_size < 2u) return EASY_API_ERR_INVALID_ARG;
    return EASY_API_OK;
}

easy_api_status_t easy_api_init(easy_api_client_t *client, const easy_api_config_t *config) {
    if (client == NULL || config == NULL) return EASY_API_ERR_INVALID_ARG;
    clear_memory(client, sizeof(*client));
    client->config = *config;
    if (client->config.timeout_ms == 0u) client->config.timeout_ms = EASY_API_DEFAULT_TIMEOUT_MS;
    return validate_client(client);
}

const char *easy_api_status_string(easy_api_status_t status) {
    switch (status) {
        case EASY_API_OK: return "OK";
        case EASY_API_ERR_INVALID_ARG: return "INVALID_ARG";
        case EASY_API_ERR_TIMEOUT: return "TIMEOUT";
        case EASY_API_ERR_NETWORK: return "NETWORK";
        case EASY_API_ERR_HTTP_STATUS: return "HTTP_STATUS";
        case EASY_API_ERR_JSON_PARSE: return "JSON_PARSE";
        case EASY_API_ERR_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case EASY_API_ERR_BACKEND_DENIED: return "BACKEND_DENIED";
        case EASY_API_ERR_SERVICE_INACTIVE: return "SERVICE_INACTIVE";
        default: return "UNKNOWN";
    }
}

const char *easy_api_mode_string(easy_api_machine_mode_t mode) {
    return mode == EASY_API_MODE_HEARTBEAT ? "heartbeat" : "single";
}

easy_api_status_t easy_api_health_check(easy_api_client_t *client, easy_api_health_response_t *response) {
    if (validate_client(client) != EASY_API_OK || response == NULL) return EASY_API_ERR_INVALID_ARG;
    clear_memory(response, sizeof(*response));
    char url[256];
    if (!build_health_url(client, url, sizeof(url))) return EASY_API_ERR_BUFFER_TOO_SMALL;
    easy_api_http_response_t http_resp;
    easy_api_status_t st = execute_json(client, url, NULL, &http_resp);
    if (st != EASY_API_OK) return st;
    if (http_resp.http_status < 200 || http_resp.http_status >= 300) {
        parse_error_info(http_resp.body, http_resp.http_status, &response->error);
        return EASY_API_ERR_HTTP_STATUS;
    }
    if (!json_get_bool(http_resp.body, "active", &response->active)) {
        return EASY_API_ERR_JSON_PARSE;
    }
    if (!response->active) {
        (void)json_get_string(http_resp.body, "status", response->status, sizeof(response->status));
        return EASY_API_ERR_SERVICE_INACTIVE;
    }
    if (!json_get_string(http_resp.body, "serviceId", response->service_id, sizeof(response->service_id))) {
        return EASY_API_ERR_JSON_PARSE;
    }
    (void)json_get_string(http_resp.body, "serviceName", response->service_name, sizeof(response->service_name));
    (void)json_get_string(http_resp.body, "status", response->status, sizeof(response->status));
}

easy_api_status_t easy_api_machine_login(easy_api_client_t *client, const char *rfid, easy_api_machine_mode_t mode, const char *pin, easy_api_login_response_t *response) {
    if (validate_client(client) != EASY_API_OK || !has_text(rfid) || response == NULL) return EASY_API_ERR_INVALID_ARG;
    if (mode != EASY_API_MODE_SINGLE && mode != EASY_API_MODE_HEARTBEAT) return EASY_API_ERR_INVALID_ARG;
    clear_memory(response, sizeof(*response));
    char url[256];
    if (!build_url(client, EASY_API_LOGIN_PATH, url, sizeof(url))) return EASY_API_ERR_BUFFER_TOO_SMALL;
    char *body = client->config.request_buffer;
    size_t pos = 0u;
    body[0] = '\0';
    if (!append_str(body, client->config.request_buffer_size, &pos, "{\"serviceId\":")) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_json_string(body, client->config.request_buffer_size, &pos, client->config.service_id)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_str(body, client->config.request_buffer_size, &pos, ",\"rfid\":")) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_json_string(body, client->config.request_buffer_size, &pos, rfid)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_str(body, client->config.request_buffer_size, &pos, ",\"mode\":")) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_json_string(body, client->config.request_buffer_size, &pos, easy_api_mode_string(mode))) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (has_text(pin)) {
        if (!append_str(body, client->config.request_buffer_size, &pos, ",\"pin\":")) return EASY_API_ERR_BUFFER_TOO_SMALL;
        if (!append_json_string(body, client->config.request_buffer_size, &pos, pin)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    }
    if (!append_char(body, client->config.request_buffer_size, &pos, '}')) return EASY_API_ERR_BUFFER_TOO_SMALL;
    easy_api_http_response_t http_resp;
    easy_api_status_t st = execute_json(client, url, body, &http_resp);
    if (st != EASY_API_OK) return st;
    if (http_resp.http_status < 200 || http_resp.http_status >= 300) {
        parse_error_info(http_resp.body, http_resp.http_status, &response->error);
        return EASY_API_ERR_HTTP_STATUS;
    }
    if (!json_get_bool(http_resp.body, "authorized", &response->authorized)) return EASY_API_ERR_JSON_PARSE;
    char mode_str[16];
    if (!json_get_string(http_resp.body, "mode", mode_str, sizeof(mode_str))) {
        if (response->authorized) return EASY_API_ERR_JSON_PARSE;
    } else {
        response->mode = strcmp(mode_str, "heartbeat") == 0 ? EASY_API_MODE_HEARTBEAT : EASY_API_MODE_SINGLE;
    }
    (void)json_get_string(http_resp.body, "serviceId", response->service_id, sizeof(response->service_id));
    (void)json_get_string(http_resp.body, "serviceName", response->service_name, sizeof(response->service_name));
    (void)json_get_string(http_resp.body, "reason", response->reason, sizeof(response->reason));
    (void)json_get_string(http_resp.body, "sessionId", response->session_id, sizeof(response->session_id));
    (void)json_get_uint(http_resp.body, "heartbeatIntervalSeconds", &response->heartbeat_interval_seconds);
    (void)json_get_uint(http_resp.body, "sessionTimeoutSeconds", &response->session_timeout_seconds);

    parse_login_user(http_resp.body, &response->user);

    if (!response->authorized) return EASY_API_ERR_BACKEND_DENIED;
    if (response->mode == EASY_API_MODE_HEARTBEAT &&
        (!has_text(response->session_id) || response->heartbeat_interval_seconds == 0u || response->session_timeout_seconds == 0u)) {
        return EASY_API_ERR_JSON_PARSE;
    }
    return EASY_API_OK;
}

easy_api_status_t easy_api_machine_heartbeat(easy_api_client_t *client, const char *session_id, const char *rfid, easy_api_heartbeat_response_t *response) {
    if (validate_client(client) != EASY_API_OK || !has_text(session_id) || !has_text(rfid) || response == NULL) return EASY_API_ERR_INVALID_ARG;
    clear_memory(response, sizeof(*response));
    char url[256];
    if (!build_url(client, EASY_API_HEARTBEAT_PATH, url, sizeof(url))) return EASY_API_ERR_BUFFER_TOO_SMALL;
    char *body = client->config.request_buffer;
    size_t pos = 0u;
    body[0] = '\0';
    if (!append_str(body, client->config.request_buffer_size, &pos, "{\"serviceId\":")) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_json_string(body, client->config.request_buffer_size, &pos, client->config.service_id)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_str(body, client->config.request_buffer_size, &pos, ",\"sessionId\":")) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_json_string(body, client->config.request_buffer_size, &pos, session_id)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_str(body, client->config.request_buffer_size, &pos, ",\"rfid\":")) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_json_string(body, client->config.request_buffer_size, &pos, rfid)) return EASY_API_ERR_BUFFER_TOO_SMALL;
    if (!append_char(body, client->config.request_buffer_size, &pos, '}')) return EASY_API_ERR_BUFFER_TOO_SMALL;
    easy_api_http_response_t http_resp;
    easy_api_status_t st = execute_json(client, url, body, &http_resp);
    if (st != EASY_API_OK) return st;
    if (http_resp.http_status < 200 || http_resp.http_status >= 300) {
        parse_error_info(http_resp.body, http_resp.http_status, &response->error);
        return EASY_API_ERR_HTTP_STATUS;
    }
    if (!json_get_bool(http_resp.body, "valid", &response->valid) ||
        !json_get_bool(http_resp.body, "sessionActive", &response->session_active)) {
        return EASY_API_ERR_JSON_PARSE;
    }
    (void)json_get_string(http_resp.body, "reason", response->reason, sizeof(response->reason));
    return (response->valid && response->session_active) ? EASY_API_OK : EASY_API_ERR_BACKEND_DENIED;
}