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


