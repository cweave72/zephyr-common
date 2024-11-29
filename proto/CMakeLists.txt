if (CONFIG_NANOPB)

    list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
    include(nanopb)

    # Create a library for all nanopb source.
    set(libname proto)
    zephyr_library_named(${libname})

    # Reference: Setting options for protoc and nanopb plugin.
    #set(NANOPB_OPTIONS
    #    "--verbose \
    #    "
    #    )
    #set(PROTOC_OPTIONS --help)

    # Gather all .proto files.
    FILE(GLOB protofiles */*.proto)

    # Construct include paths based on where output files (*.pb.h) will be
    # located. 
    set(proto_include_paths)
    foreach(p ${protofiles})
        cmake_path(GET p STEM stem)
        #message(stem=${stem})
        list(APPEND proto_include_paths ${CMAKE_CURRENT_BINARY_DIR}/${stem})
    endforeach()

    message(STATUS "[nanopb] Found proto files: ${protofiles}")
    message(STATUS "[nanopb] Setting include paths: ${proto_include_paths}")

    zephyr_include_directories(${proto_include_paths})
    zephyr_nanopb_sources(${libname} ${protofiles})

endif()
