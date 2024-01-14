set(OUTPUT_NAME vl53l5cx_demo)

add_executable(
  ${OUTPUT_NAME}
  vl53l5cx_demo.cpp
  SEFR.cpp
)

# Pull in pico libraries that we need
target_link_libraries(${OUTPUT_NAME} pico_stdlib hardware_i2c vl53l5cx pimoroni_i2c hardware_spi hardware_pwm hardware_dma rgbled button pico_display pico_graphics st7789)

# create map/bin/hex file etc.
pico_add_extra_outputs(${OUTPUT_NAME})
