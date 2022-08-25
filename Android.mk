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
#

LOCAL_PATH := $(call my-dir)

ifneq ($(filter beyond0lte beyond1lte beyond2lte beyondx d1 d2s d2x, $(TARGET_DEVICE)),)
include $(call all-subdir-makefiles,$(LOCAL_PATH))

include $(COMMON_PATH)/AndroidBoard.mk

include $(CLEAR_VARS)

EGL_LIBS := libGLES_mali.so libOpenCL.so libOpenCL.so.1 libOpenCL.so.1.1

EGL_32_SYMLINKS := $(addprefix $(TARGET_OUT_VENDOR)/lib/,$(EGL_LIBS))
$(EGL_32_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "EGL 32 lib link: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /vendor/lib/egl/libGLES_mali.so $@

EGL_64_SYMLINKS := $(addprefix $(TARGET_OUT_VENDOR)/lib64/,$(EGL_LIBS))
$(EGL_64_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "EGL 64 lib link: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /vendor/lib64/egl/libGLES_mali.so $@

LIBUTILS32_SERVICES := \
    android.hardware.graphics.composer@2.2-service \
    android.hardware.sensors-service.samsung-multihal
LIBUTILS32_FILES := $(addprefix $(TARGET_OUT_VENDOR)/bin/hw/,$(LIBUTILS32_SERVICES))
LIBUTILS32_SYMLINK := $(TARGET_OUT_VENDOR)/lib64/libuti32.so
$(LIBUTILS32_SYMLINK): $(LOCAL_INSTALLED_MODULE) $(LIBUTILS32_FILES)
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /vendor/lib64/libutils-v32.so $@
	$(hide) sed -i 's/libutils\.so/libuti32.so/g' $(LIBUTILS32_FILES)

ALL_DEFAULT_INSTALLED_MODULES += $(EGL_32_SYMLINKS) $(EGL_64_SYMLINKS) $(LIBUTILS32_SYMLINK)
endif
