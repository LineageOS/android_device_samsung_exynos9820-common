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

#define LOG_TAG "audio_hw_proxy"
#define LOG_NDEBUG 0

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGD
#else
#define ALOGVV(a...) do { } while(0)
#endif

//#define SEAMLESS_DUMP

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <expat.h>

#include <log/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <audio_utils/channels.h>
#include <audio_utils/primitives.h>
#include <audio_utils/clock.h>
#include <hardware/audio.h>
#include <sound/asound.h>

#include "audio_proxy.h"
#include "audio_proxy_interface.h"
#include "audio_tables.h"
#include "audio_definition.h"
#include "audio_board_info.h"

#include "audio_usb_proxy_interface.h"
#ifdef SUPPORT_BTA2DP_OFFLOAD
#include "audio_a2dp_proxy.h"
#endif


/* Vendor Property Definitions */
#define NUM_EARPIECE_DEFAULT    "1"
#define NUM_EARPIECE_PROPERTY   "ro.vendor.config.num_earpiece"

#define NUM_SPEAKER_DEFAULT     "1"
#define NUM_SPEAKER_PROPERTY    "ro.vendor.config.num_speaker"

#define NUM_PROXIMITY_DEFAULT   "1"
#define NUM_PROXIMITY_PROPERTY  "ro.vendor.config.num_proximity"

#define SPEAKER_AMP_DEFAULT     "1"
#define SPEAKER_AMP_PROPERTY    "ro.vendor.config.speaker_amp"

#define BLUETOOTH_DEFAULT       "external"
#define BLUETOOTH_PROPERTY      "ro.vendor.config.bluetooth"

#define FMRADIO_DEFAULT         "external"
#define FMRADIO_PROPERTY        "ro.vendor.config.fmradio"

#define USBBYPRIMARY_DEFAULT    "no"
#define USBBYPRIMARY_PROPERTY   "ro.vendor.config.usb_by_primary"


/******************************************************************************/
/**                                                                          **/
/** Audio Proxy is Singleton                                                 **/
/**                                                                          **/
/******************************************************************************/

static struct audio_proxy *instance = NULL;

static struct audio_proxy* getInstance(void)
{
    if (instance == NULL) {
        instance = calloc(1, sizeof(struct audio_proxy));
        ALOGI("proxy-%s: created Audio Proxy Instance!", __func__);
    }
    return instance;
}

static void destroyInstance(void)
{
    if (instance) {
        free(instance);
        instance = NULL;
        ALOGI("proxy-%s: destroyed Audio Proxy Instance!", __func__);
    }
    return;
}

/******************************************************************************/
/**                                                                          **/
/** Utility Interfaces                                                       **/
/**                                                                          **/
/******************************************************************************/
int get_supported_device_number(void *proxy, int device_type)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    int ret = 0;

    switch (device_type) {
        case BUILTIN_EARPIECE:
            ret = aproxy->num_earpiece;
            break;

        case BUILTIN_SPEAKER:
            ret = aproxy->num_speaker;
            break;

        case BUILTIN_MIC:
            ret = aproxy->num_mic;
            break;

        case PROXIMITY_SENSOR:
            ret = aproxy->num_proximity;
            break;

        default:
            break;
    }

    return ret;
}

int  get_supported_config(void *proxy, int device_type)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    int ret = DEVICE_CONFIG_NONE;

    switch (device_type) {
        case DEVICE_BLUETOOTH:
            if (aproxy->bt_internal)
                ret = DEVICE_CONFIG_INTERNAL;
            else if (aproxy->bt_external)
                ret = DEVICE_CONFIG_EXTERNAL;
            break;

        case DEVICE_FMRADIO:
            if (aproxy->fm_internal)
                ret = DEVICE_CONFIG_INTERNAL;
            else if (aproxy->fm_external)
                ret = DEVICE_CONFIG_EXTERNAL;
            break;

        default:
            break;
    }

    return ret;
}

bool is_needed_config(void *proxy, int config_type)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    bool ret = false;

    switch (config_type) {
        case NEED_VOICEPCM_REOPEN:
            if (aproxy->btsco_playback)
                ret = true;
            break;

        case SUPPORT_USB_BY_PRIMARY:
            if (aproxy->usb_by_primary)
                ret = true;
            break;

        default:
            break;
    }

    return ret;
}

bool is_active_usage_CPCall(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    if (aproxy->active_playback_ausage >= AUSAGE_CPCALL_MIN &&
        aproxy->active_playback_ausage <= AUSAGE_CPCALL_MAX)
        return true;
    else
        return false;
}

bool is_usage_CPCall(audio_usage ausage)
{
    if (ausage >= AUSAGE_CPCALL_MIN && ausage <= AUSAGE_CPCALL_MAX)
        return true;
    else
        return false;
}

bool is_active_usage_APCall(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    if (aproxy->active_playback_ausage >= AUSAGE_APCALL_MIN &&
        aproxy->active_playback_ausage <= AUSAGE_APCALL_MAX)
        return true;
    else
        return false;
}

bool is_usage_APCall(audio_usage ausage)
{
    if (ausage >= AUSAGE_APCALL_MIN && ausage <= AUSAGE_APCALL_MAX)
        return true;
    else
        return false;
}

bool is_usage_Call(audio_usage ausage)
{
    if (ausage >= AUSAGE_CPCALL_MIN && ausage <= AUSAGE_CPCALL_MAX)
        return true;
    else if (ausage >= AUSAGE_APCALL_MIN && ausage <= AUSAGE_APCALL_MAX)
        return true;
    else
        return false;
}

bool is_usage_Loopback(audio_usage ausage)
{
    // AUSAGE_LOOPBACK == min, AUSAGE_LOOPBACK_CODEC == max
    if (ausage >= AUSAGE_LOOPBACK && ausage <= AUSAGE_LOOPBACK_CODEC)
        return true;
    else
        return false;
}

bool is_usb_connected(void)
{
    struct audio_proxy *aproxy = getInstance();

    if (proxy_is_usb_playback_device_connected(aproxy->usb_aproxy))
        return true;
    else
        return false;
}

#ifdef SUPPORT_BTA2DP_OFFLOAD
bool proxy_is_bt_a2dp_ready(void)
{
    struct audio_proxy *aproxy = getInstance();

    // bt offload enabled and not suspend state
    if (aproxy && aproxy->a2dp_out_enabled) {
        if (!proxy_a2dp_is_suspended())
            return true;
    }

    return false;
}

static const audio_format_t AUDIO_FORMAT_SEC_BT_A2DP_OFFLOAD = (audio_format_t)0x200000u;
static inline bool audio_is_bt_offload_format(audio_format_t format){
     if ((format & AUDIO_FORMAT_SEC_BT_A2DP_OFFLOAD) == AUDIO_FORMAT_SEC_BT_A2DP_OFFLOAD) {
         return true;
     }
     return false;

}
#endif

void update_usb_clksource_info(bool flag)
{
    struct audio_proxy *aproxy = getInstance();
    struct mixer_ctl *ctrl = NULL;
    int ret = 0;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    // set usb device clock info if flag is true and usb connected
    if (flag) {
        /* USB Clock Source information Mixer control */
        ctrl = mixer_get_ctl_by_name(aproxy->mixer, MIXER_CTL_ABOX_USB_CLOCKSOURCE);
        if (ctrl) {
            ret = mixer_ctl_get_value(ctrl, 0);
            if (ret < 0) {
                ALOGE("proxy-%s: failed to get %s %d", __func__, MIXER_CTL_ABOX_USB_CLOCKSOURCE, ret);
            } else {
                aproxy->is_usb_single_clksrc = ret;
                ALOGI("proxy-%s: get USB Device ClockSource information %d",
                    __func__, aproxy->is_usb_single_clksrc);
            }
        } else {
            ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, MIXER_CTL_ABOX_USB_CLOCKSOURCE);
        }
    } else {
        // reset usb device clock info when usb disconnected
        aproxy->is_usb_single_clksrc = false;
        ALOGI("proxy-%s: reset USB Device ClockSource information %d",
            __func__, aproxy->is_usb_single_clksrc);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return;
}

bool is_usb_single_clksource()
{
    struct audio_proxy *aproxy = getInstance();

    return aproxy->is_usb_single_clksrc;
}

/******************************************************************************/
/**                                                                          **/
/** Local Fuctions for Audio Device Proxy                                    **/
/**                                                                          **/
/******************************************************************************/

static audio_format_t get_pcmformat_from_alsaformat(enum pcm_format pcmformat)
{
    audio_format_t format = AUDIO_FORMAT_PCM_16_BIT;

    switch (pcmformat) {
        case PCM_FORMAT_S16_LE:
            format = AUDIO_FORMAT_PCM_16_BIT;
            break;
        case PCM_FORMAT_S32_LE:
            format = AUDIO_FORMAT_PCM_32_BIT;
            break;
        case PCM_FORMAT_S8:
            format = AUDIO_FORMAT_PCM_8_BIT;
            break;
        case PCM_FORMAT_S24_LE:
        case PCM_FORMAT_S24_3LE:
            format = AUDIO_FORMAT_PCM_8_24_BIT;
            break;
        case PCM_FORMAT_INVALID:
        case PCM_FORMAT_MAX:
            format = AUDIO_FORMAT_PCM_16_BIT;
            break;
    }

    return format;
}

static bool is_playback_device_bt(device_type device)
{
    if (device == DEVICE_BT_HEADSET || device == DEVICE_SPEAKER_AND_BT_HEADSET
#ifdef SUPPORT_BTA2DP_OFFLOAD
        || device == DEVICE_BT_A2DP_HEADPHONE || device == DEVICE_SPEAKER_AND_BT_A2DP_HEADPHONE
#endif
    )
        return true;
    else
        return false;
}

static bool is_playback_device_speaker_dualpath(device_type device)
{
    if (device == DEVICE_SPEAKER_AND_HEADSET ||
        device == DEVICE_SPEAKER_AND_HEADPHONE ||
        device == DEVICE_SPEAKER_AND_BT_HEADSET ||
        device == DEVICE_SPEAKER_AND_USB_HEADSET
#ifdef SUPPORT_BTA2DP_OFFLOAD
        || device == DEVICE_SPEAKER_AND_BT_A2DP_HEADPHONE
#endif
    )
        return true;
    else
        return false;
}

#ifdef SUPPORT_BTA2DP_OFFLOAD
static bool is_active_playback_device_bta2dp(struct audio_proxy *aproxy)
{
    if (aproxy->active_playback_device == DEVICE_BT_A2DP_HEADPHONE ||
        aproxy->active_playback_device == DEVICE_SPEAKER_AND_BT_A2DP_HEADPHONE)
        return true;
    else
        return false;
}

static bool is_playback_device_bta2dp(device_type device)
{
    if (device == DEVICE_BT_A2DP_HEADPHONE || device == DEVICE_SPEAKER_AND_BT_A2DP_HEADPHONE)
        return true;
    else
        return false;
}
#endif

static bool is_device_speaker(device_type device)
{
    if (device < DEVICE_MAIN_MIC) {
        if (device == DEVICE_SPEAKER
#ifdef SEC_AUDIO_SUPPORT_GAMECHAT_SPK_AEC
                || (device == DEVICE_SPEAKER_GAMING)
#endif
                || (device == DEVICE_SPEAKER_DEX)) {
            return true;
        }
        return false;
    } else {
        if (device == DEVICE_SPEAKER_MIC
#ifdef SEC_AUDIO_SUPPORT_GAMECHAT_SPK_AEC
                || (device == DEVICE_SPEAKER_GAMING_MIC)
#endif
                || (device == DEVICE_SPEAKER_DEX_MIC)) {
            return true;
        }
        return false;
    }
}

static bool is_usb_mic_device(device_type device)
{
    return (device == DEVICE_USB_HEADSET_MIC /*||
                device == DEVICE_USB_FULL_MIC ||
                device == DEVICE_USB_HCO_MIC*/);
}

#ifdef SUPPORT_QUAD_MIC
static bool is_quad_mic_device(device_type device)
{
    struct audio_proxy *aproxy = getInstance();
    bool flag = false;

    if (device == DEVICE_QUAD_MIC)
        flag = true;
    else if (is_usage_CPCall(aproxy->active_capture_ausage) ||
            is_usage_APCall(aproxy->active_capture_ausage))
        flag = (device == DEVICE_MAIN_MIC ||
                device == DEVICE_HANDSET_MIC ||
                device == DEVICE_HEADPHONE_MIC ||
                device == DEVICE_SPEAKER_MIC ||
                device == DEVICE_SPEAKER_DEX_MIC /*||
                device == DEVICE_SPEAKER_GAMING_MIC*/);

    return flag;
}
#endif

// If there are specific device number in mixer_paths.xml, it get the specific device number from mixer_paths.xml
static int get_pcm_device_number(void *proxy, void *proxy_stream)
{
    struct audio_proxy *aproxy = proxy;
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int pcm_device_number = -1;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);
    if (apstream) {
        switch(apstream->stream_type) {
            case ASTREAM_PLAYBACK_PRIMARY:
                pcm_device_number = PRIMARY_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_FAST:
                pcm_device_number = FAST_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_LOW_LATENCY:
                pcm_device_number = LOW_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_DEEP_BUFFER:
                pcm_device_number = DEEP_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_COMPR_OFFLOAD:
                pcm_device_number = OFFLOAD_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_MMAP:
                pcm_device_number = MMAP_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_AUX_DIGITAL:
                pcm_device_number = AUX_PLAYBACK_DEVICE;
                break;

            case ASTREAM_PLAYBACK_DIRECT:
                pcm_device_number = DIRECT_PLAYBACK_DEVICE;
                break;

            case ASTREAM_CAPTURE_PRIMARY:
                pcm_device_number = PRIMARY_CAPTURE_DEVICE;
                break;

            case ASTREAM_CAPTURE_CALL:
                pcm_device_number = CALL_RECORD_DEVICE;
                break;

            case ASTREAM_CAPTURE_LOW_LATENCY:
                pcm_device_number = LOW_CAPTURE_DEVICE;
                break;

            case ASTREAM_CAPTURE_MMAP:
                pcm_device_number = MMAP_CAPTURE_DEVICE;
                break;

            case ASTREAM_CAPTURE_FM_TUNER:
            case ASTREAM_CAPTURE_FM_RECORDING:
                pcm_device_number = FM_RECORD_DEVICE;
                break;

            default:
                break;
        }
    } else {
    }
    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return pcm_device_number;
}

/*
 * Internal Path Control Functions for A-Box
 */
static void disable_erap_in(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_out_loopback) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 ERAP_IN_CARD, ERAP_IN_DEVICE, 'c');

        /* Disables ERAP In Path */
        if (aproxy->erap_in) {
            pcm_stop(aproxy->erap_in);
            pcm_close(aproxy->erap_in);
            aproxy->erap_in = NULL;

            ALOGI("proxy-%s: ERAP In PCM Device(%s) is stopped & closed!", __func__, pcm_path);
        }
    }

    return ;
}

static void enable_erap_in(void *proxy, device_type target_device)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig = pcm_config_erap_in;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_out_loopback) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 ERAP_IN_CARD, ERAP_IN_DEVICE, 'c');

        /* Enables ERAP In Path */
        if (aproxy->erap_in == NULL) {
            /* If target device is USB Headset, then loopback path's PCM channels should be
             * matched with USB device supported channels */
            if (target_device == DEVICE_SPEAKER_AND_USB_HEADSET) {
                pcmconfig.channels = proxy_usb_get_playback_channels(aproxy->usb_aproxy);
                /* check if connected USB headset's highest channel count is 6, then forcelly
                 * change it to 8 channels as A-Box HW cannot support 6 channel conversion */
                if (pcmconfig.channels == ABOX_UNSUPPORTED_CHANNELS) {
                    ALOGI("proxy-%s: supported CH is(%d) Changed to (%d)", __func__, pcmconfig.channels,
                        ABOX_SUPPORTED_MAX_CHANNELS);
                    pcmconfig.channels = ABOX_SUPPORTED_MAX_CHANNELS;
                }
                ALOGI("proxy-%s: ERAP In USB Device channels updated as CC(%d)",
                  __func__, pcmconfig.channels);
            }
#ifdef SUPPORT_QUAD_MIC
            else if (target_device == DEVICE_CALL_FWD) {
                pcmconfig.channels = MEDIA_4_CHANNELS;
                ALOGI("proxy-%s: Call-forwarding/spectro ERAP In channels fixed to (%d)", __func__, pcmconfig.channels);
            }
#endif
            aproxy->erap_in = pcm_open(ERAP_IN_CARD, ERAP_IN_DEVICE,
                                       PCM_IN | PCM_MONOTONIC, &pcmconfig);
            if (aproxy->erap_in && !pcm_is_ready(aproxy->erap_in)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("proxy-%s: ERAP In PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->erap_in));
                goto err_open;
            }
            ALOGVV("proxy-%s: ERAP In PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

            if (pcm_start(aproxy->erap_in) == 0) {
                ALOGI("proxy-%s: ERAP In PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: ERAP In PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->erap_in));
                goto err_open;
            }
        }
    }

    return ;

err_open:
    disable_erap_in(proxy);
    return ;
}

static void disable_voice_tx_direct_in(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->call_tx_direct) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VC_FMRADIO_CAPTURE_CARD, VC_FMRADIO_CAPTURE_DEVICE, 'c');

        pcm_stop(aproxy->call_tx_direct);
        pcm_close(aproxy->call_tx_direct);
        aproxy->call_tx_direct= NULL;
        ALOGI("proxy-%s: Voice Call TX Direct PCM Device(%s) is stopped & closed!", __func__, pcm_path);
    }

    return;
}

static void enable_voice_tx_direct_in(void *proxy, device_type target_device __unused)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->call_tx_direct== NULL) {
#ifdef SUPPORT_QUAD_MIC
        if (is_quad_mic_device(target_device)) {
            pcmconfig = pcm_config_vc_quad_mic_capture;
            ALOGI("proxy-%s: Quad-Mic config for Voice Call TX Direct ", __func__);
        } else
#endif
            pcmconfig = pcm_config_vc_fmradio_capture;
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VC_FMRADIO_CAPTURE_CARD, VC_FMRADIO_CAPTURE_DEVICE, 'c');

        aproxy->call_tx_direct = pcm_open(VC_FMRADIO_CAPTURE_CARD,
                                        VC_FMRADIO_CAPTURE_DEVICE,
                                        PCM_IN | PCM_MONOTONIC, &pcmconfig);
        if (aproxy->call_tx_direct && !pcm_is_ready(aproxy->call_tx_direct)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("proxy-%s: Voice Call TX Direct PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_tx_direct));
            goto err_open;
        }
        ALOGVV("proxy-%s: Voice Call TX Direct PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
              __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

        if (pcm_start(aproxy->call_tx_direct) == 0) {
            ALOGI("proxy-%s: Voice Call TX Direct PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
        } else {
            ALOGE("proxy-%s: Voice Call TX Direct PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_tx_direct));
            goto err_open;
        }
    }

    return;
err_open:
    disable_voice_tx_direct_in(proxy);
}

static void disable_usb_out_loopback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_usb_out_loopback) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 USBOUT_LOOPBACK_CARD, USBOUT_LOOPBACK_DEVICE, 'c');

        /* Disables USB Out Loopback Path */
        if (aproxy->usb_out_loopback) {
            pcm_stop(aproxy->usb_out_loopback);
            pcm_close(aproxy->usb_out_loopback);
            aproxy->usb_out_loopback = NULL;

            ALOGI("proxy-%s: USBOut Loopback PCM Device(%s) is stopped & closed!", __func__, pcm_path);
        }
    }

    return ;
}

static void enable_usb_out_loopback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig = pcm_config_usb_out_loopback;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_usb_out_loopback) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 USBOUT_LOOPBACK_CARD, USBOUT_LOOPBACK_DEVICE, 'c');

        /* Enables USB Out Loopback path */
        if (aproxy->usb_out_loopback == NULL) {
            // Updates PCM Configuration same as USB PCM Configuration
            pcmconfig.rate = proxy_usb_get_playback_samplerate(aproxy->usb_aproxy);
            pcmconfig.channels = proxy_usb_get_playback_channels(aproxy->usb_aproxy);
            /* A-Box limitation all DMA buffer size should be multiple of 16
               therefore Period Size(Frame Count) is rounded of to nearest 4 multiple */
            pcmconfig.period_size = ((pcmconfig.rate * PREDEFINED_USB_PLAYBACK_DURATION) / 1000) & ~0x3;
            pcmconfig.format = proxy_usb_get_playback_format(aproxy->usb_aproxy);

            /* check if connected USB headset's channel count is 6, then forcelly
              * change it to 8 channels as A-Box HW cannot support 6 channel conversion */
            if (pcmconfig.channels == ABOX_UNSUPPORTED_CHANNELS) {
                ALOGI("proxy-%s: supported CH is(%d) Changed to (%d)", __func__, pcmconfig.channels,
                    ABOX_SUPPORTED_MAX_CHANNELS);
                pcmconfig.channels = ABOX_SUPPORTED_MAX_CHANNELS;
            }

            /* PCM_FORMAT_S24_3LE (24bit packed) format is not supported by A-Box hardware
             * therefore forcefully change the format to PCM_FORMAT_S24_LE */
            if (pcmconfig.format == PCM_FORMAT_S24_3LE) {
                ALOGI("proxy-%s: USB Format is forcefully changed 24bit packed -> 24bit padded", __func__);
                pcmconfig.format = PCM_FORMAT_S24_LE;
            }

            aproxy->usb_out_loopback = pcm_open(USBOUT_LOOPBACK_CARD, USBOUT_LOOPBACK_DEVICE,
                                               PCM_IN | PCM_MONOTONIC, &pcmconfig);
            if (aproxy->usb_out_loopback && !pcm_is_ready(aproxy->usb_out_loopback)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("proxy-%s: USBOut Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->usb_out_loopback));
                goto err_open;
            }
            ALOGI("proxy-%s: USBOut Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) PdSz(%d) PdCnt(%d) is opened",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcmconfig.period_size, pcmconfig.period_count);

            if (pcm_start(aproxy->usb_out_loopback) == 0) {
                ALOGI("proxy-%s: USBOut Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: USBOut Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->usb_out_loopback));
                goto err_open;
            }
        }
    }

    return ;

err_open:
    disable_usb_out_loopback(proxy);
    return ;
}


static void disable_usb_in_loopback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_usb_in_loopback) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 USBIN_LOOPBACK_CARD, USBIN_LOOPBACK_DEVICE, 'p');

        /* Disables USB In Loopback Path */
        if (aproxy->usb_in_loopback) {
            pcm_stop(aproxy->usb_in_loopback);
            pcm_close(aproxy->usb_in_loopback);
            aproxy->usb_in_loopback = NULL;

            ALOGI("proxy-%s: USBIn Loopback PCM Device(%s) is stopped & closed!", __func__, pcm_path);
        }
    }

    return ;
}

static void enable_usb_in_loopback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig = pcm_config_usb_in_loopback;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_usb_in_loopback) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 USBIN_LOOPBACK_CARD, USBIN_LOOPBACK_DEVICE, 'p');

        /* Enables USB In Loopback path */
        if (aproxy->usb_in_loopback == NULL) {
            // Updates PCM Configuration same as USB PCM Configuration
            pcmconfig.rate = proxy_usb_get_capture_samplerate(aproxy->usb_aproxy);
            pcmconfig.channels = proxy_usb_get_capture_channels(aproxy->usb_aproxy);
            /* A-Box limitation all DMA buffer size should be multiple of 16
               therefore Period Size(Frame Count) is rounded of to nearest 4 multiple */
            pcmconfig.period_size = ((pcmconfig.rate * PREDEFINED_USB_PLAYBACK_DURATION) / 1000) & ~0x3;
            pcmconfig.format = proxy_usb_get_capture_format(aproxy->usb_aproxy);

            /* check if connected USB headset's channel count is 6, then forcelly
              * change it to 8 channels as A-Box HW cannot support 6 channel conversion */
            if (pcmconfig.channels == ABOX_UNSUPPORTED_CHANNELS) {
                ALOGI("proxy-%s: supported CH is(%d) Changed to (%d)", __func__, pcmconfig.channels,
                    ABOX_SUPPORTED_MAX_CHANNELS);
                pcmconfig.channels = ABOX_SUPPORTED_MAX_CHANNELS;
            }

            /* PCM_FORMAT_S24_3LE (24bit packed) format is not supported by A-Box hardware
             * therefore forcefully change the format to PCM_FORMAT_S24_LE */
            if (pcmconfig.format == PCM_FORMAT_S24_3LE) {
                ALOGI("proxy-%s: USB Format is forcefully changed from 24bit packed -> 24bit padded", __func__);
                pcmconfig.format = PCM_FORMAT_S24_LE;
            }

            aproxy->usb_in_loopback = pcm_open(USBIN_LOOPBACK_CARD, USBIN_LOOPBACK_DEVICE,
                                               PCM_OUT | PCM_MONOTONIC, &pcmconfig);
            if (aproxy->usb_in_loopback && !pcm_is_ready(aproxy->usb_in_loopback)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("proxy-%s: USBIn Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->usb_in_loopback));
                goto err_open;
            }
            ALOGI("proxy-%s: USBIn Loopback PCM Device(%s) with SR(%u)PF(%d) CC(%d) PdSz(%d) PdCnt(%d) is opened",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcmconfig.period_size, pcmconfig.period_count);

            if (pcm_start(aproxy->usb_in_loopback) == 0) {
                ALOGI("proxy-%s: USBIn Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: USBIn Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->usb_in_loopback));
                goto err_open;
            }
        }
    }

    return ;

err_open:
    disable_usb_in_loopback(proxy);
    return ;
}

static void disable_spkamp_reference(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_spkamp) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 SPKAMP_REFERENCE_CARD, SPKAMP_REFERENCE_DEVICE, 'c');

        /* Disables Speaker AMP Reference Path */
        if (aproxy->spkamp_reference) {
            pcm_stop(aproxy->spkamp_reference);
            pcm_close(aproxy->spkamp_reference);
            aproxy->spkamp_reference = NULL;

            ALOGI("proxy-%s: SPKAMP Reference PCM Device(%s) is stopped & closed!", __func__, pcm_path);
        }
    }

    return ;
}

static void enable_spkamp_reference(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig = pcm_config_spkamp_reference;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_spkamp) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 SPKAMP_REFERENCE_CARD, SPKAMP_REFERENCE_DEVICE, 'c');

#ifndef SUPPORT_QUAD_MIC
        if (is_active_usage_CPCall(aproxy)) {
            pcmconfig.channels = 2;
            ALOGI("proxy-%s: RCV Reference Channel set as 2CH", __func__);
        }
#endif

        /* Enables Speaker AMP Reference Path */
        if (aproxy->spkamp_reference == NULL) {
            aproxy->spkamp_reference = pcm_open(SPKAMP_REFERENCE_CARD, SPKAMP_REFERENCE_DEVICE,
                                                PCM_IN | PCM_MONOTONIC, &pcmconfig);
            if (aproxy->spkamp_reference && !pcm_is_ready(aproxy->spkamp_reference)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("proxy-%s: SPKAMP Reference PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->spkamp_reference));
                goto err_open;
            }
            ALOGVV("proxy-%s: SPKAMP Reference PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

            if (pcm_start(aproxy->spkamp_reference) == 0) {
                ALOGI("proxy-%s: SPKAMP Reference PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: SPKAMP Reference PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->spkamp_reference));
                goto err_open;
            }
        }
    }

    return ;

err_open:
    disable_spkamp_reference(proxy);
    return ;
}

static void disable_spkamp_playback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_spkamp && ENABLE_SPKAMP_PLAYBACK) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 SPKAMP_PLAYBACK_CARD, SPKAMP_PLAYBACK_DEVICE, 'p');

        /* Disables Speaker AMP Playback Path */
        if (aproxy->spkamp_playback) {
            pcm_stop(aproxy->spkamp_playback);
            pcm_close(aproxy->spkamp_playback);
            aproxy->spkamp_playback = NULL;

            ALOGI("proxy-%s: SPKAMP Playback PCM Device(%s) is stopped & closed!", __func__, pcm_path);
        }
    }

    return ;
}

