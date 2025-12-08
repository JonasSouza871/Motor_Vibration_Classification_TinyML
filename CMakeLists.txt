# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# Configuração básica do projeto
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)
project(Motor_Classification_TinyML C CXX ASM)
pico_sdk_init()

# Criação do executável com arquivos organizados
add_executable(${PROJECT_NAME}
    firmware/src/main.c
    firmware/src/mpu6050.c
    firmware/src/ssd1306.c
)

# Diretórios de inclusão
target_include_directories(${PROJECT_NAME} PRIVATE
    firmware/libs
)

# Bibliotecas necessárias
target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_stdlib
    hardware_i2c
)

# Configuração de saída
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_add_extra_outputs(${PROJECT_NAME})
