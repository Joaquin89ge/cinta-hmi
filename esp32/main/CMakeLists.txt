idf_component_register(
    SRCS
        "app_main.cpp"
        "drivers/led.cpp"
        "drivers/button.cpp"
        "drivers/stepper.cpp"
        "drivers/uart_link.cpp"
        "domain/belt.cpp"
        "domain/protocol.cpp"
    INCLUDE_DIRS "." "drivers" "domain"
    REQUIRES esp_driver_gpio esp_driver_uart
)
