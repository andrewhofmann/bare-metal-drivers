cmake_minimum_required(VERSION 3.22)

# -----------------------------------------------------------------------------
# ARM Cortex-M cross-compilation toolchain (must be set before project())
# -----------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

# Skip compiler tests — they fail without a linker script
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER_WORKS 1)

# -----------------------------------------------------------------------------
# Project definition
# -----------------------------------------------------------------------------
project(bare-metal-drivers
    VERSION 0.1.0
    LANGUAGES C ASM
    DESCRIPTION "Register-level peripheral drivers for STM32"
)

# Target MCU configuration — default to STM32F103C8 (Blue Pill)
set(MCU_FAMILY "STM32F1" CACHE STRING "MCU family (e.g. STM32F1, STM32F4)")
set(MCU_MODEL  "STM32F103C8" CACHE STRING "Specific MCU model")
set(MCU_CORTEX "cortex-m3" CACHE STRING "ARM Cortex-M core variant")

# Flash and RAM sizes for the linker script (STM32F103C8 defaults)
set(FLASH_SIZE "64K" CACHE STRING "MCU flash size")
set(RAM_SIZE   "20K" CACHE STRING "MCU SRAM size")
set(FLASH_ORIGIN "0x08000000" CACHE STRING "Flash start address")
set(RAM_ORIGIN   "0x20000000" CACHE STRING "SRAM start address")

# Linker script
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/${MCU_MODEL}.ld)

# -----------------------------------------------------------------------------
# Common compiler / linker flags
# -----------------------------------------------------------------------------
set(COMMON_FLAGS
    -mcpu=${MCU_CORTEX}
    -mthumb
    -mfloat-abi=soft
    -ffunction-sections
    -fdata-sections
    -fno-common
    -fno-exceptions
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
    -Wformat-truncation
    -Wundef
    -fno-strict-aliasing
    -std=gnu11
)

set(DEBUG_FLAGS   -Og -g3 -gdwarf-2 -DDEBUG)
set(RELEASE_FLAGS -Os -DNDEBUG)

add_compile_options(${COMMON_FLAGS})
add_compile_options("$<$<CONFIG:Debug>:${DEBUG_FLAGS}>")
add_compile_options("$<$<CONFIG:Release>:${RELEASE_FLAGS}>")

# MCU-family define available to all source files
add_compile_definitions(${MCU_MODEL})

# -----------------------------------------------------------------------------
# Include paths
# -----------------------------------------------------------------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/cmsis
)

# -----------------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------------
file(GLOB_RECURSE DRIVER_SOURCES src/drivers/*.c)
file(GLOB_RECURSE APP_SOURCES    src/app/*.c)

set(STARTUP_SOURCE ${CMAKE_SOURCE_DIR}/startup/startup_${MCU_FAMILY}.c)

# -----------------------------------------------------------------------------
# Firmware ELF target
# -----------------------------------------------------------------------------
set(TARGET_NAME firmware)

add_executable(${TARGET_NAME}
    src/main.c
    ${STARTUP_SOURCE}
    ${DRIVER_SOURCES}
    ${APP_SOURCES}
)

target_link_options(${TARGET_NAME} PRIVATE
    -mcpu=${MCU_CORTEX}
    -mthumb
    -mfloat-abi=soft
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${TARGET_NAME}.map
    -Wl,--print-memory-usage
    --specs=nosys.specs
    --specs=nano.specs
    -nostartfiles
)

set_target_properties(${TARGET_NAME} PROPERTIES
    SUFFIX ".elf"
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# -----------------------------------------------------------------------------
# Post-build: generate .bin and .hex, print size
# -----------------------------------------------------------------------------
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.hex
    COMMAND ${CMAKE_SIZE}    $<TARGET_FILE:${TARGET_NAME}>
    COMMENT "Generating ${TARGET_NAME}.bin and ${TARGET_NAME}.hex"
)

# -----------------------------------------------------------------------------
# Flash target (OpenOCD)
# -----------------------------------------------------------------------------
add_custom_target(flash
    COMMAND openocd
        -f interface/stlink.cfg
        -f target/stm32f1x.cfg
        -c "program ${TARGET_NAME}.elf verify reset exit"
    DEPENDS ${TARGET_NAME}
    COMMENT "Flashing ${TARGET_NAME}.elf via ST-Link"
)
