SHELL := /bin/bash
DST ?= .
PROTOC = python -m grpc_tools.protoc --python_betterproto_out=$(DST)/lib


.PHONY: proto help

help:
	@echo "Makefile targets:"
	@echo "   proto [DST=path] <INC=paths> <PROTO=.proto>    : DST=path to build results; INC=list of include paths; PROTO=source .proto file"

proto: $(DST)/lib/__init__.py

$(DST)/lib/__init__.py: $(INC) $(PROTO)
	@mkdir -p $(DST)/lib
	@echo "Building python API (betterproto) for protobuf source file $(PROTO):"
	@echo "   INCLUDES : $(INC)"
	@echo "   DST      : $(DST)/lib"
	# The following commands run protoc within the virtual env.
	@( \
	. init_env.sh; \
	$(PROTOC) -I $(INC) $(PROTO); \
	)
