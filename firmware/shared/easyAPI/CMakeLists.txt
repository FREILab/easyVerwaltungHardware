cmake_minimum_required(VERSION 3.13)

project(easyAPI C)

option(EASY_API_BUILD_TESTS "Build easyAPI host unit tests" OFF)
option(EASY_API_ENABLE_COVERAGE "Enable GCC/Clang coverage flags for easyAPI tests" OFF)
option(EASY_API_BUILD_EXAMPLES "Build easyAPI host examples" ON)

add_library(easyAPI STATIC
    src/easy_api.c
    src/easy_api_service.c
)

add_library(easyAPI::easyAPI ALIAS easyAPI)

target_include_directories(easyAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(easyAPI PUBLIC c_std_99)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(easyAPI PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(EASY_API_ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(easyAPI PRIVATE -O0 -g --coverage)
    target_link_options(easyAPI PUBLIC --coverage)
endif()

if(EASY_API_BUILD_TESTS)
    enable_testing()
    add_executable(easyAPI_tests
        tests/test_easy_api.c
        tests/test_easy_api_service.c
    )
    target_link_libraries(easyAPI_tests PRIVATE easyAPI)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(easyAPI_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(EASY_API_ENABLE_COVERAGE)
            target_compile_options(easyAPI_tests PRIVATE -O0 -g --coverage)
            target_link_options(easyAPI_tests PRIVATE --coverage)
        endif()
    endif()
    add_test(NAME easyAPI_tests COMMAND easyAPI_tests)
endif()

if(EASY_API_BUILD_EXAMPLES)
    foreach(example_name
        basic_health_check
        login_single_mode
        login_heartbeat_mode
        full_machine_flow
        service_layer_real_backend
    )
        add_executable(easyAPI_example_${example_name}
            examples/${example_name}/main.c
        )
        target_link_libraries(easyAPI_example_${example_name} PRIVATE easyAPI)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(easyAPI_example_${example_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
            target_compile_definitions(easyAPI_example_${example_name} PRIVATE _POSIX_C_SOURCE=200112L)
        endif()
    endforeach()
endif()
