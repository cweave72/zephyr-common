if (CONFIG_FSAPI)

    zephyr_include_directories(include)
    zephyr_library_sources("src/FsApi.c")
    zephyr_library_sources_ifdef(CONFIG_FSAPIRPC "src/FsApiRpc.c")

    # Write the flash file system layout (each zephyr,fstab,littlefs node) to
    # <build>/fsapi_layout.json. The branding tool (fsapi-brand) reads it, so a
    # littlefs image always has the geometry of the firmware.
    set(FSAPI_LAYOUT_JSON ${CMAKE_BINARY_DIR}/fsapi_layout.json)
    add_custom_command(
        OUTPUT ${FSAPI_LAYOUT_JSON}
        COMMAND ${CMAKE_COMMAND} -E env ZEPHYR_BASE=${ZEPHYR_BASE}
            ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/fsapi_layout.py
            --edt-pickle ${EDT_PICKLE}
            --out ${FSAPI_LAYOUT_JSON}
        DEPENDS ${EDT_PICKLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/fsapi_layout.py
        COMMENT "Writing the FsApi layout: ${FSAPI_LAYOUT_JSON}"
    )
    add_custom_target(fsapi_layout ALL DEPENDS ${FSAPI_LAYOUT_JSON})

endif()
