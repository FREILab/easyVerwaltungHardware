add_executable(machine_node_bootloader
    main.c
)

target_include_directories(machine_node_bootloader PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(machine_node_bootloader
    pico_stdlib
    hardware_flash
    hardware_sync
)

# Bootloader läuft ohne USB/UART-Overhead — minimale Startzeit
pico_enable_stdio_usb(machine_node_bootloader 0)
pico_enable_stdio_uart(machine_node_bootloader 0)

# Bootloader muss im ersten Flash-Bereich liegen (ab 0x10000000, max 256 KB)
pico_set_linker_script(machine_node_bootloader ${CMAKE_CURRENT_SOURCE_DIR}/memmap_bootloader.ld)

# UF2 + BIN für Erstflash
pico_add_extra_outputs(machine_node_bootloader)
