# Make-flow settings for W55RP20-EVB-Pico (RP2040 + W5500).
#
# Included automatically by common.mk when this is the board being built or
# flashed. Everything here is overridable from the command line.

# This file's own directory, so the board's support/ configs can be located
# without hard-coding a path. Same idiom as THIS_DIR in common.mk.
BOARD_MK_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# --- OpenOCD ----------------------------------------------------------------
# board.cmake makes 'openocd' the default flash and debug runner, sourcing
# interface/cmsis-dap.cfg and target/rp2040.cfg. The Zephyr SDK ships OpenOCD
# 0.11, whose script tree has target/rp2040-core0.cfg but *no* target/rp2040.cfg,
# so the SDK's binary dies in OpenOCD's config phase. An out-of-tree 0.12+ build
# is required; see README.md in this directory for how to build one.
#
# Set OPENOCD_HOME= (empty) to fall back to the SDK's OpenOCD -- harmless with
# RUNNER=uf2, which needs no probe at all.
OPENOCD_HOME ?= $(HOME)/.local/opt/openocd-rp2040

ifneq ($(strip $(OPENOCD_HOME)),)
    # --openocd and --openocd-search are west-level options, accepted by flash,
    # debug, debugserver and attach whatever the runner. So the tool is chosen at
    # flash time: no rebuild, and no -DOPENOCD= CMake cache entry to go stale.
    RUNNER_OPTS += --openocd $(OPENOCD_HOME)/bin/openocd
    RUNNER_OPTS += --openocd-search $(OPENOCD_HOME)/share/openocd/scripts

    # 'make reset' -- restart the running image without reflashing it. There is no
    # 'west reset' subcommand (runners expose only flash/debug/debugserver/attach/
    # rtt), so common.mk's reset target just runs whatever RESET_CMD a board sets.
    #
    # 'reset run' rather than 'reset halt': halting at the reset vector and then
    # resuming does not reliably boot this chip, whereas reset run does. See the
    # Gotchas section of README.md.
    RESET_CMD = $(OPENOCD_HOME)/bin/openocd \
        -s $(BOARD_MK_DIR)/support \
        -f openocd.cfg \
        -c "source [find interface/cmsis-dap.cfg]" \
        -c "transport select swd" \
        -c "source [find w55rp20_evb_pico-rp2040.cfg]" \
        -c "set_adapter_speed_if_not_set 2000" \
        -c "init" -c "reset run" -c "shutdown"
endif