static void enable_spkamp_playback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig = pcm_config_spkamp_playback;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_spkamp && ENABLE_SPKAMP_PLAYBACK) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 SPKAMP_PLAYBACK_CARD, SPKAMP_PLAYBACK_DEVICE, 'p');

        /* Enables Speaker AMP Playback path */
        if (aproxy->spkamp_playback == NULL) {
            aproxy->spkamp_playback = pcm_open(SPKAMP_PLAYBACK_CARD, SPKAMP_PLAYBACK_DEVICE,
                                               PCM_OUT | PCM_MONOTONIC, &pcmconfig);
            if (aproxy->spkamp_playback && !pcm_is_ready(aproxy->spkamp_playback)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("proxy-%s: SPKAMP Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->spkamp_playback));
                goto err_open;
            }
            ALOGVV("proxy-%s: SPKAMP Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

            if (pcm_start(aproxy->spkamp_playback) == 0) {
                ALOGI("proxy-%s: SPKAMP Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: SPKAMP Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->spkamp_playback));
                goto err_open;
            }
        }
    }

    return ;

err_open:
    disable_spkamp_playback(proxy);
    return ;
}

static void disable_btsco_playback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_btsco) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 BTSCO_PLAYBACK_CARD, BTSCO_PLAYBACK_DEVICE, 'p');

        /* Disables BT-SCO Playback Path */
        if (aproxy->btsco_playback) {
            pcm_stop(aproxy->btsco_playback);
            pcm_close(aproxy->btsco_playback);
            aproxy->btsco_playback = NULL;

            ALOGI("proxy-%s: BTSCO Playback PCM Device(%s) is stopped & closed!", __func__, pcm_path);
        }
    }

    return ;
}

static void enable_btsco_playback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig = pcm_config_btsco_playback;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_btsco) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 BTSCO_PLAYBACK_CARD, BTSCO_PLAYBACK_DEVICE, 'p');

        /* Enables BT-SCO Playback Path */
        if (aproxy->btsco_playback == NULL) {
            aproxy->btsco_playback = pcm_open(BTSCO_PLAYBACK_CARD, BTSCO_PLAYBACK_DEVICE,
                                              PCM_OUT | PCM_MONOTONIC, &pcmconfig);
            if (aproxy->btsco_playback && !pcm_is_ready(aproxy->btsco_playback)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("proxy-%s: BTSCO Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->btsco_playback));
                goto err_open;
            }
            ALOGVV("proxy-%s: BTSCO Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

            if (pcm_start(aproxy->btsco_playback) == 0) {
                ALOGI("proxy-%s: BTSCO Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: BTSCO Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->btsco_playback));
                goto err_open;
            }
        }
    }

    return ;

err_open:
    disable_btsco_playback(proxy);
    return ;
}

#ifdef SUPPORT_BTA2DP_OFFLOAD
static void disable_bta2dp_out_loopback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_bta2dp) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 BTA2DP_OUT_LOOPBACK_CARD, BTA2DP_OUT_LOOPBACK_DEVICE, 'c');

        /* Disables BT-A2DP Out Loopback Path */
        if (aproxy->bta2dp_out_loopback) {
            snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                     BTA2DP_OUT_LOOPBACK_CARD, BTA2DP_OUT_LOOPBACK_DEVICE, 'c');
            pcm_stop(aproxy->bta2dp_out_loopback);
            pcm_close(aproxy->bta2dp_out_loopback);
            aproxy->bta2dp_out_loopback = NULL;

            ALOGI("proxy-%s: BT A2DP Out Loopback PCM Device(%s) is stopped & closed!", __func__, pcm_path);
        }
    }

    return;
}

static void enable_bta2dp_out_loopback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig = pcm_config_bta2dp_out_loopback;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_bta2dp) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 BTA2DP_OUT_LOOPBACK_CARD, BTA2DP_OUT_LOOPBACK_DEVICE, 'c');

        /* Enables BT-A2DP Out Loopback Path */
        if (aproxy->bta2dp_out_loopback == NULL) {
            aproxy->bta2dp_out_loopback = pcm_open(BTA2DP_OUT_LOOPBACK_CARD, BTA2DP_OUT_LOOPBACK_DEVICE,
                                                 PCM_IN | PCM_MONOTONIC, &pcmconfig);
            if (aproxy->bta2dp_out_loopback && !pcm_is_ready(aproxy->bta2dp_out_loopback)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("proxy-%s: BT A2DP Out Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->bta2dp_out_loopback));
                goto err_open;
            }
            ALOGI("proxy-%s: BT A2DP Out Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

            if (pcm_start(aproxy->bta2dp_out_loopback) == 0) {
                ALOGI("proxy-%s: BT A2DP Out Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is started",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: BT A2DP Out Loopback PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->bta2dp_out_loopback));
                goto err_open;
            }
        }
    }

    return;

err_open:
    disable_bta2dp_out_loopback(aproxy);
}

static void disable_bta2dp_playback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_bta2dp) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 BTA2DP_PLAYBACK_CARD, BTA2DP_PLAYBACK_DEVICE, 'p');

        /* Disables BT-SCO Playback Path */
        if (aproxy->bta2dp_playback) {
            pcm_stop(aproxy->bta2dp_playback);
            pcm_close(aproxy->bta2dp_playback);
            aproxy->bta2dp_playback = NULL;

            ALOGI("proxy-%s: BTA2DP Playback PCM Device(%s) is stopped & closed!", __func__, pcm_path);
        }
    }

    return ;
}

static void enable_bta2dp_playback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig = pcm_config_bta2dp_playback;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_bta2dp) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 BTA2DP_PLAYBACK_CARD, BTA2DP_PLAYBACK_DEVICE, 'p');

        /* Enables BT-SCO Playback Path */
        if (aproxy->bta2dp_playback == NULL) {
            aproxy->bta2dp_playback = pcm_open(BTA2DP_PLAYBACK_CARD, BTA2DP_PLAYBACK_DEVICE,
                                              PCM_OUT | PCM_MONOTONIC, &pcmconfig);
            if (aproxy->bta2dp_playback && !pcm_is_ready(aproxy->bta2dp_playback)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("proxy-%s: BTA2DP Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->bta2dp_playback));
                goto err_open;
            }
            ALOGI("proxy-%s: BTA2DP Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

            if (pcm_start(aproxy->bta2dp_playback) == 0) {
                ALOGI("proxy-%s: BTA2DP Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is started",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: BTA2DP Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->bta2dp_playback));
                goto err_open;
            }
        }
    }

    return ;

err_open:
    disable_bta2dp_playback(proxy);
}

/* modified by samsung convgergence */
void set_a2dp_suspend_mixer(int a2dp_suspend)
{
    struct audio_proxy *aproxy = getInstance();
    uint32_t value[MIXER_CTL_ABOX_A2DP_SUSPEND_PARAMS_CNT] = {0, };

    ALOGI("proxy-%s: a2dp-suspend[%d]", __func__, a2dp_suspend);

    value[0] = a2dp_suspend;

    proxy_set_mixer_value_array(aproxy, MIXER_CTL_ABOX_A2DP_SUSPEND_PARAMS, value,
                                MIXER_CTL_ABOX_A2DP_SUSPEND_PARAMS_CNT);

    /* Forcefully disonnect A2DP to RDMA6 connection to fix BTSCO switching */
    if (is_active_playback_device_bta2dp(aproxy)) {
        if (a2dp_suspend == MIXER_ON) {
            proxy_set_mixer_value_string(aproxy, "ABOX SPUS OUT6", "RESERVED");
            ALOGI("proxy-%s: set ABOX SPUS OUT6 to RESERVED", __func__);
            proxy_set_mixer_value_string(aproxy, "ABOX SIFS2", "RESERVED");
            ALOGI("proxy-%s: set ABOX SIFS2 to RESERVED", __func__);
        } else {
            proxy_set_mixer_value_string(aproxy, "ABOX SPUS OUT6", "SIFS2");
            ALOGI("proxy-%s: set ABOX SPUS OUT6 to SIFS2", __func__);
            proxy_set_mixer_value_string(aproxy, "ABOX SIFS2", "SPUS OUT6");
            ALOGI("proxy-%s: set ABOX SIFS2 to SPUS OUT6", __func__);
        }
    }
}
#endif

// Specific Mixer Control Functions for Internal Loopback Handling
void proxy_set_mixercontrol(struct audio_proxy *aproxy, erap_trigger type, int value)
{
    struct mixer_ctl *ctrl = NULL;
    char mixer_name[MAX_MIXER_NAME_LEN];
    int ret = 0, val = value;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    if (type == MUTE_CONTROL) {
        ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_MUTE_CONTROL_NAME);
        snprintf(mixer_name, sizeof(mixer_name), ABOX_MUTE_CONTROL_NAME);
    } else if (type == TICKLE_CONTROL) {
        ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_TICKLE_CONTROL_NAME);
        snprintf(mixer_name, sizeof(mixer_name), ABOX_TICKLE_CONTROL_NAME);
    }

    if (ctrl) {
        ret = mixer_ctl_set_value(ctrl, 0,val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set Mixer Control(%s)", __func__, mixer_name);
        else
            ALOGI("proxy-%s: set Mixer Control(%s) to %d", __func__, mixer_name, val);
    } else {
        ALOGE("proxy-%s: cannot find Mixer Control", __func__);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Enable usb playback new Modifier */
static void set_usb_playback_modifier(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret, val = 0;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    /* Mixer out sample rate configuration */
    ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_SAMPLE_RATE_MIXER_NAME);
    if (ctrl) {
        val = proxy_usb_get_playback_samplerate(aproxy->usb_aproxy);
        ALOGI("proxy-%s: configured SR(%d)", __func__, val);
        ret = mixer_ctl_set_value(ctrl, 0, val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, ABOX_SAMPLE_RATE_MIXER_NAME);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, ABOX_SAMPLE_RATE_MIXER_NAME);
    }

    /* Mixer out channels configuration */
    ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_CHANNELS_MIXER_NAME);
    if (ctrl) {
        val = proxy_usb_get_playback_channels(aproxy->usb_aproxy);
        /* check if connected USB headset's highest channel count is 6, then forcelly
          * change it to 8 channels as A-Box HW cannot support 6 channel conversion */
        if (val == ABOX_UNSUPPORTED_CHANNELS) {
            ALOGI("proxy-%s: supported CH is(%d) Changed to (%d)", __func__, val,
                ABOX_SUPPORTED_MAX_CHANNELS);
            val = ABOX_SUPPORTED_MAX_CHANNELS;
        }
        ALOGI("proxy-%s: configured CH(%d)", __func__, val);
        ret = mixer_ctl_set_value(ctrl, 0, val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, ABOX_CHANNELS_MIXER_NAME);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, ABOX_CHANNELS_MIXER_NAME);
    }

    /* Mixer out bit width configuration */
    ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_BIT_WIDTH_MIXER_NAME);
    if (ctrl) {
        val = proxy_usb_get_playback_bitwidth(aproxy->usb_aproxy);
        ALOGI("proxy-%s: configured BW(%d)", __func__, val);
        ret = mixer_ctl_set_value(ctrl, 0, val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, ABOX_BIT_WIDTH_MIXER_NAME);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, ABOX_BIT_WIDTH_MIXER_NAME);
    }

#ifdef SUPPORT_DIRECT_RCVSPK_PATH
        /*
         * SIFS0 switch control is required to reconfigure all running DMA ASRC configurations
        */
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS0_SWITCH, MIXER_OFF);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS0_SWITCH, MIXER_ON);
        ALOGI("proxy-%s: control SIFS0 Off/On", __func__);
#endif

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Resset Modifier to default values */
static void reset_playback_modifier(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret, val = 0;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    /* Mixer out sample rate configuration */
    ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_SAMPLE_RATE_MIXER_NAME);
    if (ctrl) {
        val = DEFAULT_MEDIA_SAMPLING_RATE;
        ALOGI("proxy-%s: configured SR(%d)", __func__, val);
        ret = mixer_ctl_set_value(ctrl, 0, val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, ABOX_SAMPLE_RATE_MIXER_NAME);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, ABOX_SAMPLE_RATE_MIXER_NAME);
    }

    /* Mixer out channels configuration */
    ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_CHANNELS_MIXER_NAME);
    if (ctrl) {
        val = DEFAULT_MEDIA_CHANNELS;
        ALOGI("proxy-%s: configured CH(%d)", __func__, val);
        ret = mixer_ctl_set_value(ctrl, 0, val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, ABOX_CHANNELS_MIXER_NAME);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, ABOX_CHANNELS_MIXER_NAME);
    }

    /* Mixer out bit width configuration */
    ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_BIT_WIDTH_MIXER_NAME);
    if (ctrl) {
        val = DEFAULT_MEDIA_BITWIDTH;
        ALOGI("proxy-%s: configured BW(%d)", __func__, val);
        ret = mixer_ctl_set_value(ctrl, 0, val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, ABOX_BIT_WIDTH_MIXER_NAME);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, ABOX_BIT_WIDTH_MIXER_NAME);
    }

#ifdef SUPPORT_DIRECT_RCVSPK_PATH
    /*
     * SIFS0 switch control is required to reconfigure all running DMA ASRC configurations
    */
    proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS0_SWITCH, MIXER_OFF);
    proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS0_SWITCH, MIXER_ON);
    ALOGI("proxy-%s: control SIFS0 Off/On", __func__);
#endif

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

#ifdef SUPPORT_BTA2DP_OFFLOAD
/* BT A2DP Audio Specific Functions */
static void bta2dp_playback_start(struct audio_proxy *aproxy)
{
    audio_format_t codec_type = AUDIO_FORMAT_SBC;   // SBC is Max Size Structure, so it is default
    audio_sbc_encoder_config codec_info;
    int ret = 0;

    if (aproxy && aproxy->a2dp_out_enabled) {
        ret = proxy_a2dp_start();
        if (ret == 0) {
            ALOGI("proxy-%s: started BT A2DP", __func__);

            ret = proxy_a2dp_get_config((uint32_t *)&codec_type, (void *)&codec_info);
            if (ret == 0) {
                if (codec_type == AUDIO_FORMAT_SBC) {
                    struct sbc_enc_cfg_t config;
                    audio_sbc_encoder_config *sbc_config = (audio_sbc_encoder_config *)&codec_info;
                    memset(&config, 0, sizeof(struct sbc_enc_cfg_t));

                    config.enc_format   = ENC_MEDIA_FMT_SBC;
                    config.num_subbands = (uint32_t)sbc_config->subband;
                    config.blk_len      = (uint32_t)sbc_config->blk_len;
                    config.channel_mode = (uint32_t)sbc_config->channels;
                    config.alloc_method = (uint32_t)sbc_config->alloc;
                    config.bit_rate     = (uint32_t)sbc_config->bitrate;
                    config.sample_rate  = (uint32_t)sbc_config->sampling_rate;

                    proxy_set_mixer_value_array(aproxy, ABOX_A2DP_OFFLOAD_SET_PARAMS_NAME,
                                               &config, ABOX_A2DP_OFFLOAD_SET_PARAMS_COUNT);
                    ALOGI("proxy-%s: set A2DP SBC Encoder Configurations", __func__);

                    // Default SBC Latency = 150ms
                    aproxy->a2dp_default_delay = 150;
                } else if (codec_type == AUDIO_FORMAT_APTX) {
                    struct aptx_enc_cfg_t config;
                    audio_aptx_encoder_config *aptx_config = (audio_aptx_encoder_config *)&codec_info;
                    memset(&config, 0, sizeof(struct aptx_enc_cfg_t));

                    config.enc_format   = ENC_MEDIA_FMT_APTX;
                    config.sample_rate  = (uint32_t)aptx_config->sampling_rate;
                    config.num_channels = (uint32_t)aptx_config->channels;
                    switch (config.num_channels) {
                        case 1:
                            config.channel_mapping[0] = PCM_CHANNEL_C;
                            break;
                        case 2:
                        default:
                            config.channel_mapping[0] = PCM_CHANNEL_L;
                            config.channel_mapping[1] = PCM_CHANNEL_R;
                    }

                    proxy_set_mixer_value_array(aproxy, ABOX_A2DP_OFFLOAD_SET_PARAMS_NAME,
                                               &config, ABOX_A2DP_OFFLOAD_SET_PARAMS_COUNT);
                    ALOGI("proxy-%s: set A2DP APTX Encoder Configurations", __func__);

                    // Default APTX Latency = 200ms
                    aproxy->a2dp_default_delay = 200;
                }
            } else
                ALOGE("proxy-%s: failed to get BT A2DP Codec Configurations", __func__);
        }
    }

    return ;
}

static void bta2dp_playback_stop(struct audio_proxy *aproxy)
{
    int ret = 0;

    if (aproxy && aproxy->a2dp_out_enabled) {
        ret = proxy_a2dp_stop();
        if (ret == 0)
            ALOGI("proxy-%s: stopped stream for BT A2DP", __func__);
    }

    return ;
}

static void disable_a2dp_mute_playback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_bta2dp) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 A2DPMUTE_PLAYBACK_CARD, A2DPMUTE_PLAYBACK_DEVICE, 'p');

        /* Disables a2dp mute playback Path */
        if (aproxy->a2dp_mute_playback) {
            pcm_stop(aproxy->a2dp_mute_playback);
            pcm_close(aproxy->a2dp_mute_playback);
            aproxy->a2dp_mute_playback = NULL;

            ALOGI("proxy-%s: A2DP Mute playback PCM Device(%s) is stopped & closed!", __func__, pcm_path);
        }
    }

    return ;
}

static void enable_a2dp_mute_playback(void *proxy)
{
    struct audio_proxy *aproxy = proxy;
    struct pcm_config pcmconfig = pcm_config_a2dp_mute_playback;
    char pcm_path[MAX_PCM_PATH_LEN];

    if (aproxy->support_bta2dp) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 A2DPMUTE_PLAYBACK_CARD, A2DPMUTE_PLAYBACK_DEVICE, 'p');

        /* Enables A2DP Mute playback path */
        if (aproxy->a2dp_mute_playback == NULL) {
            aproxy->a2dp_mute_playback = pcm_open(A2DPMUTE_PLAYBACK_CARD, A2DPMUTE_PLAYBACK_DEVICE,
                                               PCM_OUT | PCM_MONOTONIC, &pcmconfig);
            if (aproxy->a2dp_mute_playback && !pcm_is_ready(aproxy->a2dp_mute_playback)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("proxy-%s: A2DP Mute playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->a2dp_mute_playback));
                goto err_open;
            }
            ALOGI("proxy-%s: A2DP Mute playback PCM Device(%s) with SR(%u)PF(%d) CC(%d) PdSz(%d) PdCnt(%d) is opened",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcmconfig.period_size, pcmconfig.period_count);

            if (pcm_start(aproxy->a2dp_mute_playback) == 0) {
                ALOGI("proxy-%s: A2DP Mute playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: A2DP Mute playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                      pcm_get_error(aproxy->a2dp_mute_playback));
                goto err_open;
            }
        }
    }

    return ;

err_open:
    disable_a2dp_mute_playback(proxy);
    return ;
}
#endif

static void enable_internal_path(void *proxy, int ausage, device_type target_device)
{
    struct audio_proxy *aproxy = proxy;

    /* skip internal pcm controls for VoiceCall bandwidth change */
    if (aproxy->skip_internalpath) {
        ALOGI("proxy-%s: skip enabling internal path", __func__);
        return;
    }

    if (target_device == DEVICE_EARPIECE ||
        target_device == DEVICE_SPEAKER || target_device == DEVICE_SPEAKER2 ||
        target_device == DEVICE_SPEAKER_DUAL || target_device == DEVICE_SPEAKER_DEX ||
        target_device == DEVICE_SPEAKER_AND_HEADSET || target_device == DEVICE_SPEAKER_AND_HEADPHONE) {
#ifdef SUPPORT_DIRECT_RCVSPK_PATH
            if (is_playback_device_speaker_dualpath(target_device)
                || ausage == AUSAGE_FM_RADIO_CAPTURE
                || ausage == AUSAGE_FM_RADIO_TUNER)
#endif
            {
                enable_spkamp_playback(aproxy);
                enable_erap_in(aproxy, target_device);
            }
        enable_spkamp_reference(aproxy);
#ifdef SUPPORT_BTA2DP_OFFLOAD
    } else if (target_device == DEVICE_BT_A2DP_HEADPHONE ||
               target_device == DEVICE_SPEAKER_AND_BT_A2DP_HEADPHONE) {
        /* Transit BT A2DP Status */
        pthread_mutex_lock(&aproxy->a2dp_lock);
        // Case : Audio Path changed from Others to A2DP Device
        //        BT A2DP need to be started
        bta2dp_playback_start(aproxy);
        pthread_mutex_unlock(&aproxy->a2dp_lock);

        if (target_device == DEVICE_SPEAKER_AND_BT_A2DP_HEADPHONE) {
            enable_erap_in(aproxy, target_device);
            enable_spkamp_reference(aproxy);
            enable_spkamp_playback(aproxy);
        }
        enable_bta2dp_playback(aproxy);
        enable_bta2dp_out_loopback(aproxy);

        // Start A2DP Mute playback node
        enable_a2dp_mute_playback(proxy);
#endif
    } else if (target_device == DEVICE_BT_HEADSET || target_device == DEVICE_SPEAKER_AND_BT_HEADSET) {
        enable_erap_in(aproxy, target_device);
        if (target_device == DEVICE_SPEAKER_AND_BT_HEADSET) {
            enable_spkamp_reference(aproxy);
            enable_spkamp_playback(aproxy);
        }
        enable_btsco_playback(aproxy);
    } else if (target_device == DEVICE_HEADSET || target_device == DEVICE_HEADPHONE ||
               target_device == DEVICE_CALL_FWD) {
        /* In cases of CP/AP Calland Loopback, ERAP Path is needed for SE */
        // In case of Normal Media, ERAP Path is not needed
        if (is_active_usage_CPCall(aproxy) || is_active_usage_APCall(aproxy) ||
            is_usage_Loopback(ausage))
            enable_erap_in(aproxy, target_device);
    } else if (target_device == DEVICE_USB_HEADSET ||
               target_device == DEVICE_SPEAKER_AND_USB_HEADSET) {
        /* Prepare USB device configuration based upon usage */
        if (aproxy->usb_aproxy) {
            /* USB output playback constraints
             * Full configuration for all modes except CP Call mode
             * CP Call mode: fix configuration to 48KHz 16bit or
             * supported configuration */
            if (is_usage_CPCall(ausage) &&
                !proxy_is_usb_playback_CPCall_prepared(aproxy->usb_aproxy)) {
                /* prepare for cp call playback with fixed configuration */
                proxy_usb_playback_prepare(aproxy->usb_aproxy, false);
            } else if (!is_usage_CPCall(ausage) &&
                proxy_is_usb_playback_CPCall_prepared(aproxy->usb_aproxy)) {
                /* prepare for playback with default configuration */
                proxy_usb_playback_prepare(aproxy->usb_aproxy, true);
            }
            proxy_usb_open_out_proxy(aproxy->usb_aproxy);
        }

        /* set USB playback modifier controls */
        set_usb_playback_modifier(aproxy);

        if (target_device == DEVICE_SPEAKER_AND_USB_HEADSET) {
            enable_spkamp_playback(aproxy);
            enable_spkamp_reference(aproxy);
        }
        // In cases of CP/AP Call, Internal Loop & ERAP Path is needed for SE
        // In case of Normal Media, No Paths are needed
        if (target_device == DEVICE_SPEAKER_AND_USB_HEADSET ||
            is_active_usage_CPCall(aproxy) || is_active_usage_APCall(aproxy) ||
            is_usage_Loopback(ausage)) {
            enable_erap_in(aproxy, target_device);
        }

        enable_usb_out_loopback(aproxy);
    } else if (is_usb_mic_device(target_device)) {
        // Check whether USB device is single clocksource, and match samplerate
        // with playback
        if (aproxy->is_usb_single_clksrc)
            proxy_usb_capture_prepare(aproxy->usb_aproxy, true);

        if (aproxy->usb_aproxy)
            proxy_usb_open_in_proxy(aproxy->usb_aproxy);
        enable_usb_in_loopback(proxy);
    }

    /* enable direct MIC path pcm for voiceCall scenario */
    if ((is_usage_CPCall(ausage) || is_usage_Loopback(ausage)) &&
        target_device >= DEVICE_MAIN_MIC)
        enable_voice_tx_direct_in(aproxy, target_device);

    return;
}

static void disable_internal_path(void *proxy, int ausage, device_type target_device)
{
    struct audio_proxy *aproxy = proxy;

    /* skip internal pcm controls for VoiceCall bandwidth change */
    if (aproxy->skip_internalpath) {
        ALOGI("proxy-%s: skip disabling internal path", __func__);
        return;
    }

    /* disable direct MIC path pcm for voiceCall scenario */
    if ((is_usage_CPCall(ausage) || is_usage_Loopback(ausage)) &&
        target_device >= DEVICE_MAIN_MIC)
        disable_voice_tx_direct_in(aproxy);

    if (target_device == DEVICE_SPEAKER ||
        target_device == DEVICE_SPEAKER2 || target_device == DEVICE_SPEAKER_DUAL ||
        target_device == DEVICE_EARPIECE || target_device == DEVICE_SPEAKER_DEX ||
        target_device == DEVICE_SPEAKER_AND_HEADSET || target_device == DEVICE_SPEAKER_AND_HEADPHONE) {
#ifdef SUPPORT_DIRECT_RCVSPK_PATH
        if (is_playback_device_speaker_dualpath(target_device)
            || ausage == AUSAGE_FM_RADIO_CAPTURE
            || ausage == AUSAGE_FM_RADIO_TUNER)
#endif
        {
            disable_erap_in(aproxy);
            disable_spkamp_playback(aproxy);
        }
        disable_spkamp_reference(aproxy);
#ifdef SUPPORT_BTA2DP_OFFLOAD
    } else if (target_device == DEVICE_BT_A2DP_HEADPHONE ||
               target_device == DEVICE_SPEAKER_AND_BT_A2DP_HEADPHONE) {
        /* Transit BT A2DP Status */
        pthread_mutex_lock(&aproxy->a2dp_lock);
        // Case : Audio Path reset for A2DP Device
        //        BT A2DP need to be stoped
        bta2dp_playback_stop(aproxy);
        pthread_mutex_unlock(&aproxy->a2dp_lock);

        // Stop A2DP Mute playback node
        disable_a2dp_mute_playback(proxy);

        if (target_device == DEVICE_SPEAKER_AND_BT_A2DP_HEADPHONE) {
            disable_spkamp_playback(aproxy);
            disable_spkamp_reference(aproxy);
            disable_erap_in(aproxy);
        }
        disable_bta2dp_out_loopback(aproxy);
        disable_bta2dp_playback(aproxy);
#endif
    } else if (target_device == DEVICE_BT_HEADSET || target_device == DEVICE_SPEAKER_AND_BT_HEADSET) {
        disable_btsco_playback(aproxy);
        if (target_device == DEVICE_SPEAKER_AND_BT_HEADSET) {
            disable_spkamp_playback(aproxy);
            disable_spkamp_reference(aproxy);
        }
        disable_erap_in(aproxy);

        /* reset Mixp configuration to default values when path is disabled */
        reset_playback_modifier(aproxy);
    } else if (target_device == DEVICE_HEADSET || target_device == DEVICE_HEADPHONE ||
               target_device == DEVICE_CALL_FWD) {
        if (is_active_usage_CPCall(aproxy) || is_active_usage_APCall(aproxy) ||
            is_usage_Loopback(ausage))
            disable_erap_in(aproxy);
    } else if (target_device == DEVICE_USB_HEADSET ||
                target_device == DEVICE_SPEAKER_AND_USB_HEADSET) {
        if (target_device == DEVICE_SPEAKER_AND_USB_HEADSET ||
            is_active_usage_CPCall(aproxy) || is_active_usage_APCall(aproxy) ||
            is_usage_Loopback(ausage)) {
            disable_erap_in(aproxy);
        }

        if (target_device == DEVICE_SPEAKER_AND_USB_HEADSET) {
            disable_spkamp_playback(aproxy);
            disable_spkamp_reference(aproxy);
        }
        disable_usb_out_loopback(aproxy);
        if (aproxy->usb_aproxy)
            proxy_usb_close_out_proxy(aproxy->usb_aproxy);

        /* reset Mixp configuration to default values when path is disabled */
        reset_playback_modifier(aproxy);
    } else if (is_usb_mic_device(target_device)) {
        disable_usb_in_loopback(proxy);
        if (aproxy->usb_aproxy)
            proxy_usb_close_in_proxy(aproxy->usb_aproxy);
    }

    return ;
}

