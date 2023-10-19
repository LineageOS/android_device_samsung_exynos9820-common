/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "audio_hw_a2dp_proxy"
#define LOG_NDEBUG 0

#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>

#include <system/audio.h>
#include <log/log.h>

#include <android/hidl/allocator/1.0/IAllocator.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <hidlmemory/mapping.h>

#include "vendor/samsung_slsi/hardware/ExynosA2DPOffload/1.0/IExynosA2DPOffload.h"
#include "audio_a2dp_proxy.h"


/*****************************************************************************/
/**                                                                         **/
/** BT A2DP Offload HAL                                                     **/
/**                                                                         **/
/*****************************************************************************/

using vendor::samsung_slsi::hardware::ExynosA2DPOffload::V1_0::IExynosA2DPOffload;

using ::android::sp;

using ::android::hidl::base::V1_0::IBase;
using ::android::hidl::allocator::V1_0::IAllocator;
using ::android::hidl::memory::V1_0::IMemory;

using ::android::hardware::hidl_memory;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_death_recipient;


static android::sp<IExynosA2DPOffload> gA2DPHal_ = nullptr;
static std::mutex gA2DPHalMutex;

struct A2DPHalDeathRecipient : virtual public hidl_death_recipient {
    // hidl_death_recipient interface
    virtual void serviceDied(uint64_t, const android::wp<IBase>&) override {
        std::lock_guard<std::mutex> lock(gA2DPHalMutex);
        ALOGE("A2DPHAL just died");
        gA2DPHal_ = nullptr;
    }
};

// Retrieve a copy of client
static android::sp<IExynosA2DPOffload> getA2DPHal() {
    std::lock_guard<std::mutex> lock(gA2DPHalMutex);
    static android::sp<A2DPHalDeathRecipient> gA2DPHalDeathRecipient = nullptr;
    static bool gA2DPHalExists = true;

    if (gA2DPHalExists && gA2DPHal_ == nullptr) {
        gA2DPHal_ = IExynosA2DPOffload::getService();

        if (gA2DPHal_ == nullptr) {
            ALOGE("Unable to get A2DP Offload HAL service");
            gA2DPHalExists = false;
        } else {
            if (gA2DPHalDeathRecipient == nullptr) {
                gA2DPHalDeathRecipient = new A2DPHalDeathRecipient();
            }
            Return<bool> linked = gA2DPHal_->linkToDeath(
                gA2DPHalDeathRecipient, 0 /* cookie */);
            if (!linked.isOk()) {
                ALOGE("Transaction error in linking to A2DP HAL death: %s",
                      linked.description().c_str());
                gA2DPHal_ = nullptr;
            } else if (!linked) {
                ALOGW("Unable to link to A2DP HAL death notifications");
                gA2DPHal_ = nullptr;
            } else {
                ALOGD("Connect to A2DP HAL and link to death "
                      "notification successfully");
            }
        }
    }
    return gA2DPHal_;
}


/*****************************************************************************
**  Constants & Macros
******************************************************************************/

/* BT A2DP Host Status */
typedef enum {
    AUDIO_A2DP_STATUS_NONE,
    AUDIO_A2DP_STATUS_INIT,         // Load BT A2DP Host IPC Library & BT A2DP Stream is closed
    AUDIO_A2DP_STATUS_STANDBY,      // BT A2DP Stream is opened, but not working
    AUDIO_A2DP_STATUS_STARTED,      // BT A2DP Stream is working
    AUDIO_A2DP_STATUS_SUSPENDED,    // BT A2DP Stream us suspended
    AUDIO_A2DP_STATUS_CNT,
} a2dp_status;

const char * a2dpstatus_table[AUDIO_A2DP_STATUS_CNT] = {
    [AUDIO_A2DP_STATUS_NONE]      = "A2DP_STATUS_NONE",
    [AUDIO_A2DP_STATUS_INIT]      = "A2DP_STATUS_INIT",
    [AUDIO_A2DP_STATUS_STANDBY]   = "A2DP_STATUS_STANDBY",
    [AUDIO_A2DP_STATUS_STARTED]   = "A2DP_STATUS_STARTED",
    [AUDIO_A2DP_STATUS_SUSPENDED] = "A2DP_STATUS_SUSPENDED",
};

struct a2dp_proxy {
    /* Local variables */
    a2dp_status cur_status;
    a2dp_status prev_status;
};


/******************************************************************************/
/**                                                                          **/
/** A2DP Proxy is Singleton                                                  **/
/**                                                                          **/
/******************************************************************************/

static struct a2dp_proxy *instance = NULL;

static struct a2dp_proxy* getInstance(void)
{
    if (instance == NULL) {
        instance = (struct a2dp_proxy *)calloc(1, sizeof(struct a2dp_proxy));
        ALOGI("proxy-%s: created A2DP Proxy Instance!", __func__);
    }
    return instance;
}

static void destroyInstance(void)
{
    if (instance) {
        free(instance);
        instance = NULL;
        ALOGI("proxy-%s: destroyed A2DP Proxy Instance!", __func__);
    }
    return;
}


