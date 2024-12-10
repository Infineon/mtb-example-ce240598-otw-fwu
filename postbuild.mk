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
#This file cofngures the MCU Boot Image header/signature

#for EPB MCUboot header offset
SB_HDR_OFFSET:=0x400

COMPILED_HEX=$(MTB_TOOLS__OUTPUT_CONFIG_DIR)/$(APPNAME)

#delete existing hex files, to avoid double header/signature
PREBUILD += bash ./scripts/prebuild.sh $(COMPILED_HEX) 

#The user application file
INPUT_IMAGE?=$(COMPILED_HEX).hex

#Output application image file
OUTPUT_IMAGE?=$(COMPILED_HEX).hex

# Slot size is set to 128KB. Calcuated as (128*1024)Bytes 
# EdgeProtectTools assumes the slot size as 128KB by default. 
# If you don't pass this argument, default slot size will be used. 
SLOT_SIZE?=0x10000

#private key used to sign the image.
KEY_PATH=./keys/oem_rot_priv_key_0.pem

#Primary image offset of the CM33 secure application in flash
#make sure the linker_s_flash.ld file has the same start address
PRIMARY_IMAGE_START_ADDR=0x32000000

#Primary image offset of the CM33 secure application in flash
#SECONDARY_IMAGE_START_ADDR=0x32010000 

#alignment of the flash device ;default 8
FLASH_ALIGNMENT?=1

#roll back counter value
RB_COUNTER?=0x01

#The value, which is read back from erased flash. Default: 0
ERASED_VAL?=0

#Sets minimum erase size. Default 0x200. 
MIN_ERASE_SIZE?=0x200

ifneq ($(EPB),TRUE)
ADDITIONAL_ARGS+=--public-key-format full		\
				--pubkey-encoding raw			\
				--signature-encoding raw		\
				--overwrite-only
else
UPGRADE_BY?=OVERWRITE
ifneq ($(UPGRADE_BY),SWAP)				
ADDITIONAL_ARGS+=--overwrite-only				
endif
endif

#Image version for Boot usecase
ifeq ($(IMG_TYPE),BOOT)
IMAGE_VERSION=1.0.0
APP_START_ADDR=$(shell expr $$(( $(PRIMARY_IMAGE_START_ADDR) + $(EPB_SIZE) )) )

#Image version for update usecase
else ifeq ($(IMG_TYPE),UPDATE)
IMAGE_VERSION=2.0.0

#assign the update/secondary image start address
APP_START_ADDR=$(shell expr $$(( $(PRIMARY_IMAGE_START_ADDR) + $(SLOT_SIZE) + $(EPB_SIZE) )) )
ADDITIONAL_ARGS+=--pad 

else
$(error Invalid IMG_TYPE. Please set it to either BOOT or UPDATE)
endif

$(info APP_START_ADDR is $(APP_START_ADDR))
EPT_PATH?=edgeprotecttools
 
#ifneq ($(EPB),TRUE)										
#Signing the image for secured boot
POSTBUILD+=$(EPT_PATH) sign-image				\
			--image $(INPUT_IMAGE)				\
			--output $(OUTPUT_IMAGE)			\
			--header-size $(SB_HDR_OFFSET)		\
			--image-version $(IMAGE_VERSION)	\
			--slot-size $(SLOT_SIZE)			\
			--key $(KEY_PATH)					\
			--hex-addr $(APP_START_ADDR)		\
			--align $(FLASH_ALIGNMENT)			\
			--min-erase-size $(MIN_ERASE_SIZE)	\
			-s $(RB_COUNTER) 					\
			--erased-val $(ERASED_VAL) 			\
			$(ADDITIONAL_ARGS)	
#endif
