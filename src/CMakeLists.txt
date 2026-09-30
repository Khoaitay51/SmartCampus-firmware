FILE(GLOB app_sources ${CMAKE_CURRENT_LIST_DIR}/*.c)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../include"
    REQUIRES 
        nvs_flash 
        esp_wifi 
        esp_event 
        esp_netif 
        esp_timer
        esp_driver_gpio 
        esp_driver_ledc 
        esp_driver_i2c 
        esp_driver_spi 
        esp_adc
        lwip
)
