# Network-type selection for applications that ship one settings fragment per
# networking transport.
#
# THE PROBLEM THIS SOLVES
#
# An app wants to pick a conf/ fragment based on which network type the board uses.
# The obvious spelling does not work:
#
#     if (CONFIG_APP_NET_TYPE_ETH)                 # always false here
#         list(APPEND EXTRA_CONF_FILE conf/wired_eth_net.conf)
#     endif()
#     find_package(Zephyr ...)
#
# CONFIG_* are not CMake variables until find_package(Zephyr) runs Kconfig and
# imports the resulting .config. Before that line every CONFIG_ test is false. And
# the branch cannot simply move later, because EXTRA_CONF_FILE must be final BEFORE
# find_package -- that is when Kconfig consumes it. The fragments are the input to
# Kconfig; the symbol is its output.
#
# (It appears to work with -DCONFIG_APP_NET_TYPE_ETH=y only because -D creates a
# CMake cache variable that happens to share the name. Nothing to do with Kconfig.)
#
# So: read the declaration ourselves, from the same fragments Kconfig will later
# read, and then assert after the fact that we agreed with it.

set(_APP_NET_TYPE_TOKENS wifi serial eth usb)

# Map a token to the Kconfig symbol suffix and back.
function(_app_net_type_token_to_symbol token out_var)
    string(TOUPPER "${token}" upper)
    set(${out_var} "CONFIG_APP_NET_TYPE_${upper}" PARENT_SCOPE)
endfunction()

function(_app_net_type_describe token out_var)
    if (token STREQUAL "wifi")
        set(${out_var} "wifi networking" PARENT_SCOPE)
    elseif (token STREQUAL "serial")
        set(${out_var} "serial networking" PARENT_SCOPE)
    elseif (token STREQUAL "eth")
        set(${out_var} "wired ethernet networking" PARENT_SCOPE)
    elseif (token STREQUAL "usb")
        set(${out_var} "usb (CDC-ECM) networking" PARENT_SCOPE)
    else()
        set(${out_var} "${token} networking" PARENT_SCOPE)
    endif()
endfunction()

# Scan one .conf fragment for a CONFIG_APP_NET_TYPE_<X>=y line.
# Sets <out_var> to the lowercase token, or "" if the file declares nothing.
function(_app_net_type_scan_file path out_var)
    set(${out_var} "" PARENT_SCOPE)
    if (NOT EXISTS "${path}")
        return()
    endif()

    # Anchored at line start, so a commented-out '#CONFIG_...' does not match.
    # Requires '=y', so an explicit '=n' does not select either.
    file(STRINGS "${path}" hits REGEX "^CONFIG_APP_NET_TYPE_[A-Z]+=y")

    set(found "")
    foreach(line ${hits})
        string(REGEX MATCH "^CONFIG_APP_NET_TYPE_([A-Z]+)=y" _m "${line}")
        string(TOLOWER "${CMAKE_MATCH_1}" token)

        if (NOT token IN_LIST _APP_NET_TYPE_TOKENS)
            message(FATAL_ERROR
                "[app] ${path}: unknown network type 'CONFIG_APP_NET_TYPE_${CMAKE_MATCH_1}'.\n"
                "      Valid types: ${_APP_NET_TYPE_TOKENS}")
        endif()

        if (found AND NOT found STREQUAL token)
            message(FATAL_ERROR
                "[app] ${path} declares two network types ('${found}' and '${token}').\n"
                "      Declare exactly one CONFIG_APP_NET_TYPE_<X>=y.")
        endif()
        set(found "${token}")
    endforeach()

    set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

