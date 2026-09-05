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
mount, no BOOTSEL, and no button presses, and enables `make debug`. See
"Flashing and debugging over SWD" below.

## Flashing and debugging over SWD

A Raspberry Pi Debug Probe on the SWD header replaces the BOOTSEL/UF2 dance
entirely and is what makes `make debug` work.

### Wiring

- Probe **D** connector (3-pin JST-SH) to the board's 3-pin SWD header:
  `SWCLK` / `GND` / `SWDIO`. Check against the silkscreen.
- Probe **U** connector (UART), only for the `probe-console` snippet below:
  probe TX -> `GP1` (UART0 RX), probe RX -> `GP0` (UART0 TX), GND -> GND.

The probe's own UART bridge and the board's USB-C console both enumerate as
`/dev/ttyACM*`; `ls -l /dev/serial/by-id/` tells them apart
(`...Debug_Probe__CMSIS-DAP...` vs `...ZEPHYR_USB-DEV...`).

### Host setup (once)

**1. OpenOCD.** The Zephyr SDK ships OpenOCD 0.11, whose script tree has
`target/rp2040-core0.cfg` but no `target/rp2040.cfg`, so it cannot drive this
chip at all. Ubuntu's packaged OpenOCD is 0.11 too. Build Raspberry Pi's fork:

```
sudo apt install -y libusb-1.0-0-dev libhidapi-dev libftdi1-dev
git clone --branch rp2040-v0.12.0 --depth 1 \
    https://github.com/raspberrypi/openocd.git ~/src/openocd-rp2040
cd ~/src/openocd-rp2040
./bootstrap
./configure --prefix=$HOME/.local/opt/openocd-rp2040 \
            --enable-cmsis-dap --enable-cmsis-dap-v2 --disable-werror
make -j"$(nproc)" && make install
```

`board.mk` in this directory points the make flow at
`~/.local/opt/openocd-rp2040`; override `OPENOCD_HOME` to use a different build.

**2. udev.** Without a rule the probe's USB node is `root:root 0664` and OpenOCD
fails with "unable to open CMSIS-DAP device". Create
`/etc/udev/rules.d/60-openocd-rpi-probe.rules`:

```
SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", ATTR{idProduct}=="000c", MODE="660", GROUP="plugdev", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", ATTR{idProduct}=="0004", MODE="660", GROUP="plugdev", TAG+="uaccess"
```

then `sudo udevadm control --reload-rules && sudo udevadm trigger`, and replug.
Like `99-rpi-rp2.rules`, this is **host configuration outside this repo** and does
not follow the workspace to another machine.

### Usage

```
make BOARD=w55rp20_evb_pico PRISTINE=y build
make flash          # over SWD; no BOOTSEL, no mount, no buttons
make debug          # flash, then gdb stopped at main
make attach         # gdb onto a running target, no reset, no reflash
make debugserver    # gdb server on :3333 only (VS Code, or your own gdb)
make reset          # restart the image already on the target, no reflash
```

`RUNNER=uf2` still works and needs no probe.

`make reset` is the odd one out: the others are west subcommands, but west has no
`reset`, so the board supplies the OpenOCD invocation as `RESET_CMD` in its
`board.mk` and `common.mk` just runs it. It needs no build directory, so it still
works after `make clean`. Note it uses `reset run`, not `reset halt` -- see the
Gotchas below.

For source-level debugging add the snippets (see `common/snippets/`):

```
make BOARD=w55rp20_evb_pico PRISTINE=y SNIPPET="debug probe-console" build
make debug
```

`debug` turns on `CONFIG_DEBUG_THREAD_INFO`, which gives gdb real Zephyr thread
awareness -- `info threads` lists `main`, `eth_w5500`, `sysworkq`, `tcp_work`,
`idle` and the rest by name and priority rather than one bare core.
`probe-console` moves the console to UART0 so it survives target halts; the
USB CDC console is enumerated by the RP2040 itself and drops whenever gdb stops
the core.

### Gotchas

**Use `make debug`, not `debugserver` + bare `continue`.** After OpenOCD's
`reset init` the core is halted in the bootrom, and simply continuing from there
lands in a Zephyr fatal error (`arch_system_halt`) instead of reaching `main`.
gdb's `load` -- which `make debug` issues and `debugserver` does not --
reprograms flash and leaves XIP in a state the boot sequence survives. With a
raw `debugserver` session, run `load` (or `monitor reset run`) before
`continue`. `make attach`, which never resets, is unaffected.

**The module's flash is not a Winbond.** It is a Puya P25Q16H (JEDEC `85 20 15`,
2MB), which is absent from OpenOCD's SPI device table, so autodetection fails:

```
Error: Unknown flash device (ID 0x00152085)
Error: auto_probe failed
```

This is why `board.cmake` sources `support/w55rp20_evb_pico-rp2040.cfg` instead
of the stock `target/rp2040.cfg`: it declares the bank with an explicit 2MB size,
which makes the driver skip the JEDEC lookup. That file also defines core0 only,
which `--target-handle=_TARGETNAME_0` and `-rtos Zephyr` depend on.

**`Error: BUG: unknown adapter clock mode`** is printed on every connect and is
harmless -- `set_adapter_speed_if_not_set` probes the speed with `catch`, and
OpenOCD logs the failed probe before the speed is set.

**`-O0` does not fit this app.** The `debug-noopt` snippet triples the image
(242KB -> 710KB) and deepens every stack frame; on rpc_demo it overflows a stack
during boot, so the board dies before `main` with no console at all. Raise the
thread stacks if you need it -- see the notes in that snippet.

