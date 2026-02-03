cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nrf)

target_sources(app PRIVATE
    src/main.c
    src/temp_sensor.c
    src/display.c
    src/relay.c
)