// Voice Call PCM Handler
static void voice_rx_stop(struct audio_proxy *aproxy)
{
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Disables Voice Call RX Playback Stream */
    if (aproxy->call_rx) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VRX_PLAYBACK_CARD, VRX_PLAYBACK_DEVICE, 'p');

        pcm_stop(aproxy->call_rx);
        pcm_close(aproxy->call_rx);
        aproxy->call_rx = NULL;

        ALOGI("proxy-%s: Voice Call RX PCM Device(%s) is stopped & closed!", __func__, pcm_path);
    }
}

static int voice_rx_start(struct audio_proxy *aproxy)
{
    struct pcm_config pcmconfig = pcm_config_voicerx_playback;
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Enables Voice Call RX Playback Stream */
    if (aproxy->call_rx == NULL) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VRX_PLAYBACK_CARD, VRX_PLAYBACK_DEVICE, 'p');

        aproxy->call_rx = pcm_open(VRX_PLAYBACK_CARD, VRX_PLAYBACK_DEVICE,
                                   PCM_OUT | PCM_MONOTONIC, &pcmconfig);
        if (aproxy->call_rx && !pcm_is_ready(aproxy->call_rx)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("proxy-%s: Voice Call RX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_rx));
            goto err_open;
        }
        ALOGVV("proxy-%s: Voice Call RX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
              __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

        if (pcm_start(aproxy->call_rx) == 0) {
            ALOGI("proxy-%s: Voice Call RX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
        } else {
            ALOGE("proxy-%s: Voice Call RX PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_rx));
            goto err_open;
        }
    }
    return 0;

err_open:
    voice_rx_stop(aproxy);
    return -1;
}

static void voice_tx_stop(struct audio_proxy *aproxy)
{
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Disables Voice Call TX Capture Stream */
    if (aproxy->call_tx) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VTX_CAPTURE_CARD, VTX_CAPTURE_DEVICE, 'c');

        pcm_stop(aproxy->call_tx);
        pcm_close(aproxy->call_tx);
        aproxy->call_tx = NULL;
        ALOGI("proxy-%s: Voice Call TX PCM Device(%s) is stopped & closed!", __func__, pcm_path);
    }
}

static int voice_tx_start(struct audio_proxy *aproxy)
{
    struct pcm_config pcmconfig;
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Enables Voice Call TX Capture Stream */
    if (aproxy->call_tx == NULL) {
#ifdef SUPPORT_QUAD_MIC
        if (is_quad_mic_device(aproxy->active_capture_device)) {
            pcmconfig = pcm_config_quad_mic_voicetx_capture;
            ALOGI("proxy-%s: Quad-Mic config for Voice Call TX", __func__);
        } else
#endif
            pcmconfig = pcm_config_voicetx_capture;
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VTX_CAPTURE_CARD, VTX_CAPTURE_DEVICE, 'c');

        aproxy->call_tx = pcm_open(VTX_CAPTURE_CARD, VTX_CAPTURE_DEVICE,
                                   PCM_IN | PCM_MONOTONIC, &pcmconfig);
        if (aproxy->call_tx && !pcm_is_ready(aproxy->call_tx)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("proxy-%s: Voice Call TX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_tx));
            goto err_open;
        }
        ALOGVV("proxy-%s: Voice Call TX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
              __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

        if (pcm_start(aproxy->call_tx) == 0) {
            ALOGI("proxy-%s: Voice Call TX PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
        } else {
            ALOGE("proxy-%s: Voice Call TX PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->call_tx));
            goto err_open;
        }
    }
    return 0;

err_open:
    voice_tx_stop(aproxy);
    return -1;
}

// FM Radio PCM Handler
static void fmradio_playback_stop(struct audio_proxy *aproxy)
{
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Disables FM Radio Playback Stream */
    if (aproxy->fm_playback) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 FMRADIO_PLAYBACK_CARD, FMRADIO_PLAYBACK_DEVICE, 'p');

        pcm_stop(aproxy->fm_playback);
        pcm_close(aproxy->fm_playback);
        aproxy->fm_playback = NULL;

        ALOGI("proxy-%s: FM Radio Playback PCM Device(%s) is stopped & closed!", __func__, pcm_path);
    }
}

static int fmradio_playback_start(struct audio_proxy *aproxy)
{
    struct pcm_config pcmconfig = pcm_config_fmradio_playback;
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Enables RM Radio Playback Stream */
    if (aproxy->fm_playback == NULL) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 FMRADIO_PLAYBACK_CARD, FMRADIO_PLAYBACK_DEVICE, 'p');

        aproxy->fm_playback = pcm_open(FMRADIO_PLAYBACK_CARD, FMRADIO_PLAYBACK_DEVICE,
                                       PCM_OUT | PCM_MONOTONIC, &pcmconfig);
        if (aproxy->fm_playback && !pcm_is_ready(aproxy->fm_playback)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("proxy-%s: FM Radio Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->fm_playback));
            goto err_open;
        }
        ALOGVV("proxy-%s: FM Radio Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
              __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

        if (pcm_start(aproxy->fm_playback) == 0) {
            ALOGI("proxy-%s: FM Radio Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
        } else {
            ALOGE("proxy-%s: FM Radio Playback PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->fm_playback));
            goto err_open;
        }
    }

    return 0;

err_open:
    fmradio_playback_stop(aproxy);
    return -1;
}

static void fmradio_capture_stop(struct audio_proxy *aproxy)
{
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Disables FM Radio Capture Stream */
    if (aproxy->fm_capture) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VC_FMRADIO_CAPTURE_CARD, VC_FMRADIO_CAPTURE_DEVICE, 'c');

        pcm_stop(aproxy->fm_capture);
        pcm_close(aproxy->fm_capture);
        aproxy->fm_capture = NULL;

        ALOGI("proxy-%s: FM Radio Capture PCM Device(%s) is stopped & closed!", __func__, pcm_path);
    }
}

static int fmradio_capture_start(struct audio_proxy *aproxy)
{
    struct pcm_config pcmconfig = pcm_config_vc_fmradio_capture;
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Enables RM Radio Capture Stream */
    if (aproxy->fm_capture == NULL) {
        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 VC_FMRADIO_CAPTURE_CARD, VC_FMRADIO_CAPTURE_DEVICE, 'c');

        aproxy->fm_capture = pcm_open(VC_FMRADIO_CAPTURE_CARD, VC_FMRADIO_CAPTURE_DEVICE,
                                                           PCM_IN | PCM_MONOTONIC, &pcmconfig);
        if (aproxy->fm_capture && !pcm_is_ready(aproxy->fm_capture)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("proxy-%s: FM Radio Capture PCM Device(%s) with SR(%u) PF(%d) CC(%d) is not ready as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->fm_capture));
            goto err_open;
        }
        ALOGVV("proxy-%s: FM Radio Capture PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened",
              __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);

        if (pcm_start(aproxy->fm_capture) == 0) {
            ALOGI("proxy-%s: FM Radio Capture PCM Device(%s) with SR(%u) PF(%d) CC(%d) is opened & started",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels);
        } else {
            ALOGE("proxy-%s: FM Radio Capture PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                  __func__, pcm_path, pcmconfig.rate, pcmconfig.format, pcmconfig.channels,
                  pcm_get_error(aproxy->fm_capture));
            goto err_open;
        }
    }

    return 0;

err_open:
    fmradio_capture_stop(aproxy);
    return -1;
}

struct mixer {
    int fd;
    struct snd_ctl_card_info card_info;
    struct snd_ctl_elem_info *elem_info;
    struct mixer_ctl *ctl;
    unsigned int count;
};

static struct snd_ctl_event *mixer_read_event_sec(struct mixer *mixer, unsigned int mask)
{
    struct snd_ctl_event *ev;

    if (!mixer)
        return 0;

    ev = calloc(1, sizeof(*ev));
    if (!ev)
        return 0;

    while (read(mixer->fd, ev, sizeof(*ev)) > 0) {
        if (ev->type != SNDRV_CTL_EVENT_ELEM)
            continue;

        if (!(ev->data.elem.mask & mask))
            continue;

        return ev;
    }

    free(ev);
    return 0;
}

static int audio_route_missing_ctl(struct audio_route *ar) {
    return 0;
}

/* Mask for mixer_read_event()
 * It should be same with SNDRV_CTL_EVENT_MASK_* in asound.h.
 */
#define MIXER_EVENT_VALUE    (1 << 0)
#define MIXER_EVENT_INFO     (1 << 1)
#define MIXER_EVENT_ADD      (1 << 2)
#define MIXER_EVENT_TLV      (1 << 3)
#define MIXER_EVENT_REMOVE   (~0U)

static void *mixer_update_loop(void *context)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)context;
    struct snd_ctl_event *event = NULL;
    struct timespec ts_start, ts_tick;

    ALOGI("proxy-%s: started running Mixer Updater Thread", __func__);

    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    do {
        if (aproxy->mixer) {
            ALOGD("proxy-%s: wait add event", __func__);
            event = mixer_read_event_sec(aproxy->mixer, MIXER_EVENT_ADD);
            if (!event) {
                ALOGE("proxy-%s: returned as error or mixer close", __func__);
                clock_gettime(CLOCK_MONOTONIC, &ts_tick);
                if ((ts_tick.tv_sec - ts_start.tv_sec) > MIXER_UPDATE_TIMEOUT) {
                    ALOGI("proxy-%s: Mixer Update Timeout, it will be destroyed", __func__);
                    break;
                }
                continue;
            }
            ALOGD("proxy-%s: returned as add event", __func__);
        } else
            continue;

        pthread_rwlock_wrlock(&aproxy->mixer_update_lock);

        mixer_close(aproxy->mixer);
        aproxy->mixer = mixer_open(MIXER_CARD0);
        if (!aproxy->mixer)
            ALOGE("proxy-%s: failed to re-open Mixer", __func__);

        mixer_subscribe_events(aproxy->mixer, 1);
        audio_route_free(aproxy->aroute);
        aproxy->aroute = audio_route_init(MIXER_CARD0, aproxy->xml_path);
        if (!aproxy->aroute)
            ALOGE("proxy-%s: failed to re-init audio route", __func__);

        ALOGI("proxy-%s: mixer and route are updated", __func__);

        pthread_rwlock_unlock(&aproxy->mixer_update_lock);
        free(event);
    } while (aproxy->mixer && aproxy->aroute && audio_route_missing_ctl(aproxy->aroute));

    ALOGI("proxy-%s: all mixer controls are found", __func__);

    if (aproxy->mixer)
        mixer_subscribe_events(aproxy->mixer, 0);

    ALOGI("proxy-%s: stopped running Mixer Updater Thread", __func__);
    return NULL;
}

static void make_path(audio_usage ausage, device_type device, char *path_name)
{
    memset(path_name, 0, MAX_PATH_NAME_LEN);
    strlcpy(path_name, usage_path_table[ausage], MAX_PATH_NAME_LEN);
    if (strlen(device_table[device]) > 0) {
        strlcat(path_name, "-", MAX_PATH_NAME_LEN);
        strlcat(path_name, device_table[device], MAX_PATH_NAME_LEN);
    }

    return ;
}

static void make_gain(char *path_name, char *gain_name)
{
    memset(gain_name, 0, MAX_GAIN_PATH_NAME_LEN);
    strlcpy(gain_name, "gain-", MAX_PATH_NAME_LEN);
    strlcat(gain_name, path_name, MAX_PATH_NAME_LEN);

    return ;
}

static void add_dual_path(void *proxy, char *path_name)
{
    struct audio_proxy *aproxy = proxy;

    if (aproxy->support_dualspk) {
        char tempStr[MAX_PATH_NAME_LEN] = {0};
        char* szDump;
        szDump = strstr(path_name, "speaker");

        // do not add dual- path for loopback
        if (strstr(path_name, "loopback")) {
            return ;
        }

        if (szDump != NULL) {
            char tempRet[MAX_PATH_NAME_LEN] = {0};
            strncpy(tempStr, path_name, szDump - path_name);
            sprintf(tempRet, "%s%s%s", tempStr, "dual-", szDump);
            strncpy(path_name, tempRet, MAX_PATH_NAME_LEN);
        }
    }
}

