include $(COMMON_MAKE_SCRIPTS)/utils.mk

DIRS := $(abspath $(wildcard $(PROTO_BASE)/*/))

HANDLER_SOURCE_OUT ?= protorpc_build/handlers.c

PROTORPC_INCS := $(foreach d,$(DIRS), -i $(d))
PROTORPC_INCS += -i $(NANOPB_BASE)/generator/proto
PROTORPC_GEN_LOGLEVEL ?= info
PROTORPC_PLUGIN_LOGLEVEL ?= info
PROTORPC_OUT := $(dir $(HANDLER_SOURCE_OUT))

define PROTORPC_GEN_HANDLER_CMD
run_protorpc_gen \
--loglevel=$(PROTORPC_GEN_LOGLEVEL) \
--gen-loglevel=$(PROTORPC_PLUGIN_LOGLEVEL) \
$(PROTORPC_INCS) \
--outpath=$(PROTORPC_OUT) \
$(PROTO_SOURCE_IN)
endef

.PHONY: help
help:
	@echo 'Usage: make handlers PROTO_SOURCE_IN=$$PROTO_BASE/dir/your_proto.proto'
	@echo "Generates C handlers for the protobuf definitions."

.PHONY: handlers
handlers: $(HANDLER_SOURCE_OUT)

$(HANDLER_SOURCE_OUT): $(PROTO_SOURCE_IN)
	@echo "-- [protorpc] Generating C handlers for $(PROTO_SOURCE)"
	$(eval VENV_CMD=$(PROTORPC_GEN_HANDLER_CMD))
	$(invoke_venv)
