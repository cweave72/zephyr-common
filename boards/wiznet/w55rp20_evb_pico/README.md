# w55rp20_evb_pico (out-of-tree backport)

Board support for the WIZnet **W55RP20-EVB-Pico** — the W55RP20 SiP, which fuses
an RP2040 with a W5500 Ethernet MAC/PHY and 2 MB of flash into one package on a
Pi-Pico form factor.

## Why this lives here

Upstream Zephyr added this board in commit
[`ff138eb3fe2bb9f6a54d92e7396610a8865aedec`](https://github.com/zephyrproject-rtos/zephyr/commit/ff138eb3fe2bb9f6a54d92e7396610a8865aedec)
("boards: wiznet: Add w55rp20_evb_pico", merged 2026-08-06). That commit is
~10.6k commits ahead of `v4.4.0` and is **not in any tagged release**, so it
cannot be picked up by bumping the manifest. This workspace is pinned to Zephyr
`v4.0.0`.

Do not confuse this board with the in-tree `w5500_evb_pico`. That is a different
product — discrete RP2040 + W5500 on **hardware SPI0**, LED on GP25, 16 MB flash.
On the W55RP20 the Ethernet controller is wired to pins that are not on the
hardware SPI block, so it must be driven over **PIO SPI**.

## Hardware wiring

The W5500 hangs off `&pio0` via `raspberrypi,pico-spi-pio` (`w55rp20_pio_spi`);
`&spi0` is disabled outright.

| Signal | GPIO |
| ------ | ---- |
| SCK    | GP21 |
| MOSI   | GP23 |
| MISO   | GP22 |
| CS     | GP20 |
| RST    | GP25 |
| IRQ    | GP24 |
| LED    | GP19 |

GP16–GP25 are consumed internally by the SiP, so `pico_header` exposes only
GP0–GP15 and GP26–GP28. `pico_spi` is aliased to the PIO SPI bus so that
`pico_spi`-based application overlays keep working.

## Console

**The console is on USB CDC-ACM by default**, over the USB-C connector — the
board enumerates as `/dev/ttyACM*` a second or two after reset. This is a board
default, not a build-time option: no snippet, no `ARGS`, no per-application
overlay. A plain `make BOARD=w55rp20_evb_pico build` produces it.

UART0 on GP0/GP1 would otherwise need an external USB-TTL adapter, which is the
reason for the default. `uart0` is still enabled and pinmuxed, so an application
can move the console back to the wire with its own overlay:

```dts
/ {
	chosen {
		zephyr,console = &uart0;
		zephyr,shell-uart = &uart0;
	};
};
```

Two consequences worth knowing:

- **Early boot output is lost.** USB enumeration finishes long after the first
  log lines are emitted. If you need those, UART0 is the only way to see them —
  it is live from the first instruction.
- **The USB device stack is on for every application built for this board**,
  costing roughly 14 KB of flash and 9 KB of RAM. Set `CONFIG_USB_DEVICE_STACK=n`
  in an application's `prj.conf` (together with the overlay above) to opt out.

## Deltas from upstream

The upstream files target Zephyr `main`. Five changes were needed to build on
`v4.0.0`. **All of them should be reverted by deleting this directory once the
board ships in a Zephyr release this workspace tracks.**

1. **SoC dtsi include path.** `<raspberrypi/rpi_pico/rp2040.dtsi>` →
   `<rpi_pico/rp2040.dtsi>`. The vendor-prefixed path postdates v4.0.
2. **Flash partitions.** Upstream uses `compatible = "zephyr,mapped-partition"`
   plus `ranges`, neither of which exists in v4.0. Reverted to the v4.0 form —
   `compatible = "fixed-partitions"` on the `partitions` node, no `ranges`.
   The 2 MB sizing is genuine and was kept.
3. **`&clocks` pinctrl state.** v4.0's `drivers/clock_control/clock_control_rpi_pico.c`
   calls `PINCTRL_DT_INST_DEFINE(0)` and `pinctrl_apply_state()` unconditionally,
   so the node must have a `pinctrl-0`. Added an empty `clocks_default` group to
   the pinctrl dtsi and referenced it, matching what in-tree
   `boards/wiznet/w5500_evb_pico` does on v4.0. Upstream omits this because
   later Zephyr made it optional.
4. **`CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC=125000000`** added to `_defconfig`.
   Newer Zephyr sets this at the SoC level; on v4.0 every RP2040 board defconfig
   must set it itself.
5. **PIO SPI clock source.** Upstream hardcodes `clocks = <&clocks 10>`, which is
   `RPI_PICO_CLKID_PLL_SYS`. Changed to `RPI_PICO_CLKID_CLK_SYS` — the driver
   calls `clock_control_on()`/`clock_control_get_rate()` on this subsys, and
   `CLK_SYS` is what the only in-tree consumer of the binding uses
   (`samples/sensor/bme280/rpi_pico_spi_pio.overlay`). Both read 125 MHz on
   RP2040, so the divisor math is unchanged.

Not backported: `MAINTAINERS.yml`, `doc/index.rst`, `doc/img/*.webp`.

### Local addition beyond the backport

The USB CDC-ACM console is a local decision, not part of the upstream board.
Upstream puts the console on `uart0`. Four pieces implement it:

- **`w55rp20_evb_pico.dts`** — a `cdc_acm_uart0` node under `zephyr_udc0`, and
  `zephyr,console`/`zephyr,shell-uart` pointed at it instead of `&uart0`.
  Declaring the node is what enables the class driver: `USB_CDC_ACM` is
  `default y` gated on `DT_HAS_ZEPHYR_CDC_ACM_UART_ENABLED`.
- **`w55rp20_evb_pico_defconfig`** — `CONFIG_USB_DEVICE_STACK=y` and
  `CONFIG_UART_LINE_CTRL=y`.
- **`Kconfig.defconfig`**, `USB_DEVICE_INITIALIZE_AT_BOOT` — `default y if CONSOLE`.
  This symbol has no default of its own, so without it nothing calls
  `usb_enable()`, the device never enumerates, and you get a clean build with a
  dead port. Boards exposing a USB console set it themselves; see
  `boards/nordic/nrf52840dongle/Kconfig.defconfig`.
- **`Kconfig.defconfig`**, `SHELL_BACKEND_SERIAL_CHECK_DTR` — `default SHELL`, so
  the shell holds output until the terminal asserts DTR rather than losing the
  banner into a not-yet-connected port.

Unlike the five deltas above, upstream will not supersede this. If this
directory is ever deleted in favour of an in-tree board, these four pieces are
the ones worth carrying forward.

## Requirements

Needs `hal_rpi_pico` in the manifest — it supplies both the pico-sdk headers the
PIO SPI driver includes and the RP2040 second-stage bootloader. It is in the
`name-allowlist` in `manifest-repo/west.yml`.

## Usage

```
cd applications/blinky
make BOARD=w55rp20_evb_pico PRISTINE=y build
make BOARD=w55rp20_evb_pico RUNNER=uf2 flash
```

`CONFIG_ETH_W5500` is `default y` gated on `DT_HAS_WIZNET_W5500_ENABLED`, so the
Ethernet driver enables itself from devicetree. Application `.conf` files only
need the L3/L4 options (`CONFIG_NETWORKING`, `CONFIG_NET_IPV4`,
`CONFIG_NET_DHCPV4`, ...).

Flashing is easiest over UF2. Enter the bootloader by holding **BOOTSEL** and
tapping **RUN** (RUN is the RP2040 reset), or by holding BOOTSEL while plugging
in USB-C. The default runner is OpenOCD, which requires Raspberry Pi's OpenOCD
fork.

The `RPI-RP2` volume the bootrom exposes is a synthetic FAT filesystem, not a
view of the flash — it is write-only in practice, and it disappears the instant
the UF2 write completes and the chip reboots. Since `west flash -r uf2` only
scans *mounted* filesystems, the volume has to be mounted on every BOOTSEL
entry, and it enumerates as a fresh device each time (the `/dev/sdX` letter can
change).

This host handles that with a udev rule that auto-mounts it at `/mnt/rpi-rp2`:

    /etc/udev/rules.d/99-rpi-rp2.rules

That file is **host configuration and lives outside this repo**, so it does not
follow the workspace to another machine. Without it, flashing needs a manual
mount first:

    udisksctl mount -b "$(lsblk -o PATH,LABEL -nr | awk '$2=="RPI-RP2"{print $1}')"

A SWD probe on the debug header avoids all of this — OpenOCD flashing needs no
mount, no BOOTSEL, and no button presses, and enables `west debug`.

## Serial monitor

`make mon` without `PORT` runs `west espressif monitor`, which does not apply to
this board. Set `PORT` instead and it uses pyserial's miniterm (Ctrl+] to exit):

```
make PORT=/dev/ttyACM0 mon       # USB-C console (the board default)
make PORT=/dev/ttyUSB0 mon       # UART0 on GP0/GP1, if overridden back to the wire
```

`BAUD` defaults to 115200 and matters only for the UART0 path; CDC-ACM ignores
the line rate.
