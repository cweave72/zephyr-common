# common

This repository is one part of the Zephyr workspace. The workspace also
contains the `applications`, `proto`, and `python` repositories. Each
repository has its own git history.

This repository holds the shared device code: the modules and drivers that the
applications use, the out-of-tree board definitions, the build scripts, and the
template that generates a new application.

## Repository structure

```
├── zephyr
│   ├── module.yml          Declares this repository as a Zephyr module
│   └── Kconfig             Sources modules/Kconfig and drivers/Kconfig
├── CMakeLists.txt          Adds modules/ and drivers/ to the build
├── modules                 Device modules. One directory for each module.
│   ├── Kconfig             Sources the Kconfig of each module
│   ├── CMakeLists.txt
│   ├── ProtoRpc            The RPC dispatcher
│   ├── System              An example module. It also supplies an RPC callset.
│   └── ...
├── drivers                 Out-of-tree drivers
│   └── net
├── boards                  Out-of-tree board definitions, by vendor
│   ├── espressif
│   └── wiznet
├── snippets                Zephyr snippets: debug, debug-noopt, probe-console
├── scripts                 Build scripts. common.mk and the applications use them.
│   ├── cmake
│   └── make
├── templates
│   └── app                 The Copier template for a new application
├── copier.yml              The template answers. Copier needs it at the root.
└── tests                   Unit tests
```

The build reads this repository through `zephyr/module.yml`. The next section
describes that mechanism.

## A Zephyr module

`zephyr/module.yml` declares this repository as a Zephyr module. Thus Zephyr
reads the code here during a build:

| Setting        | Result                                                                                                                                      |
|----------------|---------------------------------------------------------------------------------------------------------------------------------------------|
| `cmake: .`     | Zephyr processes `CMakeLists.txt`, which adds `modules` and `drivers`.                                                                      |
| `kconfig`      | Zephyr reads `zephyr/Kconfig`, which sources `modules/Kconfig` and `drivers/Kconfig`. `modules/Kconfig` sources the Kconfig of each module. |
| `board_root`   | Zephyr finds the boards in `boards`.                                                                                                        |
| `dts_root`     | Zephyr finds the device tree files here.                                                                                                    |
| `snippet_root` | Zephyr finds the snippets in `snippets`.                                                                                                    |

An application needs no path to this repository. The workspace manifest
supplies it, and west registers the module.

## Modules

Each directory in `modules` is one module. A module supplies one or more
Kconfig symbols. An application enables a module through its
`conf/modules.conf` file.

A module has this layout. The `System` module is the example:

```
System/
├── CMakeLists.txt      Adds the sources when the Kconfig symbol is set
├── Kconfig             The symbols and their dependencies
├── Makefile            Optional. Only an RPC module needs it.
├── include/            The public headers
│   ├── System.h
│   └── SystemRpc.h
└── src/                The implementation
    ├── System.c
    └── SystemRpc.c
```

`modules/Kconfig` must source the Kconfig file of a new module. Zephyr does not
find a module that `modules/Kconfig` does not source.

To add a module, use the `add-zephyr-module` procedure, or copy the layout of
an existing module.

### RPC modules

A module can also supply an RPC callset. `System` is an example: `System.c`
holds the library, and `SystemRpc.c` holds the RPC surface.

A callset needs these parts:

| Part                          | Location                                                  |
|-------------------------------|-----------------------------------------------------------|
| The message definitions       | `<Name>.proto` in the `proto` repository                  |
| The C messages                | nanopb generates them during the build                    |
| The handlers and the resolver | `src/<Name>Rpc.c` in the module                           |
| The Python bindings           | `proto_builder` generates them in the `python` repository |

The resolver maps a call to its handler. The `ProtoRpc` module supplies the
dispatcher that calls the resolver.

The `Makefile` of an RPC module includes
`scripts/make/protorpc_handlers.mk`. Run `make handlers` in the module
directory to generate the handler stubs from the proto file. The stubs go to
`protorpc_build/`, which git does not track. Generate the stubs one time, then
write the handler code in `src` and keep it.

## Boards

Currently supported boards:

| Board              | Vendor    | Identifier                      |
|--------------------|-----------|---------------------------------|
| `w55rp20_evb_pico` | WIZnet    | `w55rp20_evb_pico`              |
| `esp32s3_matrix`   | Espressif | `esp32s3_matrix/esp32s3/procpu` |
| `esp32s3_qtpy`     | Espressif | `esp32s3_qtpy/esp32s3/procpu`   |

Each board has a directory under `boards`. That directory is the current
list.

Use the identifier with `west build -b` and with `make BOARD=`. The identifier
comes from the `identifier` field of the Twister yaml of the board. Do not
build the identifier from the board name: a board with more than one CPU
cluster needs the cluster name, and Zephyr rejects a name without it.

## Scripts

`common.mk` at the workspace root includes these files. An application gets
them through its own `Makefile`.

| Script                              | Task                                                                     |
|-------------------------------------|--------------------------------------------------------------------------|
| `scripts/cmake/build_proto.cmake`   | Generates the nanopb C bindings. Accepts one or more proto search paths. |
| `scripts/cmake/app_net_type.cmake`  | Selects and verifies the networking fragment.                            |
| `scripts/make/protorpc.mk`          | Builds the Python bindings for the protos.                               |
| `scripts/make/protorpc_handlers.mk` | Generates the C RPC handler source.                                      |
| `scripts/make/utils.mk`             | Shared make functions.                                                   |

## Application template

`templates/app` holds the Copier template for a new application. `copier.yml`
sits at the repository root, not beside the template, because Copier records
the source commit only when the source is a repository root.

The workspace supplies a generator that drives this template. Refer to the
workspace README. Do not run Copier directly: the generator also writes the
per-board files and the module configuration, which the template cannot
supply.

## Host tools

This repository holds no host tools. They are in the `python` repository.
`scripts/make/protorpc_handlers.mk` calls `run_protorpc_gen`, which is a
console script of that repository.
