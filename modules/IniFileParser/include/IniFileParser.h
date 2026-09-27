/*******************************************************************************
 *  @file: IniFileParser.h
 *
 *  @brief: Read-only parser for INI-style configuration files on a FsApi
 *  mount, for example /flash/etc/config/net.conf.
 *
 *  File format:
 *
 *      # A comment line. A line which starts with ';' is also a comment.
 *      global_key = value          # Keys before any section: section "".
 *
 *      [section]
 *      key = value                 # Inline comment: whitespace, then '#'.
 *      name = "a value # with a hash and  spaces"
 *
 *  - Whitespace around sections, keys and values is removed.
 *  - Double quotes keep '#', ';' and leading or trailing spaces in a value.
 *  - Section and key names are case-sensitive. The first match is returned.
 *  - The parser uses no heap. The longest line is
 *    CONFIG_INIFILEPARSER_MAX_LINE.
*******************************************************************************/
#ifndef INIFILEPARSER_H
#define INIFILEPARSER_H

#include <stddef.h>
#include <stdint.h>

/******************************************************************************
    [docexport IniFileParser_get]
*//**
    @brief Reads the value of a key as a string.
    @param[in] path  Absolute path of the file, for example
      "/flash/etc/config/net.conf".
    @param[in] section  The section name without brackets. "" selects the keys
      before the first section.
    @param[in] key  The key name.
    @param[out] buf  Destination for the value. The value is NUL-terminated.
    @param[in] len  Size of buf.
    @return The length of the value (without the NUL) on success. -ENOENT if
      the file, the section or the key does not exist. -ENOSPC if the value
      does not fit in buf. -E2BIG if a line is longer than
      CONFIG_INIFILEPARSER_MAX_LINE. Other negative errno values on a read
      error.
******************************************************************************/
int
IniFileParser_get(const char *path, const char *section, const char *key,
    char *buf, size_t len);

/******************************************************************************
    [docexport IniFileParser_getInt]
*//**
    @brief Reads the value of a key as an integer. Decimal, hexadecimal (0x)
    and octal (0) values are accepted.
    @param[in] path  Absolute path of the file.
    @param[in] section  The section name. "" selects the keys before the first
      section.
    @param[in] key  The key name.
    @param[out] val  Destination for the value.
    @return 0 on success. -EINVAL if the value is not an integer. The other
      errors are the same as for IniFileParser_get.
******************************************************************************/
int
IniFileParser_getInt(const char *path, const char *section, const char *key,
    int32_t *val);

#endif /* INIFILEPARSER_H */
