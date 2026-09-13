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
reads the code here during a build.

`zephyr/module.yml`
```yaml
build:
  cmake: .
  kconfig: zephyr/Kconfig
  settings:
    board_root: .
    dts_root: .
    snippet_root: .
```

| Setting        | Result                                   |
|----------------|------------------------------------------|
| `cmake`        | Zephyr reads `CMakeLists.txt`.           |
| `kconfig`      | Zephyr reads `zephyr/Kconfig`.           |
| `board_root`   | Zephyr finds the boards in `boards`.     |
| `dts_root`     | Zephyr finds the device tree files here. |
| `snippet_root` | Zephyr finds the snippets in `snippets`. |

`CMakeLists.txt` adds `modules` and `drivers`. `zephyr/Kconfig` sources
`modules/Kconfig` and `drivers/Kconfig`, and `modules/Kconfig` sources the
Kconfig of each module.

An application needs no path to this repository. The workspace manifest
supplies it, and west registers the module.

## Modules

Each directory in `modules` provides a standalone module for use in an
application. A module supplies one or more Kconfig symbols. Each module's
Kconfig specifies any other dependencies. An application
enables a module through an app-level `.conf` file.

A module has this layout. See the `System` module as an example:

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

### The CMakeLists.txt of a module

A guard on the Kconfig symbol of the module wraps the whole file. Thus the
module adds nothing to the build when the application does not enable it.

```cmake
if (CONFIG_SYSTEM)

    zephyr_include_directories(include)
    zephyr_library_sources("src/System.c")
    zephyr_library_sources_ifdef(CONFIG_SYSTEMRPC "src/SystemRpc.c")

endif()
```

| Command                          | Task                            |
|----------------------------------|---------------------------------|
| `if (CONFIG_<SYMBOL>)`           | Builds nothing when unset.      |
| `zephyr_include_directories()`   | Adds the public header path.    |
| `zephyr_library_sources()`       | Adds the sources of the module. |
| `zephyr_library_sources_ifdef()` | Adds a source conditionally.    |

`System` uses the `_ifdef` form to add its RPC surface only when
`CONFIG_SYSTEMRPC` is set.

A module with more than one or two sources can collect them first:

```cmake
if (CONFIG_COBS)
    set(srcs "src/Cobs.c"
             "src/Cobs_frame.c"
             )

    zephyr_include_directories(include)
    zephyr_library_sources(${srcs})
endif()
```

### How to register a module

Two files register a module. Zephyr does not find a module that both files do
not name:

| File                     | Entry                      |
|--------------------------|----------------------------|
| `modules/Kconfig`        | `rsource "<Name>/Kconfig"` |
| `modules/CMakeLists.txt` | `add_subdirectory(<Name>)` |

### RPC modules

A module can also supply an RPC callset. `System` is an example: `System.c`
holds the library, and `SystemRpc.c` holds the RPC surface.

A callset needs these parts:

| Part                  | Location                            |
|-----------------------|-------------------------------------|
| Message definitions   | `<Name>.proto` in the `proto` repo  |
| C messages            | nanopb generates them at build time |
| Handlers and resolver | `src/<Name>Rpc.c` in the module     |
| Python bindings       | `proto_builder` generates them      |

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

Paths below are relative to `scripts`.

| Script                      | Task                             |
|-----------------------------|----------------------------------|
| `cmake/build_proto.cmake`   | Generates the nanopb C bindings. |
| `cmake/app_net_type.cmake`  | Selects the networking fragment. |
| `make/protorpc.mk`          | Builds the Python bindings.      |
| `make/protorpc_handlers.mk` | Generates the C RPC handlers.    |
| `make/utils.mk`             | Shared make functions.           |

`build_proto.cmake` accepts one or more proto search paths.
`app_net_type.cmake` also verifies the fragment against the merged Kconfig.

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
