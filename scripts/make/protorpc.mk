# This makefile will build python bindings for all proto files found under the
# directory where invoked.
include $(COMMON_MAKE_SCRIPTS)/utils.mk

# Get directory paths and build protoc -I includes.
DIRS := $(abspath $(wildcard */))
INCS := $(foreach d,$(DIRS), -I$(d))

# Add include path for nanopb.proto
ifeq ($(NANOPB_BASE),)
    $(error "NANOPB_BASE must be set.")
endif
INCS += -I$(NANOPB_BASE)/generator/proto

# DST is the base path where libraries will be generated
DST ?= $(CURDIR)
PROTOS := $(foreach d,$(DIRS), $(notdir $(d)))

PROTOC := python -m grpc_tools.protoc

$(info PROTOS=$(PROTOS))
$(info INCS=$(INCS))

.PHONY: proto

proto: $(PROTOS)

.PHONY: $(PROTOS)
$(PROTOS):
	@echo "-- [betterproto] Building python API bindings: $@.proto"
	@echo "-- [betterproto] Destination directory: $(DST)/$@/lib"
	@mkdir -p $(DST)/$@/lib
	$(eval VENV_CMD=$(PROTOC) $(INCS) --python_betterproto_out=$(DST)/$@/lib $@.proto)
	$(invoke_venv)

PROTORPC_INCS := $(foreach d,$(DIRS), -i $(d))
PROTORPC_INCS += -i $(NANOPB_BASE)/generator/proto
PROTORPC_GEN_LOGLEVEL ?= info
PROTORPC_PLUGIN_LOGLEVEL ?= info
PROTORPC_OUT := $(dir $(HANDLER_SOURCE_OUT))

define PROTORPC_GEN_HANDLER_CMD
run_proto_rpc_gen \
--loglevel=$(PROTORPC_GEN_LOGLEVEL) \
--gen-loglevel=$(PROTORPC_PLUGIN_LOGLEVEL) \
$(PROTORPC_INCS) \
--outpath=$(PROTORPC_OUT) \
$(PROTO_SOURCE_IN)
endef

$(HANDLER_SOURCE_OUT): $(PROTO_SOURCE_IN)
	@echo "-- [protorpc] Generating C handlers for $(PROTO_SOURCE)"
	$(PROTORPC_GEN_HANDLER_CMD)

