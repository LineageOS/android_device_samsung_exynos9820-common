/*
 * Copyright (C) 2021-2023 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "SamsungCameraProvider@2.5"

#include "SamsungCameraProvider.h"

#include <algorithm>

using ::android::NO_ERROR;
using ::android::OK;

const int kMaxCameraIdLen = 16;

SamsungCameraProvider::SamsungCameraProvider() : LegacyCameraProviderImpl_2_5() {
#if !defined(EXYNOS9820_MODEL_beyond0lte) && !defined(EXYNOS9820_MODEL_f62)
    // ID=52 is telephoto
    mExtraIDs.push_back(52);
#endif

#if defined(EXYNOS9820_MODEL_beyond2lte) || defined(EXYNOS9820_MODEL_beyondx)
    // ID=51 is the second front cam
    mExtraIDs.push_back(51);
#endif

#ifdef EXYNOS9820_MODEL_f62
   // ID=52 is depth camera
   mExtraIDs.push_back(52);
  // ID=54 is macro
   mExtraIDs.push_back(54);
#endif

    if (!mInitFailed) {
        for (int i : mExtraIDs) {
            struct camera_info info;
            auto rc = mModule->getCameraInfo(i, &info);

            if (rc != NO_ERROR) {
                continue;
            }

            if (checkCameraVersion(i, info) != OK) {
                ALOGE("Camera version check failed!");
                mModule.clear();
                mInitFailed = true;
                return;
            }

#ifdef SAMSUNG_CAMERA_DEBUG
            ALOGI("ID=%d is at index %d", i, mNumberOfLegacyCameras);
#endif

            char cameraId[kMaxCameraIdLen];
            snprintf(cameraId, sizeof(cameraId), "%d", i);
            std::string cameraIdStr(cameraId);
            mCameraStatusMap[cameraIdStr] = CAMERA_DEVICE_STATUS_PRESENT;

            addDeviceNames(i);
            mNumberOfLegacyCameras++;
        }
    }
}

SamsungCameraProvider::~SamsungCameraProvider() {}
