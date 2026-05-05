# easyAPI

`shared/easyAPI` is a small embedded C library for backend communication from firmware projects. It is intended for Pico SDK-style CMake builds and remains transport-agnostic so the core request construction, JSON parsing, validation, and error mapping can be tested on a host without physical hardware.

The current API targets the easyVerwaltung machine-service contract:

- `POST /api/service/health?serviceId=...`
- `POST /api/service/machine/login`
- `POST /api/service/machine/heartbeat`

The library does not own Wi-Fi, TLS, sockets, Pico SDK initialization, RFID reading, relays, UI, logging, or local session scheduling. Those are application responsibilities.

## Public API

Public declarations live in `include/easy_api.h`.

Main types:

- `easy_api_client_t`: configured client instance.
- `easy_api_config_t`: explicit configuration for base URL, service ID, service token, timeouts, buffers, and transport callback.
- `easy_api_transport_fn`: platform callback used to execute HTTP requests.
- `easy_api_status_t`: enum-based status model.
- `easy_api_health_response_t`, `easy_api_login_response_t`, `easy_api_heartbeat_response_t`: parsed response objects.

Main functions:

- `easy_api_init(...)`
- `easy_api_health_check(...)`
- `easy_api_machine_login(...)`
- `easy_api_machine_heartbeat(...)`
- `easy_api_status_string(...)`
- `easy_api_mode_string(...)`

All public functions validate arguments. Network calls are blocking and must be bounded by `easy_api_config_t.timeout_ms`; the transport implementation is responsible for enforcing that timeout.

## Configuration

Example configuration:

```c
static char request_buffer[512];
static char response_buffer[1024];

static easy_api_status_t pico_transport(
    const easy_api_http_request_t *request,
    easy_api_http_response_t *response,
    void *user_context);

static easy_api_client_t client;

void backend_init(void) {
    easy_api_config_t config = {
        .base_url = "https://verwaltung.example.org",
        .service_id = "svc_laser_cutter_01",
        .service_token = service_token_from_secret_store,
        .timeout_ms = 5000u,
        .request_buffer = request_buffer,
        .request_buffer_size = sizeof(request_buffer),
        .response_buffer = response_buffer,
        .response_buffer_size = sizeof(response_buffer),
        .transport = pico_transport,
        .transport_context = NULL,
    };

    (void)easy_api_init(&client, &config);
}
```

Keep `service_token` secret. Do not log it. Do not log raw RFID values. Do not place tokens in URLs.

## Transport boundary

The transport callback receives a complete HTTP request description:

- method, currently `POST`
- URL
- headers, including `X-Service-Token`, `Content-Type`, and `Accept`
- JSON body and body length
- timeout in milliseconds

The callback fills:

- `response->http_status`
- `response->body`
- `response->body_len`

Return `EASY_API_ERR_TIMEOUT` for timeout, `EASY_API_ERR_NETWORK` for network/TLS failures, and `EASY_API_ERR_BUFFER_TOO_SMALL` if the response buffer cannot hold the body.

This keeps Pico SDK, lwIP, TLS, Arduino HTTP clients, or any other network implementation outside the reusable core.

## Pico SDK integration

From a Pico SDK CMake project, add the module and link it:

```cmake
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../shared/easyAPI easyAPI_build)

target_link_libraries(my_firmware_target
    PRIVATE
        easyAPI::easyAPI
        pico_stdlib
        pico_cyw43_arch_lwip_threadsafe_background
)
```

Only the application transport should link Pico networking libraries. `easyAPI` itself links no Pico SDK libraries.

## Examples

Example programs live under `examples/` and use mocked transports, so they can run on a host without Pico hardware. They demonstrate API usage only. Real firmware still owns Wi-Fi, TLS, RFID, relays, display, and scheduling. Shared demo values are defined in `examples/easy_api_example_config.h`:

```c
#define EASY_API_EXAMPLE_SERVICE_TOKEN "demo-service-token-aa00"
#define EASY_API_EXAMPLE_SERVICE_ID    "svc_inst_001"
#define EASY_API_EXAMPLE_RFID          "CARD-1001"
#define EASY_API_EXAMPLE_PIN           "1234"
```

The demo token, RFID, and PIN are intentionally hard-coded only in examples. Production firmware must load them from protected runtime configuration and must not log raw token or RFID values.

| Example | Purpose |
| --- | --- |
| `basic_health_check` | Startup presence check against `/api/service/health`. |
| `login_single_mode` | One-shot authorization flow with optional PIN. |
| `login_heartbeat_mode` | Login plus repeated heartbeat calls using backend-owned session data. |
| `full_machine_flow` | Minimal fail-closed state-machine example for an embedded machine controller. |
| `service_layer_mocked_flow` | Non-blocking service-layer login flow with a mocked executor. |

Build examples on a host:

```bash
cd firmware/shared/easyAPI
cmake -S . -B build-examples -DEASY_API_BUILD_EXAMPLES=ON
cmake --build build-examples
./build-examples/easyAPI_example_basic_health_check
./build-examples/easyAPI_example_login_single_mode
./build-examples/easyAPI_example_login_heartbeat_mode
./build-examples/easyAPI_example_full_machine_flow
./build-examples/easyAPI_example_service_layer_mocked_flow
```

Typical heartbeat login usage:

```c
easy_api_login_response_t login;
easy_api_status_t status = easy_api_machine_login(
    &client,
    EASY_API_EXAMPLE_RFID,
    EASY_API_MODE_HEARTBEAT,
    EASY_API_EXAMPLE_PIN,
    &login);

if (status == EASY_API_OK && login.authorized) {
    // Store login.session_id only in volatile local session state.
    // Schedule the next heartbeat using login.heartbeat_interval_seconds.
} else {
    // Fail closed. Do not start local machine access.
}
```

