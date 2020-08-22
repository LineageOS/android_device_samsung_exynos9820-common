#
# Copyright (C) 2021 The LineageOS Project
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

include $(CLEAR_VARS)

LIBGLES_MALI_LIBRARY := /vendor/lib/egl/libGLES_mali.so
LIBGLES_MALI64_LIBRARY := /vendor/lib64/egl/libGLES_mali.so

LIBGLES_MALI_SYMLINK := $(TARGET_OUT_VENDOR)/lib/libGLES_mali.so
$(LIBGLES_MALI_SYMLINK): $(LOCAL_INSTALLED_MODULE)
	@echo "Creating lib/libGLES_mali.so symlink: $@"
	@mkdir -p $(dir $@)
	$(hide) ln -sf $(LIBGLES_MALI_LIBRARY) $@

LIBGLES_MALI64_SYMLINK := $(TARGET_OUT_VENDOR)/lib64/libGLES_mali.so
$(LIBGLES_MALI64_SYMLINK): $(LOCAL_INSTALLED_MODULE)
	@echo "Creating lib64/libGLES_mali.so symlink: $@"
	@mkdir -p $(dir $@)
	$(hide) ln -sf $(LIBGLES_MALI64_LIBRARY) $@

ALL_DEFAULT_INSTALLED_MODULES += \
	$(LIBGLES_MALI_SYMLINK) \
	$(LIBGLES_MALI64_SYMLINK) \