/* Enable new Audio Path */
static void set_route(void *proxy, audio_usage ausage, device_type device)
{
    struct audio_proxy *aproxy = proxy;
    char path_name[MAX_PATH_NAME_LEN];
    char gain_name[MAX_GAIN_PATH_NAME_LEN];

    if (device == DEVICE_AUX_DIGITAL)
        return ;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    make_path(ausage, device, path_name);
    add_dual_path(aproxy, path_name);
    audio_route_apply_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: routed to %s", __func__, path_name);

    make_gain(path_name, gain_name);
    audio_route_apply_and_update_path(aproxy->aroute, gain_name);
    ALOGI("proxy-%s: set gain as %s", __func__, gain_name);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* reroute Audio Path */
static void set_reroute(void *proxy, audio_usage old_ausage, device_type old_device,
                                     audio_usage new_ausage, device_type new_device)
{
    struct audio_proxy *aproxy = proxy;
    char path_name[MAX_PATH_NAME_LEN];
    char gain_name[MAX_GAIN_PATH_NAME_LEN];

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    // 1. Unset Active Route
    make_path(old_ausage, old_device, path_name);
    add_dual_path(aproxy, path_name);
    /* Updated to reset_and_update to match Q audio-route changes
     * otherwise noise issue happened in alarm/ringtone scenarios
     */
    audio_route_reset_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: unrouted %s", __func__, path_name);

    make_gain(path_name, gain_name);
    audio_route_reset_and_update_path(aproxy->aroute, gain_name);
    ALOGI("proxy-%s: reset gain %s", __func__, gain_name);

    // 2. Set New Route
    if (new_device != DEVICE_AUX_DIGITAL) {
        make_path(new_ausage, new_device, path_name);
        add_dual_path(aproxy, path_name);
        audio_route_apply_and_update_path(aproxy->aroute, path_name);
        ALOGI("proxy-%s: routed %s", __func__, path_name);

        make_gain(path_name, gain_name);
        audio_route_apply_and_update_path(aproxy->aroute, gain_name);
        ALOGI("proxy-%s: set gain as %s", __func__, gain_name);
    }

    // 3. Update Mixers
    audio_route_update_mixer(aproxy->aroute);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Disable Audio Path */
static void reset_route(void *proxy, audio_usage ausage, device_type device)
{
    struct audio_proxy *aproxy = proxy;
    char path_name[MAX_PATH_NAME_LEN];
    char gain_name[MAX_GAIN_PATH_NAME_LEN];

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    make_path(ausage, device, path_name);
    add_dual_path(aproxy, path_name);
    audio_route_reset_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: unrouted %s", __func__, path_name);

    make_gain(path_name, gain_name);
    audio_route_reset_and_update_path(aproxy->aroute, gain_name);
    ALOGI("proxy-%s: reset gain %s", __func__, gain_name);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Enable new Modifier */
static void set_modifier(void *proxy, modifier_type modifier)
{
    struct audio_proxy *aproxy = proxy;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    audio_route_apply_and_update_path(aproxy->aroute, modifier_table[modifier]);
    ALOGI("proxy-%s: enabled to %s", __func__, modifier_table[modifier]);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Update Modifier */
static void update_modifier(void *proxy, modifier_type old_modifier, modifier_type new_modifier)
{
    struct audio_proxy *aproxy = proxy;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    // 1. Unset Active Modifier
    audio_route_reset_path(aproxy->aroute, modifier_table[old_modifier]);
    ALOGI("proxy-%s: disabled %s", __func__, modifier_table[old_modifier]);

    // 2. Set New Modifier
    audio_route_apply_path(aproxy->aroute, modifier_table[new_modifier]);
    ALOGI("proxy-%s: enabled %s", __func__, modifier_table[new_modifier]);

    // 3. Update Mixers
    audio_route_update_mixer(aproxy->aroute);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

/* Disable Modifier */
static void reset_modifier(void *proxy, modifier_type modifier)
{
    struct audio_proxy *aproxy = proxy;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    audio_route_reset_and_update_path(aproxy->aroute, modifier_table[modifier]);
    ALOGI("proxy-%s: disabled %s", __func__, modifier_table[modifier]);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

static void do_operations_by_playback_route_set(struct audio_proxy *aproxy,
                                                audio_usage routed_ausage, device_type routed_device)
{
    /* skip internal pcm controls */
    if (aproxy->skip_internalpath) {
        ALOGI("proxy-%s: skip internal path pcm controls", __func__);
        return;
    }

    /* Open/Close FM Radio PCM node based on Enable/disable */
    if (routed_ausage != AUSAGE_FM_RADIO_CAPTURE && routed_ausage != AUSAGE_FM_RADIO_TUNER) {
        fmradio_playback_stop(aproxy);
        fmradio_capture_stop(aproxy);
    }

    /* Set Mute during APCall Path Change */
    if ((aproxy->active_playback_device != routed_device) &&
        (is_active_usage_APCall(aproxy) || is_usage_APCall(routed_ausage)))
        proxy_set_mixercontrol(aproxy, MUTE_CONTROL, ABOX_MUTE_CNT_FOR_PATH_CHANGE);

    return ;
}

static void do_operations_by_playback_route_reset(struct audio_proxy *aproxy __unused)
{
    return ;
}


/*
 * Dump functions
 */
static void calliope_cleanup_old(const char *path, const char *prefix)
{
    struct dirent **namelist;
    int n, match = 0;

    ALOGV("proxy-%s", __func__);

    n = scandir(path, &namelist, NULL, alphasort);
    if (n > 0) {
        /* interate in reverse order to get old file */
        while (n--) {
            if (strstr(namelist[n]->d_name, prefix) == namelist[n]->d_name) {
                if (++match > ABOX_DUMP_LIMIT) {
                    char *tgt;

                    if (asprintf(&tgt, "%s/%s", path, namelist[n]->d_name) != -1) {
                        remove(tgt);
                        free(tgt);
                    }
                }
            }
            free(namelist[n]);
        }
        free(namelist);
    }

    return ;
}

static void __calliope_dump(int fd, const char *in_prefix, const char *in_file, const char *out_prefix, const char *out_suffix)
{
    static const int buf_size = 4096;
    char *buf, in_path[128], out_path[128];
    int fd_in = -1, fd_out = -1, n;
    mode_t mask;

    ALOGV("proxy-%s", __func__);

    if (snprintf(in_path, sizeof(in_path) - 1, "%s%s", in_prefix, in_file) < 0) {
        ALOGE("proxy-%s: in path error: %s", __func__, strerror(errno));
        return;
    }

    if (snprintf(out_path, sizeof(out_path) - 1, "%s%s_%s.bin", out_prefix, in_file, out_suffix) < 0) {
        ALOGE("proxy-%s: out path error: %s", __func__, strerror(errno));
        return;
    }

    buf = malloc(buf_size);
    if (!buf) {
        ALOGE("proxy-%s: malloc failed: %s", __func__, strerror(errno));
        return;
    }

    mask = umask(0);
    ALOGV("umask = %o", mask);

    fd_in = open(in_path, O_RDONLY | O_NONBLOCK);
    if (fd_in < 0)
        ALOGE("proxy-%s: open error: %s, fd_in=%s", __func__, strerror(errno), in_path);
    fd_out = open(out_path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (fd_out < 0)
        ALOGE("proxy-%s: open error: %s, fd_out=%s", __func__, strerror(errno), out_path);
    if (fd_in >= 0 && fd_out >= 0) {
        while((n = read(fd_in, buf, buf_size)) > 0) {
            if (write(fd_out, buf, n) < 0) {
                ALOGE("proxy-%s: write error: %s", __func__, strerror(errno));
            }
        }
        n = snprintf(buf, buf_size, " %s_%s.bin <= %s\n", in_file, out_suffix, in_file);
        write(fd, buf, n);
        ALOGI("proxy-%s", buf);
    }

    calliope_cleanup_old(out_prefix, in_file);

    if (fd_in >= 0)
        close(fd_in);
    if (fd_out >= 0)
        close(fd_out);

    mask = umask(mask);
    free(buf);

    return ;
}

static void calliope_ramdump(int fd)
{
    char str_time[32];
    time_t t;
    struct tm *lt;

    ALOGD("%s", __func__);

    t = time(NULL);
    lt = localtime(&t);
    if (lt == NULL) {
        ALOGE("%s: time conversion error: %s", __func__, strerror(errno));
        return;
    }
    if (strftime(str_time, sizeof(str_time), "%Y%m%d_%H%M%S", lt) == 0) {
        ALOGE("%s: time error: %s", __func__, strerror(errno));
    }

    write(fd, "\n", strlen("\n"));
    write(fd, "Calliope snapshot:\n", strlen("Calliope snapshot:\n"));
    ALOGI("Calliope snapshot:\n");
    __calliope_dump(fd, SYSFS_PREFIX ABOX_DEV ABOX_DEBUG, ABOX_GPR, ABOX_DUMP, str_time);
    __calliope_dump(fd, CALLIOPE_DBG_PATH, CALLIOPE_LOG, ABOX_DUMP, str_time);
    __calliope_dump(fd, SYSFS_PREFIX ABOX_DEV ABOX_DEBUG, ABOX_SRAM, ABOX_DUMP, str_time);
    __calliope_dump(fd, SYSFS_PREFIX ABOX_DEV ABOX_DEBUG, ABOX_DRAM, ABOX_DUMP, str_time);
    __calliope_dump(fd, ABOX_REGMAP_PATH, ABOX_REG_FILE, ABOX_DUMP, str_time);
    write(fd, "Calliope snapshot done\n", strlen("Calliope snapshot done\n"));

    return ;
}

/******************************************************************************/
/**                                                                          **/
/** Local Functions for Audio Stream Proxy                                   **/
/**                                                                          **/
/******************************************************************************/
/* Compress Offload Specific Functions */
static bool is_supported_compressed_format(audio_format_t format)
{
    switch (format & AUDIO_FORMAT_MAIN_MASK) {
    case AUDIO_FORMAT_MP3:
    case AUDIO_FORMAT_AAC:
    case AUDIO_FORMAT_FLAC:
        return true;
    default:
        break;
    }

    return false;
}

static int get_snd_codec_id(audio_format_t format)
{
    int id = 0;

    switch (format & AUDIO_FORMAT_MAIN_MASK) {
    case AUDIO_FORMAT_MP3:
        id = SND_AUDIOCODEC_MP3;
        break;
    case AUDIO_FORMAT_AAC:
        id = SND_AUDIOCODEC_AAC;
        break;
    case AUDIO_FORMAT_FLAC:
        id = SND_AUDIOCODEC_FLAC;
        break;
    default:
            ALOGE("offload_out-%s: Unsupported audio format", __func__);
    }

    return id;
}

static int check_direct_config_support(struct audio_proxy_stream *apstream)
{
    int i;
    int ret = 0;

    // Check Sampling Rate
    for (i = 0; i < MAX_NUM_PLAYBACK_SR; i++) {
        if (apstream->requested_sample_rate == supported_playback_samplingrate[i]) {
            if (apstream->requested_sample_rate != apstream->pcmconfig.rate) {
                apstream->pcmconfig.rate = apstream->requested_sample_rate;
            }
            apstream->pcmconfig.period_size = (apstream->pcmconfig.rate * PREDEFINED_USB_PLAYBACK_DURATION) / 1000;

            // DMA in A-Box is 128-bit aligned, so period_size has to be multiple of 4 frames
            apstream->pcmconfig.period_size &= 0xFFFFFFFC;
            ALOGD("%s-%s: updates samplig rate to %u, period_size to %u", stream_table[apstream->stream_type],
                __func__, apstream->pcmconfig.rate,
                apstream->pcmconfig.period_size);
            break;
        }
    }

    if (i == MAX_NUM_PLAYBACK_SR) {
        ALOGD("%s-%s: unsupported samplerate to %u", stream_table[apstream->stream_type], __func__,
                                                apstream->requested_sample_rate);
        ret = -EINVAL;
        goto err;
    }

    // Check Channel Mask
    for (i = 0; i < MAX_NUM_DIRECT_PLAYBACK_CM; i++) {
        if (apstream->requested_channel_mask == supported_direct_playback_channelmask[i]) {
            if (audio_channel_count_from_out_mask(apstream->requested_channel_mask)
                != apstream->pcmconfig.channels) {
                if (apstream->requested_channel_mask == AUDIO_CHANNEL_OUT_5POINT1) {
                    ALOGD("%s-%s: channel padding needed from 6 Channels to %u channels",
                        stream_table[apstream->stream_type], __func__,
                        apstream->pcmconfig.channels);
                    /* A-Box HW doesn't 6 channels therefore 2 channel padding is required */
                    apstream->need_channelpadding = true;
                } else {
                    apstream->pcmconfig.channels =
                        audio_channel_count_from_out_mask(apstream->requested_channel_mask);
                    ALOGD("%s-%s: channel count updated to %u",
                        stream_table[apstream->stream_type], __func__,
                        apstream->pcmconfig.channels);
                }
            }
            ALOGD("%s-%s: requested channel mask %u configured channels %d ",
                stream_table[apstream->stream_type], __func__,
                audio_channel_count_from_out_mask(apstream->requested_channel_mask),
                apstream->pcmconfig.channels);
            break;
        }
    }

    if (i == MAX_NUM_DIRECT_PLAYBACK_CM) {
        ALOGD("%s-%s: unsupported channel mask %u ", stream_table[apstream->stream_type],
            __func__, audio_channel_count_from_out_mask(apstream->requested_channel_mask));
        ret = -EINVAL;
    }

    // Check PCM Format
    for (i = 0; i < MAX_NUM_PLAYBACK_PF; i++) {
        if (apstream->requested_format == supported_playback_pcmformat[i]) {
            if (pcm_format_from_audio_format(apstream->requested_format) !=
                apstream->pcmconfig.format) {
                apstream->pcmconfig.format =
                    pcm_format_from_audio_format(apstream->requested_format);
                ALOGD("%s-%s: updates PCM format to %d", stream_table[apstream->stream_type],
                    __func__, apstream->pcmconfig.format);
            }
            break;
        }
    }

    if (i == MAX_NUM_PLAYBACK_PF) {
        ALOGD("%s-%s: unsupported format 0x%x", stream_table[apstream->stream_type],
            __func__, apstream->requested_format);
        ret = -EINVAL;
        goto err;
    }

err:
    return ret;
}

static void save_written_frames(struct audio_proxy_stream *apstream, int bytes)
{
    apstream->frames += bytes / (apstream->pcmconfig.channels *
                audio_bytes_per_sample(audio_format_from_pcm_format(apstream->pcmconfig.format)));
    ALOGVV("%s-%s: written = %u frames", stream_table[apstream->stream_type], __func__,
                                         (unsigned int)apstream->frames);
    return ;
}

static void skip_pcm_processing(struct audio_proxy_stream *apstream, int bytes)
{
    unsigned int frames = 0;

    frames = bytes / (apstream->pcmconfig.channels *
             audio_bytes_per_sample(audio_format_from_pcm_format(apstream->pcmconfig.format)));
    usleep(frames * 1000000 / proxy_get_actual_sampling_rate(apstream));
    return ;
}

static void update_capture_pcmconfig(struct audio_proxy_stream *apstream)
{
#ifdef SUPPORT_QUAD_MIC
    struct audio_proxy *aproxy = getInstance();
#endif
    int i;

    // Check Sampling Rate
    for (i = 0; i < MAX_NUM_CAPTURE_SR; i++) {
        if (apstream->requested_sample_rate == supported_capture_samplingrate[i]) {
            if (apstream->requested_sample_rate != apstream->pcmconfig.rate) {
                apstream->pcmconfig.rate = apstream->requested_sample_rate;
                if (apstream->stream_type == ASTREAM_CAPTURE_PRIMARY)
                    apstream->pcmconfig.period_size = (apstream->pcmconfig.rate * PREDEFINED_MEDIA_CAPTURE_DURATION) / 1000;
                else if (apstream->stream_type == ASTREAM_CAPTURE_LOW_LATENCY)
                    apstream->pcmconfig.period_size = (apstream->pcmconfig.rate * PREDEFINED_LOW_CAPTURE_DURATION) / 1000;

                // WDMA in A-Box is 128-bit aligned, so period_size has to be multiple of 4 frames
                apstream->pcmconfig.period_size &= 0xFFFFFFFC;
                ALOGD("%s-%s: updates samplig rate to %u, period_size to %u", stream_table[apstream->stream_type],
                                                           __func__, apstream->pcmconfig.rate,
                                                           apstream->pcmconfig.period_size);
            }
            break;
        }
    }

    if (i == MAX_NUM_CAPTURE_SR)
        ALOGD("%s-%s: needs re-sampling to %u", stream_table[apstream->stream_type], __func__,
                                                apstream->requested_sample_rate);

    // Check Channel Mask
    for (i = 0; i < MAX_NUM_CAPTURE_CM; i++) {
        if (apstream->requested_channel_mask == supported_capture_channelmask[i]) {
            if (audio_channel_count_from_in_mask(apstream->requested_channel_mask)
                != apstream->pcmconfig.channels) {
#ifdef SUPPORT_QUAD_MIC
                if ((is_active_usage_CPCall(aproxy) || is_active_usage_APCall(aproxy)
                    || apstream->stream_usage == AUSAGE_CAMCORDER)
                    && is_quad_mic_device(aproxy->active_capture_device)) {
                    ALOGD("%s-%s: Skip channel count updating to %u", stream_table[apstream->stream_type],
                                            __func__, apstream->pcmconfig.channels);
                } else
#endif
                {
                    apstream->pcmconfig.channels = audio_channel_count_from_in_mask(apstream->requested_channel_mask);
                    ALOGD("%s-%s: updates channel count to %u", stream_table[apstream->stream_type],
                                                                __func__, apstream->pcmconfig.channels);
                }
            }
            break;
        }
    }

    if (i == MAX_NUM_CAPTURE_CM)
        ALOGD("%s-%s: needs re-channeling to %u from %u", stream_table[apstream->stream_type], __func__,
              audio_channel_count_from_in_mask(apstream->requested_channel_mask), apstream->pcmconfig.channels);

    // Check PCM Format
    for (i = 0; i < MAX_NUM_CAPTURE_PF; i++) {
        if (apstream->requested_format == supported_capture_pcmformat[i]) {
            if (pcm_format_from_audio_format(apstream->requested_format) != apstream->pcmconfig.format) {
                apstream->pcmconfig.format = pcm_format_from_audio_format(apstream->requested_format);
                ALOGD("%s-%s: updates PCM format to %d", stream_table[apstream->stream_type], __func__,
                                                         apstream->pcmconfig.format);
            }
            break;
        }
    }

    if (i == MAX_NUM_CAPTURE_PF)
        ALOGD("%s-%s: needs re-formating to 0x%x", stream_table[apstream->stream_type], __func__,
                                                   apstream->requested_format);

    return ;
}

// For Resampler
int proxy_get_requested_frame_size(struct audio_proxy_stream *apstream)
{
    return audio_channel_count_from_in_mask(apstream->requested_channel_mask) *
           audio_bytes_per_sample(apstream->requested_format);
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer* buffer)
{
    struct audio_proxy_stream *apstream;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    apstream = (struct audio_proxy_stream *)((char *)buffer_provider -
                                             offsetof(struct audio_proxy_stream, buf_provider));

    if (apstream->pcm) {
        if (apstream->read_buf_frames == 0) {
            unsigned int size_in_bytes = pcm_frames_to_bytes(apstream->pcm, apstream->pcmconfig.period_size);
            if (apstream->actual_read_buf_size < size_in_bytes) {
                apstream->actual_read_buf_size = size_in_bytes;
                apstream->actual_read_buf = (int16_t *) realloc(apstream->actual_read_buf, size_in_bytes);
                if (apstream->actual_read_buf != NULL)
                    ALOGI("%s-%s: alloc actual read buffer with %u bytes",
                           stream_table[apstream->stream_type], __func__, size_in_bytes);
            }

            if (apstream->actual_read_buf != NULL) {
                apstream->actual_read_status = pcm_read(apstream->pcm, (void*)apstream->actual_read_buf, size_in_bytes);
                if (apstream->actual_read_status != 0) {
                    ALOGE("%s-%s: pcm_read error %d(%s)", stream_table[apstream->stream_type],
                        __func__, apstream->actual_read_status, pcm_get_error(apstream->pcm));
                    buffer->raw = NULL;
                    buffer->frame_count = 0;
                    return apstream->actual_read_status;
                }

                if (apstream->stream_type == ASTREAM_CAPTURE_CALL) {
                    /*
                     * [Call Recording Case]
                     * In case of Call Recording, A-Box sends stereo stream which uplink/downlink voice
                     * allocated in left/right to AudioHAL.
                     * AudioHAL has to select and mix uplink/downlink voice from left/right channel as usage.
                     */
                    int16_t data_mono;
                    int16_t *vc_buf = (int16_t *)(apstream->actual_read_buf);

                    // Channel Selection
                    // output : Stereo with Left/Right contains same selected channel PCM & Device SR
                    for (unsigned int i = 0; i < apstream->pcmconfig.period_size; i++){
                        if (apstream->stream_usage == AUSAGE_INCALL_UPLINK)
                            data_mono = (*(vc_buf + 2*i + 1)); // Tx
                        else if (apstream->stream_usage == AUSAGE_INCALL_DOWNLINK){
                            data_mono = (*(vc_buf + 2*i));     // Rx
                        } else {
                            data_mono = clamp16(((int32_t)*(vc_buf+2*i) + (int32_t)*(vc_buf+2*i+1))*0.7); // mix Rx/Tx
                        }

                        *(vc_buf + 2*i)     = data_mono;
                        *(vc_buf + 2*i + 1) = data_mono;
                    }
                }

                /* Convert A-Box's zero padded 24bit format to signed extension */
                if (apstream->pcmconfig.format == PCM_FORMAT_S24_LE) {
                    int *rd_buf = (int *)(apstream->actual_read_buf);

                    for (unsigned int i = 0;
                        i < (apstream->pcmconfig.period_size * apstream->pcmconfig.channels); i++) {
                        if (*(rd_buf + i) & 0x800000)
                            *(rd_buf + i) |= 0xFF000000;
                    }
                }
                apstream->read_buf_frames = apstream->pcmconfig.period_size;
            } else {
                ALOGE("%s-%s: failed to reallocate actual_read_buf",
                      stream_table[apstream->stream_type], __func__);
                buffer->raw = NULL;
                buffer->frame_count = 0;
                apstream->actual_read_status = -ENOMEM;
                return -ENOMEM;
            }
        }

        buffer->frame_count = (buffer->frame_count > apstream->read_buf_frames) ?
                               apstream->read_buf_frames : buffer->frame_count;
        buffer->i16 = apstream->actual_read_buf + (apstream->pcmconfig.period_size - apstream->read_buf_frames) *
                                                  apstream->pcmconfig.channels;
        return apstream->actual_read_status;
    } else {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        apstream->actual_read_status = -ENODEV;
        return -ENODEV;
    }
}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer* buffer)
{
    struct audio_proxy_stream *apstream;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    apstream = (struct audio_proxy_stream *)((char *)buffer_provider -
                                             offsetof(struct audio_proxy_stream, buf_provider));

    apstream->read_buf_frames -= buffer->frame_count;
}

static int read_frames(struct audio_proxy_stream *apstream, void *buffer, int frames)
{
    int frames_wr = 0;

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        ALOGVV("%s-%s: frames_rd: %zd, frames_wr: %d",
           stream_table[apstream->stream_type], __func__, frames_rd, frames_wr);

        if (apstream->resampler != NULL) {
            apstream->resampler->resample_from_provider(apstream->resampler,
            (int16_t *)((char *)buffer + pcm_frames_to_bytes(apstream->pcm, frames_wr)), &frames_rd);
        } else {
            struct resampler_buffer buf;
            buf.raw= NULL;
            buf.frame_count = frames_rd;

            get_next_buffer(&apstream->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer + pcm_frames_to_bytes(apstream->pcm, frames_wr),
                        buf.raw, pcm_frames_to_bytes(apstream->pcm, buf.frame_count));
                frames_rd = buf.frame_count;
            }
            release_buffer(&apstream->buf_provider, &buf);
        }

        /* apstream->actual_read_status is updated by getNextBuffer() also called by
         * apstream->resampler->resample_from_provider() */
        if (apstream->actual_read_status != 0)
            return apstream->actual_read_status;

        frames_wr += frames_rd;
    }

    return frames_wr;
}

static int read_and_process_frames(struct audio_proxy_stream *apstream, void* buffer, int frames_num)
{
    int frames_wr = 0;
    unsigned int bytes_per_sample = (pcm_format_to_bits(apstream->pcmconfig.format) >> 3);
    void *proc_buf_out = buffer;

    int num_device_channels = proxy_get_actual_channel_count(apstream);
    int num_req_channels = audio_channel_count_from_in_mask(apstream->requested_channel_mask);

    /* Prepare Channel Conversion Input Buffer */
    if (apstream->need_channelconversion && (num_device_channels != num_req_channels)) {
        int src_buffer_size = frames_num * num_device_channels * bytes_per_sample;

        if (apstream->proc_buf_size < src_buffer_size) {
            apstream->proc_buf_size = src_buffer_size;
            apstream->proc_buf_out = realloc(apstream->proc_buf_out, src_buffer_size);
            ALOGI("%s-%s: alloc resampled read buffer with %d bytes",
                      stream_table[apstream->stream_type], __func__, src_buffer_size);
        }
        proc_buf_out = apstream->proc_buf_out;
    }

    frames_wr = read_frames(apstream, proc_buf_out, frames_num);
    if ((frames_wr > 0) && (frames_wr > frames_num))
        ALOGE("%s-%s: read more frames than requested", stream_table[apstream->stream_type], __func__);

    /*
     * A-Box can support only Stereo channel, not Mono channel.
     * If platform wants Mono Channel Recording, AudioHAL has to support mono conversion.
     */
    if (apstream->actual_read_status == 0) {
        if (apstream->need_channelconversion && (num_device_channels != num_req_channels)) {
            size_t ret = adjust_channels(proc_buf_out, num_device_channels,
                                         buffer, num_req_channels,
                                         bytes_per_sample, (frames_wr * num_device_channels * bytes_per_sample));
            if (ret != (frames_wr * num_req_channels * bytes_per_sample))
                ALOGE("%s-%s: channel convert failed", stream_table[apstream->stream_type], __func__);
        }
    } else {
        ALOGE("%s-%s: Read Fail = %d", stream_table[apstream->stream_type], __func__, frames_wr);
    }

    return frames_wr;
}

static void check_conversion(struct audio_proxy_stream *apstream)
{
    int request_cc = audio_channel_count_from_in_mask(apstream->requested_channel_mask);

    // Check Mono Conversion is needed or not
    if ((request_cc == MEDIA_1_CHANNEL && apstream->pcmconfig.channels == DEFAULT_MEDIA_CHANNELS)
#ifdef SUPPORT_QUAD_MIC
        || ((request_cc == DEFAULT_MEDIA_CHANNELS || request_cc == MEDIA_1_CHANNEL)
        && apstream->pcmconfig.channels == MEDIA_4_CHANNELS)
#endif
        ) {
        // enable channel Conversion
        apstream->need_channelconversion = true;
        ALOGD("%s-%s: needs re-channeling to %u from %u", stream_table[apstream->stream_type], __func__,
              request_cc, apstream->pcmconfig.channels);
    }

    // Check Re-Sampler is needed or not
    if (apstream->requested_sample_rate &&
        apstream->requested_sample_rate != apstream->pcmconfig.rate) {
        // Only support Stereo Resampling
        if (apstream->resampler) {
            release_resampler(apstream->resampler);
            apstream->resampler = NULL;
        }

        apstream->buf_provider.get_next_buffer = get_next_buffer;
        apstream->buf_provider.release_buffer = release_buffer;
        int ret = create_resampler(apstream->pcmconfig.rate, apstream->requested_sample_rate,
                                   apstream->pcmconfig.channels, RESAMPLER_QUALITY_DEFAULT,
                                   &apstream->buf_provider, &apstream->resampler);
        if (ret !=0) {
            ALOGE("proxy-%s: failed to create resampler", __func__);
        } else {
            ALOGV("proxy-%s: resampler created in-samplerate %d out-samplereate %d",
                  __func__, apstream->pcmconfig.rate, apstream->requested_sample_rate);

            apstream->need_resampling = true;
            ALOGD("%s-%s: needs re-sampling to %u Hz from %u Hz", stream_table[apstream->stream_type], __func__,
                  apstream->requested_sample_rate, apstream->pcmconfig.rate);

            apstream->actual_read_buf = NULL;
            apstream->actual_read_buf_size = 0;
            apstream->read_buf_frames = 0;

            apstream->resampler->reset(apstream->resampler);
        }
    }

    return ;
}

/*
 * Modify config->period_count based on min_size_frames
 */
static void adjust_mmap_period_count(struct audio_proxy_stream *apstream, struct pcm_config *config,
                                     int32_t min_size_frames)
{
    int periodCountRequested = (min_size_frames + config->period_size - 1)
                               / config->period_size;
    int periodCount = MMAP_PERIOD_COUNT_MIN;

    ALOGV("%s-%s: original config.period_size = %d config.period_count = %d",
          stream_table[apstream->stream_type], __func__, config->period_size, config->period_count);

    while (periodCount < periodCountRequested && (periodCount * 2) < MMAP_PERIOD_COUNT_MAX) {
        periodCount *= 2;
    }
    config->period_count = periodCount;

    ALOGV("%s-%s: requested config.period_count = %d", stream_table[apstream->stream_type], __func__,
                                                       config->period_count);
}

int get_mmap_data_fd(void *proxy_stream, audio_usage_type usage_type,
                                                            int *fd, unsigned int *size)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct snd_pcm_mmap_fd mmapfd_info;
    char dev_name[128];
    int hw_fd = -1;
    int ret = 0;
    int hwdev_node = -1;

    memset(&mmapfd_info, 0, sizeof(mmapfd_info));
    mmapfd_info.dir = usage_type;

    // get MMAP device node number based on usage direction
    hwdev_node = ((usage_type ==  AUSAGE_PLAYBACK) ? MMAP_PLAYBACK_DEVICE :
                            MMAP_CAPTURE_DEVICE);
    snprintf(dev_name, sizeof(dev_name), "/dev/snd/hwC0D%d", hwdev_node);
    hw_fd = open(dev_name, O_RDONLY);
    if (hw_fd < 0) {
        ALOGE("%s: hw %s node open failed", __func__, dev_name);
        ret = -1;
        goto err;
    }

    // get mmap fd for exclusive mode
    if (ioctl(hw_fd, SNDRV_PCM_IOCTL_MMAP_DATA_FD, &mmapfd_info) < 0) {
        ALOGE("%s-%s: get MMAP FD IOCTL failed",
		   stream_table[apstream->stream_type], __func__);
        ret = -1;
        goto err;
    }
    *fd = mmapfd_info.fd;
    *size = mmapfd_info.size;

err:
    if (hw_fd >= 0)
        close(hw_fd);
    return ret;
}


/******************************************************************************/
/**                                                                          **/
/** Interfaces for Audio Stream Proxy                                        **/
/**                                                                          **/
/******************************************************************************/

uint32_t proxy_get_actual_channel_count(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t actual_channel_count = 0;

    if (apstream) {
        if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD)
            actual_channel_count = (uint32_t)audio_channel_count_from_out_mask(apstream->comprconfig.codec->ch_in);
        else
            actual_channel_count = (uint32_t)apstream->pcmconfig.channels;
    }

    return actual_channel_count;
}

uint32_t proxy_get_actual_sampling_rate(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t actual_sampling_rate = 0;

    if (apstream) {
        if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD)
            actual_sampling_rate = (uint32_t)apstream->comprconfig.codec->sample_rate;
        else
            actual_sampling_rate = (uint32_t)apstream->pcmconfig.rate;
    }

    return actual_sampling_rate;
}

uint32_t proxy_get_actual_period_size(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t actual_period_size = 0;

    if (apstream) {
        if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD)
            actual_period_size = (uint32_t)apstream->comprconfig.fragment_size;
        else
            actual_period_size = (uint32_t)apstream->pcmconfig.period_size;
    }

    return actual_period_size;
}

uint32_t proxy_get_actual_period_count(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t actual_period_count = 0;

    if (apstream) {
        if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD)
            actual_period_count = (uint32_t)apstream->comprconfig.fragments;
        else
            actual_period_count = (uint32_t)apstream->pcmconfig.period_count;
    }

    return actual_period_count;
}

int32_t proxy_get_actual_format(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int32_t actual_format = (int32_t)AUDIO_FORMAT_INVALID;

    if (apstream) {
        if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD)
            actual_format = (int32_t)apstream->comprconfig.codec->format;
        else
            actual_format = (int32_t)audio_format_from_pcm_format(apstream->pcmconfig.format);
    }

    return actual_format;
}

void  proxy_offload_set_nonblock(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD)
        apstream->nonblock_flag = 1;

    return ;
}

int proxy_offload_compress_func(void *proxy_stream, int func_type)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        if (apstream->compress) {
            switch (func_type) {
                case COMPRESS_TYPE_WAIT:
                    ret = compress_wait(apstream->compress, -1);
                    ALOGVV("%s-%s: returned from waiting", stream_table[apstream->stream_type], __func__);
                    break;

                case COMPRESS_TYPE_NEXTTRACK:
                    ret = compress_next_track(apstream->compress);
                    ALOGI("%s-%s: set next track", stream_table[apstream->stream_type], __func__);
                    break;

                case COMPRESS_TYPE_PARTIALDRAIN:
                    ret = compress_partial_drain(apstream->compress);
                    ALOGI("%s-%s: drained this track partially", stream_table[apstream->stream_type], __func__);

                    /* Resend the metadata for next iteration */
                    apstream->ready_new_metadata = 1;
                    break;

                case COMPRESS_TYPE_DRAIN:
                    ret = compress_drain(apstream->compress);
                    ALOGI("%s-%s: drained this track", stream_table[apstream->stream_type], __func__);
                    break;

                default:
                    ALOGE("%s-%s: unsupported Offload Compress Function(%d)",
                           stream_table[apstream->stream_type], __func__, func_type);
            }
        }
    }

    return ret;
}

int proxy_offload_pause(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        if (apstream->compress) {
            ret = compress_pause(apstream->compress);
            ALOGV("%s-%s: paused compress offload!", stream_table[apstream->stream_type], __func__);
        }
    }

    return ret;
}

int proxy_offload_resume(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        if (apstream->compress) {
            ret = compress_resume(apstream->compress);
            ALOGV("%s-%s: resumed compress offload!", stream_table[apstream->stream_type], __func__);
        }
    }

    return ret;
}


void *proxy_create_playback_stream(void *proxy, int type, void *config, char *address __unused)
{
    struct audio_proxy *aproxy = proxy;
    audio_stream_type stream_type = (audio_stream_type)type;
    struct audio_config *requested_config = (struct audio_config *)config;

    struct audio_proxy_stream *apstream;

    apstream = (struct audio_proxy_stream *)calloc(1, sizeof(struct audio_proxy_stream));
    if (!apstream) {
        ALOGE("proxy-%s: failed to allocate memory for Proxy Stream", __func__);
        return NULL;;
    }

    /* Stores the requested configurations. */
    apstream->requested_sample_rate = requested_config->sample_rate;
    apstream->requested_channel_mask = requested_config->channel_mask;
    apstream->requested_format = requested_config->format;

    apstream->stream_type = stream_type;
    apstream->need_update_pcm_config = false;

    /* Sets basic configuration from Stream Type. */
    switch (apstream->stream_type) {
        // For VTS
        case ASTREAM_PLAYBACK_NO_ATTRIBUTE:
            apstream->sound_card = PRIMARY_PLAYBACK_CARD;
            apstream->sound_device = PRIMARY_PLAYBACK_DEVICE;
            apstream->pcmconfig = pcm_config_primary_playback;

            break;

        case ASTREAM_PLAYBACK_PRIMARY:
            apstream->sound_card = PRIMARY_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_primary_playback;

            if (aproxy->primary_out == NULL)
                aproxy->primary_out = apstream;
            else
                ALOGE("proxy-%s: Primary Output Proxy Stream is already created!!!", __func__);
            break;

        case ASTREAM_PLAYBACK_DEEP_BUFFER:
            apstream->sound_card = DEEP_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_deep_playback;
            break;

        case ASTREAM_PLAYBACK_FAST:
            apstream->sound_card = FAST_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_fast_playback;
            break;

        case ASTREAM_PLAYBACK_LOW_LATENCY:
            apstream->sound_card = LOW_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_low_playback;
            break;

        case ASTREAM_PLAYBACK_COMPR_OFFLOAD:
            apstream->sound_card = OFFLOAD_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->comprconfig = compr_config_offload_playback;
            /* dummy primary pcmconfig used for best match selection */
            apstream->pcmconfig = pcm_config_primary_playback;

            if (is_supported_compressed_format(requested_config->offload_info.format)) {
                apstream->comprconfig.codec = (struct snd_codec *)calloc(1, sizeof(struct snd_codec));
                if (apstream->comprconfig.codec == NULL) {
                    ALOGE("proxy-%s: fail to allocate memory for Sound Codec", __func__);
                    goto err_open;
                }

                apstream->comprconfig.codec->id = get_snd_codec_id(requested_config->offload_info.format);
                apstream->comprconfig.codec->ch_in = requested_config->channel_mask;
                apstream->comprconfig.codec->ch_out = requested_config->channel_mask;
                apstream->comprconfig.codec->sample_rate = requested_config->sample_rate;
                apstream->comprconfig.codec->bit_rate = requested_config->offload_info.bit_rate;
                apstream->comprconfig.codec->format = requested_config->format;

                apstream->ready_new_metadata = 1;
            } else {
                ALOGE("proxy-%s: unsupported Compressed Format(%x)", __func__,
                                                            requested_config->offload_info.format);
                goto err_open;
            }
            break;

        case ASTREAM_PLAYBACK_MMAP:
            apstream->sound_card = MMAP_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_mmap_playback;

            break;

        case ASTREAM_PLAYBACK_AUX_DIGITAL:
            apstream->sound_card = AUX_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_aux_playback;

            if (apstream->requested_sample_rate != 0) {
                apstream->pcmconfig.rate = apstream->requested_sample_rate;
                // It needs Period Size adjustment based with predefined duration
                // to avoid underrun noise by small buffer at high sampling rate
                if (apstream->requested_sample_rate > DEFAULT_MEDIA_SAMPLING_RATE) {
                    apstream->pcmconfig.period_size = (apstream->requested_sample_rate * PREDEFINED_DP_PLAYBACK_DURATION) / 1000;
                    ALOGI("proxy-%s: changed Period Size(%d) as requested sampling rate(%d)",
                          __func__, apstream->pcmconfig.period_size, apstream->pcmconfig.rate);
                }
            }
            if (apstream->requested_channel_mask != AUDIO_CHANNEL_NONE) {
                apstream->pcmconfig.channels = audio_channel_count_from_out_mask(apstream->requested_channel_mask);
            }
            if (apstream->requested_format != AUDIO_FORMAT_DEFAULT) {
                apstream->pcmconfig.format = pcm_format_from_audio_format(apstream->requested_format);
            }

            break;

        case ASTREAM_PLAYBACK_DIRECT:
            apstream->sound_card = DIRECT_PLAYBACK_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_direct_playback;

            apstream->need_channelpadding = false;
            apstream->proc_buf_out = NULL;
            apstream->proc_buf_size = 0;

            /* check whether connected USB device supports requested channels or not */
            if (!(proxy_is_usb_playback_device_connected(aproxy->usb_aproxy) &&
                (int)audio_channel_count_from_out_mask(apstream->requested_channel_mask) <=
                proxy_usb_get_playback_highest_supported_channels(aproxy->usb_aproxy))) {
                if (proxy_is_usb_playback_device_connected(aproxy->usb_aproxy))
                    ALOGE("proxy-%s: Direct stream channel mismatch (request channels %u supported channels %u) ",
                        __func__, audio_channel_count_from_out_mask(apstream->requested_channel_mask),
                        proxy_usb_get_playback_highest_supported_channels(aproxy->usb_aproxy));
                else
                    ALOGE("proxy-%s: Direct stream is not supported for other output devices except USB ", __func__);
                goto err_open;
            }

            /* check whether request configurations are supported by Direct
             * stream or not, and update pcmconfig */
            if (check_direct_config_support(apstream)) {
                ALOGE("proxy-%s: Direct stream unsupported configuration ", __func__);
                goto err_open;
            }
            break;

        default:
            ALOGE("proxy-%s: failed to open Proxy Stream as unknown stream type(%d)", __func__,
                                                                          apstream->stream_type);
            goto err_open;
    }

    apstream->pcm = NULL;
    apstream->compress = NULL;

    ALOGI("proxy-%s: opened Proxy Stream(%s)", __func__, stream_table[apstream->stream_type]);
    return (void *)apstream;

err_open:
    free(apstream);
    return NULL;
}

void proxy_destroy_playback_stream(void *proxy_stream)
{
    struct audio_proxy *aproxy = getInstance();
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    if (apstream) {
        if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
            if (apstream->comprconfig.codec != NULL)
                free(apstream->comprconfig.codec);
        }

        if (apstream->stream_type == ASTREAM_PLAYBACK_PRIMARY) {
            if (aproxy->primary_out != NULL)
                aproxy->primary_out = NULL;
        }

        if (apstream->proc_buf_out)
            free(apstream->proc_buf_out);

        free(apstream);
    }

    return ;
}

int proxy_close_playback_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    /* Close Noamrl PCM Device */
    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        if (apstream->compress) {
            compress_close(apstream->compress);
            apstream->compress = NULL;
        }
        ALOGI("%s-%s: closed Compress Device", stream_table[apstream->stream_type], __func__);
    } else {
        if (apstream->pcm) {
            ret = pcm_close(apstream->pcm);
            apstream->pcm = NULL;
        }
        if (apstream->dma_pcm) {
            pcm_close(apstream->dma_pcm);
            apstream->dma_pcm = NULL;
        }
        ALOGI("%s-%s: closed PCM Device", stream_table[apstream->stream_type], __func__);
    }

    return ret;
}

