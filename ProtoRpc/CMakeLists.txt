if (CONFIG_PROTORPC)
    zephyr_include_directories(include)
    set(srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/ProtoRpc.c"
             "${CMAKE_CURRENT_SOURCE_DIR}/src/ProtoRpc.pb.c")

    zephyr_library_sources(${srcs})

    include(nanopb)
    zephyr_nanopb_sources(app src/ProtoRcp.proto)

endif()
