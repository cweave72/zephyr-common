# IniFileParser

IniFileParser reads INI-style configuration files on a FsApi mount, for example
`/flash/etc/config/net.conf`. It is read-only and uses no heap.

Use it with branding: `make brand` writes the files into the flash file system
at build time (see `common/modules/FsApi/README.md`).

## File format

```
# A comment line. A line which starts with ';' is also a comment.
global_key = value          # Keys before any section: section "".

[ipv4]
mode = static               # Inline comment: whitespace, then '#' or ';'.
address = 192.168.1.16

[wifi]
ssid = "My Net # 2"         # Quotes keep '#', ';' and spaces.
key = pa#ss                 # '#' without whitespace before it is data.
```

Rules:

- Whitespace around section names, keys and values is removed.
- An inline comment starts with whitespace, then `#` or `;`.
- Double quotes keep `#`, `;` and leading or trailing spaces in a value.
- Section and key names are case-sensitive.
- If a key occurs two times in a section, the first value is used.
- CRLF line ends are accepted. The last line can have no line end.
- A line without `=` is ignored, with a warning.

## API

| Function               | Description                                                             |
|------------------------|-------------------------------------------------------------------------|
| `IniFileParser_get`    | The value of a key as a NUL-terminated string. Returns its length.      |
| `IniFileParser_getInt` | The value of a key as an `int32_t`: decimal, hex (`0x`) or octal (`0`). |

Return values:

| Value     | Meaning                                                                |
|-----------|------------------------------------------------------------------------|
| >= 0      | Success (`IniFileParser_get`: the length of the value).                |
| `-ENOENT` | The file, the section or the key does not exist.                       |
| `-ENOSPC` | The value does not fit in the buffer.                                  |
| `-E2BIG`  | A line is longer than `CONFIG_INIFILEPARSER_MAX_LINE`.                 |
| `-EINVAL` | Bad argument, or (`IniFileParser_getInt`) the value is not an integer. |

Each call opens the file, reads it from the start and closes it. It uses one
FsApi file handle while it runs.

```c
#include "IniFileParser.h"

char addr[16];
int ret = IniFileParser_get("/flash/etc/config/net.conf", "ipv4", "address",
    addr, sizeof(addr));
if (ret == -ENOENT)
{
    /* Use a build-time default. */
}
```

## Kconfig

| Symbol                   | Default | Description                                             |
|--------------------------|---------|---------------------------------------------------------|
| `INIFILEPARSER`          | n       | Enables IniFileParser. Depends on `FSAPI`.              |
| `INIFILEPARSER_MAX_LINE` | 128     | Longest line, with the NUL. The buffer is on the stack. |

## Tests

`common/tests/IniFileParser` runs on `qemu_x86` with a RAM disk:

```bash
cd common/tests/IniFileParser
make BOARD=qemu_x86 test
```