## Serial monitor

There are **two** serial ports in play, and which one carries the console depends
on how the application was built:

| Port | What it is | Carries the console when |
|---|---|---|
| Board USB-C | the RP2040's own USB CDC-ACM device | default build |
| Probe **U** connector | the Debug Probe's USB-to-UART bridge | built with the `probe-console` snippet |

### Identifying the port

Do not assume `ttyACM` numbering — it depends on plug order, and with the probe
attached the board is often *not* `ttyACM0`. Use the stable by-id names:

```
ls -l /dev/serial/by-id/
```

```
usb-Raspberry_Pi_Debug_Probe__CMSIS-DAP__E6647C74037E882F-if01 -> ../../ttyACM0
usb-ZEPHYR_USB-DEV_554D383836380019-if00                       -> ../../ttyACM1
```

- `...Debug_Probe__CMSIS-DAP...-if01` is the probe's UART bridge. It exists
  whenever the probe is plugged in, whether or not the board is even powered.
- `...ZEPHYR_USB-DEV...` is the board's own console. It appears only once the
  firmware has booted far enough to enumerate USB.

That asymmetry is a useful diagnostic in itself: if the `ZEPHYR_USB-DEV` entry
never shows up, the application is not reaching USB initialisation.

### Running the monitor

`make mon` without `PORT` runs `west espressif monitor`, which does not apply to
this board. Set `PORT` and it uses pyserial's miniterm (Ctrl+] to exit):

```
make PORT=/dev/serial/by-id/usb-ZEPHYR_USB-DEV_554D383836380019-if00 mon
make PORT=/dev/ttyACM1 mon      # same thing, if you have checked the number
```

Prefer the by-id path: it survives replugging and does not shift when another
USB serial device appears.

### Any terminal emulator works

Nothing about these ports is probe-specific — both are ordinary USB CDC serial
devices, so `minicom`, `picocom`, `screen` or anything else is fine. `make mon` is
just a convenience wrapper around pyserial's miniterm.

```
minicom -D /dev/ttyACM0 -b 115200          # Ctrl-A X to exit, Ctrl-A Z for help
screen /dev/ttyACM0 115200                 # Ctrl-A K to kill
picocom -b 115200 /dev/ttyACM0             # Ctrl-A Ctrl-X to exit
```

**minicom gotcha: turn hardware flow control off.** With no `/etc/minicom/minirc.dfl`
or `~/.minirc.dfl`, minicom falls back to compiled-in defaults, which have hardware
flow control *on*. Neither the probe's UART bridge nor the board's CDC-ACM device
wires RTS/CTS, so minicom will refuse to transmit and the port looks dead — output
may appear, but nothing you type reaches the shell.

Turn it off for the session with `Ctrl-A O` -> *Serial port setup* -> `F`, or make it
permanent by writing `~/.minirc.dfl`:

```
pu baudrate 115200
pu bits 8
pu parity N
pu stopbits 1
pu rtscts No
pu xonxoff No
```

Membership of the `dialout` group is required either way (already the case on this
host).

### Using the Debug Probe as the console

Three things have to line up. Miss any one and the port is simply silent.

**1. Wire the U connector.** The probe has two 3-pin JST-SH sockets: **D** is
SWD, **U** is UART. TX and RX cross over:

| Probe **U** | Cable colour | Board pin |
|---|---|---|
| TX | orange | `GP1` — UART0 **RX** |
| GND | black | `GND` |
| RX | yellow | `GP0` — UART0 **TX** |

**2. Build with the console on UART0.** The board default is USB CDC-ACM, so
without this the probe's UART carries nothing:

```
make BOARD=w55rp20_evb_pico PRISTINE=y SNIPPET=probe-console build
make flash
```

**3. Point the monitor at the probe**, not at the board:

```
make PORT=/dev/serial/by-id/usb-Raspberry_Pi_Debug_Probe__CMSIS-DAP__E6647C74037E882F-if01 mon
```

`BAUD` defaults to 115200, which matches `current-speed` on `uart0` in the board
dts. Unlike CDC-ACM, a real UART *does* care about the line rate, so this is the
one path where `BAUD` matters.

### Why use the probe's UART at all

- **The terminal stays open across resets and reflashes.** This is the one that
  matters most day to day. The board's CDC console is enumerated by the RP2040
  itself, so every `make flash`, every reset and every gdb halt tears that USB
  device down: minicom loses the port and has to be restarted, and the `ttyACM`
  number can move if enumeration order shifts. The probe's UART belongs to the
  probe's own MCU, and its link to the host is independent of the target — leave
  the terminal open and just watch output stop and resume.

  | Port | Survives target reset / reflash / halt |
  |---|---|
  | Board CDC-ACM (USB-C) | no |
  | Probe **U** connector | yes |

  For the CDC console, `tio -a <port>` (`sudo apt install tio`) reconnects
  automatically when the device comes back; minicom and picocom do not.
- **It shows early boot output.** UART0 is live from the first instruction,
  whereas USB enumeration completes long after the first log lines are emitted.
- **One cable does everything** — flash, debug and console over the same probe.
- **It works with the USB stack off.** An application that sets
  `CONFIG_USB_DEVICE_STACK=n` to reclaim ~14 KB flash / ~9 KB RAM still has a
  console.

The UART bridge is an ordinary USB-serial adapter and is independent of SWD: it
works with no debug session running, and equally well while gdb is attached.