int proxy_open_playback_stream(void *proxy_stream, int32_t min_size_frames, void *mmap_info)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_proxy *aproxy = getInstance();
    struct audio_mmap_buffer_info *info = (struct audio_mmap_buffer_info *)mmap_info;
    unsigned int sound_card;
    unsigned int sound_device;
    unsigned int flags;
    int ret = 0;
    char pcm_path[MAX_PCM_PATH_LEN];

    /* Get PCM/Compress Device */
    sound_card = apstream->sound_card;
    sound_device = apstream->sound_device;

    /* Open Normal PCM Device */
    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        if (apstream->compress == NULL) {
            flags = COMPRESS_IN;

            apstream->compress = compress_open(sound_card, sound_device, flags, &apstream->comprconfig);
            if (apstream->compress && !is_compress_ready(apstream->compress)) {
                /* compress_open does always return compress structure, not NULL */
                ALOGE("%s-%s: Compress Device is not ready with Sampling_Rate(%u) error(%s)!",
                      stream_table[apstream->stream_type], __func__, apstream->comprconfig.codec->sample_rate,
                      compress_get_error(apstream->compress));
                goto err_open;
            }

            snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/comprC%uD%u", sound_card, sound_device);
            ALOGI("%s-%s: The opened Compress Device is %s with Sampling_Rate(%u) PCM_Format(%d) Fragment_Size(%u)",
                  stream_table[apstream->stream_type], __func__, pcm_path,
                  apstream->comprconfig.codec->sample_rate, apstream->comprconfig.codec->format,
                  apstream->comprconfig.fragment_size);

            apstream->pcm = NULL;
        }
    } else {
        if (apstream->pcm == NULL) {
            struct pcm_config *ppcmconfig = &apstream->pcmconfig;

            if (apstream->stream_type == ASTREAM_PLAYBACK_MMAP) {
                flags = PCM_OUT | PCM_MMAP | PCM_NOIRQ | PCM_MONOTONIC;

                adjust_mmap_period_count(apstream, ppcmconfig, min_size_frames);
            } else
                flags = PCM_OUT | PCM_MONOTONIC;

            apstream->dma_pcm = pcm_open(sound_card, sound_device, flags, ppcmconfig);
            if (apstream->dma_pcm && !pcm_is_ready(apstream->dma_pcm)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("%s-%s: PCM Device is not ready with Sampling_Rate(%u) error(%s)!",
                      stream_table[apstream->stream_type], __func__, ppcmconfig->rate,
                      pcm_get_error(apstream->dma_pcm));
                goto err_open;
            }

            snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c", sound_card, sound_device ,'p');
            ALOGI("%s-%s: The opened PCM Device is %s with Sampling_Rate(%u) PCM_Format(%d)  PCM_start-threshold(%d) PCM_stop-threshold(%d)",
                  stream_table[apstream->stream_type], __func__, pcm_path,
                  ppcmconfig->rate, ppcmconfig->format,
                  ppcmconfig->start_threshold, ppcmconfig->stop_threshold);

            /* virtual pcm handling for Primary & Deep streams */
            if (apstream->stream_type == ASTREAM_PLAYBACK_PRIMARY) {
                /* DMA PCM should be started */
                if (pcm_start(apstream->dma_pcm) == 0) {
                    ALOGI("proxy-%s: PCM Device(%s) with SR(%u) PF(%d) CC(%d) is started",
                          __func__, pcm_path, ppcmconfig->rate, ppcmconfig->format, ppcmconfig->channels);
                } else {
                    ALOGE("proxy-%s: PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                          __func__, pcm_path, ppcmconfig->rate, ppcmconfig->format, ppcmconfig->channels,
                          pcm_get_error(apstream->dma_pcm));
                    goto err_open;
                }

                /* open virtual primary pcm node */
                apstream->pcm = pcm_open(VIRTUAL_PRIMARY_PLAYBACK_CARD, VIRTUAL_PRIMARY_PLAYBACK_DEVICE,
                                            flags, &apstream->pcmconfig);
                if (apstream->pcm && !pcm_is_ready(apstream->pcm)) {
                    /* pcm_open does always return pcm structure, not NULL */
                    ALOGE("%s-%s: Virtual PCM Device is not ready with Sampling_Rate(%u) error(%s)!",
                          stream_table[apstream->stream_type], __func__, apstream->pcmconfig.rate,
                          pcm_get_error(apstream->pcm));
                    goto err_open;
                }

                snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c", VIRTUAL_PRIMARY_PLAYBACK_CARD, VIRTUAL_PRIMARY_PLAYBACK_DEVICE,'p');
                ALOGI("%s-%s: The opened Virtual PCM Device is %s with Sampling_Rate(%u) PCM_Format(%d)  PCM_start-threshold(%d) PCM_stop-threshold(%d)",
                      stream_table[apstream->stream_type], __func__, pcm_path,
                      apstream->pcmconfig.rate, apstream->pcmconfig.format,
                      apstream->pcmconfig.start_threshold, apstream->pcmconfig.stop_threshold);

            } else {
                apstream->pcm = apstream->dma_pcm;
                apstream->dma_pcm = NULL;
            }

            apstream->compress = NULL;

            if (apstream->stream_type == ASTREAM_PLAYBACK_MMAP) {
                unsigned int offset1 = 0;
                unsigned int frames1 = 0;
                unsigned int buf_size = 0;
                unsigned int mmap_size = 0;

                ret = pcm_mmap_begin(apstream->pcm, &info->shared_memory_address, &offset1, &frames1);
                if (ret == 0)  {
                    ALOGI("%s-%s: PCM Device begin MMAP", stream_table[apstream->stream_type], __func__);

                    info->buffer_size_frames = pcm_get_buffer_size(apstream->pcm);
                    buf_size = pcm_frames_to_bytes(apstream->pcm, info->buffer_size_frames);
                    info->burst_size_frames = apstream->pcmconfig.period_size;
                    // get mmap buffer fd
                    ret = get_mmap_data_fd(proxy_stream, AUSAGE_PLAYBACK,
                                                            &info->shared_memory_fd, &mmap_size);
                    if (ret < 0) {
                        // Fall back to poll_fd mode, shared mode
                        info->shared_memory_fd = pcm_get_poll_fd(apstream->pcm);
                        ALOGI("%s-%s: PCM Device MMAP Exclusive mode not support",
                            stream_table[apstream->stream_type], __func__);
                    } else {
                        if (mmap_size < buf_size) {
                            ALOGE("%s-%s: PCM Device MMAP buffer size not matching",
                                  stream_table[apstream->stream_type], __func__);
                            goto err_open;
                        }
                        // FIXME: indicate exclusive mode support by returning a negative buffer size
                        info->buffer_size_frames *= -1;
                    }

                    memset(info->shared_memory_address, 0,
                           pcm_frames_to_bytes(apstream->pcm, info->buffer_size_frames));

                    ret = pcm_mmap_commit(apstream->pcm, 0, MMAP_PERIOD_SIZE);
                    if (ret < 0) {
                        ALOGE("%s-%s: PCM Device cannot commit MMAP with error(%s)",
                              stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                        goto err_open;
                    } else {
                        ALOGI("%s-%s: PCM Device commit MMAP", stream_table[apstream->stream_type], __func__);
                        ret = 0;
                    }
                } else {
                    ALOGE("%s-%s: PCM Device cannot begin MMAP with error(%s)",
                          stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                    goto err_open;
                }
            }
        } else
            ALOGW("%s-%s: PCM Device is already opened!", stream_table[apstream->stream_type], __func__);
    }

    if(aproxy->support_dualspk) {
        if (aproxy->active_playback_device == DEVICE_EARPIECE)
            proxy_set_mixer_value_int(aproxy, SPK_AMPL_POWER_NAME, true);
        else
            proxy_set_mixer_value_int(aproxy, SPK_AMPL_POWER_NAME, aproxy->spk_ampL_powerOn);
    }

    apstream->need_update_pcm_config = false;

    return ret;

err_open:
    proxy_close_playback_stream(proxy_stream);
    return -ENODEV;
}

int proxy_start_playback_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        if (apstream->compress) {
            if (apstream->nonblock_flag) {
                compress_nonblock(apstream->compress, apstream->nonblock_flag);
                ALOGV("%s-%s: set Nonblock mode!", stream_table[apstream->stream_type], __func__);
            } else {
                compress_nonblock(apstream->compress, 0);
                ALOGV("%s-%s: set Block mode!", stream_table[apstream->stream_type], __func__);
            }

            ret = compress_start(apstream->compress);
            if (ret == 0)
                ALOGI("%s-%s: started Compress Device", stream_table[apstream->stream_type], __func__);
            else
                ALOGE("%s-%s: cannot start Compress Offload(%s)", stream_table[apstream->stream_type],
                                               __func__, compress_get_error(apstream->compress));
        } else
            ret = -ENOSYS;
    } else if (apstream->stream_type == ASTREAM_PLAYBACK_MMAP) {
        if (apstream->pcm) {
            ret = pcm_start(apstream->pcm);
            if (ret == 0)
                ALOGI("%s-%s: started MMAP Device", stream_table[apstream->stream_type], __func__);
            else
                ALOGE("%s-%s: cannot start MMAP device with error(%s)", stream_table[apstream->stream_type],
                                               __func__, pcm_get_error(apstream->pcm));
        } else
            ret = -ENOSYS;
    }

    return ret;
}

int proxy_write_playback_buffer(void *proxy_stream, void* buffer, int bytes)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0, wrote = 0;

    /* Skip other sounds except AUX Digital Stream when AUX_DIGITAL is connected */
    if (apstream->stream_type != ASTREAM_PLAYBACK_AUX_DIGITAL &&
        getInstance()->active_playback_device == DEVICE_AUX_DIGITAL) {
        skip_pcm_processing(apstream, wrote);
        wrote = bytes;
        save_written_frames(apstream, wrote);
        return wrote;
     }

    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        if (apstream->compress) {
            if (apstream->ready_new_metadata) {
                compress_set_gapless_metadata(apstream->compress, &apstream->offload_metadata);
                ALOGI("%s-%s: sent gapless metadata(delay = %u, padding = %u) to Compress Device",
                       stream_table[apstream->stream_type], __func__,
                       apstream->offload_metadata.encoder_delay, apstream->offload_metadata.encoder_padding);
                apstream->ready_new_metadata = 0;
        }

            wrote = compress_write(apstream->compress, buffer, bytes);
            ALOGVV("%s-%s: wrote Request(%u bytes) to Compress Device, and Accepted (%u bytes)",
                    stream_table[apstream->stream_type], __func__, (unsigned int)bytes, wrote);
        }
    } else {
        if (apstream->pcm) {
            void *proc_buf_out = buffer;
            int dst_buffer_size = bytes;

            /* Direct stream volume control & channel expanding if needed */
            if (apstream->stream_type == ASTREAM_PLAYBACK_DIRECT && apstream->need_channelpadding) {
                unsigned int bytes_per_src_sample = audio_bytes_per_sample(apstream->requested_format);
                unsigned int bytes_per_dst_sample = (pcm_format_to_bits(apstream->pcmconfig.format) >> 3);
                int num_device_channels = proxy_get_actual_channel_count(apstream);
                int num_req_channels = audio_channel_count_from_out_mask(apstream->requested_channel_mask);

                int frames_num = bytes / (num_req_channels *
                    audio_bytes_per_sample(apstream->requested_format));

                /* Prepare Channel Conversion output Buffer */
                dst_buffer_size = frames_num * num_device_channels * bytes_per_dst_sample;

                if (apstream->proc_buf_size < dst_buffer_size) {
                    apstream->proc_buf_size = dst_buffer_size;
                    apstream->proc_buf_out = realloc(apstream->proc_buf_out, dst_buffer_size);
                    ALOGI("%s-%s: alloc expand channel buffer with %d bytes req_channels %d device_channels %d",
                              stream_table[apstream->stream_type], __func__, dst_buffer_size, num_req_channels, num_device_channels);
                    ALOGI("%s-%s: Channel adjust src-channels %d to %d, bytes per sample src-bytes %d to %d ",
                              stream_table[apstream->stream_type], __func__, num_req_channels,
                              num_device_channels, bytes_per_src_sample, bytes_per_dst_sample);
                }

                /* Assigned allocated buffer as output buffer for channel expanding */
                proc_buf_out = apstream->proc_buf_out;

                /* Adjust channels by adding zeros to audio frame end, for Direct output stream */
                ret = adjust_channels(buffer, num_req_channels,
                            proc_buf_out, num_device_channels,
                            bytes_per_src_sample, bytes);
                if (ret != dst_buffer_size)
                    ALOGE("%s-%s: channel convert failed", stream_table[apstream->stream_type], __func__);

            }

            ret = pcm_write(apstream->pcm, (void *)proc_buf_out, (unsigned int)dst_buffer_size);
            if (ret == 0) {
                ALOGVV("%s-%s: writed %u bytes to PCM Device", stream_table[apstream->stream_type],
                                                               __func__, (unsigned int)bytes);
            } else {
                ALOGE("%s-%s: failed to write to PCM Device with %s",
                      stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                skip_pcm_processing(apstream, wrote);
            }
            wrote = bytes;
            save_written_frames(apstream, wrote);
        }
    }

    return wrote;
}

int proxy_stop_playback_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        if (apstream->compress) {
            ret = compress_stop(apstream->compress);
            if (ret == 0)
                ALOGI("%s-%s: stopped Compress Device", stream_table[apstream->stream_type], __func__);
            else
                ALOGE("%s-%s: cannot stop Compress Offload(%s)", stream_table[apstream->stream_type],
                       __func__, compress_get_error(apstream->compress));

            apstream->ready_new_metadata = 1;
        }
    } else if (apstream->stream_type == ASTREAM_PLAYBACK_MMAP) {
        if (apstream->pcm) {
            ret = pcm_stop(apstream->pcm);
            if (ret == 0)
                ALOGI("%s-%s: stop MMAP Device", stream_table[apstream->stream_type], __func__);
            else
                ALOGE("%s-%s: cannot stop MMAP device with error(%s)", stream_table[apstream->stream_type],
                                               __func__, pcm_get_error(apstream->pcm));
        }
    }

    return ret;
}

int proxy_reconfig_playback_stream(void *proxy_stream, int type, void *config)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    audio_stream_type new_type = (audio_stream_type)type;
    struct audio_config *new_config = (struct audio_config *)config;

    if (apstream) {
        apstream->stream_type = new_type;
        apstream->requested_sample_rate = new_config->sample_rate;
        apstream->requested_channel_mask = new_config->channel_mask;
        apstream->requested_format = new_config->format;

        return 0;
    } else
        return -1;
}

#ifdef SUPPORT_BTA2DP_OFFLOAD
// dummy update of playback buffer for calculating presentation position
int proxy_update_playback_buffer(void *proxy_stream, void *buffer, int bytes)
{
    struct audio_proxy *aproxy = getInstance();
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    // if bt offload on & suspend state or compr case, skip to use temp add routine
    if ((aproxy->a2dp_out_enabled)
            || (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD)) {
        return 0;
    }

    skip_pcm_processing(apstream, 0);
    save_written_frames(apstream, bytes);

    ALOGE("%s-%s: failed to write and just update written buffer byte (%d), apstream->frames (%u)",
          stream_table[apstream->stream_type], __func__, bytes, (unsigned int)apstream->frames);

    return bytes;
}

// dummy calculation of presentation position
int proxy_get_presen_position_temp(void *proxy_stream, uint64_t *frames, struct timespec *timestamp)
{
    struct audio_proxy *aproxy = getInstance();
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = -ENODATA;

    if (aproxy->a2dp_out_enabled) {
         // if bt offload on & suspend state, then get original position from hw
         ret = proxy_get_presen_position(proxy_stream, frames, timestamp);
    } else {
        // use bt offload disbaled state only
        *frames = apstream->frames;
        clock_gettime(CLOCK_MONOTONIC, timestamp);
        ret = 0;
    }
    return ret;
}
#endif

int proxy_get_render_position(void *proxy_stream, uint32_t *frames)
{
    struct audio_proxy *aproxy = getInstance();
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t presented_frames = 0;

    unsigned long hw_frames;
    unsigned int sample_rate = 0;
    int ret = -ENODATA;

    if (frames != NULL) {
        *frames = 0;

        if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
            if (apstream->compress) {
                ret = compress_get_tstamp(apstream->compress, &hw_frames, &sample_rate);
                if (ret == 0) {
                    ALOGVV("%s-%s: rendered frames %u with sample_rate %u",
                           stream_table[apstream->stream_type], __func__, *frames, sample_rate);

                    presented_frames = (uint32_t)hw_frames;
#ifdef SUPPORT_BTA2DP_OFFLOAD
                    if (aproxy->a2dp_out_enabled && is_active_playback_device_bta2dp(aproxy)) {
                        uint32_t a2dp_delay = 0;
                        if (aproxy->a2dp_delay > aproxy->a2dp_default_delay)
                            a2dp_delay = aproxy->a2dp_delay;
                        else
                            a2dp_delay = aproxy->a2dp_default_delay;
                        uint32_t latency_frames = (a2dp_delay *
                                                   proxy_get_actual_sampling_rate(apstream)) / 1000;

                        if (presented_frames > latency_frames)
                            *frames = presented_frames - latency_frames;
                        else
                            ret = -ENODATA;
                    } else
#endif
                        *frames = presented_frames;
                } else
                    ret = -ENODATA;
            }
        }
    } else {
        ALOGE("%s-%s: Invalid Parameter with Null pointer parameter",
              stream_table[apstream->stream_type], __func__);
        ret =  -EINVAL;
    }

    return ret;
}

int proxy_get_presen_position(void *proxy_stream, uint64_t *frames, struct timespec *timestamp)
{
    struct audio_proxy *aproxy = getInstance();
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint64_t presented_frames = 0;

    unsigned long hw_frames;
    unsigned int sample_rate = 0;
    unsigned int avail = 0;
    int ret = -ENODATA;

    if (frames != NULL && timestamp != NULL) {
        *frames = 0;

        if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
            if (apstream->compress) {
                ret = compress_get_tstamp(apstream->compress, &hw_frames, &sample_rate);
                if (ret == 0) {
                    ALOGVV("%s-%s: presented frames %lu with sample_rate %u",
                       stream_table[apstream->stream_type], __func__, hw_frames, sample_rate);

                    presented_frames = (uint64_t)hw_frames;
#ifdef SUPPORT_BTA2DP_OFFLOAD
                    if (aproxy->a2dp_out_enabled && is_active_playback_device_bta2dp(aproxy)) {
                        uint32_t a2dp_delay = 0;
                        if (aproxy->a2dp_delay > aproxy->a2dp_default_delay)
                            a2dp_delay = aproxy->a2dp_delay;
                        else
                            a2dp_delay = aproxy->a2dp_default_delay;
                        uint32_t latency_frames = (a2dp_delay *
                                                   proxy_get_actual_sampling_rate(apstream)) / 1000;

                        if (presented_frames > latency_frames)
                            *frames = presented_frames - latency_frames;
                        else
                            ret = -ENODATA;
                    } else
#endif
                        *frames = presented_frames;

                    clock_gettime(CLOCK_MONOTONIC, timestamp);
                } else
                    ret = -ENODATA;
            }
        } else {
            if (apstream->pcm) {
                ret = pcm_get_htimestamp(apstream->pcm, &avail, timestamp);
                if (ret == 0) {
                    // Total Frame Count in kernel Buffer
                    uint64_t kernel_buffer_size = (uint64_t)apstream->pcmconfig.period_size *
                                                  (uint64_t)apstream->pcmconfig.period_count;

                    // Real frames which played out to device
                    int64_t signed_frames = apstream->frames - kernel_buffer_size + avail;
                    if (signed_frames >= 0) {
                        presented_frames = (uint64_t)signed_frames;
#ifdef SUPPORT_BTA2DP_OFFLOAD
                        if (aproxy->a2dp_out_enabled && is_active_playback_device_bta2dp(aproxy)) {
                            uint32_t a2dp_delay = 0;
                            if (aproxy->a2dp_delay > aproxy->a2dp_default_delay)
                                a2dp_delay = aproxy->a2dp_delay;
                            else
                                a2dp_delay = aproxy->a2dp_default_delay;
                            uint32_t latency_frames = (a2dp_delay *
                                                  proxy_get_actual_sampling_rate(apstream)) / 1000;

                            if (presented_frames > latency_frames)
                                *frames = presented_frames - latency_frames;
                            else
                                ret = -ENODATA;
                        } else
#endif
                            *frames = presented_frames;
                    } else {
                        ret = -ENODATA;
                    }
                } else {
                    ret = -ENODATA;
                }
            }
        }
    } else {
        ALOGE("%s-%s: Invalid Parameter with Null pointer parameter",
              stream_table[apstream->stream_type], __func__);
        ret =  -EINVAL;
    }

    return ret;
}

int proxy_getparam_playback_stream(void *proxy_stream, void *query_params, void *reply_params)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_proxy *aproxy = getInstance();
    struct str_parms *query = (struct str_parms *)query_params;
    struct str_parms *reply = (struct str_parms *)reply_params;

    if (apstream->stream_type == ASTREAM_PLAYBACK_NO_ATTRIBUTE &&
        proxy_is_usb_playback_device_connected(aproxy->usb_aproxy)) {
        // get USB playback param information
        proxy_usb_getparam_playback_stream(aproxy->usb_aproxy, query, reply);
    } else {
        /*
         * Supported Audio Configuration can be different as Target Project.
         * AudioHAL engineers have to modify these codes based on Target Project.
         */
        // supported audio formats
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
            char formats_list[256];

            memset(formats_list, 0, 256);
            strncpy(formats_list, stream_format_table[apstream->stream_type],
                           strlen(stream_format_table[apstream->stream_type]));
            str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_FORMATS, formats_list);
        }

        // supported audio channel masks
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
            char channels_list[256];

            memset(channels_list, 0, 256);
            strncpy(channels_list, stream_channel_table[apstream->stream_type],
                            strlen(stream_channel_table[apstream->stream_type]));
            str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, channels_list);
        }

        // supported audio samspling rates
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
            char rates_list[256];

            memset(rates_list, 0, 256);
            strncpy(rates_list, stream_rate_table[apstream->stream_type],
                         strlen(stream_rate_table[apstream->stream_type]));
            str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES, rates_list);
        }
    }

    return 0;
}

int proxy_setparam_playback_stream(void *proxy_stream, void *parameters)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct str_parms *parms = (struct str_parms *)parameters;

    char value[32];
    int ret = 0;

    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        struct compr_gapless_mdata tmp_mdata;
        bool need_to_set_metadata = false;

        tmp_mdata.encoder_delay = 0;
        tmp_mdata.encoder_padding = 0;

        ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES, value, sizeof(value));
        if (ret >= 0) {
            tmp_mdata.encoder_delay = atoi(value);
            ALOGI("%s-%s: Codec Delay Samples(%u)", stream_table[apstream->stream_type], __func__,
                                                    tmp_mdata.encoder_delay);
            need_to_set_metadata = true;
        }

        ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES, value, sizeof(value));
        if (ret >= 0) {
            tmp_mdata.encoder_padding = atoi(value);
            ALOGI("%s-%s: Codec Padding Samples(%u)", stream_table[apstream->stream_type], __func__,
                                                      tmp_mdata.encoder_padding);
            need_to_set_metadata = true;
        }

        if (need_to_set_metadata) {
            apstream->offload_metadata = tmp_mdata;
            apstream->ready_new_metadata = 1;
        }
    }

    return ret;
}

uint32_t proxy_get_playback_latency(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    uint32_t latency;

    // Total Latency = ALSA Buffer latency + HW Latency
    if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        /*
         * Offload HW latency
         * - A-Box firmware triggers offload play once two buffers of 20msec each are decoded - 20msec
         * - Post processing of decoded data - 10 msec
         * - 20msec extra is provided considering scheduling delays
         * therefore total offload HW latency will 50msec
         */
        latency = 50;
    } else {
        latency = (apstream->pcmconfig.period_count * apstream->pcmconfig.period_size * 1000) / (apstream->pcmconfig.rate);
        latency += 0;   // Need to check HW Latency
    }

    return latency;
}

// select best pcmconfig among requested two configs
bool proxy_select_best_playback_pcmconfig(
    void *proxy,
    void *cur_proxy_stream,
    int compr_upscaler)
{
    struct audio_proxy *aproxy = proxy;
    struct audio_proxy_stream *cur_apstream = (struct audio_proxy_stream *)cur_proxy_stream;

    /* need to update compress stream's dummy pcmconfig based upon upscaler value
     * before selecting best pcmconfig
     * compress offload-upscaler values are defined as shown
     * 0: 48KHz, 16bit
     * 1: 192KHz, 24bit
     * 2: 48KHz, 24bit
     */
    if (cur_apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
        if (compr_upscaler == 2)
            cur_apstream->pcmconfig = pcm_config_deep_playback;
        else if (compr_upscaler == 1)
            cur_apstream->pcmconfig = pcm_config_deep_playback_uhqa;
        else
            cur_apstream->pcmconfig = pcm_config_primary_playback;

        ALOGI("%s-%s: upscaler: %d pcmconfig rate[%d] format[%d]",
            stream_table[cur_apstream->stream_type], __func__, compr_upscaler,
            cur_apstream->pcmconfig.rate, cur_apstream->pcmconfig.format);
    }

    return proxy_usb_out_pick_best_pcmconfig(aproxy->usb_aproxy, cur_apstream->pcmconfig);
}

/* selecting best playback pcm config to configure USB device */
void proxy_set_best_playback_pcmconfig(void *proxy, void *proxy_stream)
{
    struct audio_proxy *aproxy = proxy;
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    bool reprepare_needed = false;

    if (!aproxy->usb_aproxy) {
        ALOGI("%s-%s: USB audio offload is not initialized",
            stream_table[apstream->stream_type], __func__);
        return;
    }

    /* update & check whether USB device re-configuraiton is required for best config */
    reprepare_needed = proxy_usb_out_reconfig_needed(aproxy->usb_aproxy);

    if ((aproxy->active_playback_device == DEVICE_USB_HEADSET
        || aproxy->active_playback_device == DEVICE_SPEAKER_AND_USB_HEADSET)
        && !aproxy->is_usb_single_clksrc && !is_usage_CPCall(aproxy->active_playback_ausage)
        && reprepare_needed) {
        /* steps for re-configuring USB device configuration
         * - Close WDMA6 & usb out pcm,
         * - prepare usb for new config,
         * - modify sifs0 setting to new selected PCM config
         * - open wdma6 & usb out pcm
         */
        /* close loopback and USB pcm nodes */
        disable_usb_out_loopback(aproxy);
        proxy_usb_close_out_proxy(aproxy->usb_aproxy);

        /* Prepare USB device for new configuraiton */
        proxy_usb_playback_prepare(aproxy->usb_aproxy, true);
        set_usb_playback_modifier(aproxy);

        /* re-open loopback and USB pcm nodes */
        proxy_usb_open_out_proxy(aproxy->usb_aproxy);
        enable_usb_out_loopback(aproxy);
        ALOGI("%s-%s: USB Device re-configured",
            stream_table[apstream->stream_type], __func__);
    }

    return;
}

/* reset playback pcm config for USB device default */
void proxy_reset_playback_pcmconfig(void *proxy)
{
    struct audio_proxy *aproxy = proxy;

    /* reset USB playback config to default values */
    proxy_usb_out_reset_config(aproxy->usb_aproxy);

    return;
}

