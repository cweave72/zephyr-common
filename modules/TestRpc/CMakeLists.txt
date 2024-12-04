if (CONFIG_TESTRPC)
    set(name TestRpc)

    zephyr_include_directories(include)
    zephyr_library_sources("src/${name}.c")

    # Below adds a custom command for generating the handler C api.
    set(proto_file $ENV{BASEDIR}/common/proto/${name}/${name}.proto)
    set(handler_source_output build/${name}.c)

    add_custom_command(
        OUTPUT ${handler_source_output}
        COMMAND ${CMAKE_COMMAND} -E echo "[protorpc] Generating handler source template for ${name}."
        COMMAND make --no-print-directory -C ${CMAKE_CURRENT_SOURCE_DIR} HANDLER_SOURCE_OUT=${handler_source_output} PROTO_SOURCE_IN=${proto_file}
        VERBATIM
        DEPENDS ${proto_file}
        )
    
    add_custom_target(
        ${name}_protorpc_handler ALL
        DEPENDS ${handler_source_output}
        )
endif()
