export SHELL := /bin/bash

# BASEDIR is the workspace root. common.mk sets it during an application
# build. A module Makefile includes this file without common.mk, thus derive
# the path from the location of this file: common/scripts/make -> the root.
THIS_MK := $(abspath $(lastword $(MAKEFILE_LIST)))
BASEDIR ?= $(abspath $(dir $(THIS_MK))../../..)

# Macro to invoke the virtual environment.
define invoke_venv
   @(\
   source $(BASEDIR)/utils.sh; \
   init_ws init_venv.sh; \
   $(VENV_CMD); \
   )
endef

