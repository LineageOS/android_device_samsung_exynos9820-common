#
# Copyright (C) 2021-2022 The LineageOS Project
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

## Include path
TARGET_SPECIFIC_HEADER_PATH := $(COMMON_PATH)/include

## Inherit proprietary vendor configuration
include vendor/samsung/exynos9820-common/BoardConfigVendor.mk

## Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-2a-dotprod
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := cortex-a75

## Architecture (Secondary)
TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-2a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := cortex-a55

## Bluetooth
BOARD_CUSTOM_BT_CONFIG := $(COMMON_PATH)/bluetooth/libbt_vndcfg.txt
BOARD_HAVE_BLUETOOTH_BCM := true

## Boot Image
BOARD_BOOTIMG_HEADER_VERSION := 1
BOARD_CUSTOM_BOOTIMG := true
BOARD_KERNEL_BASE := 0x10000000
BOARD_KERNEL_OFFSET := 0x00008000
BOARD_KERNEL_PAGESIZE := 2048
BOARD_RAMDISK_OFFSET := 0x01000000
BOARD_TAGS_OFFSET := 0x00000100

BOARD_MKBOOTIMG_ARGS := --base $(BOARD_KERNEL_BASE)
BOARD_MKBOOTIMG_ARGS += --pagesize $(BOARD_KERNEL_PAGESIZE)
BOARD_MKBOOTIMG_ARGS += --kernel_offset $(BOARD_KERNEL_OFFSET)
BOARD_MKBOOTIMG_ARGS += --ramdisk_offset $(BOARD_RAMDISK_OFFSET)
BOARD_MKBOOTIMG_ARGS += --tags_offset $(BOARD_TAGS_OFFSET)
BOARD_MKBOOTIMG_ARGS += --header_version $(BOARD_BOOTIMG_HEADER_VERSION)

## Camera
$(call soong_config_set,exynos9820CameraVars,exynos9820_model,$(TARGET_DEVICE))

## DTB
BOARD_PACK_RADIOIMAGES += dtb.img

## DTBO
BOARD_KERNEL_SEPARATED_DTBO := true
BOARD_DTBO_CFG := $(COMMON_PATH)/configs/kernel/$(TARGET_DEVICE).cfg

## Filesystem
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
ifneq ($(TARGET_RO_FILE_SYSTEM_TYPE),)
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := $(TARGET_RO_FILE_SYSTEM_TYPE)
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := $(TARGET_RO_FILE_SYSTEM_TYPE)
endif
TARGET_COPY_OUT_VENDOR := vendor
TARGET_USERIMAGES_USE_EXT4 := true

## Kernel
BOARD_KERNEL_IMAGE_NAME := Image
TARGET_KERNEL_NO_GCC := true
TARGET_KERNEL_SOURCE := kernel/samsung/exynos9820

## Keymaster
TARGET_KEYMASTER_VARIANT := samsung

## Manifest
# HIDL
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE := \
    $(COMMON_PATH)/device_framework_matrix.xml \
    vendor/lineage/config/device_framework_matrix.xml
DEVICE_MANIFEST_FILE += $(COMMON_PATH)/manifest.xml
DEVICE_MATRIX_FILE := $(COMMON_PATH)/compatibility_matrix.xml

## Partitions
BOARD_BOOTIMAGE_PARTITION_SIZE := 57671680
BOARD_CACHEIMAGE_PARTITION_SIZE := 629145600
BOARD_DTBIMG_PARTITION_SIZE := 8388608
BOARD_DTBOIMG_PARTITION_SIZE := 8388608
BOARD_FLASH_BLOCK_SIZE := 4096
BOARD_PRODUCTIMAGE_PARTITION_SIZE := 650117120
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 67633152
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 5976883200
BOARD_VENDORIMAGE_PARTITION_SIZE := 1153433600

BOARD_USES_METADATA_PARTITION := true

BOARD_ROOT_EXTRA_FOLDERS := efs

## Platform
BOARD_VENDOR := samsung
ifneq ($(filter beyond0lte beyond1lte beyond2lte beyondx,$(TARGET_DEVICE)),)
TARGET_BOARD_PLATFORM := universal9820
TARGET_BOOTLOADER_BOARD_NAME := exynos9820
else
DEVICE_MANIFEST_FILE += $(COMMON_PATH)/exynos9825_manifest.xml
TARGET_BOARD_PLATFORM := universal9825
TARGET_BOOTLOADER_BOARD_NAME := exynos9825
endif
TARGET_SOC := exynos9820
include hardware/samsung_slsi-linaro/config/BoardConfig9820.mk

## Properties
TARGET_PRODUCT_PROP += $(COMMON_PATH)/product.prop
TARGET_VENDOR_PROP += $(COMMON_PATH)/vendor.prop

## Recovery
BOARD_INCLUDE_RECOVERY_DTBO := true
BOARD_USES_FULL_RECOVERY_IMAGE := true
TARGET_RECOVERY_FSTAB := $(COMMON_PATH)/configs/init/fstab.exynos9820
TARGET_RECOVERY_PIXEL_FORMAT := "ABGR_8888"
TARGET_RECOVERY_UPDATER_LIBS := librecovery_updater_exynos9820

## Releasetools
TARGET_RELEASETOOLS_EXTENSIONS := $(COMMON_PATH)/releasetools

## RIL
ENABLE_VENDOR_RIL_SERVICE := true

## Security
VENDOR_SECURITY_PATCH := 2023-01-01

## SELinux
BOARD_SEPOLICY_TEE_FLAVOR := teegris
include device/lineage/sepolicy/exynos/sepolicy.mk
include device/samsung_slsi/sepolicy/sepolicy.mk

BOARD_VENDOR_SEPOLICY_DIRS += $(COMMON_PATH)/sepolicy/vendor

## Verified Boot
BOARD_AVB_ENABLE := true
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS += --include_descriptors_from_image $(PRODUCT_OUT)/dtb.img
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS += --flags 3
BOARD_AVB_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_RECOVERY_ALGORITHM := SHA256_RSA4096
BOARD_AVB_RECOVERY_KEY_PATH := external/avb/test/data/testkey_rsa4096.pem
BOARD_AVB_RECOVERY_ROLLBACK_INDEX := 1
BOARD_AVB_RECOVERY_ROLLBACK_INDEX_LOCATION := 1

## Wi-Fi
BOARD_WLAN_BCMDHD_SAE            := true
BOARD_WLAN_DEVICE                := bcmdhd
BOARD_WPA_SUPPLICANT_DRIVER      := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_HOSTAPD_DRIVER             := NL80211
BOARD_HOSTAPD_PRIVATE_LIB        := lib_driver_cmd_bcmdhd
WIFI_AVOID_IFACE_RESET_MAC_CHANGE := true
WIFI_FEATURE_HOSTAPD_11AX        := true
WIFI_HIDL_FEATURE_DUAL_INTERFACE := true
WIFI_HIDL_UNIFIED_SUPPLICANT_SERVICE_RC_ENTRY := true
WPA_SUPPLICANT_VERSION           := VER_0_8_X
