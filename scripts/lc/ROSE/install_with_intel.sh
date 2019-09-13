#!/bin/bash
# Clone and build ROSE 0.9.10.XX with intel 16.0.3 or 18.0.1:

# This script calls srun_do and run_and_log as needed, so no need to call it with them.
# For debug:
#set -x

export REL_SCRIPT_DIR=`dirname $0`
export ROSE_SCRIPT_DIR=`(cd ${REL_SCRIPT_DIR}; pwd)`
export ROSE_PROJECT_BASE="${HOME}/code/ROSE"

# Depends on ROSE_PROJECT_BASE:
source ${ROSE_SCRIPT_DIR}/declare_install_functions.sh

set_main_vars
clone_latest_workspace
do_preconfigure
#use_existing_workspace "0.9.10.242"
setup_intel_compiler
setup_boost
do_intel_configure
make_and_install

