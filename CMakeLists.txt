cmake_minimum_required(VERSION 3.16)
project(py32_sdk C ASM)

set(DEVICE py32f003x4)
set(APP all CACHE STRING "Example to build: blink, uart, pwm, pwm_pa2, adc, i2c_pwm, i2c_on_off, i2c_adc, or all")
set(OPENCM3_DIR ${CMAKE_SOURCE_DIR}/libopencm3)
set(GENERATED_LD ${CMAKE_BINARY_DIR}/generated.${DEVICE}.ld)
set(VALID_APPS blink uart pwm pwm_pa2 adc i2c_pwm i2c_on_off i2c_adc)

set(MCU_FLAGS -mcpu=cortex-m0plus -mthumb -msoft-float)
set(DEFS -DPY32F0 -DPY32F003X4)

add_compile_options(${MCU_FLAGS})
add_definitions(${DEFS})
add_compile_options(-Os -ggdb3 -Wall -Wextra -Wundef -Wshadow -fno-common -ffunction-sections -fdata-sections)

include_directories(${OPENCM3_DIR}/include)

if(NOT APP STREQUAL "all")
    list(FIND VALID_APPS ${APP} APP_INDEX)
    if(APP_INDEX EQUAL -1)
        message(FATAL_ERROR "Unknown APP='${APP}'. Valid values: ${VALID_APPS}, all")
    endif()
endif()

add_custom_command(
    OUTPUT ${GENERATED_LD}
    COMMAND ${CMAKE_C_COMPILER} -E ${MCU_FLAGS} ${DEFS}
            -D_ROM=16K -D_RAM=2K -D_ROM_OFF=0x08000000 -D_RAM_OFF=0x20000000
            -P -E ${OPENCM3_DIR}/ld/linker.ld.S -o ${GENERATED_LD}
    DEPENDS ${OPENCM3_DIR}/ld/linker.ld.S
    COMMENT "Generating linker script for ${DEVICE}"
)

add_custom_target(generate_linker ALL DEPENDS ${GENERATED_LD})

set(OUTPUT_DIR ${CMAKE_BINARY_DIR})
set(PYOCD_TARGET py32f003x4 CACHE STRING "pyOCD target name")
set(PYOCD_CONFIG "${CMAKE_BINARY_DIR}/pyocd.yaml" CACHE FILEPATH "pyOCD config file")

# Generate pyocd.yaml in build directory (auto-generated, never committed)
if(NOT EXISTS ${PYOCD_CONFIG})
    file(WRITE ${PYOCD_CONFIG} "pack:\n  - ${CMAKE_SOURCE_DIR}/pyocd-packs/Puya.PY32.pack\n")
    message(STATUS "Created ${PYOCD_CONFIG}")
endif()

file(MAKE_DIRECTORY ${OUTPUT_DIR})

function(add_py32_example EXAMPLE_NAME)
    set(EXAMPLE_SOURCE ${CMAKE_SOURCE_DIR}/examples/${EXAMPLE_NAME}.c)
    if(NOT EXISTS ${EXAMPLE_SOURCE})
        message(FATAL_ERROR "Missing source: ${EXAMPLE_SOURCE}")
    endif()

    message(STATUS "Adding example '${EXAMPLE_NAME}' from ${EXAMPLE_SOURCE}")

    add_executable(${EXAMPLE_NAME}.elf ${EXAMPLE_SOURCE})
    add_dependencies(${EXAMPLE_NAME}.elf generate_linker)

    target_link_libraries(${EXAMPLE_NAME}.elf ${OPENCM3_DIR}/lib/libopencm3_py32f0.a)
    target_link_options(${EXAMPLE_NAME}.elf PRIVATE
        ${MCU_FLAGS}
        -nostartfiles
        --specs=nano.specs
        --specs=nosys.specs
        -Wl,--gc-sections
        -T${GENERATED_LD}
        -Wl,-Map=${CMAKE_BINARY_DIR}/${EXAMPLE_NAME}.map
    )

    add_custom_command(TARGET ${EXAMPLE_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${EXAMPLE_NAME}.elf> ${OUTPUT_DIR}/${EXAMPLE_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -Oihex   $<TARGET_FILE:${EXAMPLE_NAME}.elf> ${OUTPUT_DIR}/${EXAMPLE_NAME}.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${EXAMPLE_NAME}.elf>
        COMMENT "Creating ${EXAMPLE_NAME}.bin / ${EXAMPLE_NAME}.hex in ${OUTPUT_DIR}"
    )

    add_custom_target(flash_${EXAMPLE_NAME}
        COMMAND pyocd flash -t ${PYOCD_TARGET} --config ${PYOCD_CONFIG} ${OUTPUT_DIR}/${EXAMPLE_NAME}.hex
        DEPENDS ${EXAMPLE_NAME}.elf
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Flashing ${EXAMPLE_NAME}.hex with pyOCD"
    )

    add_custom_target(flash_reset_${EXAMPLE_NAME}
        COMMAND pyocd flash -t ${PYOCD_TARGET} --config ${PYOCD_CONFIG} ${OUTPUT_DIR}/${EXAMPLE_NAME}.hex
        COMMAND pyocd reset -t ${PYOCD_TARGET} --config ${PYOCD_CONFIG}
        DEPENDS ${EXAMPLE_NAME}.elf
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Flashing ${EXAMPLE_NAME}.hex and resetting target"
    )
endfunction()

if(APP STREQUAL "all")
    foreach(EXAMPLE_NAME IN LISTS VALID_APPS)
        add_py32_example(${EXAMPLE_NAME})
    endforeach()
else()
    add_py32_example(${APP})
    add_custom_target(flash DEPENDS flash_${APP})
    add_custom_target(flash-reset DEPENDS flash_reset_${APP})
endif()
