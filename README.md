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

# Set something like
# - lambda: |
#     id(tesla_ble_id).test();
# somewhere to try to interract w/ the car

ble_client:
  - mac_address: AA:BB:CC:DD:EE:FF # Set you car BLE MAC address here
    id: ble_tesla_id
    auto_connect: true

tesla_ble:
  id: tesla_ble_id
  ble_client_id: ble_tesla_id
```
