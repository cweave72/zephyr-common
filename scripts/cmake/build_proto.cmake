function(nanopb_build_sources target)

    list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
    include(nanopb)

    set(libname ${target}_proto)
    message(STATUS "[nanopb] Building protos into library: ${libname}")
    zephyr_library_named(${libname})

    # Reference: Setting options for protoc and nanopb plugin.
    #set(NANOPB_OPTIONS
    #    "--verbose \
    #    "
    #    )
    #set(PROTOC_OPTIONS --help)

    # Find all .proto files in each search path. A search path can use two
    # layouts: <path>/<Name>/<Name>.proto or <path>/<Name>.proto.
    set(protofiles)
    foreach(base_path ${ARGN})
        message(STATUS "[nanopb] Searching for protos from path: ${base_path}")
        FILE(GLOB found ${base_path}/*/*.proto ${base_path}/*.proto)
        list(APPEND protofiles ${found})

        # nanopb writes headers to a subdirectory of the binary directory.
        # The subdirectory has the same path as the proto file relative to
        # CMAKE_CURRENT_SOURCE_DIR. This occurs when the proto file is in
        # the application source tree. Refer to NANOPB_GENERATE_CPP_RELPATH
        # in FindNanopb.cmake.
        # Example: an application proto/ directory generates headers in
        # ${CMAKE_CURRENT_BINARY_DIR}/proto.
        # Add this subdirectory to the include path. If you do not add it,
        # the compiler cannot find <name>.pb.h.
        file(RELATIVE_PATH base_path_rel ${CMAKE_CURRENT_SOURCE_DIR} ${base_path})
        if(NOT base_path_rel MATCHES "^\\.\\.")
            zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR}/${base_path_rel})
        endif()
    endforeach()

    # Must remove nanopb.proto from the list.
    list(FILTER protofiles EXCLUDE REGEX ".*nanopb.proto")

    message(STATUS "[nanopb] Found proto files: ${protofiles}")

    zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})

    zephyr_nanopb_sources(${libname} ${protofiles})

endfunction()
