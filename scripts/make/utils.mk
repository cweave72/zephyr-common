export SHELL := /bin/bash

# Macro to invoke the virtual environment.
define invoke_venv
   @(\
   source $(BASEDIR)/utils.sh; \
   init_ws init_venv.sh; \
   $(VENV_CMD); \
   )
endef

