# BLE - Wi-Fi Bridge Example

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

## What to expect in this example?
- This example showcases a BLE - Wi-Fi bridge to facilitate access to registered BLE devices over the cloud.
- These BLE devices are discovered by the bridge and are represented as RainMaker devices which can be seen and controlled through the iOS/Android app.
- When a parameter update is received to ESP32 over Wi-Fi from the iOS/Android app, it is mapped to the format accepted by the corresponding device and a characteristic write over BLE for is executed. This results into the actual action on the BLE device.
- For demonstration, bridging of a Syska Light and a PLAYBULB CANDLE is implemented in this example.

<img src="https://raw.githubusercontent.com/wiki/dhrishi/rainmaker-ble-wifi-bridge/images/BLE_Wi-Fi_Bridge_App.jpeg" width="250"/>


### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.