void proxy_dump_playback_stream(void *proxy_stream, int fd)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    const size_t len = 256;
    char buffer[len];

    if (apstream->pcm != NULL) {
        snprintf(buffer, len, "\toutput pcm config sample rate: %d\n",apstream->pcmconfig.rate);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\toutput pcm config period size : %d\n",apstream->pcmconfig.period_size);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\toutput pcm config format: %d\n",apstream->pcmconfig.format);
        write(fd,buffer,strlen(buffer));
    }

    if (apstream->compress != NULL) {
        if (apstream->comprconfig.codec != NULL) {
            snprintf(buffer, len, "\toutput offload codec id: %d\n",apstream->comprconfig.codec->id);
            write(fd,buffer,strlen(buffer));
            snprintf(buffer, len, "\toutput offload codec input channel: %d\n",apstream->comprconfig.codec->ch_in);
            write(fd,buffer,strlen(buffer));
            snprintf(buffer, len, "\toutput offload codec output channel: %d\n",apstream->comprconfig.codec->ch_out);
            write(fd,buffer,strlen(buffer));
            snprintf(buffer, len, "\toutput offload sample rate: %d\n",apstream->comprconfig.codec->sample_rate);
            write(fd,buffer,strlen(buffer));
            snprintf(buffer, len, "\toutput offload bit rate : %d\n",apstream->comprconfig.codec->bit_rate);
            write(fd,buffer,strlen(buffer));
            snprintf(buffer, len, "\toutput offload config format: %d\n",apstream->comprconfig.codec->format);
            write(fd,buffer,strlen(buffer));
        }

        snprintf(buffer, len, "\tOffload Fragment Size: %d\n",apstream->comprconfig.fragment_size);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\tOffload Fragments: %d\n",apstream->comprconfig.fragments);
        write(fd,buffer,strlen(buffer));
    }

    return ;
}


void *proxy_create_capture_stream(void *proxy, int type, int usage, void *config, char *address __unused)
{
    struct audio_proxy *aproxy = proxy;
    audio_stream_type stream_type = (audio_stream_type)type;
    audio_usage       stream_usage = (audio_usage)usage;
    struct audio_config *requested_config = (struct audio_config *)config;

    struct audio_proxy_stream *apstream;

    apstream = (struct audio_proxy_stream *)calloc(1, sizeof(struct audio_proxy_stream));
    if (!apstream) {
        ALOGE("proxy-%s: failed to allocate memory for Proxy Stream", __func__);
        return NULL;;
    }

    /* Stores the requested configurationss */
    apstream->requested_sample_rate = requested_config->sample_rate;
    apstream->requested_channel_mask = requested_config->channel_mask;
    apstream->requested_format = requested_config->format;

    apstream->stream_type = stream_type;
    apstream->stream_usage = stream_usage;

    // Initialize Post-Processing
    apstream->need_channelconversion = false;
    apstream->need_resampling = false;

    apstream->actual_read_buf = NULL;
    apstream->actual_read_buf_size = 0;

    apstream->proc_buf_out = NULL;
    apstream->proc_buf_size = 0;

    apstream->resampler = NULL;

    apstream->need_update_pcm_config = false;
    apstream->skip_ch_convert = false;

    /* Sets basic configuration from Stream Type. */
    switch (apstream->stream_type) {
        // For VTS
        case ASTREAM_CAPTURE_NO_ATTRIBUTE:
            apstream->sound_card = PRIMARY_CAPTURE_CARD;
            apstream->sound_device = PRIMARY_CAPTURE_DEVICE;
            apstream->pcmconfig = pcm_config_primary_capture;

            break;

        case ASTREAM_CAPTURE_PRIMARY:
            apstream->sound_card = PRIMARY_CAPTURE_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
#ifdef SUPPORT_QUAD_MIC
            if ((is_active_usage_CPCall(aproxy)
                || is_active_usage_APCall(aproxy)
                || apstream->stream_usage == AUSAGE_CAMCORDER)
                && is_quad_mic_device(aproxy->active_capture_device)) {
                apstream->pcmconfig = pcm_config_primary_quad_mic_capture;
                ALOGE("proxy-%s: Primary reconfig as Quad-Mic", __func__);
            } else
#endif
                apstream->pcmconfig = pcm_config_primary_capture;

            update_capture_pcmconfig(apstream);
            check_conversion(apstream);
            break;

        case ASTREAM_CAPTURE_CALL:
            apstream->sound_card = CALL_RECORD_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_call_record;

            check_conversion(apstream);
            break;

        case ASTREAM_CAPTURE_LOW_LATENCY:
            apstream->sound_card = LOW_CAPTURE_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_low_capture;

            update_capture_pcmconfig(apstream);
            check_conversion(apstream);
            break;

        case ASTREAM_CAPTURE_MMAP:
            apstream->sound_card = MMAP_CAPTURE_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_mmap_capture;

            /* update HW PCM configuration with requested config, as MMAP usage cann't
                use software conversions for sample rate and channels, format is fixed to
                16bit */
            if (apstream->requested_sample_rate != apstream->pcmconfig.rate) {
                apstream->pcmconfig.rate = apstream->requested_sample_rate;
                // Adjust period_size according to sample rate
                apstream->pcmconfig.period_size = (apstream->pcmconfig.rate * PREDEFINED_MMAP_CAPTURE_DURATION) / 1000;

                // WDMA in A-Box is 128-bit aligned, so period_size has to be multiple of 4 frames
                apstream->pcmconfig.period_size &= 0xFFFFFFFC;
                ALOGD("%s-%s: updates samplig rate to %u, period_size to %u",
                    stream_table[apstream->stream_type], __func__,
                    apstream->pcmconfig.rate, apstream->pcmconfig.period_size);
            }

            if (audio_channel_count_from_in_mask(apstream->requested_channel_mask)
                != apstream->pcmconfig.channels) {
                apstream->pcmconfig.channels = audio_channel_count_from_in_mask(apstream->requested_channel_mask);
                ALOGD("%s-%s: updates channel count to %u", stream_table[apstream->stream_type],
                                                            __func__, apstream->pcmconfig.channels);
            }
            break;

        case ASTREAM_CAPTURE_FM_TUNER:
        case ASTREAM_CAPTURE_FM_RECORDING:
            apstream->sound_card = FM_RECORD_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_fm_record;

            check_conversion(apstream);
            break;

#ifdef SUPPORT_STHAL_INTERFACE
        case ASTREAM_CAPTURE_HOTWORD:
            apstream->pcmconfig = pcm_config_hotword_capture;
            break;
#endif

        default:
            ALOGE("proxy-%s: failed to open Proxy Stream as unknown stream type(%d)", __func__,
                                                                          apstream->stream_type);
            goto err_open;
    }

    apstream->pcm = NULL;
    apstream->compress = NULL;

    ALOGI("proxy-%s: opened Proxy Stream(%s)", __func__, stream_table[apstream->stream_type]);
    return (void *)apstream;

err_open:
    free(apstream);
    return NULL;
}

void proxy_destroy_capture_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    if (apstream) {
        if (apstream->resampler) {
            ALOGV("%s-%s: released resampler", stream_table[apstream->stream_type], __func__);
            release_resampler(apstream->resampler);
        }

        if (apstream->actual_read_buf)
            free(apstream->actual_read_buf);

        if (apstream->proc_buf_out)
            free(apstream->proc_buf_out);

        free(apstream);
    }

    return ;
}

int proxy_close_capture_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_proxy *aproxy = getInstance();
    int ret = 0;

#ifdef SUPPORT_STHAL_INTERFACE
    /* Handle HOTWORD soure separately */
    if (apstream->stream_type == ASTREAM_CAPTURE_HOTWORD) {
        if (aproxy->sound_trigger_close_for_streaming) {
            if (apstream->soundtrigger_handle > 0) {
                if (apstream->stream_usage == AUSAGE_HOTWORD_SEAMLESS) {
                    aproxy->sound_trigger_close_for_streaming(apstream->soundtrigger_handle);
                } else {
                    aproxy->sound_trigger_close_recording(apstream->soundtrigger_handle);
                }
            }

            apstream->soundtrigger_handle = 0;
#ifdef SEAMLESS_DUMP
            if (apstream->fp)
                fclose(apstream->fp);
#endif
            ALOGI("VTS PCM Node closed");
        } else {
            ALOGE("%s-%s: SoundTrigger HAL Close function Not available!",
                                    stream_table[apstream->stream_type], __func__);
            ret = -EIO;
        }

        return ret;
    }
#endif

    /* Close Normal PCM Device */
    if (apstream->pcm) {
        ret = pcm_close(apstream->pcm);
        apstream->pcm = NULL;
    }
    if (apstream->dma_pcm) {
        pcm_close(apstream->dma_pcm);
        apstream->dma_pcm = NULL;
    }
    ALOGI("%s-%s: closed PCM Device", stream_table[apstream->stream_type], __func__);

    return ret;
}

int proxy_open_capture_stream(void *proxy_stream, int32_t min_size_frames, void *mmap_info)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_proxy *aproxy = getInstance();
    struct audio_mmap_buffer_info *info = (struct audio_mmap_buffer_info *)mmap_info;
    unsigned int sound_card;
    unsigned int sound_device;
    unsigned int flags;
    int ret = 0;
    char pcm_path[MAX_PCM_PATH_LEN];

#ifdef SUPPORT_STHAL_INTERFACE
    /* Handle HOTWORD soure separately */
    if (apstream->stream_type == ASTREAM_CAPTURE_HOTWORD) {
        if (aproxy->sound_trigger_open_for_streaming) {
            if (apstream->stream_usage == AUSAGE_HOTWORD_SEAMLESS) {
                apstream->soundtrigger_handle = aproxy->sound_trigger_open_for_streaming();
            } else {
                apstream->soundtrigger_handle = aproxy->sound_trigger_open_recording();
            }
            if (apstream->soundtrigger_handle <= 0) {
                ALOGE("%s: Failed to open VTS PCM Node for streaming", __func__);
                ret = -EIO;
                goto err_open;
            }
#ifdef SEAMLESS_DUMP
            apstream->fp = fopen("/data/seamdump.raw", "wr+");
            if (!apstream->fp)
                ALOGI("failed to open /data/seamdump.raw");
#endif
            ALOGI("Opened VTS PCM Node successfully");
        } else {
            ALOGE("%s-%s: SoundTrigger HAL Open function Not available!",
                        stream_table[apstream->stream_type], __func__);
            ret = -EIO;
        }

        apstream->need_update_pcm_config = false;

        return ret;
    }
#endif

    if (is_active_usage_APCall(aproxy) && apstream->pcmconfig.rate != 48000) {
        apstream->sound_card = PRIMARY_CAPTURE_CARD;
        apstream->sound_device = get_pcm_device_number(aproxy, apstream);
        apstream->pcmconfig = pcm_config_primary_capture;

        check_conversion(apstream);
    }

    /* Get PCM Device */
    sound_card = apstream->sound_card;
    sound_device = apstream->sound_device;

    /* Open Normal PCM Device */
    if (apstream->pcm == NULL) {
        if (apstream->stream_type == ASTREAM_CAPTURE_MMAP) {
            flags = PCM_IN | PCM_MMAP | PCM_NOIRQ | PCM_MONOTONIC;

            adjust_mmap_period_count(apstream, &apstream->pcmconfig, min_size_frames);
        } else
            flags = PCM_IN | PCM_MONOTONIC;

        /* open WDMA pcm first to trigger DMA */
        apstream->dma_pcm = pcm_open(sound_card, sound_device, flags, &apstream->pcmconfig);
        if (apstream->dma_pcm && !pcm_is_ready(apstream->dma_pcm)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("%s-%s: PCM Device is not ready with Sampling_Rate(%u) error(%s)!",
                  stream_table[apstream->stream_type], __func__, apstream->pcmconfig.rate,
                  pcm_get_error(apstream->dma_pcm));
            goto err_open;
        }

        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c", sound_card, sound_device, 'c');

        ALOGI("%s-%s: The opened PCM Device is %s with Sampling_Rate(%u) PCM_Format(%d) Channel(%d)",
              stream_table[apstream->stream_type], __func__, pcm_path,
              apstream->pcmconfig.rate, apstream->pcmconfig.format, apstream->pcmconfig.channels);

        /* Virtual dai PCM is required only for normal capture */
        if (apstream->stream_type != ASTREAM_CAPTURE_LOW_LATENCY &&
            apstream->stream_type != ASTREAM_CAPTURE_CALL &&
            apstream->stream_type != ASTREAM_CAPTURE_FM_TUNER &&
            apstream->stream_type != ASTREAM_CAPTURE_FM_RECORDING &&
            apstream->stream_type != ASTREAM_CAPTURE_MMAP) {
            /* WDMA pcm should be started before opening virtual pcm */
            if (pcm_start(apstream->dma_pcm) == 0) {
                ALOGI("proxy-%s: PCM Device(%s) with SR(%u) PF(%d) CC(%d) is started",
                      __func__, pcm_path, apstream->pcmconfig.rate, apstream->pcmconfig.format, apstream->pcmconfig.channels);
            } else {
                ALOGE("proxy-%s: PCM Device(%s) with SR(%u) PF(%d) CC(%d) cannot be started as error(%s)",
                      __func__, pcm_path, apstream->pcmconfig.rate, apstream->pcmconfig.format, apstream->pcmconfig.channels,
                      pcm_get_error(apstream->dma_pcm));
                goto err_open;
            }

            apstream->pcm = pcm_open(VIRTUAL_PRIMARY_CAPTURE_CARD, VIRTUAL_PRIMARY_CAPTURE_DEVICE, flags, &apstream->pcmconfig);
            if (apstream->pcm && !pcm_is_ready(apstream->pcm)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("%s-%s: Virtual PCM Device is not ready with Sampling_Rate(%u) error(%s)!",
                      stream_table[apstream->stream_type], __func__, apstream->pcmconfig.rate,
                      pcm_get_error(apstream->pcm));
                goto err_open;
            }

            snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c", VIRTUAL_PRIMARY_CAPTURE_CARD, VIRTUAL_PRIMARY_CAPTURE_DEVICE, 'c');
            ALOGI("%s-%s: The opened Virtual PCM Device is %s with Sampling_Rate(%u) PCM_Format(%d) Channel(%d)",
                  stream_table[apstream->stream_type], __func__, pcm_path,
                  apstream->pcmconfig.rate, apstream->pcmconfig.format, apstream->pcmconfig.channels);
        } else {
            apstream->pcm = apstream->dma_pcm;
            apstream->dma_pcm = NULL;
        }

        apstream->compress = NULL;

        if (apstream->stream_type == ASTREAM_CAPTURE_MMAP) {
            unsigned int offset1 = 0;
            unsigned int frames1 = 0;
            unsigned int buf_size = 0;
            unsigned int mmap_size = 0;

            ret = pcm_mmap_begin(apstream->pcm, &info->shared_memory_address, &offset1, &frames1);
            if (ret == 0)  {
                ALOGI("%s-%s: PCM Device begin MMAP", stream_table[apstream->stream_type], __func__);

                info->buffer_size_frames = pcm_get_buffer_size(apstream->pcm);
                buf_size = pcm_frames_to_bytes(apstream->pcm, info->buffer_size_frames);
                info->burst_size_frames = apstream->pcmconfig.period_size;
                // get mmap buffer fd
                ret = get_mmap_data_fd(proxy_stream, AUSAGE_CAPTURE,
                                                        &info->shared_memory_fd, &mmap_size);
                if (ret < 0) {
                    // Fall back to poll_fd mode, shared mode
                    info->shared_memory_fd = pcm_get_poll_fd(apstream->pcm);
                    ALOGI("%s-%s: PCM Device MMAP Exclusive mode not support",
                        stream_table[apstream->stream_type], __func__);
                } else {
                    if (mmap_size < buf_size) {
                        ALOGE("%s-%s: PCM Device MMAP buffer size not matching",
                              stream_table[apstream->stream_type], __func__);
                        goto err_open;
                    }
                    // FIXME: indicate exclusive mode support by returning a negative buffer size
                    info->buffer_size_frames *= -1;
                }

                memset(info->shared_memory_address, 0,
                       pcm_frames_to_bytes(apstream->pcm, info->buffer_size_frames));

                ret = pcm_mmap_commit(apstream->pcm, 0, MMAP_PERIOD_SIZE);
                if (ret < 0) {
                    ALOGE("%s-%s: PCM Device cannot commit MMAP with error(%s)",
                          stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                    goto err_open;
                } else {
                    ALOGI("%s-%s: PCM Device commit MMAP", stream_table[apstream->stream_type], __func__);
                    ret = 0;
                }
            } else {
                ALOGE("%s-%s: PCM Device cannot begin MMAP with error(%s)",
                      stream_table[apstream->stream_type], __func__, pcm_get_error(apstream->pcm));
                goto err_open;
            }
        }

        /* HACK for MMAP/Low-latency capture path routing, normal recording uses virtualPCM DAI
        * firmware component but MMAP/Low-latency case for reducing latency we have to capture
        * data directly from WDMA, therefore VirtualPCM DAI is disabled after routing */
        if (apstream->stream_type == ASTREAM_CAPTURE_MMAP ||
            apstream->stream_type == ASTREAM_CAPTURE_LOW_LATENCY) {
            proxy_set_mixer_value_string(aproxy, MIXER_CTL_ABOX_CATPURE_VPCMDAI_INSRC, "None");
            ALOGI("%s-%s: MMAP VPCMIN_DAI0 component disconnect forcefully",
                        stream_table[apstream->stream_type], __func__);
        }
    } else
        ALOGW("%s-%s: PCM Device is already opened!", stream_table[apstream->stream_type], __func__);

    apstream->need_update_pcm_config = false;

    return ret;

err_open:
    proxy_close_capture_stream(proxy_stream);
    return -ENODEV;
}

int proxy_start_capture_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

#ifdef SUPPORT_STHAL_INTERFACE
    /* Handle HOTWORD soure separately */
    if (apstream->stream_type == ASTREAM_CAPTURE_HOTWORD)
        return ret;
#endif

    // In case of PCM Playback, pcm_start call is not needed as auto-start
    if (apstream->pcm) {
        ret = pcm_start(apstream->pcm);
        if (ret == 0)
            ALOGI("%s-%s: started PCM Device", stream_table[apstream->stream_type], __func__);
        else
            ALOGE("%s-%s: cannot start PCM(%s)", stream_table[apstream->stream_type], __func__,
                                                 pcm_get_error(apstream->pcm));
    }

    return ret;
}

int proxy_read_capture_buffer(void *proxy_stream, void *buffer, int bytes)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int frames_request = bytes / proxy_get_requested_frame_size(apstream);
    int frames_actual = -1;

    if (apstream->skip_ch_convert) {
        frames_request = bytes / (proxy_get_actual_channel_count(apstream) *
                            audio_bytes_per_sample(apstream->requested_format));
    }

#ifdef SUPPORT_STHAL_INTERFACE
    int ret = 0, read = 0;
    struct audio_proxy *aproxy = getInstance();
    if (apstream->stream_type == ASTREAM_CAPTURE_HOTWORD) {
        if (aproxy->sound_trigger_read_samples) {
            if (apstream->soundtrigger_handle > 0) {
                if (apstream->stream_usage == AUSAGE_HOTWORD_SEAMLESS) {
                    ret = aproxy->sound_trigger_read_samples(apstream->soundtrigger_handle,
                                                                        buffer, bytes);
                } else {
                    ret = aproxy->sound_trigger_read_recording_samples(buffer, bytes);
                }

                if (!ret) {
                    read = bytes;
#ifdef SEAMLESS_DUMP
                    if (apstream->fp ) {
                        fwrite((void*)buffer, bytes, 1, apstream->fp);
                        ALOGE("Model binary /data/seamdump.raw write completed");
                    } else
                        ALOGE("Error opening /sdcard/seamdump.raw");
#endif
                }
            }
        } else {
            ALOGE("%s-%s: SoundTrigger HAL Read function Not available!",
                        stream_table[apstream->stream_type], __func__);
        }

        return read;
    } else
#endif
    {
        frames_actual = read_and_process_frames(apstream, buffer, frames_request);
        ALOGVV("%s-%s: requested read frames = %d vs. actual processed read frames = %d",
               stream_table[apstream->stream_type], __func__, frames_request, frames_actual);
    }

    if (frames_actual < 0) {
        return frames_actual;
    } else {
        /* Saves read frames to calcurate timestamp */
        apstream->frames += frames_actual;
        ALOGVV("%s-%s: cumulative read = %u frames", stream_table[apstream->stream_type], __func__,
                                          (unsigned int)apstream->frames);
        return bytes;
    }
}

int proxy_stop_capture_stream(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    int ret = 0;

#ifdef SUPPORT_STHAL_INTERFACE
    /* Handle HOTWORD soure separately */
    if (apstream->stream_type == ASTREAM_CAPTURE_HOTWORD)
        return ret;
#endif

    if (apstream->pcm) {
        ret = pcm_stop(apstream->pcm);
        if (ret == 0)
            ALOGI("%s-%s: stopped PCM Device", stream_table[apstream->stream_type], __func__);
        else
            ALOGE("%s-%s: cannot stop PCM(%s)", stream_table[apstream->stream_type], __func__,
                                                pcm_get_error(apstream->pcm));
    }

    return ret;
}

int proxy_reconfig_capture_stream(void *proxy_stream, int type, void *config)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    audio_stream_type new_type = (audio_stream_type)type;
    struct audio_config *new_config = (struct audio_config *)config;

    if (apstream) {
        apstream->stream_type = new_type;
        apstream->requested_sample_rate = new_config->sample_rate;
        apstream->requested_channel_mask = new_config->channel_mask;
        apstream->requested_format = new_config->format;

        // If some stream types need to be reset, it has to reconfigure conversions

        return 0;
    } else
        return -1;
}

int proxy_reconfig_capture_usage(void *proxy_stream, int type, int usage)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    if (apstream == NULL)
        return -1;

    struct audio_proxy *aproxy = getInstance();
    audio_stream_type stream_type = (audio_stream_type)type;
    audio_usage       stream_usage = (audio_usage)usage;

    if (stream_usage != AUSAGE_NONE)
        apstream->stream_usage = stream_usage;

     switch (stream_type) {
        case ASTREAM_CAPTURE_PRIMARY:
            apstream->stream_type = stream_type;
            apstream->sound_card = PRIMARY_CAPTURE_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);

#ifdef SUPPORT_QUAD_MIC
            if ((is_active_usage_CPCall(aproxy)
                || is_active_usage_APCall(aproxy)
                || apstream->stream_usage == AUSAGE_CAMCORDER)
                && is_quad_mic_device(aproxy->active_capture_device)) {
                apstream->pcmconfig = pcm_config_primary_quad_mic_capture;
                ALOGE("proxy-%s: Primary reconfig as Quad-Mic", __func__);
            } else
#endif
                apstream->pcmconfig = pcm_config_primary_capture;

            update_capture_pcmconfig(apstream);

            /*
            ** Reset previous configurations and release resampler if running
            ** for reconfiguration purpose
            */
            apstream->need_channelconversion = false;
            if (apstream->resampler) {
                ALOGI("%s-%s: released resampler", stream_table[apstream->stream_type], __func__);
                release_resampler(apstream->resampler);
                apstream->resampler = NULL;
            }

            check_conversion(apstream);
            break;

        case ASTREAM_CAPTURE_CALL:
            apstream->stream_type = stream_type;
            apstream->sound_card = CALL_RECORD_CARD;
            apstream->sound_device = get_pcm_device_number(aproxy, apstream);
            apstream->pcmconfig = pcm_config_call_record;

            check_conversion(apstream);
            break;
        default:
            ALOGE("proxy-%s: failed to reconfig Proxy Stream as unknown stream type(%d)", __func__, stream_type);
            return -1;
    }

    ALOGI("proxy-%s: reconfig Proxy Stream(%s)", __func__, stream_table[apstream->stream_type]);

    return 0;
}

int proxy_get_capture_pos(void *proxy_stream, int64_t *frames, int64_t *time)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    unsigned int avail = 0;
    struct timespec timestamp;
    int ret = -ENOSYS;;

    if (frames != NULL && time != NULL) {
        *frames = 0;
        *time = 0;

        if (apstream->pcm) {
            ret = pcm_get_htimestamp(apstream->pcm, &avail, &timestamp);
            if (ret == 0) {
                // Real frames which captured in from device
                *frames = apstream->frames + avail;
                // Nano Seconds Unit Time
                *time = timestamp.tv_sec * 1000000000LL + timestamp.tv_nsec;
                ret = 0;
            }
        }
    } else {
        ALOGE("%s-%s: Invalid Parameter with Null pointer parameter",
              stream_table[apstream->stream_type], __func__);
        ret =  -EINVAL;
    }

    return ret;
}

int proxy_get_active_microphones(void *proxy_stream, void *array, int *count)
{
    struct audio_proxy *aproxy = getInstance();
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_microphone_characteristic_t *mic_array = array;
    size_t *mic_count = (size_t *)count;
    size_t actual_mic_count = 0;
    int ret = 0;

    if (apstream) {
        if (apstream->stream_type == ASTREAM_CAPTURE_NO_ATTRIBUTE ||
            apstream->stream_type == ASTREAM_CAPTURE_PRIMARY ||
            apstream->stream_type == ASTREAM_CAPTURE_LOW_LATENCY ||
            apstream->stream_type == ASTREAM_CAPTURE_MMAP) {
            device_type active_device = aproxy->active_capture_device;
            if (active_device == DEVICE_NONE) {
                ALOGE("%s-%s: There are no active MIC", stream_table[apstream->stream_type], __func__);
                ret = -ENOSYS;
            }

            if (*mic_count == 0) {
                if (active_device == DEVICE_STEREO_MIC)
                    actual_mic_count = 2;
                else
                    actual_mic_count = 1;
                ALOGI("proxy-%s: requested number of microphone, return %zu", __func__, *mic_count);
            } else {
                if (active_device == DEVICE_STEREO_MIC) {
                    for (int i = 0; i < 2; i++) {
                        mic_array[i] = aproxy->mic_info[i];
                        ALOGD("%s-%s: %dth MIC = %s", stream_table[apstream->stream_type], __func__,
                                                      i+1, mic_array[i].device_id);
                        actual_mic_count++;
                    }
                } else if (active_device == DEVICE_MAIN_MIC) {
                        mic_array[0] = aproxy->mic_info[0];
                        ALOGD("%s-%s: Active MIC = %s", stream_table[apstream->stream_type], __func__,
                                                        mic_array[0].device_id);
                        actual_mic_count = 1;
                } else if (active_device == DEVICE_SUB_MIC) {
                        mic_array[0] = aproxy->mic_info[1];
                        ALOGD("%s-%s: Active MIC = %s", stream_table[apstream->stream_type], __func__,
                                                        mic_array[0].device_id);
                        actual_mic_count = 1;
                } else {
                    ALOGE("%s-%s: Abnormal active device(%s)", stream_table[apstream->stream_type],
                                                               __func__, device_table[active_device]);
                    ret = -ENOSYS;
                }
            }
        } else {
            ALOGE("%s-%s: This stream doesn't have active MIC", stream_table[apstream->stream_type],
                                                                __func__);
            ret = -ENOSYS;
        }
    } else {
        ALOGE("proxy-%s: apstream is NULL", __func__);
        ret = -ENOSYS;
    }

    *mic_count = actual_mic_count;

    return ret;
}

