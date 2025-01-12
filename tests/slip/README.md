# Test for slip

Running:
```
make test BOARD=qemu_x86
```
See output at: `twister-out/qemu_x86_atom/slip_tests.test_simple_framer/handler.log`

## TODO

Note: Currently only supports testing on qemu_x86 platform. Using twister with
`--device-testing` resulted in flash failure:

```
make test BOARD=esp32_devkitc_wroom/esp32/procpu ARGS="--device-testing \
--hardware-map map.yaml"
```
