# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ENV_PROFILE "machine_dev_usb" CACHE STRING "Firmware environment profile")
set(RFIDBOX_ENV_PROFILES
    machine_dev_usb
    machine_dev_ota
    rfidbox_usb
    rfidbox_ota
    printer3d_01_usb
    printer3d_01_ota
    printer3d_02_usb
    printer3d_02_ota
    printer3d_03_usb
    printer3d_03_ota
    lathe_01_usb
    lathe_01_ota
    lathe_02_usb
    lathe_02_ota
    metal_mill_01_usb
    metal_mill_01_ota
    metal_mill_02_usb
    metal_mill_02_ota
    embroiderymachine_usb
    embroiderymachine_ota
    cncmill_wood_usb
    cncmill_wood_ota
    cncmill_metal_usb
    cncmill_metal_ota
    printer3d_xl_usb
    printer3d_xl_ota
    panel_saw_usb
    panel_saw_ota
    wood_planer_usb
    wood_planer_ota
    wood_bandsaw_usb
    wood_bandsaw_ota
    miter_saw_usb
    miter_saw_ota
    wood_routertable_usb
    wood_routertable_ota
)
set_property(CACHE ENV_PROFILE PROPERTY STRINGS ${RFIDBOX_ENV_PROFILES})

set(WIFI_SSID "NotSet" CACHE STRING "WiFi SSID")
set(WIFI_PASSWORD "NotSet" CACHE STRING "WiFi password")
set(SERVER_HOST "localhost" CACHE STRING "Backend host")
set(AUTHENTICATION_TOKEN "NoToken" CACHE STRING "Backend auth token")
set(MACHINE_NAME "machine-node" CACHE STRING "Machine type")
set(MACHINE_ID "machine-node-01" CACHE STRING "Machine identifier")
set(OTA_HOSTNAME "machine-node-01" CACHE STRING "OTA host name")
set(OTA_ENABLED "0" CACHE STRING "Enable OTA hooks (0/1)")

if(NOT ENV_PROFILE IN_LIST RFIDBOX_ENV_PROFILES)
    message(FATAL_ERROR "Unknown ENV_PROFILE='${ENV_PROFILE}'")
endif()

if(ENV_PROFILE MATCHES "_ota$")
    set(OTA_ENABLED "1")
endif()

if(ENV_PROFILE MATCHES "^machine_dev_")
    # Keep machine-node defaults for local development profiles.
elseif(ENV_PROFILE MATCHES "^rfidbox_")
    set(MACHINE_NAME "lasercutterXTool")
    set(MACHINE_ID "rfidbox-01")
    set(OTA_HOSTNAME "rfidbox-01")
elseif(ENV_PROFILE MATCHES "^printer3d_01_")
    set(MACHINE_NAME "3dprinter")
    set(MACHINE_ID "3dprinter-01")
    set(OTA_HOSTNAME "3dprinter-01")
elseif(ENV_PROFILE MATCHES "^printer3d_02_")
    set(MACHINE_NAME "3dprinter")
    set(MACHINE_ID "3dprinter-02")
    set(OTA_HOSTNAME "3dprinter-02")
elseif(ENV_PROFILE MATCHES "^printer3d_03_")
    set(MACHINE_NAME "3dprinter")
    set(MACHINE_ID "3dprinter-03")
    set(OTA_HOSTNAME "3dprinter-03")
elseif(ENV_PROFILE MATCHES "^lathe_01_")
    set(MACHINE_NAME "lathe")
    set(MACHINE_ID "lathe-01")
    set(OTA_HOSTNAME "lathe-01")
elseif(ENV_PROFILE MATCHES "^lathe_02_")
    set(MACHINE_NAME "lathe")
    set(MACHINE_ID "lathe-02")
    set(OTA_HOSTNAME "lathe-02")
elseif(ENV_PROFILE MATCHES "^metal_mill_01_")
    set(MACHINE_NAME "metal-mill")
    set(MACHINE_ID "metal-mill-01")
    set(OTA_HOSTNAME "metal-mill-01")
elseif(ENV_PROFILE MATCHES "^metal_mill_02_")
    set(MACHINE_NAME "metal-mill")
    set(MACHINE_ID "metal-mill-02")
    set(OTA_HOSTNAME "metal-mill-02")
elseif(ENV_PROFILE MATCHES "^embroiderymachine_")
    set(MACHINE_NAME "embroiderymachine")
    set(MACHINE_ID "embroiderymachine-01")
    set(OTA_HOSTNAME "embroiderymachine-01")
elseif(ENV_PROFILE MATCHES "^cncmill_wood_")
    set(MACHINE_NAME "cncmill-wood")
    set(MACHINE_ID "cncmill-wood-01")
    set(OTA_HOSTNAME "cncmill-wood-01")
elseif(ENV_PROFILE MATCHES "^cncmill_metal_")
    set(MACHINE_NAME "cncmill-metal")
    set(MACHINE_ID "cncmill-metal-01")
    set(OTA_HOSTNAME "cncmill-metal-01")
elseif(ENV_PROFILE MATCHES "^printer3d_xl_")
    set(MACHINE_NAME "3dprinter-xl")
    set(MACHINE_ID "3dprinter-xl-01")
    set(OTA_HOSTNAME "3dprinter-xl-01")
elseif(ENV_PROFILE MATCHES "^panel_saw_")
    set(MACHINE_NAME "panel-saw")
    set(MACHINE_ID "panel-saw-01")
    set(OTA_HOSTNAME "panel-saw-01")
elseif(ENV_PROFILE MATCHES "^wood_planer_")
    set(MACHINE_NAME "wood-planer")
    set(MACHINE_ID "wood-planer-01")
    set(OTA_HOSTNAME "wood-planer-01")
elseif(ENV_PROFILE MATCHES "^wood_bandsaw_")
    set(MACHINE_NAME "wood-bandsaw")
    set(MACHINE_ID "wood-bandsaw-01")
    set(OTA_HOSTNAME "wood-bandsaw-01")
elseif(ENV_PROFILE MATCHES "^miter_saw_")
    set(MACHINE_NAME "miter-saw")
    set(MACHINE_ID "miter-saw-01")
    set(OTA_HOSTNAME "miter-saw-01")
elseif(ENV_PROFILE MATCHES "^wood_routertable_")
    set(MACHINE_NAME "wood-routertable")
    set(MACHINE_ID "wood-routertable-01")
    set(OTA_HOSTNAME "wood-routertable-01")
endif()

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/env.local.cmake)
    include(${CMAKE_CURRENT_LIST_DIR}/env.local.cmake)
endif()

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type" FORCE)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(MachineNode_legacy C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(MachineNode_legacy MachineNode_legacy.cpp )

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/firmware_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/firmware_config.h
    @ONLY
)

pico_set_program_name(MachineNode_legacy "MachineNode_legacy")
pico_set_program_version(MachineNode_legacy "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(MachineNode_legacy 0)
pico_enable_stdio_usb(MachineNode_legacy 1)

# Add the standard library to the build
target_link_libraries(MachineNode_legacy
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(MachineNode_legacy PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Add any user requested libraries
target_link_libraries(MachineNode_legacy 
    pico_cyw43_arch_lwip_threadsafe_background
        )

pico_add_extra_outputs(MachineNode_legacy)

