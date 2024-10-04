set(srcs "src/RtosUtils.c")
zephyr_include_directories(include)
zephyr_library_sources(${srcs})
         
#set(nanopb_path $ENV{ESP32_TOOLS}/nanopb)
#set(proto_file "${CMAKE_CURRENT_SOURCE_DIR}/src/${name}.proto")
#set(proto_inc_paths
#    ${nanopb_path}/generator/proto
#    )
#
## Build the C-bindings for proto_file.
#include(${nanopb_path}/nanopb.cmake)
#nanopb_build(
#    ${proto_file}
#    PROTO_PATHS ${proto_inc_paths}
#    HEADER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include
#    )

# Uncomment build handler template.
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
