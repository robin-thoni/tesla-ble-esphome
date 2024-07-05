## Status

This is work in progress and did *NOT* work the last time I tried

### Usage

Clone the repo as `tesla_ble` in `esphome/custom_components`

Add this to you device config:
```yaml
external_components:
  - source:
      type: local
      path: custom_components

# Set something like this to interract w/ the car
# First, use "Connect to car" to connect the BLE client
# If this is the first time, tap your card on the reader in the car to confirm the new key
# If it's already paired, no further action are required
# Then you can lock/unlock the car w/ the buttons below
button:
  - platform: template
    name: Connect to car
    on_press: 
      then:
        - esp32_ble_tracker.stop_scan:
        - ble_client.connect: ble_tesla_id
  - platform: template
    name: Disconnect from car
    on_press: 
      then:
        - ble_client.disconnect: ble_tesla_id
  - platform: template
    name: Lock the car
    on_press: 
      then:
        - lambda: |
            id(tesla_ble_id).lock();
  - platform: template
    name: Unlock the car
    on_press: 
      then:
        - lambda: |
            id(tesla_ble_id).unlock();

ble_client:
  - mac_address: AA:BB:CC:DD:EE:FF # Set you car BLE MAC address here
    id: ble_tesla_id
    auto_connect: true

tesla_ble:
  id: tesla_ble_id
  ble_client_id: ble_tesla_id
```

### Protobuf

We're using a patched version of https://github.com/trifinite/vcsec-archive/blob/master/protos/VCSECv4.20.69.proto.
A few patches are required because nanopb uses a callback mechanism when dealing w/ arbitrary data length/size.
e.g. variable length strings, bytes, arrays. This makes it very impractical in our case.
The (original author)[https://github.com/pmdroid/tesla-ble] of the BLE client also made a few patched I had reverse engineer, because he did not commit the proto file.

pb files generation:
install nanopb-0.4.7
```shell
nanopb-0.4.7-linux-x86/generator-bin/nanopb_generator VCSECv4.20.69.proto -L quote
```

### Refs

- https://github.com/nanopb/nanopb
- https://jpa.kapsi.fi/nanopb/docs/concepts.html
- https://github.com/trifinite/vcsec-archive
- https://github.com/teslamotors/vehicle-command
- https://github.com/pmdroid/tesla-ble
- https://teslabtapi.lexnastin.com/docs/start
