#!/usr/bin/env python3
"""Writes the FsApi flash file system layout of a build as JSON.

The build runs this script. It reads the devicetree of the build (edt.pickle)
and writes one entry for each enabled zephyr,fstab,littlefs node. The branding
tool (fsapi-brand) reads the JSON, so a littlefs image always has the geometry
of the firmware.

Usage: fsapi_layout.py --edt-pickle <build>/zephyr/edt.pickle --out <file>
"""
import argparse
import json
import os
import pickle
import sys

# edt.pickle holds objects from Zephyr's devicetree package.
sys.path.insert(0, os.path.join(os.environ.get("ZEPHYR_BASE", ""), "scripts",
                                "dts", "python-devicetree", "src"))

COMPAT = "zephyr,fstab,littlefs"


def mount_entry(node):
    """-> the layout of one fstab node."""
    part = node.props["partition"].val
    # partition -> partitions -> flash device node.
    flash = part.parent.parent
    return {
        "mount_point": node.props["mount-point"].val,
        "fstab_node": node.path,
        "partition_node": part.path,
        "partition_label": part.labels[0] if part.labels else None,
        "partition_offset": part.regs[0].addr,
        "partition_size": part.regs[0].size,
        "flash_base": flash.regs[0].addr if flash.regs else 0,
        "erase_block_size": (flash.props["erase-block-size"].val
                             if "erase-block-size" in flash.props else 4096),
        "read_size": node.props["read-size"].val,
        "prog_size": node.props["prog-size"].val,
        "cache_size": node.props["cache-size"].val,
        "lookahead_size": node.props["lookahead-size"].val,
        "block_cycles": node.props["block-cycles"].val,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--edt-pickle", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    with open(args.edt_pickle, "rb") as f:
        edt = pickle.load(f)

    layout = {
        "format": 1,
        "mounts": [mount_entry(n) for n in edt.compat2okay.get(COMPAT, [])],
    }

    text = json.dumps(layout, indent=2) + "\n"
    # Write only on a change, so dependent steps do not run again.
    try:
        with open(args.out) as f:
            if f.read() == text:
                return
    except FileNotFoundError:
        pass
    with open(args.out, "w") as f:
        f.write(text)


if __name__ == "__main__":
    main()
