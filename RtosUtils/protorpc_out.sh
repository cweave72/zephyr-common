#!/bin/bash

# This script generates the RPC handler C code boilerplate as a starting point
# for RPC functionality. Run this initially when a proto file is created or
# after changes are made.
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

proto_file=RtosUtilsRpc.proto

outpath=$SCRIPTPATH/protorpc_out
loglevel=info
plugin_loglevel=info

PROTO_PATHS=(
"$ESP32_TOOLS/nanopb/generator/proto"
"$SCRIPTPATH/src"
)

PROTO_INCS=""
for path in ${PROTO_PATHS[@]}; do
    PROTO_INCS+="-i $path "
done

(\
source $ESP32_TOOLS/.venv/bin/activate && \
run_protorpc_gen \
    --loglevel=$loglevel \
    --gen-loglevel=$plugin_loglevel \
    $PROTO_INCS \
    --outpath=$outpath \
    $proto_file
)