/******************************************************************************/
/**                                                                          **/
/** Bluetooth A2DP Proxy Interfaces                                          **/
/**                                                                          **/
/******************************************************************************/

int proxy_a2dp_get_config(uint32_t *type, void *config)
{
    struct a2dp_proxy *aproxy = getInstance();
    hidl_memory codec_info;
    int ret = -1;

    if (aproxy) {
        android::sp<IExynosA2DPOffload> a2dpHal = getA2DPHal();
        if (a2dpHal == nullptr) {
            return ret;
        }

        if (aproxy->cur_status == AUDIO_A2DP_STATUS_STARTED) {
            sp<IAllocator> ashmemAllocator = IAllocator::getService("ashmem");
            Return<void> allocReturn = ashmemAllocator->allocate(4, [&](bool success, const hidl_memory& m) {
                if (!success)
                    ALOGE("proxy-%s: Failed to get AshMem Allocator", __func__);
                else
                    codec_info = m;
            });

            sp<IMemory> memory = ::android::hardware::mapMemory(codec_info);
            if (memory.get() == nullptr)
                ALOGE("proxy-%s: Failed to map Shared Memory", __func__);
            else {
                ret = a2dpHal->a2dp_get_codec_config(codec_info);
                if (ret == 0) {
                    uint32_t* codec_type = static_cast<uint32_t*>(static_cast<void*>(memory->getPointer()));
                    ALOGI("proxy-%s: Codec Type = %d", __func__, codec_type[0]);

                    // Copy A2DP CODEC Configurations based on CODEC Type
                    *type = codec_type[0];
                    if (codec_type[0] == (uint32_t)AUDIO_FORMAT_SBC) {
                        memcpy(config, (void *)&codec_type[1], sizeof(audio_sbc_encoder_config));
                    } else if (codec_type[0] == (uint32_t)AUDIO_FORMAT_APTX) {
                        memcpy(config, (void *)&codec_type[1], sizeof(audio_aptx_encoder_config));
                    }
                } else
                    ALOGE("proxy-%s: A2DP Stream did not get codec config", __func__);
            }
        } else
            ALOGI("proxy-%s: Abnormal A2DP Status(%s)", __func__, a2dpstatus_table[aproxy->cur_status]);
    }

    return ret;
}

int proxy_a2dp_start(void)
{
    struct a2dp_proxy *aproxy = getInstance();
    int ret = -1;

    if (aproxy) {
        android::sp<IExynosA2DPOffload> a2dpHal = getA2DPHal();
        if (a2dpHal == nullptr) {
            return ret;
        }

        if (aproxy->cur_status == AUDIO_A2DP_STATUS_STANDBY) {
            ret = a2dpHal->a2dp_start_stream();
            if (ret == 0) {
                aproxy->prev_status = aproxy->cur_status;
                aproxy->cur_status = AUDIO_A2DP_STATUS_STARTED;
                ALOGI("proxy-%s: Transit to %s from %s", __func__,
                      a2dpstatus_table[aproxy->cur_status], a2dpstatus_table[aproxy->prev_status]);
            } else
                ALOGE("proxy-%s: A2DP Stream did not started", __func__);
        } else
            ALOGI("proxy-%s: Abnormal A2DP Status(%s)", __func__, a2dpstatus_table[aproxy->cur_status]);
    }

    return ret;
}

int proxy_a2dp_stop(void)
{
    struct a2dp_proxy *aproxy = getInstance();
    int ret = -1;

    if (aproxy) {
        android::sp<IExynosA2DPOffload> a2dpHal = getA2DPHal();
        if (a2dpHal == nullptr) {
            return ret;
        }

        if (aproxy->cur_status == AUDIO_A2DP_STATUS_STARTED) {
            ret = a2dpHal->a2dp_stop_stream();
            if (ret == 0) {
                aproxy->prev_status = aproxy->cur_status;
                aproxy->cur_status = AUDIO_A2DP_STATUS_STANDBY;
                ALOGI("proxy-%s: Transit to %s from %s", __func__,
                      a2dpstatus_table[aproxy->cur_status], a2dpstatus_table[aproxy->prev_status]);
            } else
                ALOGE("proxy-%s: A2DP Stream did not stopped", __func__);
        } else
            ALOGI("proxy-%s: Ignored as A2DP Status(%s)", __func__, a2dpstatus_table[aproxy->cur_status]);
    }

    return ret;
}