int proxy_getparam_capture_stream(void *proxy_stream, void *query_params, void *reply_params)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_proxy *aproxy = getInstance();
    struct str_parms *query = (struct str_parms *)query_params;
    struct str_parms *reply = (struct str_parms *)reply_params;

    if (proxy_is_usb_capture_device_connected(aproxy->usb_aproxy)) {
        // get USB capture param information
        proxy_usb_getparam_capture_stream(getInstance()->usb_aproxy, query, reply);
    } else {
        /*
         * Supported Audio Configuration can be different as Target Project.
         * AudioHAL engineers have to modify these codes based on Target Project.
         */
        // supported audio formats
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
            char formats_list[256];

            memset(formats_list, 0, 256);
            strncpy(formats_list, stream_format_table[apstream->stream_type],
                           strlen(stream_format_table[apstream->stream_type]));
            str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_FORMATS, formats_list);
        }

        // supported audio channel masks
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
            char channels_list[256];

            memset(channels_list, 0, 256);
            strncpy(channels_list, stream_channel_table[apstream->stream_type],
                            strlen(stream_channel_table[apstream->stream_type]));
            str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, channels_list);
        }

        // supported audio samspling rates
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
            char rates_list[256];

            memset(rates_list, 0, 256);
            strncpy(rates_list, stream_rate_table[apstream->stream_type],
                         strlen(stream_rate_table[apstream->stream_type]));
            str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES, rates_list);
        }
    }

    return 0;
}

int proxy_setparam_capture_stream(void *proxy_stream, void *parameters)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_proxy *aproxy = getInstance();
    int ret = 0;

    if (proxy_is_usb_capture_device_connected(aproxy->usb_aproxy)) {
        /* Set USB parameters */
        ret = proxy_usb_setparam_capture_stream(aproxy->usb_aproxy, parameters);
    }

    return ret;
}

void proxy_dump_capture_stream(void *proxy_stream, int fd)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    const size_t len = 256;
    char buffer[len];

    if (apstream->pcm != NULL) {
        snprintf(buffer, len, "\tinput pcm config sample rate: %d\n",apstream->pcmconfig.rate);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\tinput pcm config period size : %d\n",apstream->pcmconfig.period_size);
        write(fd,buffer,strlen(buffer));
        snprintf(buffer, len, "\tinput pcm config format: %d\n",apstream->pcmconfig.format);
        write(fd,buffer,strlen(buffer));
    }

    return ;
}

void proxy_update_capture_usage(void *proxy_stream, int usage)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    audio_usage    stream_usage = (audio_usage)usage;

    if(apstream) {
        apstream->stream_usage = stream_usage;
        ALOGD("proxy-%s: apstream->stream_usage = %d", __func__, apstream->stream_usage);
    } else {
        ALOGD("proxy-%s: apstream is NULL", __func__);
    }
    return ;
}

int proxy_get_mmap_position(void *proxy_stream, void *pos)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    struct audio_mmap_position *position = (struct audio_mmap_position *)pos;
    int ret = -ENOSYS;

    if ((apstream->stream_type == ASTREAM_PLAYBACK_MMAP || apstream->stream_type == ASTREAM_CAPTURE_MMAP)&&
         apstream->pcm) {
        struct timespec ts = { 0, 0 };

        ret = pcm_mmap_get_hw_ptr(apstream->pcm, (unsigned int *)&position->position_frames, &ts);
        if (ret < 0) {
            ALOGE("proxy-%s: get_hw_ptr error %s ", __func__, pcm_get_error(apstream->pcm));
        } else if (ret == 0) {
             position->time_nanoseconds = audio_utils_ns_from_timespec(&ts);
        }
    }

    return ret;
}


/******************************************************************************/
/**                                                                          **/
/** Interfaces for Audio Device Proxy                                        **/
/**                                                                          **/
/******************************************************************************/

/*
 *  Route Control Functions
 */
bool proxy_init_route(void *proxy, char *path)
{
    struct audio_proxy *aproxy = proxy;
    struct audio_route *ar = NULL;
    bool ret = false;

    if (aproxy) {
        aproxy->mixer = mixer_open(MIXER_CARD0);
        proxy_set_mixercontrol(aproxy, TICKLE_CONTROL, ABOX_TICKLE_ON);
        if (aproxy->mixer) {
            // In order to get add event, subscription has to be here!
            mixer_subscribe_events(aproxy->mixer, 1);

            ar = audio_route_init(MIXER_CARD0, path);
            if (!ar) {
                ALOGE("proxy-%s: failed to init audio route", __func__);
                mixer_subscribe_events(aproxy->mixer, 0);
                mixer_close(aproxy->mixer);
                aproxy->mixer = NULL;
            } else {
                aproxy->aroute = ar;
                aproxy->xml_path = strdup(path);    // Save Mixer Paths XML File path

                aproxy->active_playback_ausage   = AUSAGE_NONE;
                aproxy->active_playback_device   = DEVICE_NONE;
                aproxy->active_playback_modifier = MODIFIER_NONE;

                aproxy->active_capture_ausage   = AUSAGE_NONE;
                aproxy->active_capture_device   = DEVICE_NONE;
                aproxy->active_capture_modifier = MODIFIER_NONE;

                ALOGI("proxy-%s: opened Mixer & initialized audio route", __func__);
                ret = true;

                /* Create Mixer Control Update Thread */
                pthread_rwlock_init(&aproxy->mixer_update_lock, NULL);

                if (audio_route_missing_ctl(ar)) {
                    pthread_create(&aproxy->mixer_update_thread, NULL, mixer_update_loop, aproxy);
                    ALOGI("proxy-%s: missing control found, update thread is created", __func__);
                } else
                    mixer_subscribe_events(aproxy->mixer, 0);
            }
        } else
            ALOGE("proxy-%s: failed to open Mixer", __func__);
    }

    return ret;
}

void proxy_deinit_route(void *proxy)
{
    struct audio_proxy *aproxy = proxy;

    if (aproxy) {
        pthread_rwlock_wrlock(&aproxy->mixer_update_lock);

        if (aproxy->aroute) {
            audio_route_free(aproxy->aroute);
            aproxy->aroute = NULL;
        }
        if (aproxy->mixer) {
            mixer_close(aproxy->mixer);
            aproxy->mixer = NULL;
        }

        pthread_rwlock_unlock(&aproxy->mixer_update_lock);
        pthread_rwlock_destroy(&aproxy->mixer_update_lock);
        free(aproxy->xml_path);
    }
    ALOGI("proxy-%s: closed Mixer & deinitialized audio route", __func__);

    return ;
}

bool proxy_update_route(void *proxy, int ausage, int device)
{
    struct audio_proxy *aproxy = proxy;
    audio_usage __unused routed_ausage = (audio_usage)ausage;
    device_type __unused routed_device = (device_type)device;

    // Temp
    if (aproxy != NULL) {
        routed_ausage = AUSAGE_NONE;
        routed_device = DEVICE_NONE;
    }

    return true;
}

bool proxy_set_route(void *proxy, int ausage, int device, int modifier, bool set)
{
    struct audio_proxy *aproxy = proxy;
    char path_name[MAX_PATH_NAME_LEN];

    audio_usage   routed_ausage = (audio_usage)ausage;
    device_type   routed_device = (device_type)device;
    audio_usage   active_ausage = AUSAGE_NONE;
    device_type   active_device = DEVICE_NONE;

    modifier_type routed_modifier = (modifier_type)modifier;

#ifdef SUPPORT_CAMCORDER_QUAD_MIC
    // HACK: Force quad mic for camcorder
    if (routed_device == DEVICE_MAIN_MIC && routed_ausage == AUSAGE_CAMCORDER)
        routed_device = DEVICE_QUAD_MIC;
#endif

    if (set) {
        /* check whether path routing is for AP/CP call bandwidth or speaker/DEX Device Change */
        if (routed_device < DEVICE_MAIN_MIC) {
            active_ausage = aproxy->active_playback_ausage;
            active_device = aproxy->active_playback_device;
        } else {
            active_ausage = aproxy->active_capture_ausage;
            active_device = aproxy->active_capture_device;
        }

        if (is_usage_Call(active_ausage) &&
                is_usage_Call(routed_ausage)) {
            /* check whether internal path nodes close/re-open should be skipped,
             * for following scenarios
             * - cp call bandwidth change
             * - Dex speaker device state change during ap/cp call
            */
            if (((active_ausage != routed_ausage) && (active_device == routed_device) &&
                (is_usage_CPCall(active_ausage) && is_usage_CPCall(routed_ausage))) ||
                ((active_ausage == routed_ausage) && (active_device != routed_device) &&
                is_device_speaker(routed_device) && is_device_speaker(active_device))) {
                ALOGI("proxy-%s: skip output path loopback PCMs re-open",
                    __func__);
                ALOGI("proxy-%s: active-device(%s) requested-device(%s)", __func__,
                    device_table[active_device],
                    device_table[routed_device]);
                aproxy->skip_internalpath = true;
            }
        }

        if (routed_device < DEVICE_MAIN_MIC) {
            /* Do Specific Operation based on Audio Path */
            do_operations_by_playback_route_set(aproxy, routed_ausage, routed_device);

            if (aproxy->active_playback_ausage != AUSAGE_NONE &&
                aproxy->active_playback_device != DEVICE_NONE) {
                disable_internal_path(aproxy, aproxy->active_playback_ausage,
                                        aproxy->active_playback_device);
                set_reroute(aproxy, aproxy->active_playback_ausage, aproxy->active_playback_device,
                                    routed_ausage, routed_device);
            } else
                set_route(aproxy, routed_ausage, routed_device);

            aproxy->active_playback_ausage = routed_ausage;
            aproxy->active_playback_device = routed_device;

            // Audio Path Modifier for Playback Path
            if (routed_modifier < MODIFIER_BT_SCO_TX_NB) {
                if (aproxy->active_playback_modifier == MODIFIER_NONE)
                    set_modifier(aproxy, routed_modifier);
                else
                    update_modifier(aproxy, aproxy->active_playback_modifier, routed_modifier);
            } else if (routed_modifier == MODIFIER_NONE && aproxy->active_playback_modifier != MODIFIER_NONE)
                reset_modifier(aproxy, aproxy->active_playback_modifier);

            if (routed_device == DEVICE_USB_HEADSET ||
                routed_device == DEVICE_SPEAKER_AND_USB_HEADSET) {
                /* set USB gain controls if required */
                make_path(routed_ausage, routed_device, path_name);
                proxy_usb_set_gain(aproxy->usb_aproxy, path_name);
            }

            aproxy->active_playback_modifier = routed_modifier;

            // Set Loopback for Playback Path
            enable_internal_path(aproxy, routed_ausage, routed_device);

            if (ausage == AUSAGE_FM_RADIO_CAPTURE || ausage == AUSAGE_FM_RADIO_TUNER) {
                /* Open/Close FM Radio PCM node based on Enable/disable */
                proxy_start_fm_radio(aproxy);
            }
        } else {
            // Audio Path Routing for Capture Path
            if (aproxy->active_capture_ausage != AUSAGE_NONE &&
                aproxy->active_capture_device != DEVICE_NONE) {
                disable_internal_path(aproxy, aproxy->active_capture_ausage,
                                        aproxy->active_capture_device);
                set_reroute(aproxy, aproxy->active_capture_ausage, aproxy->active_capture_device,
                                    routed_ausage, routed_device);
            } else {
                // In case of capture routing setup, it needs A-Box early-wakeup
                proxy_set_mixercontrol(aproxy, TICKLE_CONTROL, ABOX_TICKLE_ON);

                set_route(aproxy, routed_ausage, routed_device);
            }

            aproxy->active_capture_ausage = routed_ausage;
            aproxy->active_capture_device = routed_device;

            // Audio Path Modifier for Capture Path
            if (routed_modifier >= MODIFIER_BT_SCO_TX_NB && routed_modifier < MODIFIER_NONE) {
                if (aproxy->active_capture_modifier == MODIFIER_NONE)
                    set_modifier(aproxy, routed_modifier);
                else
                    update_modifier(aproxy, aproxy->active_capture_modifier, routed_modifier);
            } else if (routed_modifier == MODIFIER_NONE && aproxy->active_capture_modifier != MODIFIER_NONE)
                reset_modifier(aproxy, aproxy->active_capture_modifier);

            if (is_usb_mic_device(routed_device)) {
                /* set USB gain controls if required */
                make_path(routed_ausage, routed_device, path_name);
                proxy_usb_set_gain(aproxy->usb_aproxy, path_name);
            }

            aproxy->active_capture_modifier = routed_modifier;

            // Set Loopback for Capture Path
            enable_internal_path(aproxy, routed_ausage, routed_device);
        }
    } else {
        /* Do Specific Operation based on Audio Path */
        if (routed_device < DEVICE_MAIN_MIC)
            do_operations_by_playback_route_reset(aproxy);

        // Reset Loopback
        disable_internal_path(aproxy, routed_ausage, routed_device);

        // Audio Path Modifier
        if (routed_modifier != MODIFIER_NONE) {
            reset_modifier(aproxy, routed_modifier);

            if (routed_modifier < MODIFIER_BT_SCO_TX_NB)
                aproxy->active_playback_modifier = MODIFIER_NONE;
            else
                aproxy->active_capture_modifier = MODIFIER_NONE;
        } else {
            aproxy->active_playback_modifier = MODIFIER_NONE;
            aproxy->active_capture_modifier = MODIFIER_NONE;
        }

        if (routed_device == DEVICE_USB_HEADSET ||
            routed_device == DEVICE_SPEAKER_AND_USB_HEADSET ||
            is_usb_mic_device(routed_device)) {
            /* reset USB gain controls */
            make_path(routed_ausage, routed_device, path_name);
            proxy_usb_reset_gain(aproxy->usb_aproxy, path_name);
        }

        // Audio Path Routing
        reset_route(aproxy, routed_ausage, routed_device);

        if (routed_device < DEVICE_MAIN_MIC) {
            aproxy->active_playback_ausage = AUSAGE_NONE;
            aproxy->active_playback_device = DEVICE_NONE;
        } else {
            aproxy->active_capture_ausage = AUSAGE_NONE;
            aproxy->active_capture_device = DEVICE_NONE;
        }
    }

    /* reset voicecall bandwidth change flag */
    aproxy->skip_internalpath = false;

    return true;
}


/*
 *  Proxy Voice Call Control
 */
void  proxy_stop_voice_call(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    voice_rx_stop(aproxy);
    voice_tx_stop(aproxy);

    return ;
}

void proxy_start_voice_call(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    voice_rx_start(aproxy);

    /*
    ** Voice TX and FM Radio are sharing same WDMA.
    ** So, it needs to check and close WDMA when FM Radio is working at Voice Call Start.
    */
    if (aproxy->fm_playback != NULL && aproxy->fm_capture != NULL) {
        fmradio_playback_stop(aproxy);
        fmradio_capture_stop(aproxy);
    }

    voice_tx_start(aproxy);

    return ;
}

/*
 *  Proxy FM Radio Control
 */
void proxy_stop_fm_radio(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    fmradio_playback_stop(aproxy);
    fmradio_capture_stop(aproxy);

    return ;
}

void proxy_start_fm_radio(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    fmradio_playback_start(aproxy);
    fmradio_capture_start(aproxy);

    return ;
}


// General Mixer Control Functions
int proxy_get_mixer_value_int(void *proxy, const char *name)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = -1;

    if (name == NULL)
        return ret;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_get_value(ctrl, 0);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ret;
}

int proxy_get_mixer_value_array(void *proxy, const char *name, void *value, int count)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = -1;

    if (name == NULL)
        return ret;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_get_array(ctrl, value, count);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ret;
}

void proxy_set_mixer_value_int(void *proxy, const char *name, int value)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0, val = value;

    if (name == NULL)
        return ;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_set_value(ctrl, 0, val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, name);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_mixer_value_string(void *proxy, const char *name, const char *value)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0;

    if (name == NULL)
        return ;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_set_enum_by_string(ctrl, value);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, name);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_mixer_value_array(void *proxy, const char *name, const void *value, int count)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0;

    if (aproxy == NULL)
        aproxy = getInstance();

    if (name == NULL)
        return ;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, name);
    if (ctrl) {
        ret = mixer_ctl_set_array(ctrl, value, count);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set %s", __func__, name);
    } else {
        ALOGE("proxy-%s: cannot find %s Mixer Control", __func__, name);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_audio_interface(void *proxy, unsigned int interface, unsigned int sample_rate,
                               unsigned int bit_width, unsigned int channel)
{
    struct audio_proxy *aproxy = proxy;

    if (aproxy == NULL)
        return ;

    if (interface == UAIF0) {
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF0_SWITCH, MIXER_OFF);
        /* SIFS0 Switch Off/On control is required only when SISF0 connected to UAIF0 */
        if (aproxy->active_playback_device == DEVICE_HEADPHONE ||
            aproxy->active_playback_device == DEVICE_HEADSET ||
            aproxy->active_playback_device == DEVICE_SPEAKER_AND_HEADPHONE ||
            aproxy->active_playback_device == DEVICE_SPEAKER_AND_HEADSET)
            proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS0_SWITCH, MIXER_OFF);

        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF0_SAMPLERATE, sample_rate);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF0_WIDTH, bit_width);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF0_CHANNEL, channel);

        /* skip SIFS0 configuration for USB device */
        if (!(aproxy->active_playback_device == DEVICE_USB_HEADSET ||
            aproxy->active_playback_device == DEVICE_SPEAKER_AND_USB_HEADSET)) {
            proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS0_SAMPLERATE, sample_rate);
            proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS0_WIDTH, bit_width);
            proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS0_CHANNEL, channel);
        } else {
            ALOGI("proxy-%s: skip SIFS0 config for %d", __func__, aproxy->active_playback_device);
        }

        if (aproxy->active_playback_device == DEVICE_HEADPHONE ||
            aproxy->active_playback_device == DEVICE_HEADSET ||
            aproxy->active_playback_device == DEVICE_SPEAKER_AND_HEADPHONE ||
            aproxy->active_playback_device == DEVICE_SPEAKER_AND_HEADSET)
            proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_SIFS0_SWITCH, MIXER_ON);

        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF0_SWITCH, MIXER_ON);
    } else if (interface == UAIF1) {
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF1_SWITCH, MIXER_OFF);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF1_SAMPLERATE, sample_rate);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF1_WIDTH, bit_width);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF1_CHANNEL, channel);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF1_SWITCH, MIXER_ON);
    } else if (interface == UAIF2) {
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF2_SWITCH, MIXER_OFF);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF2_SAMPLERATE, sample_rate);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF2_WIDTH, bit_width);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF2_CHANNEL, channel);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF2_SWITCH, MIXER_ON);
    } else if (interface == UAIF3) {
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF3_SWITCH, MIXER_OFF);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF3_SAMPLERATE, sample_rate);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF3_WIDTH, bit_width);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF3_CHANNEL, channel);
        proxy_set_mixer_value_int(proxy, MIXER_CTL_ABOX_UAIF3_SWITCH, MIXER_ON);
    }

    return ;
}

// Specific Mixer Control Functions
void proxy_set_audiomode(void *proxy, int audiomode)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0, val = audiomode;

    aproxy->audio_mode = val; // set audio mode

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    /* Set Audio Mode to Kernel */
    ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_AUDIOMODE_CONTROL_NAME);
    if (ctrl) {
        ret = mixer_ctl_set_value(ctrl, 0,val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set Android AudioMode to Kernel", __func__);
    } else {
        ALOGE("proxy-%s: cannot find AudioMode Mixer Control", __func__);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_volume(void *proxy, int volume_type, float left, float right)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = -ENAVAIL;
    int val[2];

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    if (volume_type == VOLUME_TYPE_OFFLOAD) {
        val[0] = (int)(left * COMPRESS_PLAYBACK_VOLUME_MAX);
        val[1] = (int)(right * COMPRESS_PLAYBACK_VOLUME_MAX);

        ctrl = mixer_get_ctl_by_name(aproxy->mixer, OFFLOAD_VOLUME_CONTROL_NAME);
    }

    if (ctrl) {
        if (volume_type == VOLUME_TYPE_OFFLOAD)
            ret = mixer_ctl_set_array(ctrl, val, sizeof(val)/sizeof(val[0]));

        if (ret != 0)
            ALOGE("proxy-%s: failed to set Volume", __func__);
        else
            ALOGV("proxy-%s: set Volume(%f:%f) => (%d:%d)", __func__, left, right, val[0], val[1]);
    } else {
        ALOGE("proxy-%s: cannot find Volume Control", __func__);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return;
}

void proxy_clear_apcall_txse(void)
{
    struct audio_proxy *aproxy = getInstance();
    char basic_path_name[MAX_PATH_NAME_LEN];
    char path_name[MAX_PATH_NAME_LEN];
    audio_usage ausage = aproxy->active_capture_ausage;

    memset(path_name, 0, MAX_PATH_NAME_LEN);

    if (snprintf(path_name, MAX_PATH_NAME_LEN - 1, "set-%s-txse", usage_path_table[ausage]) < 0) {
        ALOGE("proxy-%s: path name has error: %s", __func__, strerror(errno));
        return;
    }

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    audio_route_reset_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: %s is disabled", __func__, path_name);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_apcall_txse(void)
{
    struct audio_proxy *aproxy = getInstance();
    char basic_path_name[MAX_PATH_NAME_LEN];
    char path_name[MAX_PATH_NAME_LEN];
    audio_usage ausage = aproxy->active_capture_ausage;

    memset(path_name, 0, MAX_PATH_NAME_LEN);

    if (snprintf(path_name, MAX_PATH_NAME_LEN - 1, "set-%s-txse", usage_path_table[ausage]) < 0) {
        ALOGE("proxy-%s: path name has error: %s", __func__, strerror(errno));
        return;
    }

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    audio_route_apply_and_update_path(aproxy->aroute, path_name);
    ALOGI("proxy-%s: %s is enabled", __func__, path_name);

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}

void proxy_set_upscale(void *proxy, int sampling_rate, int pcm_format)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    int ret = 0, val = (int)UPSCALE_NONE;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    /* Set Compress Offload Upscaling Info to Kernel */
    ctrl = mixer_get_ctl_by_name(aproxy->mixer, OFFLOAD_UPSCALE_CONTROL_NAME);
    if (ctrl) {
        if (sampling_rate == 48000 && (audio_format_t)pcm_format == AUDIO_FORMAT_PCM_SUB_16_BIT)
            val = (int)UPSCALE_48K_16B;
        else if ((audio_format_t)pcm_format == AUDIO_FORMAT_PCM_SUB_16_BIT) {
            if (sampling_rate == 48000)
                val = (int)UPSCALE_48K_24B;
            else if (sampling_rate == 192000)
                val = (int)UPSCALE_192K_24B;
            else if (sampling_rate == 384000)
                val = (int)UPSCALE_384K_24B;
        }

        if (val != (int)UPSCALE_NONE) {
            ret = mixer_ctl_set_value(ctrl, 0, val);
            if (ret != 0)
                ALOGE("proxy-%s: failed to set Offload Upscale Info to Kernel", __func__);
            else
                ALOGV("proxy-%s: set Offload Upscale Info as %d", __func__, val);
        } else
            ALOGE("proxy-%s: invalid Offload Upscale Info", __func__);
    } else {
        ALOGE("proxy-%s: cannot find Offload Upscale Info Mixer Control", __func__);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return;
}

#ifdef SUPPORT_STHAL_INTERFACE
__attribute__ ((visibility ("default")))
int notify_sthal_status(int hwdmodel_state)
{
    struct audio_proxy *aproxy = getInstance();

    /* update sthal 'ok Google' model recognization status
        true : means recognization started
        false : means recognization stopped
    */
    aproxy->sthal_state = hwdmodel_state;

    ALOGD("proxy-%s: Ok-Google Model Recognition [%s]", __func__,
            (hwdmodel_state ? "STARTED" : "STOPPED"));

    return 0;
}

int proxy_check_sthalstate(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    return aproxy->sthal_state;
}
#endif

void proxy_call_status(void *proxy, int status)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    /* status : TRUE means call starting
        FALSE means call stopped
    */
    if (status)
        aproxy->call_state = true;
    else
        aproxy->call_state = false;

#ifdef SUPPORT_STHAL_INTERFACE
    /* Send call status notification to STHAL */
    if (aproxy->sound_trigger_voicecall_status) {
        aproxy->sound_trigger_voicecall_status(status);
    }

    ALOGD("proxy-%s: Call notification to STHAL [%s]", __func__,
             (status ? "STARTING" : "STOPPED"));
#endif

    return;
}

int proxy_set_parameters(void *proxy, void *parameters)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    struct str_parms *parms = (struct str_parms *)parameters;
    char value[256];
    int val;
    int ret = 0;     // for parameter handling
    int status = 0;  // for return value

    ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_CONNECT, &val);
    if (ret >= 0) {
        if ((audio_devices_t)val == AUDIO_DEVICE_IN_WIRED_HEADSET) {
            ALOGD("proxy-%s: Headset Device connected 0x%x", __func__, val);
#ifdef SUPPORT_STHAL_INTERFACE
            if (aproxy->sound_trigger_headset_status) {
                aproxy->sound_trigger_headset_status(true);
            }
#endif
        } else if ((audio_devices_t)val == AUDIO_DEVICE_OUT_BLUETOOTH_A2DP ||
                   (audio_devices_t)val == AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES ||
                   (audio_devices_t)val == AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER) {
            ALOGI("proxy-%s: connected BT A2DP Out Device", __func__);
#ifdef SUPPORT_BTA2DP_OFFLOAD
            if (aproxy->support_bta2dp) {
                ret = str_parms_get_int(parms, AUDIO_PARAMETER_STREAM_FORMAT, &val);
                if (ret >= 0) {
                    if (audio_is_bt_offload_format((audio_format_t)val)) {
                        pthread_mutex_lock(&aproxy->a2dp_lock);
                        if (!aproxy->a2dp_out_enabled) {
                            status = proxy_a2dp_open();
                            if (status == 0) {
                                aproxy->a2dp_out_enabled = true;
                                ALOGI("proxy-%s: set BT A2DP Offload Enabled & Open A2DP", __func__);
                                if (aproxy->a2dp_suspend) {
                                    // a2dp suspend off -> bt offlaod on case
                                    ALOGI("proxy-%s: set A2DP Suspend Flag", __func__);
                                    proxy_a2dp_suspend(true); // set suspend a2dp open
                                    /* modified by samsung convgergence */
                                    set_a2dp_suspend_mixer(MIXER_ON);
                                } else if (is_active_playback_device_bta2dp(aproxy)) {
                                    bta2dp_playback_start(aproxy);  // bt path already enabled, then bta2dp_playback_start hear
                                }
                            }
                        }
                        pthread_mutex_unlock(&aproxy->a2dp_lock);
                    }
                }
            }
#endif
        }
    }

    ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_DISCONNECT, &val);
    if (ret >= 0) {
        if ((audio_devices_t)val == AUDIO_DEVICE_IN_WIRED_HEADSET) {
            ALOGD("proxy-%s: Headset Device disconnected 0x%x", __func__, val);
#ifdef SUPPORT_STHAL_INTERFACE
            if (aproxy->sound_trigger_headset_status) {
                aproxy->sound_trigger_headset_status(false);
            }
#endif
        } else if ((audio_devices_t)val == AUDIO_DEVICE_OUT_BLUETOOTH_A2DP ||
                   (audio_devices_t)val == AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES ||
                   (audio_devices_t)val == AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER) {
            ALOGI("proxy-%s: disconnected BT A2DP Out Device", __func__);
#ifdef SUPPORT_BTA2DP_OFFLOAD
            if (aproxy->support_bta2dp) {
                pthread_mutex_lock(&aproxy->a2dp_lock);
                if (aproxy->a2dp_out_enabled) {
                    status = proxy_a2dp_close();
                    if (status == 0) {
                        aproxy->a2dp_out_enabled = false;
                        aproxy->a2dp_delay = 0;
                        ALOGI("proxy-%s: set BT A2DP Offload Disabled & Close A2DP", __func__);
                    }
                }
                pthread_mutex_unlock(&aproxy->a2dp_lock);
            }
#endif
        }
    }
