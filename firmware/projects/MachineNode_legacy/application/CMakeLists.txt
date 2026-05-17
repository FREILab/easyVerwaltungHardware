# Geräte-Identität — muss via CMakePresets.json oder -D gesetzt sein
if(NOT MACHINE_ID)
    message(FATAL_ERROR "MACHINE_ID nicht gesetzt. Bitte cmake --preset <name> verwenden.")
endif()
if(NOT MACHINE_NAME)
    message(FATAL_ERROR "MACHINE_NAME nicht gesetzt.")
endif()
if(NOT OTA_HOSTNAME)
    set(OTA_HOSTNAME ${MACHINE_ID})
endif()

add_executable(machine_node_app
    main.c
    wifi.c
    ota.c
)

target_include_directories(machine_node_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/shared
)

target_compile_definitions(machine_node_app PRIVATE
    MACHINE_ID="${MACHINE_ID}"
    MACHINE_NAME="${MACHINE_NAME}"
    OTA_HOSTNAME="${OTA_HOSTNAME}"
    # AUTH_PERMANENT=1: Gerät bleibt nach einmaligem Scan freigeschaltet
    # AUTH_PERMANENT=0: Jeder Scan wird gegen den Server geprüft
    AUTH_PERMANENT=$<IF:$<BOOL:${AUTH_PERMANENT}>,1,0>
)

target_link_libraries(machine_node_app
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_http
    hardware_flash
    hardware_sync
)

pico_enable_stdio_usb(machine_node_app 1)
pico_enable_stdio_uart(machine_node_app 0)

pico_set_linker_script(machine_node_app ${CMAKE_CURRENT_SOURCE_DIR}/memmap_app_a.ld)

pico_add_extra_outputs(machine_node_app)
