#ifndef EASY_API_EXAMPLE_HTTP_TRANSPORT_H
#define EASY_API_EXAMPLE_HTTP_TRANSPORT_H

#include "easy_api.h"

#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef EASY_API_EXAMPLE_MAX_HOST_LEN
#define EASY_API_EXAMPLE_MAX_HOST_LEN 128u
#endif

#ifndef EASY_API_EXAMPLE_MAX_PORT_LEN
#define EASY_API_EXAMPLE_MAX_PORT_LEN 8u
#endif

#ifndef EASY_API_EXAMPLE_MAX_PATH_LEN
#define EASY_API_EXAMPLE_MAX_PATH_LEN 256u
#endif

#ifndef EASY_API_EXAMPLE_TX_BUFFER_SIZE
#define EASY_API_EXAMPLE_TX_BUFFER_SIZE 2048u
#endif

typedef struct {
    char host[EASY_API_EXAMPLE_MAX_HOST_LEN];
    char port[EASY_API_EXAMPLE_MAX_PORT_LEN];
    char path[EASY_API_EXAMPLE_MAX_PATH_LEN];
} easy_api_example_url_t;

static int easy_api_example_close_socket(int fd) {
    if (fd >= 0) {
        (void)close(fd);
    }
    return -1;
}

static bool easy_api_example_copy_slice(char *dst, size_t dst_cap, const char *start, size_t len) {
    if (dst == NULL || start == NULL || dst_cap == 0u || len + 1u > dst_cap) {
        return false;
    }
    memcpy(dst, start, len);
    dst[len] = '\0';
    return true;
}

static bool easy_api_example_parse_http_url(const char *url, easy_api_example_url_t *out) {
    static const char prefix[] = "http://";
    const char *host_start;
    const char *host_end;
    const char *path_start;
    const char *port_start;

    if (url == NULL || out == NULL || strncmp(url, prefix, sizeof(prefix) - 1u) != 0) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    host_start = url + sizeof(prefix) - 1u;
    path_start = strchr(host_start, '/');
    host_end = path_start != NULL ? path_start : host_start + strlen(host_start);
    port_start = memchr(host_start, ':', (size_t)(host_end - host_start));

    if (port_start != NULL) {
        if (!easy_api_example_copy_slice(out->host, sizeof(out->host), host_start, (size_t)(port_start - host_start))) {
            return false;
        }
        if (!easy_api_example_copy_slice(out->port, sizeof(out->port), port_start + 1, (size_t)(host_end - port_start - 1))) {
            return false;
        }
    } else {
        if (!easy_api_example_copy_slice(out->host, sizeof(out->host), host_start, (size_t)(host_end - host_start))) {
            return false;
        }
        snprintf(out->port, sizeof(out->port), "%u", 80u);
    }

    if (out->host[0] == '\0' || out->port[0] == '\0') {
        return false;
    }

    if (path_start == NULL || *path_start == '\0') {
        snprintf(out->path, sizeof(out->path), "/");
    } else if (!easy_api_example_copy_slice(out->path, sizeof(out->path), path_start, strlen(path_start))) {
        return false;
    }

    return true;
}

static int easy_api_example_connect_with_timeout(const char *host, const char *port, uint32_t timeout_ms) {
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    struct addrinfo *it;
    int fd = -1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(host, port, &hints, &results);
    if (rc != 0) {
        return -1;
    }

    for (it = results; it != NULL; it = it->ai_next) {
        struct timeval tv;
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) {
            continue;
        }

        tv.tv_sec = (time_t)(timeout_ms / 1000u);
        tv.tv_usec = (suseconds_t)((timeout_ms % 1000u) * 1000u);
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
            break;
        }

        fd = easy_api_example_close_socket(fd);
    }

    freeaddrinfo(results);
    return fd;
}

static bool easy_api_example_send_all(int fd, const char *data, size_t len) {
    size_t sent = 0u;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += (size_t)n;
    }
    return true;
}