#ifdef SUPPORT_BTA2DP_OFFLOAD
    /* BT A2DP Specific */
    ret = str_parms_get_str(parms, "A2dpSuspended", value, sizeof(value));
    if (ret >= 0 && aproxy->support_bta2dp) {
        pthread_mutex_lock(&aproxy->a2dp_lock);
        bool cur_state = proxy_a2dp_is_suspended();
        if(strncmp(value, "true", 4) == 0) {
            if (aproxy->a2dp_out_enabled) {  // send suspend call to hidl, only a2dp_offload ON
                proxy_a2dp_suspend(true);
                ALOGI("proxy-%s: set A2DP Suspend Flag", __func__);
            }
            /* modified by samsung convgergence */
            set_a2dp_suspend_mixer(MIXER_ON);
            aproxy->a2dp_suspend = true;
        } else {
            proxy_a2dp_suspend(false);
            if (is_active_playback_device_bta2dp(aproxy) && cur_state) {
                bta2dp_playback_start(aproxy);  // start bt a2dp on suspend t -> f state
            }
            ALOGI("proxy-%s: cleared A2DP Suspend Flag", __func__);
            /* modified by samsung convgergence */
            set_a2dp_suspend_mixer(MIXER_OFF);
            aproxy->a2dp_suspend = false;
        }
        pthread_mutex_unlock(&aproxy->a2dp_lock);
    }

    ret = str_parms_get_str(parms, "bt_offload_enable", value, sizeof(value));
    if (ret >= 0 && aproxy->support_bta2dp) {
        pthread_mutex_lock(&aproxy->a2dp_lock);
        val = atoi(value);
        if (val == 1 && aproxy->a2dp_out_enabled == false) {
            status = proxy_a2dp_open();
            if (status == 0) {
                aproxy->a2dp_out_enabled = true;
                ALOGI("proxy-%s: set BT A2DP Offload Enabled & Open A2DP", __func__);
                if (aproxy->a2dp_suspend) {
                    // a2dp suspend off -> bt offlaod on case
                    ALOGI("proxy-%s: set A2DP Suspend Flag", __func__);
                    proxy_a2dp_suspend(true); // set suspend a2dp open
                    /* modified by samsung convgergence */
                    set_a2dp_suspend_mixer(MIXER_ON);
                } else if (is_active_playback_device_bta2dp(aproxy)) {
                    bta2dp_playback_start(aproxy);  // bt path already enabled, then bta2dp_playback_start hear
                }
            }
        } else if (val == 0 && aproxy->a2dp_out_enabled == true) {
            status = proxy_a2dp_close();
            if (status == 0) {
                aproxy->a2dp_out_enabled = false;
                aproxy->a2dp_delay = 0;
                ALOGI("proxy-%s: set BT A2DP Offload Disabled & Close A2DP", __func__);
            }
        }
        pthread_mutex_unlock(&aproxy->a2dp_lock);
    }

    ret = str_parms_get_str(parms, "A2dpDelayReport", value, sizeof(value));
    if (ret >= 0 && aproxy->support_bta2dp) {
        pthread_mutex_lock(&aproxy->a2dp_lock);
        val = atoi(value);
        /* adjustment value to make presentation position as fast as adjust_latency(ms) */
        if (val > A2DP_CAL_LATENCY_VAL)
            val = val - A2DP_CAL_LATENCY_VAL;
        else
            val = 0;

        ALOGI("proxy-%s: set BT A2DP Delay as %d ms", __func__, val);
        aproxy->a2dp_delay = (uint32_t)val;
        pthread_mutex_unlock(&aproxy->a2dp_lock);
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_RECONFIG_A2DP, value, sizeof(value));
    if (ret >= 0 && aproxy->support_bta2dp) {
        pthread_mutex_lock(&aproxy->a2dp_lock);
        if (aproxy->a2dp_out_enabled) {
            if(strncmp(value, "true", 4) == 0 && is_active_playback_device_bta2dp(aproxy)) {
                bta2dp_playback_stop(aproxy);
                bta2dp_playback_start(aproxy);
            }
        }
        pthread_mutex_unlock(&aproxy->a2dp_lock);
    }
#endif
    /* Check USB parameters */
    status = proxy_usb_set_parameters((void *)aproxy->usb_aproxy, parameters);

    return status;
}

int proxy_get_microphones(void *proxy, void *array, int *count)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    struct audio_microphone_characteristic_t *mic_array = array;
    size_t *mic_count = (size_t *)count;
    size_t actual_mic_count = 0;
    int ret = 0;

    if (aproxy) {
        if (*mic_count == 0) {
            *mic_count = (size_t)aproxy->num_mic;
            ALOGI("proxy-%s: requested number of microphone, return %zu", __func__, *mic_count);
        } else {
            for (int i = 0; i < aproxy->num_mic; i++) {
                mic_array[i] = aproxy->mic_info[i];
                ALOGD("proxy-%s: %dth MIC = %s", __func__, i+1, mic_array[i].device_id);
                actual_mic_count++;
            }
            *mic_count = actual_mic_count;
        }
    } else {
        ALOGE("proxy-%s: aproxy is NULL", __func__);
        ret = -ENOSYS;
    }

    return ret;
}

void proxy_update_uhqa_playback_stream(void *proxy_stream, int hq_mode)
{
#if 0
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    audio_quality_mode_t high_quality_mode = (audio_quality_mode_t)hq_mode;

    ALOGD("proxy-%s: mode(%d)", __func__, high_quality_mode);

    if (apstream) {
        if (apstream->stream_type == ASTREAM_PLAYBACK_COMPR_OFFLOAD) {
            // offload case
        } else if (apstream->stream_type == ASTREAM_PLAYBACK_AUX_DIGITAL) {
            // DP/HDMI case
            if (high_quality_mode == AUDIO_QUALITY_UHQ) {
                apstream->pcmconfig.format = UHQA_MEDIA_FORMAT;
            } else {
                apstream->pcmconfig.format = DEFAULT_MEDIA_FORMAT;
            }
            apstream->requested_format = get_pcmformat_from_alsaformat(apstream->pcmconfig.format);
        } else if (apstream->stream_type == ASTREAM_PLAYBACK_DEEP_BUFFER) {
            struct pcm_config pcm_config_map[AUDIO_QUALITY_CNT] = {
                    pcm_config_deep_playback,
                    pcm_config_deep_playback_uhqa,
                    pcm_config_deep_playback_wide_res,
                    pcm_config_deep_playback_suhqa,
            };
            apstream->pcmconfig = pcm_config_map[high_quality_mode];
            apstream->requested_format = get_pcmformat_from_alsaformat(apstream->pcmconfig.format);
            apstream->requested_sample_rate = apstream->pcmconfig.rate;
        } else {
            ALOGVV("proxy-%s: not supported stream",  __func__);
        }
    }
#endif
}

void proxy_set_uhqa_stream_config(void *proxy_stream, bool config)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    if (apstream)
        apstream->need_update_pcm_config = config;
}

bool proxy_get_uhqa_stream_config(void *proxy_stream)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;
    bool uhqa_stream_config = false;

    if (apstream)
        uhqa_stream_config = apstream->need_update_pcm_config;

    return uhqa_stream_config;
}

void proxy_init_offload_effect_lib(void *proxy)
{
    struct audio_proxy *aproxy = proxy;

    if(access(OFFLOAD_EFFECT_LIBRARY_PATH, R_OK) == 0){
        aproxy->offload_effect_lib = dlopen(OFFLOAD_EFFECT_LIBRARY_PATH, RTLD_NOW);
        if(aproxy->offload_effect_lib == NULL){
            ALOGI("proxy-%s: dlopen %s failed", __func__, OFFLOAD_EFFECT_LIBRARY_PATH);
        } else {
            aproxy->offload_effect_lib_update =
                (void (*)(struct mixer *, int))dlsym(aproxy->offload_effect_lib,
                "effect_update_by_hal");
            aproxy->offload_effect_lib_update(aproxy->mixer, 0);
        }
    } else {
        ALOGI("proxy-%s: access %s failed", __func__, OFFLOAD_EFFECT_LIBRARY_PATH);
    }
    return;
}

void proxy_update_offload_effect(void *proxy, int type){
    struct audio_proxy *aproxy = proxy;

    if (type && (aproxy->offload_effect_lib_update != NULL)) {
        aproxy->offload_effect_lib_update(aproxy->mixer, type);
    }
}

void proxy_set_dual_speaker_mode(void *proxy, bool state)
{
    struct audio_proxy *aproxy = proxy;
    aproxy->support_dualspk = state;
}

void proxy_set_stream_channel(void *proxy_stream, int new_channel, bool skip)
{
    struct audio_proxy_stream *apstream = (struct audio_proxy_stream *)proxy_stream;

    if (new_channel > 0) {
        apstream->pcmconfig.channels = new_channel;
    }
    apstream->skip_ch_convert = skip;
    apstream->need_channelconversion = !skip;
    ALOGI("%s: new_channel %d, skip_ch_convert %d", __func__, new_channel, apstream->skip_ch_convert);
}

void proxy_set_spk_ampL_power(void* proxy, bool state)
{
    struct audio_proxy *aproxy = proxy;
    aproxy->spk_ampL_powerOn = state;

    if(aproxy->support_dualspk)
        proxy_set_mixer_value_int(aproxy, SPK_AMPL_POWER_NAME, aproxy->spk_ampL_powerOn);
}

bool proxy_get_spk_ampL_power(void* proxy)
{
    struct audio_proxy *aproxy = proxy;
    return aproxy->spk_ampL_powerOn;
}

void proxy_set_primary_mute(void* proxy, int count)
{
    struct audio_proxy *aproxy = proxy;
    struct mixer_ctl *ctrl = NULL;
    char mixer_name[MAX_MIXER_NAME_LEN];
    int ret = 0, val = count;

    pthread_rwlock_rdlock(&aproxy->mixer_update_lock);

    ctrl = mixer_get_ctl_by_name(aproxy->mixer, ABOX_MUTE_CONTROL_NAME);
    snprintf(mixer_name, sizeof(mixer_name), ABOX_MUTE_CONTROL_NAME);

    if (ctrl) {
        ret = mixer_ctl_set_value(ctrl, 0,val);
        if (ret != 0)
            ALOGE("proxy-%s: failed to set primary mute(%s)", __func__, mixer_name);
        else
            ALOGI("proxy-%s: set set primary mute(%s) to %d", __func__, mixer_name, val);
    } else {
        ALOGE("proxy-%s: cannot find primary mute", __func__);
    }

    pthread_rwlock_unlock(&aproxy->mixer_update_lock);

    return ;
}


/*
 *  Proxy Dump
 */
int proxy_fw_dump(int fd)
{
    ALOGV("proxy-%s: enter with file descriptor(%d)", __func__, fd);

    calliope_ramdump(fd);

    ALOGV("proxy-%s: exit with file descriptor(%d)", __func__, fd);

    return 0;
}


/*
 *  Proxy Device Creation/Destruction
 */
static void check_configurations(struct audio_proxy *aproxy)
{
    char property[PROPERTY_VALUE_MAX];

    /* Audio Device Configurations */
    // BuiltIn Earpiece
    memset(property, 0, PROPERTY_VALUE_MAX);
    property_get(NUM_EARPIECE_PROPERTY, property, NUM_EARPIECE_DEFAULT);
    aproxy->num_earpiece = atoi(property);
    ALOGI("proxy-%s: The supported number of BuiltIn Earpiece = %d", __func__, aproxy->num_earpiece);

    // BuiltIn Speaker
    memset(property, 0, PROPERTY_VALUE_MAX);
    property_get(NUM_SPEAKER_PROPERTY, property, NUM_SPEAKER_DEFAULT);
    aproxy->num_speaker = atoi(property);
    ALOGI("proxy-%s: The supported number of BuiltIn Speaker = %d", __func__, aproxy->num_speaker);

    if (aproxy->num_speaker == 2)
        ALOGI("proxy-%s: This set supports Dual Speaker", __func__);

    // BuiltIn Mic
    ALOGI("proxy-%s: The number of supported BuiltIn Mic = %d", __func__, aproxy->num_mic);

    // Proximity Sensor
    memset(property, 0, PROPERTY_VALUE_MAX);
    property_get(NUM_PROXIMITY_PROPERTY, property, NUM_PROXIMITY_DEFAULT);
    aproxy->num_proximity = atoi(property);
    ALOGI("proxy-%s: The supported number of Proximity Sensor = %d", __func__, aproxy->num_proximity);

    // Speaker AMP
    memset(property, 0, PROPERTY_VALUE_MAX);
    property_get(SPEAKER_AMP_PROPERTY, property, SPEAKER_AMP_DEFAULT);
    aproxy->support_spkamp = (bool)atoi(property);
    if (aproxy->support_spkamp)
        ALOGI("proxy-%s: The Speaker AMP is supported", __func__);

    // Bluetooth
    memset(property, 0, PROPERTY_VALUE_MAX);
    property_get(BLUETOOTH_PROPERTY, property, BLUETOOTH_DEFAULT);
    if (strcmp(property, "external") == 0) {
        aproxy->bt_external = true;
        ALOGI("proxy-%s: The supported BT is External", __func__);
    } else if (strcmp(property, "internal") == 0) {
        aproxy->bt_internal = true;
        ALOGI("proxy-%s: The supported BT is Internal", __func__);
    } else
        ALOGI("proxy-%s: The supported BT is None", __func__);

    // FM Radio
    memset(property, 0, PROPERTY_VALUE_MAX);
    property_get(FMRADIO_PROPERTY, property, FMRADIO_DEFAULT);
    if (strcmp(property, "external") == 0) {
        aproxy->fm_external = true;
        ALOGI("proxy-%s: The supported FM Radio is External", __func__);
    } else if (strcmp(property, "internal") == 0) {
        aproxy->fm_internal = true;
        ALOGI("proxy-%s: The supported FM Radio is Internal", __func__);
    } else
        ALOGI("proxy-%s: The supported FM Radio is None", __func__);


    /* A-Box Configurations */
    // USB Device
    memset(property, 0, PROPERTY_VALUE_MAX);
    property_get(USBBYPRIMARY_PROPERTY, property, USBBYPRIMARY_DEFAULT);
    if (strcmp(property, "yes") == 0) {
        aproxy->usb_by_primary = true;
        ALOGI("proxy-%s: The USB Device is supported by Primary AudioHAL", __func__);
    } else {
        aproxy->usb_by_primary = false;
        ALOGI("proxy-%s: The USB Device is supported by USB AudioHAL", __func__);
    }

    return ;
}

static bool find_enum_from_string(struct audio_string_to_enum *table, const char *name,
                                  int32_t table_cnt, int *value)
{
    int i;

    for (i = 0; i < table_cnt; i++) {
        if (strcmp(table[i].name, name) == 0) {
            *value = table[i].value;
            return true;
        }
    }
    return false;
}

static void set_microphone_info(struct audio_microphone_characteristic_t *microphone, const XML_Char **attr)
{
    uint32_t curIdx = 0;
    uint32_t array_cnt = 0;
    float f_value[3] = {0, };
    char *ptr = NULL;

    if (strcmp(attr[curIdx++], "device_id") == 0)
        strcpy(microphone->device_id, attr[curIdx++]);

    if (strcmp(attr[curIdx++], "id") == 0)
        microphone->id = atoi(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "device") == 0)
        find_enum_from_string(device_in_type, attr[curIdx++], ARRAY_SIZE(device_in_type), (int *)&microphone->device);

    if (strcmp(attr[curIdx++], "address") == 0)
        strcpy(microphone->address, attr[curIdx++]);

    if (strcmp(attr[curIdx++], "location") == 0)
        find_enum_from_string(microphone_location, attr[curIdx++], AUDIO_MICROPHONE_LOCATION_CNT, (int *)&microphone->location);

    if (strcmp(attr[curIdx++], "group") == 0)
        microphone->group = atoi(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "index_in_the_group") == 0)
        microphone->index_in_the_group = atoi(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "sensitivity") == 0)
        microphone->sensitivity = atof(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "max_spl") == 0)
        microphone->max_spl = atof(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "min_spl") == 0)
        microphone->min_spl = atof(attr[curIdx++]);

    if (strcmp(attr[curIdx++], "directionality") == 0)
        find_enum_from_string(microphone_directionality, attr[curIdx++],
                              AUDIO_MICROPHONE_LOCATION_CNT, (int *)&microphone->directionality);

    if (strcmp(attr[curIdx++], "num_frequency_responses") == 0) {
        microphone->num_frequency_responses = atoi(attr[curIdx++]);
        if (microphone->num_frequency_responses > 0) {
            if (strcmp(attr[curIdx++], "frequencies") == 0) {
                ptr = strtok((char *)attr[curIdx++], " ");
                while(ptr != NULL) {
                    microphone->frequency_responses[0][array_cnt++] = atof(ptr);
                    ptr = strtok(NULL, " ");
                }
            }
            array_cnt = 0;
            if (strcmp(attr[curIdx++], "responses") == 0) {
                ptr = strtok((char *)attr[curIdx++], " ");
                while(ptr != NULL) {
                    microphone->frequency_responses[1][array_cnt++] = atof(ptr);
                    ptr = strtok(NULL, " ");
                }
            }
        }
    }

    if (strcmp(attr[curIdx++], "geometric_location") == 0) {
        ptr = strtok((char *)attr[curIdx++], " ");
        array_cnt = 0;
        while (ptr != NULL) {
            f_value[array_cnt++] = atof(ptr);
            ptr = strtok(NULL, " ");
        }
        microphone->geometric_location.x = f_value[0];
        microphone->geometric_location.y = f_value[1];
        microphone->geometric_location.z = f_value[2];
    }

    if (strcmp(attr[curIdx++], "orientation") == 0) {
        ptr = strtok((char *)attr[curIdx++], " ");
        array_cnt = 0;
        while (ptr != NULL) {
            f_value[array_cnt++] = atof(ptr);
            ptr = strtok(NULL, " ");
        }
        microphone->orientation.x = f_value[0];
        microphone->orientation.y = f_value[1];
        microphone->orientation.z = f_value[2];
    }

    /* Channel mapping doesn't used for now. */
    for (array_cnt = 0; array_cnt < AUDIO_CHANNEL_COUNT_MAX; array_cnt++)
        microphone->channel_mapping[array_cnt] = AUDIO_MICROPHONE_CHANNEL_MAPPING_UNUSED;
}

static void end_tag(void *data, const XML_Char *tag_name)
{
    if (strcmp(tag_name, "microphone_characteristis") == 0)
        set_info = INFO_NONE;
}

static void start_tag(void *data, const XML_Char *tag_name, const XML_Char **attr)
{
    struct audio_proxy *aproxy = getInstance();
    const XML_Char *attr_name  = NULL;
    const XML_Char *attr_value = NULL;

    if (strcmp(tag_name, "microphone_characteristics") == 0) {
        set_info = MICROPHONE_CHARACTERISTIC;
    } else if (strcmp(tag_name, "microphone") == 0) {
        if (set_info != MICROPHONE_CHARACTERISTIC) {
            ALOGE("proxy-%s: microphone tag should be supported with microphone_characteristics tag", __func__);
            return ;
        }
        set_microphone_info(&aproxy->mic_info[aproxy->num_mic++], attr);
    }
}

void proxy_set_board_info(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;
    XML_Parser parser = 0;
    FILE *file = NULL;
    char info_file_name[MAX_MIXER_NAME_LEN] = {0};
    void *buf = NULL;
    uint32_t buf_size = 1024;
    int32_t bytes_read = 0;

    strlcpy(info_file_name, BOARD_INFO_XML_PATH, MAX_MIXER_NAME_LEN);

    file = fopen(info_file_name, "r");
    if (file == NULL)
        ALOGE("proxy-%s: open error: %s, file=%s", __func__, strerror(errno), info_file_name);
    else
        ALOGI("proxy-%s: Board info file name is %s", __func__, info_file_name);

    parser = XML_ParserCreate(NULL);

    XML_SetElementHandler(parser, start_tag, end_tag);

    while (1) {
        buf = XML_GetBuffer(parser, buf_size);
        if (buf == NULL) {
            ALOGE("proxy-%s fail to get buffer", __func__);
            break;
        }

        bytes_read = fread(buf, 1, buf_size, file);
        if (bytes_read < 0) {
            ALOGE("proxy-%s fail to read from file", __func__);
            break;
        }

        XML_ParseBuffer(parser, bytes_read, bytes_read == 0);

        if (bytes_read == 0)
            break;
    }

    XML_ParserFree(parser);
    fclose(file);

    check_configurations(aproxy);
}

bool proxy_is_initialized(void)
{
    if (instance)
        return true;
    else
        return false;
}

void * proxy_init(void)
{
    struct audio_proxy *aproxy;
    char sound_trigger_hal_path[100] = {0, };

    /* Creates the structure for audio_proxy. */
    aproxy = getInstance();
    if (!aproxy) {
        ALOGE("proxy-%s: failed to create for audio_proxy", __func__);
        return NULL;
    }

    aproxy->primary_out = NULL;

    // In case of Output Loopback Support, initializes Out Loopback Stream
    aproxy->support_out_loopback = true;
    aproxy->out_loopback = NULL;
    aproxy->erap_in = NULL;

    // In case of External Speaker AMP Support, initializes Reference & Playback Stream
    aproxy->support_spkamp = true;
    aproxy->spkamp_reference = NULL;
    aproxy->spkamp_playback = NULL;
#ifdef SUPPORT_BTA2DP_OFFLOAD
    // BT A2DP Devices Support by Primary AudioHAL
    pthread_mutex_init(&aproxy->a2dp_lock, (const pthread_mutexattr_t *) NULL);

    pthread_mutex_lock(&aproxy->a2dp_lock);
    proxy_a2dp_init();
    aproxy->support_bta2dp = true;
    aproxy->a2dp_out_enabled = false;
    aproxy->a2dp_suspend = false;
    aproxy->a2dp_delay = 0;
    aproxy->a2dp_default_delay = 0;
    aproxy->bta2dp_playback = NULL;
    aproxy->bta2dp_out_loopback = NULL;
    pthread_mutex_unlock(&aproxy->a2dp_lock);
    aproxy->a2dp_mute_playback = NULL;
#endif
    // In case of External BT-SCO Support, initializes Playback Stream
    aproxy->support_btsco = true;
    aproxy->btsco_playback = NULL;

    // Voice Call PCM Devices
    aproxy->call_rx = NULL;
    aproxy->call_tx = NULL;
    aproxy->call_tx_direct = NULL;

    // FM Radio PCM Devices
    aproxy->fm_playback = NULL;
    aproxy->fm_capture  = NULL;

    aproxy->usb_aproxy = proxy_usb_init();
    if (!aproxy->usb_aproxy) {
        ALOGE("proxy-%s: failed to create audio_proxy_usb", __func__);
        destroyInstance();
        return NULL;
    }

    // In case of USB Input Loopback Support, initializes Out/In Loopback Streams
    aproxy->support_usb_out_loopback = true;
    aproxy->usb_out_loopback = NULL;
    aproxy->support_usb_in_loopback = true;
    aproxy->usb_in_loopback = NULL;

    // Call State
    aproxy->call_state = false;
    aproxy->skip_internalpath = false;

    /* Audio Mode */
    aproxy->audio_mode = AUDIO_MODE_NORMAL;

    // STHAL interface initialization
#ifdef SUPPORT_STHAL_INTERFACE
    aproxy->sthal_state = 0;

    snprintf(sound_trigger_hal_path, sizeof(sound_trigger_hal_path),
             SOUND_TRIGGER_HAL_LIBRARY_PATH, XSTR(TARGET_SOC_NAME));

    aproxy->sound_trigger_lib = dlopen(sound_trigger_hal_path, RTLD_NOW);
    if (aproxy->sound_trigger_lib == NULL) {
        ALOGE("%s: DLOPEN failed for %s", __func__, sound_trigger_hal_path);
    } else {
        ALOGV("%s: DLOPEN successful for %s", __func__, sound_trigger_hal_path);
        aproxy->sound_trigger_open_for_streaming =
                    (int (*)(void))dlsym(aproxy->sound_trigger_lib,
                                                    "sound_trigger_open_for_streaming");
        aproxy->sound_trigger_read_samples =
                    (size_t (*)(int, void*, size_t))dlsym(aproxy->sound_trigger_lib,
                                                    "sound_trigger_read_samples");
        aproxy->sound_trigger_close_for_streaming =
                    (int (*)(int))dlsym(aproxy->sound_trigger_lib,
                                                    "sound_trigger_close_for_streaming");
        aproxy->sound_trigger_open_recording =
                    (int (*)(void))dlsym(aproxy->sound_trigger_lib,
                                                   "sound_trigger_open_recording");
        aproxy->sound_trigger_read_recording_samples =
                    (size_t (*)(void*, size_t))dlsym(aproxy->sound_trigger_lib,
                                                   "sound_trigger_read_recording_samples");
        aproxy->sound_trigger_close_recording =
                    (int (*)(int))dlsym(aproxy->sound_trigger_lib,
                                                   "sound_trigger_close_recording");
        aproxy->sound_trigger_headset_status =
                    (int (*)(int))dlsym(aproxy->sound_trigger_lib,
                                                    "sound_trigger_headset_status");
        aproxy->sound_trigger_voicecall_status =
                    (int (*)(int))dlsym(aproxy->sound_trigger_lib,
                                                    "sound_trigger_voicecall_status");
        if (!aproxy->sound_trigger_open_for_streaming ||
            !aproxy->sound_trigger_read_samples ||
            !aproxy->sound_trigger_close_for_streaming ||
            !aproxy->sound_trigger_open_recording ||
            !aproxy->sound_trigger_read_recording_samples ||
            !aproxy->sound_trigger_close_recording ||
            !aproxy->sound_trigger_headset_status ||
            !aproxy->sound_trigger_voicecall_status) {

            ALOGE("%s: Error grabbing functions in %s", __func__, sound_trigger_hal_path);
            aproxy->sound_trigger_open_for_streaming = 0;
            aproxy->sound_trigger_read_samples = 0;
            aproxy->sound_trigger_close_for_streaming = 0;
            aproxy->sound_trigger_open_recording = 0;
            aproxy->sound_trigger_read_recording_samples = 0;
            aproxy->sound_trigger_close_recording = 0;
            aproxy->sound_trigger_headset_status = 0;
            aproxy->sound_trigger_voicecall_status = 0;
        }
    }
#endif

    /* offload effect */
    aproxy->offload_effect_lib = NULL;
    aproxy->offload_effect_lib_update = NULL;
    aproxy->spk_ampL_powerOn = false;

    // Force dual speaker
#ifdef SUPPORT_DUAL_SPEAKER
    aproxy->support_dualspk = true;
#endif

    proxy_set_board_info(aproxy);

    ALOGI("proxy-%s: opened & initialized Audio Proxy", __func__);
    return (void *)aproxy;
}

void proxy_deinit(void *proxy)
{
    struct audio_proxy *aproxy = (struct audio_proxy *)proxy;

    if (aproxy) {
#ifdef SUPPORT_BTA2DP_OFFLOAD
        // BT A2DP Devices Support by Primary AudioHAL
        if (aproxy->support_bta2dp) {
            pthread_mutex_lock(&aproxy->a2dp_lock);
            proxy_a2dp_deinit();
            pthread_mutex_unlock(&aproxy->a2dp_lock);

            pthread_mutex_destroy(&aproxy->a2dp_lock);
        }
#endif
        // USB Devices Support by Primary AudioHAL
        proxy_usb_deinit(aproxy->usb_aproxy);

        destroyInstance();
        ALOGI("proxy-%s: destroyed for audio_proxy", __func__);
    }
    return ;
}

