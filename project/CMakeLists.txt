cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)
include($ENV{PICO_SDK_PATH}/tools/CMakeLists.txt)
project(caenet_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(caenet
caenet.c
)
pico_generate_pio_header(caenet ${CMAKE_CURRENT_LIST_DIR}/send.pio)
pico_enable_stdio_usb(caenet 1)
pico_add_extra_outputs(caenet)
target_link_libraries(caenet PRIVATE
 pico_stdlib 
 hardware_pio
  hardware_dma 
  hardware_gpio
  )
