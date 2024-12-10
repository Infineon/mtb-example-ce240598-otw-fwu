################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Top-level application make file.
#
################################################################################
# \copyright
# Copyright 2024, Cypress Semiconductor Corporation (an Infineon company)
# SPDX-License-Identifier: Apache-2.0
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################


################################################################################
# Basic Configuration
################################################################################

# Type of ModusToolbox Makefile Options include:
#
# COMBINED    -- Top Level Makefile usually for single standalone application
# APPLICATION -- Top Level Makefile usually for multi project application
# PROJECT     -- Project Makefile under Application
#
MTB_TYPE=COMBINED

# Target board/hardware (BSP).
# To change the target, it is recommended to use the Library manager
# ('make library-manager' from command line), which will also update Eclipse IDE launch
# configurations.
TARGET=KIT_PSC3M5_EVK

# Name of application (used to derive name of final linked file).
#
# If APPNAME is edited, ensure to update or regenerate launch
# configurations for your IDE.
APPNAME=mtb-example-ce240598-otw-fwu

# Name of toolchain to use. Options include:
#
# GCC_ARM -- GCC provided with ModusToolbox software
# ARM     -- ARM Compiler (must be installed separately)
# IAR     -- IAR Compiler (must be installed separately)
#
# See also: CY_COMPILER_PATH below
TOOLCHAIN=GCC_ARM

# Default build configuration. Options include:
#
# Debug -- build with minimal optimizations, focus on debugging.
# Release -- build with full optimizations
# Custom -- build with custom configuration, set the optimization flag in CFLAGS
#
# If CONFIG is manually edited, ensure to update or regenerate launch configurations
# for your IDE.
CONFIG=Debug

# If set to "true" or "1", display full command-lines when building.
VERBOSE=


################################################################################
# Advanced Configuration
################################################################################

# Enable optional code that is ordinarily disabled by default.
#
# Available components depend on the specific targeted hardware and firmware
# in use. In general, if you have
#
#    COMPONENTS=foo bar
#
# ... then code in directories named COMPONENT_foo and COMPONENT_bar will be
# added to the build
#
COMPONENTS+=SECURE_DEVICE DFU_I2C DFU_UART DFU_SPI DFU_USER

# Like COMPONENTS, but disable optional code that was enabled by default.
DISABLE_COMPONENTS=

# PSoC Control C3 has only one core
CORE=CM33

# Name of the PSoC Control C3 core
CORE_NAME=CM33_0

# By default the build system automatically looks in the Makefile's directory
# tree for source code and builds it. The SOURCES variable can be used to
# manually add source code to the build process from a location not searched
# by default, or otherwise not found by the build system.
SOURCES=

# Like SOURCES, but for include directories. Value should be paths to
# directories (without a leading -I).
INCLUDES=

# Using DFU MW only as Transport
DEFINES+=CY_DFU_FLOW=CY_DFU_MCUBOOT_FLOW CY_DFU_PRODUCT=0x01020304


# Select softfloat or hardfp floating point. Default is softfloat.
VFP_SELECT=softfloat

# Additional / custom C compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CFLAGS=

# Additional / custom C++ compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CXXFLAGS=

# Additional / custom assembler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ASFLAGS=

# Additional / custom linker flags.
LDFLAGS=

# Additional / custom libraries to link in to the application.
LDLIBS=

# Path to the linker script to use (if empty, use the default linker script).
LINKER_SCRIPT=

# Custom pre-build commands to run.
PREBUILD=

# Custom post-build commands to run.
POSTBUILD=

#Configure this application to be loaded into flash/ram.  
APPTYPE=flash

# Add additional defines to the build process (without a leading -D).
DEFINES+=SECURE

#Set to true if image should be SIGNED.
SECURED_BOOT?=FALSE

DEFINES+=SECURE_BOOT_MSG='"$(SECURED_BOOT)"'

#Edge protect Bootloader flag
#Application image address must be adjusted, if you are adding EPB image in the memory.
#Below flag used to indicate EPB will be added in the flash memory or not. 
EPB?=FALSE

