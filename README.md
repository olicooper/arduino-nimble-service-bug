# Problem

If a service is added after NimBLE initialization (press the button ~5+ seconds after boot), I get one of the following problems:

* If the client is already connected I get an error `E NimBLEService: "ble_gatts_add_svcs, rc= 15, "`
    - If I manually disconnect the Android client after pressing the button, sometimes NimBLE will initialize the new service and it works.
      Question: I thought NimBLE was supposed to handle cient refresh and advertisment restarts automatically?
* If no client has connected since NimBLE was initialized, I get empty services with no characteristics.

If the service is started along with the other services during startup, it works fine (press the button while the device is booting).

# Environment

- Android with nRF Connect
- Lolin 32 Lite
- PlatformIO
- Windows 10
- Arduino 1.0.6