int proxy_a2dp_suspend(bool flag)
{
    struct a2dp_proxy *aproxy = getInstance();
    int ret = 0;

    if (aproxy) {
        android::sp<IExynosA2DPOffload> a2dpHal = getA2DPHal();
        if (a2dpHal == nullptr) {
            return -1;
        }

        if (flag == true) {
            if (aproxy->cur_status == AUDIO_A2DP_STATUS_STANDBY ||
                aproxy->cur_status == AUDIO_A2DP_STATUS_STARTED) {
                ret = a2dpHal->a2dp_suspend_stream();
                if (ret == 0) {
                    aproxy->prev_status = aproxy->cur_status;
                    aproxy->cur_status = AUDIO_A2DP_STATUS_SUSPENDED;
                    ALOGI("proxy-%s: Transit to %s from %s", __func__,
                          a2dpstatus_table[aproxy->cur_status], a2dpstatus_table[aproxy->prev_status]);
                } else
                    ALOGE("proxy-%s: A2DP Stream did not suspended", __func__);
            } else {
                ALOGI("proxy-%s: Ignored as A2DP Status(%s)", __func__, a2dpstatus_table[aproxy->cur_status]);
                ret = -1;
            }
        } else {
            ret = a2dpHal->a2dp_clear_suspend_flag();
            if (ret == 0 && aproxy->cur_status == AUDIO_A2DP_STATUS_SUSPENDED) {
                aproxy->prev_status = aproxy->cur_status;
                aproxy->cur_status = AUDIO_A2DP_STATUS_STANDBY;
                ALOGI("proxy-%s: Transit to %s from %s", __func__,
                      a2dpstatus_table[aproxy->cur_status], a2dpstatus_table[aproxy->prev_status]);
            } else {
                ALOGI("proxy-%s: Ignored as A2DP Status(%s)", __func__, a2dpstatus_table[aproxy->cur_status]);
                ret = 0;
            }
        }
    }

    return ret;
}

int proxy_a2dp_open(void)
{
    struct a2dp_proxy *aproxy = getInstance();
    int ret = -1;

    if (aproxy) {
        android::sp<IExynosA2DPOffload> a2dpHal = getA2DPHal();
        if (a2dpHal == nullptr) {
            return ret;
        }

        if (aproxy->cur_status == AUDIO_A2DP_STATUS_INIT) {
            ret = a2dpHal->a2dp_open_stream();
            if (ret == 0) {
                aproxy->prev_status = aproxy->cur_status;
                aproxy->cur_status = AUDIO_A2DP_STATUS_STANDBY;
                ALOGI("proxy-%s: Transit to %s from %s", __func__,
                      a2dpstatus_table[aproxy->cur_status], a2dpstatus_table[aproxy->prev_status]);
            } else
                ALOGE("proxy-%s: A2DP Stream did not opened", __func__);
        } else
            ALOGE("proxy-%s: Abnormal A2DP Status(%s)", __func__, a2dpstatus_table[aproxy->cur_status]);
    }

    return ret;
}

int proxy_a2dp_close(void)
{
    struct a2dp_proxy *aproxy = getInstance();
    int ret = -1;

    if (aproxy) {
        android::sp<IExynosA2DPOffload> a2dpHal = getA2DPHal();
        if (a2dpHal == nullptr) {
            return ret;
        }

        if (aproxy->cur_status == AUDIO_A2DP_STATUS_STARTED) {
            ret = a2dpHal->a2dp_stop_stream();
            if (ret == 0) {
                aproxy->prev_status = aproxy->cur_status;
                aproxy->cur_status = AUDIO_A2DP_STATUS_STANDBY;
                ALOGI("proxy-%s: Transit to %s from %s", __func__,
                      a2dpstatus_table[aproxy->cur_status], a2dpstatus_table[aproxy->prev_status]);
            } else
                ALOGE("proxy-%s: A2DP Stream did not stopped", __func__);
        }

        ret = a2dpHal->a2dp_close_stream();
        if (ret == 0) {
            aproxy->prev_status = aproxy->cur_status;
            aproxy->cur_status = AUDIO_A2DP_STATUS_INIT;
            ALOGI("proxy-%s: Transit to %s from %s", __func__,
                  a2dpstatus_table[aproxy->cur_status], a2dpstatus_table[aproxy->prev_status]);
        } else
            ALOGE("proxy-%s: A2DP Stream did not closed", __func__);
    }

    return ret;
}

int proxy_a2dp_init(void)
{
    struct a2dp_proxy *aproxy = NULL;

    /* Creates the structure for a2dp_proxy */
    aproxy = getInstance();
    if (!aproxy) {
        ALOGE("proxy-%s: Failed to create for a2dp_proxy", __func__);
        return -1;
    }

    /* Initializes variables */
    aproxy->cur_status = AUDIO_A2DP_STATUS_INIT;
    aproxy->prev_status = AUDIO_A2DP_STATUS_NONE;
    ALOGI("proxy-%s: Transit to %s from %s", __func__,
          a2dpstatus_table[aproxy->cur_status], a2dpstatus_table[aproxy->prev_status]);

    return 0;
}

int proxy_a2dp_deinit(void)
{
    struct a2dp_proxy *aproxy = getInstance();

    if (aproxy) {
        aproxy->cur_status = AUDIO_A2DP_STATUS_NONE;
        ALOGI("proxy-%s: Transit to %s", __func__, a2dpstatus_table[aproxy->cur_status]);

        destroyInstance();
        ALOGI("proxy-%s: a2dp_proxy is destroyed", __func__);
        aproxy = NULL;
    }

    return 0;
}

