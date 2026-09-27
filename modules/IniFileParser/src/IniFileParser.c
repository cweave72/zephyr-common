/*******************************************************************************
 *  @file: IniFileParser.c
 *
 *  @brief: Read-only parser for INI-style configuration files on a FsApi
 *  mount. See IniFileParser.h for the file format.
 *
 *  Each call reads the file from the start, one line at a time, through a
 *  FsApi file handle. The parser uses no heap: the line buffer is on the
 *  stack of the caller.
*******************************************************************************/
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include "CheckCond.h"
#include "FsApi.h"
#include "IniFileParser.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(IniFileParser, CONFIG_INIFILEPARSER_LOG_LEVEL);

#define MAX_LINE    CONFIG_INIFILEPARSER_MAX_LINE

/** @brief Size of the read buffer. */
#define READ_CHUNK  64

/** @brief Line reader over a FsApi file handle. */
typedef struct LineReader
{
    int fd;
    char chunk[READ_CHUNK];
    int chunk_len;
    int chunk_pos;
    bool eof;
} LineReader;

/******************************************************************************
    read_line
*//**
    @brief Reads the next line into line, without the line end.
    @return The length of the line, -1 at the end of the file, -E2BIG if the
      line is too long, or a negative errno on a read error.
******************************************************************************/
static int
read_line(LineReader *r, char *line, size_t size)
{
    size_t len = 0;
    bool got = false;
    bool too_long = false;

    while (1)
    {
        if (r->chunk_pos == r->chunk_len)
        {
            ssize_t num;

            if (r->eof)
            {
                break;
            }
            num = FsApi_read(r->fd, r->chunk, sizeof(r->chunk));
            CHECK_COND_RETURN(num < 0, (int)num);
            if (num == 0)
            {
                r->eof = true;
                break;
            }
            r->chunk_len = num;
            r->chunk_pos = 0;
        }

        char c = r->chunk[r->chunk_pos++];
        got = true;
        if (c == '\n')
        {
            break;
        }
        if (len < size - 1)
        {
            line[len++] = c;
        }
        else
        {
            too_long = true;
        }
    }

    if (!got)
    {
        return -1;
    }
    line[len] = '\0';
    /* Remove a CR of a CRLF line end. */
    if ((len > 0) && (line[len - 1] == '\r'))
    {
        line[--len] = '\0';
    }
    return too_long ? -E2BIG : (int)len;
}

/******************************************************************************
    trim
*//**
    @brief Removes leading and trailing whitespace. Returns the new start.
******************************************************************************/
static char *
trim(char *s)
{
    char *end;

    while (isspace((unsigned char)*s))
    {
        s++;
    }
    end = s + strlen(s);
    while ((end > s) && isspace((unsigned char)end[-1]))
    {
        *--end = '\0';
    }
    return s;
}

/******************************************************************************
    parse_value
*//**
    @brief Removes the inline comment and the quotes of a value, in place.
    Returns the start of the value.
******************************************************************************/
static char *
parse_value(char *v)
{
    char *p;

    v = trim(v);
    if (*v == '"')
    {
        char *close = strchr(v + 1, '"');

        if (close != NULL)
        {
            *close = '\0';
            return v + 1;
        }
        /* No closing quote: keep the value as it is. */
        return v;
    }

    /* An inline comment starts with whitespace, then '#' or ';'. */
    for (p = v; *p != '\0'; p++)
    {
        if (((*p == '#') || (*p == ';')) && (p > v) &&
            isspace((unsigned char)p[-1]))
        {
            *p = '\0';
            break;
        }
    }
    return trim(v);
}

/******************************************************************************
    [docimport IniFileParser_get]
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
    char *buf, size_t len)
{
    LineReader r = { 0 };
    char line[MAX_LINE];
    bool in_section;
    int ret = -ENOENT;
    int num;

    CHECK_COND_RETURN((path == NULL) || (section == NULL) || (key == NULL) ||
        (buf == NULL) || (len == 0), -EINVAL);

    r.fd = FsApi_open(path, FS_O_READ);
    CHECK_COND_RETURN(r.fd < 0, r.fd);

    /* The keys before the first section belong to section "". */
    in_section = (section[0] == '\0');

    while ((num = read_line(&r, line, sizeof(line))) != -1)
    {
        char *s;
        char *eq;

        if (num == -E2BIG)
        {
            LOG_ERR("%s: a line is longer than %d characters.", path,
                MAX_LINE - 1);
            ret = -E2BIG;
            break;
        }
        if (num < 0)
        {
            ret = num;
            break;
        }

        s = trim(line);
        if ((*s == '\0') || (*s == '#') || (*s == ';'))
        {
            continue;
        }

        if (*s == '[')
        {
            char *close = strchr(s, ']');

            if (close != NULL)
            {
                *close = '\0';
                in_section = (strcmp(trim(s + 1), section) == 0);
            }
            else
            {
                LOG_WRN("%s: section line without ']': %s", path, s);
                in_section = false;
            }
            continue;
        }

        if (!in_section)
        {
            continue;
        }

        eq = strchr(s, '=');
        if (eq == NULL)
        {
            LOG_WRN("%s: line without '=': %s", path, s);
            continue;
        }
        *eq = '\0';
        if (strcmp(trim(s), key) != 0)
        {
            continue;
        }

        s = parse_value(eq + 1);
        num = strlen(s);
        if ((size_t)num >= len)
        {
            ret = -ENOSPC;
        }
        else
        {
            memcpy(buf, s, num + 1);
            ret = num;
        }
        break;
    }

    (void)FsApi_close(r.fd);
    return ret;
}

/******************************************************************************
    [docimport IniFileParser_getInt]
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
    int32_t *val)
{
    char buf[24];
    char *end;
    long v;
    int ret;

    CHECK_COND_RETURN(val == NULL, -EINVAL);

    ret = IniFileParser_get(path, section, key, buf, sizeof(buf));
    CHECK_COND_RETURN(ret < 0, ret);
    CHECK_COND_RETURN(ret == 0, -EINVAL);

    errno = 0;
    v = strtol(buf, &end, 0);
    CHECK_COND_RETURN((*end != '\0') || (errno != 0) || (v < INT32_MIN) ||
        (v > INT32_MAX), -EINVAL);

    *val = (int32_t)v;
    return 0;
}
