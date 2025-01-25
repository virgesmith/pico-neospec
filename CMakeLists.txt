cmake_minimum_required(VERSION 3.13)


set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

set(PICO_SDK_PATH ../../pico-sdk)

if (${PICO_PLATFORM} STREQUAL "rp2040")
  set(PICO_BOARD pico_w CACHE STRING "Board type")
  set(PICO_TOOLCHAIN_PATH /usr)
elseif (${PICO_PLATFORM} STREQUAL "rp2350-arm-s")
  set(PICO_BOARD pico2_w CACHE STRING "Board type")
  set(PICO_TOOLCHAIN_PATH /usr)
elseif (${PICO_PLATFORM} STREQUAL "rp2350-riscv")
  set(PICO_BOARD pico2_w CACHE STRING "Board type")
  set(PICO_TOOLCHAIN_PATH /opt/riscv-gcc-14)
else()
  message(FATAL_ERROR "Specify PICO_PLATFORM: either rp2040, rp2350-arm-s or rp2350-riscv")
endif()

# ensure we have the correct pico_sdk_import.cmake
execute_process (COMMAND
  bash -c "cp ${PICO_SDK_PATH}/external/pico_sdk_import.cmake .."
)

include(pico_sdk_import.cmake)

project(adc_fft_project)

pico_sdk_init()

add_library(adcfft
  src/kiss_fft.c
  src/kiss_fftr.c
  src/adcfft.cpp
)
target_link_libraries(adcfft
  pico_stdlib
  hardware_adc
  hardware_dma
)

add_library(npx
  src/neopixel.cpp
)
target_link_libraries(npx hardware_pio)

add_executable(npx_test
  src/npx_test.cpp
  src/neopixel.cpp
)
pico_enable_stdio_usb(npx_test 1)
pico_enable_stdio_uart(npx_test 0)

pico_add_extra_outputs(npx_test)

target_link_libraries(npx_test
	pico_stdlib
  npx
)

add_executable(adcfft_test
  src/adcfft_test.cpp
)
pico_enable_stdio_usb(adcfft_test 1)
pico_enable_stdio_uart(adcfft_test 0)

pico_add_extra_outputs(adcfft_test)

target_link_libraries(adcfft_test
	pico_stdlib
  adcfft
)

add_executable(npx_spec
  src/main.cpp
)

pico_enable_stdio_usb(npx_spec 1)
pico_enable_stdio_uart(npx_spec 0)

pico_add_extra_outputs(npx_spec)

target_link_libraries(npx_spec
	npx
  adcfft
)
