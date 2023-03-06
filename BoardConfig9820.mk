#
# Copyright (C) 2023 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

COMMON_PATH := device/samsung/exynos9820-common

## Inherit common configuration
include $(COMMON_PATH)/BoardConfigCommon.mk

## Boot Image
BOARD_BOOTIMG_HEADER_VERSION := 1

BOARD_MKBOOTIMG_ARGS += --header_version $(BOARD_BOOTIMG_HEADER_VERSION)

## DTB
BOARD_PACK_RADIOIMAGES += dtb.img

## Partitions
BOARD_BOOTIMAGE_PARTITION_SIZE := 57671680
BOARD_DTBIMG_PARTITION_SIZE := 8388608
BOARD_PRODUCTIMAGE_PARTITION_SIZE := 650117120
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 67633152
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 5976883200
BOARD_VENDORIMAGE_PARTITION_SIZE := 1153433600

## Platform
TARGET_BOARD_PLATFORM := universal9820
TARGET_BOOTLOADER_BOARD_NAME := exynos9820

## Recovery
TARGET_RECOVERY_FSTAB := $(COMMON_PATH)/configs/init/fstab.exynos9820

## Verified Boot
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS += --include_descriptors_from_image $(PRODUCT_OUT)/dtb.img
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS += --flags 3
