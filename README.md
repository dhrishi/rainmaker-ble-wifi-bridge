# BLE - Wi-Fi Bridge Example

## Requirements
* Hardware
  * ESP32
* Software
  * ESP-RainMaker: Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section

## Build and Flash
```bash
$ git clone https://github.com/dhrishi/rainmaker-ble-wifi-bridge
$ cd rainmaker-ble-wifi-bridge
$ RAINMAKER_PATH=/path/to/where/esp-rainmaker/exists idf.py build flash monitor
```

## What to expect in this example?
- This example showcases a BLE - Wi-Fi bridge to facilitate access to registered BLE devices over the cloud.
- These BLE devices are discovered by the bridge and are represented as RainMaker devices.
- When a parameter update is received over Wi-Fi, it is mapped to the format accepted by the corresponding device and a characteristic write over BLE for is executed. This results into the actual action on the BLE device.
- For demonstration, bridging of a Syska Light and a PLAYBULB CANDLE is implemented in this example.

Note: This example will not work with ESP32-S2 as it does not support Bluetooth. You need to use ESP32 to run this example

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.

