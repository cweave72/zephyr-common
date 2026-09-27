/*******************************************************************************
 *  @file: main.c
 *
 *  @brief: Tests for IniFileParser. The files live on littlefs on a RAM disk,
 *  mounted at /ram through FsApi_addMount.
*******************************************************************************/
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include "FsApi.h"
#include "IniFileParser.h"

#define CONF    "/ram/test.conf"

FS_LITTLEFS_DECLARE_CUSTOM_CONFIG(ram_lfs, 4, 512, 512, 512, 2048);

static struct fs_mount_t ram_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &ram_lfs,
    .storage_dev = (void *)"RAM",
    .mnt_point = "/ram",
    .flags = FS_MOUNT_FLAG_USE_DISK_ACCESS,
};

/** @brief The main test file. */
static const char conf_text[] =
    "# A comment line.\n"
    "; Another comment line.\n"
    "top = global value\n"
    "\n"
    "[ipv4]\n"
    "mode = static           # static | dhcp\n"
    "address=192.168.1.16\n"
    "   netmask   =   255.255.255.0   \n"
    "gateway = 192.168.1.1;not a comment\n"
    "port = 0x3E8\n"
    "count = -12\n"
    "bad = 12abc\n"
    "empty =\n"
    "dup = first\n"
    "dup = second\n"
    "\n"
    "[ wifi ]\n"
    "ssid = \"My Net # 2\"\n"
    "key = pa#ss\n"
    "spaced = \"  lead and trail  \"\n"
    "Case = upper\n"
    "noequals\n";

static void
write_file(const char *path, const char *text)
{
    ssize_t num;

    (void)FsApi_remove(path);
    num = FsApi_writeFile(path, 0, text, strlen(text), FS_O_CREATE);
    zassert_equal(num, (ssize_t)strlen(text), "write of %s: %d", path,
        (int)num);
}

static void *
setup(void)
{
    int ret;

    ret = FsApi_init();
    zassert_equal(ret, 0, "FsApi_init: %d", ret);
    ret = FsApi_addMount(&ram_mnt);
    zassert_equal(ret, 0, "FsApi_addMount: %d", ret);

    write_file(CONF, conf_text);
    return NULL;
}

ZTEST_SUITE(inifileparser_tests, NULL, setup, NULL, NULL, NULL);

/** @brief Checks that key in section has the value want. */
static void
expect(const char *section, const char *key, const char *want)
{
    char buf[48];
    int ret;

    ret = IniFileParser_get(CONF, section, key, buf, sizeof(buf));
    zassert_equal(ret, (int)strlen(want), "[%s] %s: ret %d", section, key, ret);
    zassert_str_equal(buf, want, "[%s] %s: '%s'", section, key, buf);
}

ZTEST(inifileparser_tests, test_values)
{
    expect("ipv4", "mode", "static");
    expect("ipv4", "address", "192.168.1.16");
    expect("ipv4", "netmask", "255.255.255.0");
    expect("", "top", "global value");
}

ZTEST(inifileparser_tests, test_comments_and_quotes)
{
    /* ';' or '#' without whitespace before it is part of the value. */
    expect("ipv4", "gateway", "192.168.1.1;not a comment");
    expect("wifi", "key", "pa#ss");
    /* Quotes keep '#' and spaces. */
    expect("wifi", "ssid", "My Net # 2");
    expect("wifi", "spaced", "  lead and trail  ");
}

ZTEST(inifileparser_tests, test_sections_and_keys)
{
    char buf[16];

    /* The section name is trimmed: "[ wifi ]" is "wifi". */
    expect("wifi", "Case", "upper");
    /* Names are case-sensitive. */
    zassert_equal(IniFileParser_get(CONF, "wifi", "case", buf, sizeof(buf)),
        -ENOENT);
    zassert_equal(IniFileParser_get(CONF, "WIFI", "Case", buf, sizeof(buf)),
        -ENOENT);
    /* A key of another section is not found. */
    zassert_equal(IniFileParser_get(CONF, "wifi", "address", buf, sizeof(buf)),
        -ENOENT);
    /* A key of a section is not a global key. */
    zassert_equal(IniFileParser_get(CONF, "", "mode", buf, sizeof(buf)),
        -ENOENT);
    /* The first match is returned. */
    expect("ipv4", "dup", "first");
    /* An empty value has length 0. */
    expect("ipv4", "empty", "");
}

ZTEST(inifileparser_tests, test_missing)
{
    char buf[16];

    zassert_equal(IniFileParser_get(CONF, "ipv4", "nokey", buf, sizeof(buf)),
        -ENOENT);
    zassert_equal(IniFileParser_get(CONF, "nosection", "mode", buf,
        sizeof(buf)), -ENOENT);
    zassert_equal(IniFileParser_get("/ram/nofile.conf", "ipv4", "mode", buf,
        sizeof(buf)), -ENOENT);
}

ZTEST(inifileparser_tests, test_sizes)
{
    char buf[8];
    char line[80];

    /* "192.168.1.16" needs 13 bytes. */
    zassert_equal(IniFileParser_get(CONF, "ipv4", "address", buf, sizeof(buf)),
        -ENOSPC);

    /* A line longer than CONFIG_INIFILEPARSER_MAX_LINE (64). */
    memset(line, 'x', sizeof(line));
    memcpy(line, "[s]\nk = ", 8);
    line[sizeof(line) - 2] = '\n';
    line[sizeof(line) - 1] = '\0';
    write_file("/ram/long.conf", line);
    zassert_equal(IniFileParser_get("/ram/long.conf", "s", "k", buf,
        sizeof(buf)), -E2BIG);
}

ZTEST(inifileparser_tests, test_line_ends)
{
    /* CRLF line ends, and no line end after the last line. */
    char buf[8];

    write_file("/ram/crlf.conf", "[a]\r\nx = 1\r\ny = last");
    zassert_equal(IniFileParser_get("/ram/crlf.conf", "a", "x", buf,
        sizeof(buf)), 1);
    zassert_str_equal(buf, "1");
    zassert_equal(IniFileParser_get("/ram/crlf.conf", "a", "y", buf,
        sizeof(buf)), 4);
    zassert_str_equal(buf, "last");
}

ZTEST(inifileparser_tests, test_int)
{
    int32_t v = 0;

    zassert_equal(IniFileParser_getInt(CONF, "ipv4", "port", &v), 0);
    zassert_equal(v, 1000);
    zassert_equal(IniFileParser_getInt(CONF, "ipv4", "count", &v), 0);
    zassert_equal(v, -12);
    zassert_equal(IniFileParser_getInt(CONF, "ipv4", "bad", &v), -EINVAL);
    zassert_equal(IniFileParser_getInt(CONF, "ipv4", "empty", &v), -EINVAL);
    zassert_equal(IniFileParser_getInt(CONF, "ipv4", "nokey", &v), -ENOENT);
}

ZTEST(inifileparser_tests, test_no_handle_leak)
{
    char buf[16];
    int k;

    /* Each call opens and closes the file. More calls than handles must
       work. */
    for (k = 0; k < 3 * CONFIG_FSAPI_MAX_OPEN_FILES; k++)
    {
        zassert_equal(IniFileParser_get(CONF, "ipv4", "mode", buf,
            sizeof(buf)), 6);
    }
    zassert_equal(FsApi_closeAll(), 0, "a handle was left open");
}
