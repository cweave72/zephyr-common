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
    set(header_include_paths)
    set(protoc_include_paths)
    set(proto_file_stems)
    foreach(p ${protofiles})
        cmake_path(GET p STEM stem)
        #message(stem=${stem})
        list(APPEND protoc_include_paths "-I${CMAKE_CURRENT_SOURCE_DIR}/${stem}")
        list(APPEND header_include_paths ${CMAKE_CURRENT_BINARY_DIR}/${stem})
        list(APPEND proto_file_stems ${stem})

    endforeach()

    message(STATUS "[nanopb] Found proto files: ${protofiles}")
    message(STATUS "[nanopb] Setting include paths: ${header_include_paths}")

    zephyr_include_directories(${header_include_paths})
    zephyr_nanopb_sources(${libname} ${protofiles})

    # Create targets for building python bindings for each .proto file.
    foreach(s ${proto_file_stems})

        set(proto_target_name ${s}_proto_pybindings)
        message(STATUS "[nanopb] Creating python target for ${s}.proto")
        add_custom_target(
            ${proto_target_name}
            COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR} INC="${protoc_include_paths}" PROTO=${s} proto
        )
        add_dependencies(${libname} ${proto_target_name})

    endforeach()

endif()
