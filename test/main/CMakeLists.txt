idf_component_register(
    SRCS
        "main.cpp"
        "../../esp32/main/drivers/led.cpp"
        "../../esp32/main/drivers/button.cpp"
        "../../esp32/main/drivers/stepper.cpp"
        "../../esp32/main/drivers/uart_link.cpp"
    INCLUDE_DIRS "." "../../esp32/main/drivers"
    REQUIRES esp_driver_gpio esp_driver_uart
)