ifeq ($(EPB),TRUE)
#Base on EPB size update the size. This would be a offset for this application
EPB_SIZE?=0x18000
else
EPB_SIZE?=0x00
endif

#Image Type can be BOOT or UPDATE. Default to BOOT.
# To overwrite default values, set IMG_TYPE?=UPDATE here or pass IMG_TYPE=UPDATE in commandline
IMG_TYPE?=BOOT

DEFINES+=IMG_TYPE_MSG='"$(IMG_TYPE)"'

ifeq ($(IMG_TYPE),BOOT)
APP_VERSION_MAJOR?=1
APP_VERSION_MINOR?=0
APP_VERSION_BUILD?=0
else
#UPDATE
APP_VERSION_MAJOR?=2
APP_VERSION_MINOR?=0
APP_VERSION_BUILD?=0
endif
IMG_VER_ARG=$(APP_VERSION_MAJOR).$(APP_VERSION_MINOR).$(APP_VERSION_BUILD)
DEFINES+=IMG_VER_MSG='"$(IMG_VER_ARG)"'


ifeq ($(SECURED_BOOT),TRUE) 
include postbuild.mk
else
#raw image shall be generated without header
SB_HDR_OFFSET=0
endif

ifeq ($(TOOLCHAIN),IAR)
LDFLAGS+=--config_def USER_HDR_OFFSET=$(SB_HDR_OFFSET)
else ifeq ($(TOOLCHAIN),GCC_ARM)
LDFLAGS+=-Wl,'--defsym=USER_HDR_OFFSET=$(SB_HDR_OFFSET)'
else ifeq ($(TOOLCHAIN),ARM)
LDFLAGS+=--predefine="-DUSER_HDR_OFFSET=$(SB_HDR_OFFSET)"
endif


################################################################################
# Paths
################################################################################

# Relative path to the project directory (default is the Makefile's directory).
#
# This controls where automatic source code discovery looks for code.
CY_APP_PATH=

# Relative path to the shared repo location.
#
# All .mtb files have the format, <URI>#<COMMIT>#<LOCATION>. If the <LOCATION> field
# begins with $$ASSET_REPO$$, then the repo is deposited in the path specified by
# the CY_GETLIBS_SHARED_PATH variable. The default location is one directory level
# above the current app directory.
# This is used with CY_GETLIBS_SHARED_NAME variable, which specifies the directory name.
CY_GETLIBS_SHARED_PATH=../

# Directory name of the shared repo location.
#
CY_GETLIBS_SHARED_NAME=mtb_shared

# Absolute path to the compiler's "bin" directory. The variable name depends on the
# toolchain used for the build. Refer to the ModusToolbox user guide to get the correct
# variable name for the toolchain used in your build.
#
# The default depends on the selected TOOLCHAIN (GCC_ARM uses the ModusToolbox
# software provided compiler by default).
CY_COMPILER_GCC_ARM_DIR=


# Locate ModusToolbox helper tools folders in default installation
# locations for Windows, Linux, and macOS.
CY_WIN_HOME=$(subst \,/,$(USERPROFILE))
CY_TOOLS_PATHS ?= $(wildcard \
    $(CY_WIN_HOME)/ModusToolbox/tools_* \
    $(HOME)/ModusToolbox/tools_* \
    /Applications/ModusToolbox/tools_*)

# If you install ModusToolbox software in a custom location, add the path to its
# "tools_X.Y" folder (where X and Y are the version number of the tools
# folder). Make sure you use forward slashes.
CY_TOOLS_PATHS+=

# Default to the newest installed tools folder, or the users override (if it's
# found).
CY_TOOLS_DIR=$(lastword $(sort $(wildcard $(CY_TOOLS_PATHS))))

ifeq ($(CY_TOOLS_DIR),)
$(error Unable to find any of the available CY_TOOLS_PATHS -- $(CY_TOOLS_PATHS). On Windows, use forward slashes.)
endif

$(info Tools Directory: $(CY_TOOLS_DIR))

include $(CY_TOOLS_DIR)/make/start.mk
