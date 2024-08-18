idf_component_register(

SRCS 
    "main.cpp"
    "Src/RtosUtils.cpp"
    "Src/Leds.cpp"
    "Src/Matrix.cpp"
    "Src/UsbHid.cpp"
    "Src/Controller.cpp"
    "Src/Bluetooth/ble_hidd_demo_main.cpp"
    "Src/Bluetooth/esp_hidd_prf_api.cpp"
    "Src/Bluetooth/hid_dev.cpp"
    "Src/Bluetooth/hid_device_le_prf.cpp"

                    INCLUDE_DIRS
    "Inc"
    "Inc/Bluetooth"
                     )
