function(nanopb_build_sources target base_path)

    list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
    include(nanopb)

    message(STATUS "[nanopb] Building protos into library: ${target}_proto")
    message(STATUS "[nanopb] Searching for protos from path: ${base_path}")

    # Create a library for all nanopb source.
    set(libname ${target}_proto)
    zephyr_library_named(${libname})

    # Reference: Setting options for protoc and nanopb plugin.
    #set(NANOPB_OPTIONS
    #    "--verbose \
    #    "
    #    )
    #set(PROTOC_OPTIONS --help)

    # Gather all .proto files.
    FILE(GLOB protofiles ${base_path}/*/*.proto)

    # Construct include paths based on where output files (*.pb.h) will be
    # located. 
    #set(header_include_paths)
    set(protoc_include_paths)
    set(proto_file_stems)
    foreach(p ${protofiles})
        cmake_path(GET p STEM stem)
        #message(stem=${stem})
        list(APPEND protoc_include_paths "-I${CMAKE_CURRENT_SOURCE_DIR}/${stem}")
        #list(APPEND header_include_paths ${CMAKE_CURRENT_BINARY_DIR}/${stem})
        list(APPEND proto_file_stems ${stem})
    endforeach()

    # Must remove nanopb.proto from the list.
    list(FILTER protofiles EXCLUDE REGEX ".*nanopb.proto")

    message(STATUS "[nanopb] Found proto files: ${protofiles}")
    #message(STATUS "[nanopb] Setting include paths: ${header_include_paths}")

    zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
    zephyr_nanopb_sources(${libname} ${protofiles})

endfunction()
