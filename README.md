# Easy-to-extend BLE - Wi-Fi Bridge

A fully-functional BLE - Wi-Fi Bridge. Control/Access the BLE accessories in your home remotely using phone apps. If you have any BLE devices you own, simply add their BLE protocol, and any ESP32 DevKit will start acting as a BLE - Wi-Fi Bridge.

## Requirements

* Hardware
  * ESP32
* Software
  * ESP-RainMaker: Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section
  * Download the ESP RainMaker [iOS](https://apps.apple.com/app/esp-rainmaker/id1497491540)/[Android](https://play.google.com/store/apps/details?id=com.espressif.rainmaker) phone app

## Build and Flash

```bash
$ git clone https://github.com/dhrishi/rainmaker-ble-wifi-bridge
$ cd rainmaker-ble-wifi-bridge
$ RAINMAKER_PATH=/path/to/where/esp-rainmaker/exists idf.py build flash monitor
```

## Functionality

- This is a BLE - Wi-Fi bridge that facilitates access to registered BLE accessories remotely using phone apps.
- BLE devices in the vicinity are discovered by the bridge and are represented as RainMaker devices which can be seen and controlled through the iOS/Android app.
- When a parameter update is received to ESP32 over Wi-Fi (from the iOS/Android app), it is mapped to the format accepted by the corresponding device and a characteristic write over BLE for is executed. This results into the actual action on the BLE device.

### Supported BLE Accessories

Currently supported accessories:
* Syska Light Bulb
* PLAYBULB CANDLE

<img src="https://raw.githubusercontent.com/wiki/dhrishi/rainmaker-ble-wifi-bridge/images/BLE_Wi-Fi_Bridge_App.jpeg" width="250"/>

### Adding New Accessory

- Copy files `main/accessories/sample_accessory.[ch]` and rename as per your accessory name. You may consider replacing the occurrences of `sample_accessory` in the code with your `accessory-name_type`.
- Update the functions in the C source file as per the documentation and guidelines.
- Include the C header file `accessories/accessory-name_type.h` in `main/app_main.c` and add a call to the register function `accessory-name_type_register()` before `app_ble_start()`
- Add the entry of the source file `acessory-name_type.c` in `main/CMakeLists.txt`

Note:
1. Files `main/accessories/sample_accessory.[ch]` are only for reference and are not compiled.
2. The total number of registered accessories you want to use at a time should not exceed [maximum allowed devices] (https://github.com/dhrishi/rainmaker-ble-wifi-bridge/blob/master/main/app_ble.h#L12). If required, you can change the default value using menuconfig `Component config -> Bluetooth -> Bluetooth controller -> BLE Max Connections`.

### Limitations

- The added BLE accessory should be discoverable before starting the RainMaker framework.
- For now, the BLE service and characteristic UUIDs of the parameters to be controlled should be 16 bit.
- Getting the parameter values (BLE read) from the accessory is not yet supported

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.