static easy_api_status_t easy_api_example_receive_response(int fd, easy_api_http_response_t *response) {
    size_t used = 0u;
    char *header_end;
    char *body_start;
    int status = 0;

    if (response == NULL || response->body == NULL || response->body_capacity == 0u) {
        return EASY_API_ERR_INVALID_ARG;
    }

    response->body[0] = '\0';
    response->body_len = 0u;

    for (;;) {
        ssize_t n;
        if (used + 1u >= response->body_capacity) {
            return EASY_API_ERR_BUFFER_TOO_SMALL;
        }

        n = recv(fd, response->body + used, response->body_capacity - used - 1u, 0);
        if (n == 0) {
            break;
        }
        if (n < 0) {
            return errno == EAGAIN || errno == EWOULDBLOCK ? EASY_API_ERR_TIMEOUT : EASY_API_ERR_NETWORK;
        }
        used += (size_t)n;
        response->body[used] = '\0';
    }

    if (sscanf(response->body, "HTTP/%*u.%*u %d", &status) != 1) {
        return EASY_API_ERR_NETWORK;
    }
    response->http_status = status;

    header_end = strstr(response->body, "\r\n\r\n");
    if (header_end == NULL) {
        return EASY_API_ERR_NETWORK;
    }

    body_start = header_end + 4;
    response->body_len = strlen(body_start);
    memmove(response->body, body_start, response->body_len + 1u);
    return EASY_API_OK;
}

static easy_api_status_t easy_api_example_http_transport(
    const easy_api_http_request_t *request,
    easy_api_http_response_t *response,
    void *user_context) {
    easy_api_example_url_t parsed;
    char tx[EASY_API_EXAMPLE_TX_BUFFER_SIZE];
    size_t pos = 0u;
    int fd;
    int written;
    size_t i;
    (void)user_context;

    if (request == NULL || response == NULL || request->url == NULL || request->method == NULL) {
        return EASY_API_ERR_INVALID_ARG;
    }

    if (!easy_api_example_parse_http_url(request->url, &parsed)) {
        fprintf(stderr, "example transport supports only http:// URLs: %s\n", request->url);
        return EASY_API_ERR_INVALID_ARG;
    }

    written = snprintf(tx, sizeof(tx),
        "%s %s HTTP/1.1\r\n"
        "Host: %s:%s\r\n"
        "Connection: close\r\n"
        "Content-Length: %lu\r\n",
        request->method,
        parsed.path,
        parsed.host,
        parsed.port,
        (unsigned long)request->body_len);

    if (written < 0 || (size_t)written >= sizeof(tx)) {
        return EASY_API_ERR_BUFFER_TOO_SMALL;
    }
    pos = (size_t)written;

    for (i = 0u; i < request->header_count; ++i) {
        written = snprintf(tx + pos, sizeof(tx) - pos, "%s: %s\r\n", request->headers[i].name, request->headers[i].value);
        if (written < 0 || (size_t)written >= sizeof(tx) - pos) {
            return EASY_API_ERR_BUFFER_TOO_SMALL;
        }
        pos += (size_t)written;
    }

    written = snprintf(tx + pos, sizeof(tx) - pos, "\r\n");
    if (written < 0 || (size_t)written >= sizeof(tx) - pos) {
        return EASY_API_ERR_BUFFER_TOO_SMALL;
    }
    pos += (size_t)written;

    if (request->body_len > 0u) {
        if (request->body == NULL || request->body_len >= sizeof(tx) - pos) {
            return EASY_API_ERR_BUFFER_TOO_SMALL;
        }
        memcpy(tx + pos, request->body, request->body_len);
        pos += request->body_len;
    }

    fd = easy_api_example_connect_with_timeout(parsed.host, parsed.port, request->timeout_ms);
    if (fd < 0) {
        return EASY_API_ERR_NETWORK;
    }

    if (!easy_api_example_send_all(fd, tx, pos)) {
        (void)easy_api_example_close_socket(fd);
        return EASY_API_ERR_NETWORK;
    }

    {
        easy_api_status_t st = easy_api_example_receive_response(fd, response);
        (void)easy_api_example_close_socket(fd);
        return st;
    }
}

#endif