Heartbeat:

```c
easy_api_heartbeat_response_t heartbeat;
easy_api_status_t status = easy_api_machine_heartbeat(
    &client,
    session_id,
    current_rfid,
    &heartbeat);

if (status != EASY_API_OK) {
    // End local access and clear volatile session state.
}
```

## Build

Host build of the module:

```bash
cd firmware/shared/easyAPI
cmake -S . -B build
cmake --build build
```

## Tests

The tests are host-based and use a mocked transport. They do not require Pico hardware.

```bash
cd firmware/shared/easyAPI
cmake -S . -B build-tests -DEASY_API_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Covered scenarios include:

- health check success and inactive service
- authorized single login
- authorized heartbeat login
- denied login
- heartbeat accepted and rejected
- invalid arguments
- timeout and network errors
- HTTP error envelope parsing
- malformed or incomplete JSON
- request-buffer-too-small handling
- JSON string escaping

## Coverage

With GCC or Clang and `gcov`/`lcov` installed:

```bash
cd firmware/shared/easyAPI
cmake -S . -B build-coverage -DEASY_API_BUILD_TESTS=ON -DEASY_API_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure
lcov --capture --directory build-coverage --output-file build-coverage/coverage.info
lcov --remove build-coverage/coverage.info '*/tests/*' --output-file build-coverage/coverage.filtered.info
genhtml build-coverage/coverage.filtered.info --output-directory build-coverage/html
```

A simpler line-coverage check with `gcovr`:

```bash
gcovr -r . build-coverage --filter 'src/easy_api.c' --html --html-details -o build-coverage/coverage.html
```

The included tests are designed to exceed 90% line coverage for `src/easy_api.c` when run with standard GCC/Clang coverage tooling.

## Error model

`easy_api_status_t` contains:

- `EASY_API_OK`
- `EASY_API_ERR_INVALID_ARG`
- `EASY_API_ERR_TIMEOUT`
- `EASY_API_ERR_NETWORK`
- `EASY_API_ERR_HTTP_STATUS`
- `EASY_API_ERR_JSON_PARSE`
- `EASY_API_ERR_BUFFER_TOO_SMALL`
- `EASY_API_ERR_BACKEND_DENIED`
- `EASY_API_ERR_SERVICE_INACTIVE`

For non-2xx backend responses, the parsed backend error code, correlation ID, and HTTP status are stored in the response error object when present.

## Assumptions and limitations

- `easyAPI` implements the machine-service contract documented for easyVerwaltung service endpoints.
- The transport callback enforces the configured timeout.
- The library intentionally uses a small bounded parser for the known backend response fields instead of adding a JSON dependency to the reusable core.
- The parser is not a general-purpose JSON parser.
- URL encoding for `serviceId` in the health query is not implemented; use service IDs that are already URL-safe.
- TLS certificate validation, DNS, Wi-Fi state, retry/backoff policy, token rotation, and local fail-closed hardware control belong to the firmware application layer.
- Heartbeat scheduling and local session state are intentionally not hidden inside the library.

## Non-blocking Service Layer

`easy_api_service` is an optional job/event layer above the blocking easyAPI core. It is intended for firmware that has UI, RFID readers, displays, relays, watchdogs, or other work that must continue while backend communication is in progress.

The service layer itself does not open sockets or perform HTTP. Instead, it owns:

- queued job state
- bounded request copies
- deadline and timeout handling
- cancellation
- event mapping
- fail-closed result propagation

The application provides a non-blocking executor through three callbacks:

```c
easy_api_service_executor_start_fn executor_start;
easy_api_service_executor_poll_fn  executor_poll;
easy_api_service_executor_cancel_fn executor_cancel;
```

Typical executor implementations:

- RTOS worker task wrapping the blocking `easy_api_*` calls
- cooperative non-blocking HTTP transport
- host-side mock executor for tests and simulation

Do not call the blocking easyAPI core directly from UI code or from interrupt handlers. Interrupt handlers should only enqueue local events, set flags, or wake a task.

### Service Layer State Model

```text
IDLE -> QUEUED -> RUNNING -> IDLE
              \-> CANCELLING -> IDLE
```

`easy_api_service_poll()` is designed to be called from the main loop:

```c
while (1) {
    ui_update();
    rfid_update();
    machine_update();
    easy_api_service_poll(&api_service);
}
```

When a job completes, times out, fails, or is cancelled, the configured event callback receives one of:

- `EASY_API_SERVICE_EVENT_HEALTH_OK`
- `EASY_API_SERVICE_EVENT_HEALTH_FAILED`
- `EASY_API_SERVICE_EVENT_LOGIN_GRANTED`
- `EASY_API_SERVICE_EVENT_LOGIN_DENIED`
- `EASY_API_SERVICE_EVENT_HEARTBEAT_OK`
- `EASY_API_SERVICE_EVENT_HEARTBEAT_REJECTED`
- `EASY_API_SERVICE_EVENT_TIMEOUT`
- `EASY_API_SERVICE_EVENT_CANCELLED`
- `EASY_API_SERVICE_EVENT_ERROR`

### Service Layer Example

See:

```text
examples/service_layer_mocked_flow/main.c
```

This example demonstrates a non-blocking login flow with a mocked executor. In a Pico/RTOS application, replace the mock executor with a worker task or cooperative transport implementation.

### Important Limitation

The service layer can only be non-blocking if the configured executor is non-blocking. A blocking executor will still block during `executor_start()` or `executor_poll()`.
