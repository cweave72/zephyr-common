if (CONFIG_TESTRPC)
    zephyr_include_directories(include)
    zephyr_library_sources("src/TestRpc.c")

    # Uncomment to build RPC handler template.
    #add_custom_command(
    #    COMMAND ${CMAKE_COMMAND} -E echo "[protorpc] Generating handler source template for ${name}."
    #    COMMAND ${CMAKE_COMMAND} -E touch .protorpc
    #    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/protorpc_out.sh
    #    VERBATIM
    #    OUTPUT .protorpc
    #    DEPENDS ${proto_file}
    #    )
    #
    #add_custom_target(
    #    ${name}_protorpc ALL
    #    DEPENDS .protorpc
    #    )
endif()