#
# app_net_type_resolve(<out_var> [<fallback>])
#
# Resolve the network type BEFORE find_package(Zephyr), mirroring Zephyr's own
# precedence:
#
#   1. the command line  -DCONFIG_APP_NET_TYPE_<X>=y
#   2. boards/<BOARD>*.conf
#   3. prj.conf
#   4. <fallback>
#
# <fallback> defaults to "wifi" and MUST match the app Kconfig's
# 'choice APP_NET_TYPE' default. If one changes, change the other -- otherwise
# app_net_type_verify() will fail every build that declares nothing.
#
function(app_net_type_resolve out_var)
    set(fallback "wifi")
    if (ARGC GREATER 1)
        set(fallback "${ARGV1}")
    endif()

    set(resolved "")
    set(source "")

    # 1. Command line. Zephyr has not run yet, so a -DCONFIG_... is still just an
    #    ordinary cache entry under its own name.
    foreach(token ${_APP_NET_TYPE_TOKENS})
        _app_net_type_token_to_symbol(${token} sym)
        if (${sym})
            if (resolved AND NOT resolved STREQUAL token)
                message(FATAL_ERROR
                    "[app] Command line sets both -D${sym}=y and a conflicting type "
                    "'${resolved}'. Pass exactly one.")
            endif()
            set(resolved "${token}")
            set(source "the command line (-D${sym}=y)")
        endif()
    endforeach()

    # 2. Board fragments. Glob rather than reconstructing Zephyr's board-file naming:
    #    this tree has both plain (w55rp20_evb_pico.conf) and qualified
    #    (esp32s3_matrix_procpu.conf) forms.
    #
    #    BOARD must be stripped of its qualifiers first. This runs before
    #    find_package(Zephyr), so BOARD is still exactly what was passed on the
    #    command line -- for a qualified target that is
    #    "esp32s3_matrix/esp32s3/procpu", and globbing
    #    "boards/esp32s3_matrix/esp32s3/procpu*.conf" matches nothing. The scan
    #    then silently found no declaration and fell through to the default,
    #    which is only harmless when the default happens to be right.
    #    Zephyr splits these into BOARD + BOARD_QUALIFIERS itself, but not until
    #    boards.cmake has run, which is too late for us.
    if (NOT resolved AND BOARD)
        string(REGEX REPLACE "/.*$" "" _board_name "${BOARD}")
        file(GLOB board_confs "${CMAKE_CURRENT_SOURCE_DIR}/boards/${_board_name}*.conf")
        list(SORT board_confs)
        foreach(conf ${board_confs})
            _app_net_type_scan_file("${conf}" token)
            if (token)
                if (resolved AND NOT resolved STREQUAL token)
                    file(RELATIVE_PATH a ${CMAKE_CURRENT_SOURCE_DIR} ${source_path})
                    file(RELATIVE_PATH b ${CMAKE_CURRENT_SOURCE_DIR} ${conf})
                    message(FATAL_ERROR
                        "[app] Board fragments disagree on network type: "
                        "${a} says '${resolved}', ${b} says '${token}'.")
                endif()
                set(resolved "${token}")
                set(source_path "${conf}")
                file(RELATIVE_PATH rel ${CMAKE_CURRENT_SOURCE_DIR} ${conf})
                set(source "${rel}")
            endif()
        endforeach()
    endif()

    # 3. prj.conf -- an app-wide default.
    if (NOT resolved)
        _app_net_type_scan_file("${CMAKE_CURRENT_SOURCE_DIR}/prj.conf" token)
        if (token)
            set(resolved "${token}")
            set(source "prj.conf")
        endif()
    endif()

    # 4. Fallback.
    if (NOT resolved)
        set(resolved "${fallback}")
        set(source "the default (no CONFIG_APP_NET_TYPE_<X>=y declared)")
    endif()

    _app_net_type_describe("${resolved}" description)
    message(STATUS "[app] Using ${description} (from ${source}).")

    set(${out_var} "${resolved}" PARENT_SCOPE)
endfunction()

#
# app_net_type_verify(<resolved>)
#
# Call AFTER find_package(Zephyr). Fails the build if the type this module picked
# the fragment for is not the type Kconfig actually settled on.
#
# Without this the two halves can drift silently -- a Kconfig 'default' no fragment
# states, an option dropped by an unmet 'depends on', or the symbol set from a
# snippet or EXTRA_CONF_FILE that the pre-parse never looked at. Any of those would
# otherwise build one transport's sources against another's configuration.
#
function(app_net_type_verify resolved)
    _app_net_type_token_to_symbol(${resolved} expected_sym)

    if (${expected_sym})
        return()
    endif()

    # Report what Kconfig chose instead, to make the fix obvious.
    set(actual "nothing")
    foreach(token ${_APP_NET_TYPE_TOKENS})
        _app_net_type_token_to_symbol(${token} sym)
        if (${sym})
            set(actual "${sym}")
        endif()
    endforeach()

    message(FATAL_ERROR
        "[app] Network type mismatch.\n"
        "      This build applied the '${resolved}' conf/ fragment, but Kconfig "
        "resolved to ${actual} (expected ${expected_sym}=y).\n"
        "      Something set the symbol from a file the pre-parse does not scan "
        "(a snippet, an EXTRA_CONF_FILE entry, or a Kconfig default).\n"
        "      Declare the type explicitly with CONFIG_APP_NET_TYPE_<X>=y in "
        "boards/<board>.conf or prj.conf.")
endfunction()
