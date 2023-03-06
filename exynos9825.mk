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
#

COMMON_PATH := device/samsung/exynos9820-common

# init
FSTAB_FILES := \
    $(COMMON_PATH)/configs/init/fstab.exynos9820:$(TARGET_COPY_OUT_RAMDISK)/fstab.exynos9825 \
    $(COMMON_PATH)/configs/init/fstab.exynos9820:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.exynos9825

# Inherit exynos9820 configuration
$(call inherit-product, $(COMMON_PATH)/exynos9820.mk)
