/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "audio_hw_proxy_usb"
#define LOG_NDEBUG 0

//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGD
#else
#define ALOGVV(a...) do { } while(0)
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>

#include <log/log.h>
#include <cutils/str_parms.h>

#include <audio_utils/channels.h>
#include <audio_utils/primitives.h>
#include <audio_utils/clock.h>
#include <tinyalsa/asoundlib.h>

#include <system/audio.h>
#include <hardware/hardware.h>
#include <hardware/audio.h>

#include "audio_usb_proxy.h"

#define USB_READ_BUFF_SIZE      2048
#define CHANNEL_NUMBER_STR      "Channels: "
#define PLAYBACK_PROFILE_STR    "Playback:"
#define CAPTURE_PROFILE_STR     "Capture:"
#define DATA_PACKET_INTERVAL_STR "Data packet interval: "
#define USB_SIDETONE_GAIN_STR   "usb_sidetone_gain"
#define ABS_SUB(x, y) (((x) > (y)) ? ((x) - (y)):((y) - (x)))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define _MAX(x, y) (((x) >= (y)) ? (x) : (y))
#define _MIN(x, y) (((x) <= (y)) ? (x) : (y))


/* format in order of increasing preference */
static const int pcm_format_preference_map[] = {
    PCM_FORMAT_S8,
    PCM_FORMAT_S16_LE,
    PCM_FORMAT_S24_LE,
    PCM_FORMAT_S24_3LE,
    PCM_FORMAT_S32_LE
};

/******************************************************************************/
/**                                                                          **/
/** Audio Proxy is Singleton                                                 **/
/**                                                                          **/
/******************************************************************************/

static struct audio_proxy_usb *usb_instance = NULL;

static struct audio_proxy_usb* getUSBInstance(void)
{
    if (usb_instance == NULL) {
        usb_instance = calloc(1, sizeof(struct audio_proxy_usb));
        ALOGI("proxy-%s: created Audio Proxy USB Instance!", __func__);
    }
    return usb_instance;
}

static void destroyUSBInstance(void)
{
    if (usb_instance) {
        free(usb_instance);
        usb_instance = NULL;
        ALOGI("proxy-%s: destroyed Audio Proxy USB Instance!", __func__);
    }
    return;
}

/******************************************************************************/
/**                                                                          **/
/** USB card profile information util functions                              **/
/**                                                                          **/
/******************************************************************************/
static int usb_extract_rates_from_ratestr(
    char *rates_str,
    struct usb_device_info *dev_config,
    unsigned int *rates_mask)
{
    uint32_t i;
    char *next_sr_string, *temp_ptr;
    uint32_t rate, min_rate, max_rate, rate_size = 0;

    /* USB device profile Sample rate string can be in two different formats as shown below
     * Rates: 8000 - 48000 (continuous)
     * Rates: 8000, 44100, 48000
     * This function supports both formats for rates parsing
     */
    ALOGVV("%s: rates_str %s", __func__, rates_str);
    next_sr_string = strtok_r(rates_str, "Rates: ", &temp_ptr);
    if (next_sr_string == NULL) {
        ALOGE("%s: could not find min rates string", __func__);
        return -EINVAL;
    }
    if (strstr(rates_str, "continuous") != NULL) {
        min_rate = (uint32_t)atoi(next_sr_string);
        next_sr_string = strtok_r(NULL, " ,.-", &temp_ptr);
        if (next_sr_string == NULL) {
            ALOGE("%s: could not find max rates string", __func__);
            return -EINVAL;
        }
        max_rate = (uint32_t)atoi(next_sr_string);

        for (i = 0; i < MAX_NUM_USB_SR; i++) {
            if (supported_usb_samplingrates[i] >= min_rate &&
                supported_usb_samplingrates[i] <= max_rate) {
                dev_config->rates[rate_size++] = supported_usb_samplingrates[i];
                ALOGVV("%s: continuous sample rate supported_usb_samplingrates[%d] %d",
                    __func__, i, supported_usb_samplingrates[i]);
                *rates_mask |= (1<<i);
            }
        }
    } else {
        do {
            rate = (uint32_t)atoi(next_sr_string);
            for (i = 0; i < MAX_NUM_USB_SR; i++) {
                if (supported_usb_samplingrates[i] == rate) {
                    ALOGVV("%s: sr %d, supported_usb_samplingrates[%d] %d",
                        __func__, rate, i, supported_usb_samplingrates[i]);
                    dev_config->rates[rate_size++] = supported_usb_samplingrates[i];
                    *rates_mask |= (1<<i);
                }
            }
            next_sr_string = strtok_r(NULL, " ,.-", &temp_ptr);
        } while (next_sr_string != NULL);
    }
    dev_config->rate_size = rate_size;
    return 0;
}

static int usb_get_profile_capability(void *proxy, int direction)
{
    struct audio_proxy_usb *aproxy_usb = proxy;
    int32_t size = 0;
    int32_t fd =-1;
    char *start_str = NULL;
    char *end_str = NULL;
    char *channel_str_start = NULL;
    char *format_str_start = NULL;
    char *rates_str_start = NULL;
    char *format_str = NULL;
    char *rate_str = NULL;
    char *lineend_str = NULL;
    char *read_buf = NULL;
    char *interval_start_str = NULL;
    char path[128];
    int ret = 0;
    bool check = false;
    int retries = 5;
    struct usb_device_info *usb_devconfig;
    struct listnode *usb_devlist = NULL;
    int card = -1;
    unsigned long interval = 0;
    unsigned int *formats_mask;
    unsigned int *rates_mask;
    unsigned int *channels_mask;

    if (direction == USB_OUT) {
        usb_devlist = &aproxy_usb->usbplayback_devlist;
        card = aproxy_usb->usb_out_pcm_card;
        formats_mask = &aproxy_usb->usb_out_formats_mask;
        channels_mask = &aproxy_usb->usb_out_channels_mask;
        rates_mask = &aproxy_usb->usb_out_rates_mask;
    } else {
        usb_devlist = &aproxy_usb->usbcapture_devlist;
        card = aproxy_usb->usb_in_pcm_card;
        formats_mask = &aproxy_usb->usb_in_formats_mask;
        channels_mask = &aproxy_usb->usb_in_channels_mask;
        rates_mask = &aproxy_usb->usb_in_rates_mask;
    }

    memset(path, 0, sizeof(path));
    // direction: 0 for playback & 1 for capture
    ALOGI("%s: for %s", __func__, (direction == USB_OUT) ?
          PLAYBACK_PROFILE_STR : CAPTURE_PROFILE_STR);

    /* generate device access path with card information */
    ret = snprintf(path, sizeof(path), "/proc/asound/card%u/stream0",
             card);
    if (ret < 0) {
        ALOGE("%s: failed on snprintf (%d) to path %s\n",
          __func__, ret, path);
        goto done;
    }

    while (retries--) {
        if (access(path, F_OK) < 0) {
            ALOGW("stream %s doesn't exist retrying\n", path);
            sleep(1);
            continue;
        }
    }

    fd = open(path, O_RDONLY);
    if (fd <0) {
        ALOGE("%s: error failed to open config file %s error: %d\n",
              __func__, path, errno);
        ret = -EINVAL;
        goto done;
    }

    read_buf = (char *)calloc(1, USB_READ_BUFF_SIZE + 1);

    if (!read_buf) {
        ALOGE("Failed to create read_buf");
        ret = -ENOMEM;
        goto done;
    }

    if(read(fd, read_buf, USB_READ_BUFF_SIZE) < 0) {
        ALOGE("file read error\n");
        goto done;
    }

    start_str = strstr(read_buf, ((direction == USB_OUT) ?
                       PLAYBACK_PROFILE_STR : CAPTURE_PROFILE_STR));
    if (start_str == NULL) {
        ALOGE("%s: error %s section not found in usb config file",
               __func__, ((direction == USB_OUT) ?
               PLAYBACK_PROFILE_STR : CAPTURE_PROFILE_STR));
        ret = -EINVAL;
        goto done;
    }
    end_str = strstr(read_buf, ((direction == USB_OUT) ?
                       CAPTURE_PROFILE_STR : PLAYBACK_PROFILE_STR));
    if (end_str > start_str)
        check = true;

    ALOGVV("%s: usb_config = %s, check %d\n", __func__, start_str, check);

    while (start_str != NULL) {
        start_str = strstr(start_str, "Altset");
        if ((start_str == NULL) || (check  && (start_str >= end_str))) {
            ALOGVV("%s: done parsing %s\n", __func__, start_str);
            break;
        }
        ALOGVV("%s: remaining string %s\n", __func__, start_str);
        start_str += sizeof("Altset");
        usb_devconfig = calloc(1, sizeof(struct usb_device_info));
        if (usb_devconfig == NULL) {
            ALOGE("%s: error unable to allocate memory",
                  __func__);
            ret = -ENOMEM;
            break;
        }
        //usb_devconfig->type = type;
        /* format parsing */
        format_str_start = strstr(start_str, "Format: ");
        if (format_str_start == NULL) {
            ALOGI("%s: Could not find bit_width string", __func__);
            free(usb_devconfig);
            continue;
        }
        lineend_str = strchr(format_str_start, '\n');
        if (lineend_str == NULL) {
            ALOGI("%s:end of line not found", __func__);
            free(usb_devconfig);
            continue;
        }
        size = lineend_str - format_str_start;
        if ((format_str = (char *)malloc(size + 1)) == NULL) {
            ALOGE("%s: unable to allocate memory to hold bit width strings",
                  __func__);
            ret = -EINVAL;
            free(usb_devconfig);
            break;
        }
        memcpy(format_str, format_str_start, size);
        format_str[size] = '\0';
        if (strstr(format_str, "S16_LE")) {
            usb_devconfig->bit_width = 16;
            usb_devconfig->format = PCM_FORMAT_S16_LE;
            *formats_mask |= (1 << 3);
        } else if (strstr(format_str, "S24_LE")) {
            usb_devconfig->bit_width = 24;
            usb_devconfig->format = PCM_FORMAT_S24_LE;
            *formats_mask |= (1 << 2);
        } else if (strstr(format_str, "S24_3LE")) {
            usb_devconfig->bit_width = 24;
            usb_devconfig->format = PCM_FORMAT_S24_3LE;
            *formats_mask |= (1 << 1);
        } else if (strstr(format_str, "S32_LE")) {
            usb_devconfig->bit_width = 32;
            usb_devconfig->format = PCM_FORMAT_S32_LE;
            *formats_mask |= (1);
        }

        if (format_str)
            free(format_str);

        /* channels parsing */
        channel_str_start = strstr(start_str, CHANNEL_NUMBER_STR);
        if (channel_str_start == NULL) {
            ALOGI("%s: could not find Channels string", __func__);
            free(usb_devconfig);
            continue;
        }
        usb_devconfig->channels = atoi(channel_str_start + strlen(CHANNEL_NUMBER_STR));
        *channels_mask |= (1 << usb_devconfig->channels);

        /* Sample rates parsing */
        rates_str_start = strstr(start_str, "Rates: ");
        if (rates_str_start == NULL) {
            ALOGI("%s: cant find rates string", __func__);
            free(usb_devconfig);
            continue;
        }
        lineend_str = strchr(rates_str_start, '\n');
        if (lineend_str == NULL) {
            ALOGI("%s: end of line not found", __func__);
            free(usb_devconfig);
            continue;
        }
        size = lineend_str - rates_str_start;
        if ((rate_str = (char *)malloc(size + 1)) == NULL) {
            ALOGE("%s: unable to allocate memory to hold sample rate strings",
                  __func__);
            ret = -EINVAL;
            free(usb_devconfig);
            break;
        }
        memcpy(rate_str, rates_str_start, size);
        rate_str[size] = '\0';
        ret = usb_extract_rates_from_ratestr(rate_str, usb_devconfig, rates_mask);
        if (rate_str)
            free(rate_str);
        if (ret < 0) {
            ALOGE("%s: error unable to get sample rate values",
                  __func__);
            free(usb_devconfig);
            continue;
        }

        /* Add to list if every field is valid */
        list_add_tail(usb_devlist, &usb_devconfig->node);
    }

done:
    if (fd >= 0) close(fd);
    if (read_buf) free(read_buf);

    return ret;
}

static void usb_remove_device_info(void *proxy, int direction)
{
    struct audio_proxy_usb *aproxy_usb = proxy;
    struct listnode *node, *auxi;
    struct usb_device_info *devinfo_node;
    struct listnode *usb_devlist = ((direction == USB_OUT) ? &aproxy_usb->usbplayback_devlist
                : &aproxy_usb->usbcapture_devlist);
    int count = 0;

    // Removes this stream from playback list
    list_for_each_safe(node, auxi, usb_devlist)
    {
        devinfo_node = node_to_item(node, struct usb_device_info, node);
        if (devinfo_node) {
            ALOGI("%s: USB_Device[%s] %d: Info", __func__, ((direction == USB_OUT) ?
                "PLAYBACK" : "CAPTURE"), ++count);
            list_remove(node);
            free(devinfo_node);
        }
    }

    return;
}
static void usb_print_device_info(void *proxy, int direction)
{
    struct audio_proxy_usb *aproxy_usb = proxy;
    struct listnode *node;
    struct usb_device_info *devinfo_node;
    struct listnode *usb_devlist = ((direction == USB_OUT) ? &aproxy_usb->usbplayback_devlist
                : &aproxy_usb->usbcapture_devlist);
    int count = 0;

    list_for_each(node, usb_devlist)
    {
        devinfo_node = node_to_item(node, struct usb_device_info, node);
        if (devinfo_node) {
            ALOGI("USB_Device[%s] %d: Info", ((direction == USB_OUT) ? "PLAYBACK" :
                "CAPTURE"), ++count);
            if (devinfo_node->format == PCM_FORMAT_S24_3LE ||
                devinfo_node->format == PCM_FORMAT_S24_LE)
                ALOGI("\t bit-width: %d (%s)", devinfo_node->bit_width,
                ((devinfo_node->format == PCM_FORMAT_S24_3LE) ? "packed" : "padded"));
            else
                ALOGI("\t bit-width: %d", devinfo_node->bit_width);
            ALOGI("\t channels: %d", devinfo_node->channels);
            ALOGI("\t rate:");
            for(unsigned int idx = 0; idx < devinfo_node->rate_size; idx++)
                ALOGI("\t %d", devinfo_node->rates[idx]);
        }
    }

    return;
}

static int usb_get_best_matching_format(
    struct listnode *usb_devlist,
    enum pcm_format stream_format)
{
    struct listnode *node;
    struct usb_device_info *devinfo_node;
    enum pcm_format selected_format = PCM_FORMAT_INVALID;
    enum pcm_format cur_format = PCM_FORMAT_INVALID;

    list_for_each(node, usb_devlist)
    {
        devinfo_node = node_to_item(node, struct usb_device_info, node);
        if (devinfo_node) {
            cur_format = devinfo_node->format;
            ALOGVV("%s: USB fmt(%d) stream fmt(%d) selected fmt(%d)",
                __func__, cur_format, stream_format, selected_format);
            if ((cur_format == stream_format)
                || (cur_format == PCM_FORMAT_S24_3LE
                && stream_format == PCM_FORMAT_S24_LE)) {
                selected_format = cur_format;
                ALOGI("%s: found matching fmt(%d) stream fmt(%d)",
                    __func__, selected_format, stream_format);
                goto exit;
            } else if (selected_format == PCM_FORMAT_INVALID) {
                selected_format = cur_format;
            } else if (IS_HIGHEST_PCMFORMAT(cur_format, selected_format)) {
                selected_format = cur_format;
                ALOGI("%s: found better matching fmt(%d) stream fmt(%d)",
                    __func__, selected_format, stream_format);
            }
        }
    }
exit:
    return selected_format;
}

static unsigned int usb_get_best_matching_channels(
    struct listnode *usb_devlist,
    int format,
    unsigned int stream_channels)
{
    struct listnode *node;
    struct usb_device_info *devinfo_node;
    enum pcm_format cur_format = PCM_FORMAT_INVALID;
    unsigned int selected_channels = 0;
    unsigned cur_channels = 0;

    list_for_each(node, usb_devlist)
    {
        devinfo_node = node_to_item(node, struct usb_device_info, node);
        if (devinfo_node) {
            cur_format = devinfo_node->format;
            cur_channels = devinfo_node->channels;
            ALOGVV("%s: USB fmt(%d)ch(%d) stream fmt(%d)ch(%d) selected ch(%d)",
                __func__, cur_format, cur_channels, format,
                stream_channels, selected_channels);
            if (cur_format != format)
                continue;
            if (cur_channels == stream_channels) {
                selected_channels = cur_channels;
                ALOGI("%s: found matching ch(%d) stream ch(%d)",
                    __func__, selected_channels, stream_channels);
                goto exit;
            } else if (selected_channels == 0) {
                selected_channels = cur_channels;
            } else if (((cur_channels > stream_channels) &&
                    (ABS_SUB(stream_channels, cur_channels) <
                    ABS_SUB(stream_channels, selected_channels))) ||
                    ((cur_channels > selected_channels) &&
                    (stream_channels > cur_channels))) {
                selected_channels = cur_channels;
                ALOGI("%s: found better matching ch(%d) stream ch(%d)",
                    __func__, selected_channels, stream_channels);
            }
        }
    }
exit:
    return selected_channels;
}

static unsigned int usb_get_best_matching_samplerate(
    struct listnode *usb_devlist,
    int format,
    unsigned int channels,
    unsigned int stream_rate)
{
    struct listnode *node;
    struct usb_device_info *devinfo_node;
    enum pcm_format cur_format = PCM_FORMAT_INVALID;
    unsigned cur_channels = 0;
    unsigned int selected_rate = 0;
    unsigned int cur_rate = 0;
    unsigned int i = 0;

    list_for_each(node, usb_devlist)
    {
        devinfo_node = node_to_item(node, struct usb_device_info, node);
        if (devinfo_node) {
            cur_format = devinfo_node->format;
            cur_channels = devinfo_node->channels;
            ALOGVV("%s: USB fmt(%d)ch(%d) stream fmt(%d)ch(%d)rate(%d) selected rate(%d)",
                __func__, cur_format, cur_channels, format, channels,
                stream_rate, selected_rate);
            if ((cur_format != format)
                || (cur_channels != channels))
                continue;
            for (i = 0; i < devinfo_node->rate_size; i++) {
                ALOGVV("%s: usb next rate(%d) selected rate(%d)",
                        __func__, devinfo_node->rates[i], selected_rate);
                if (devinfo_node->rates[i] == stream_rate) {
                    selected_rate = devinfo_node->rates[i];
                    ALOGI("%s: found matching rate(%d) stream rate(%d)",
                        __func__, selected_rate, stream_rate);
                    goto exit;
                } else if (selected_rate == 0) {
                    selected_rate = devinfo_node->rates[i];
                    ALOGI("%s: initial updated rate(%d) stream rate(%d)",
                        __func__, selected_rate, stream_rate);
                } else if (((devinfo_node->rates[i] > stream_rate) &&
                            (ABS_SUB(stream_rate, devinfo_node->rates[i]) <
                            ABS_SUB(stream_rate, selected_rate))) ||
                            ((devinfo_node->rates[i] > selected_rate) &&
                            (stream_rate > devinfo_node->rates[i]))) {
                    selected_rate = devinfo_node->rates[i];
                    ALOGI("%s: found better matching rate(%d) stream rate(%d)",
                        __func__, selected_rate, stream_rate);
                }
            }
        }
    }
exit:
    return selected_rate;
}

static bool usb_get_best_matching_config(
    struct listnode *usb_devlist,
    enum pcm_format stream_format,
    unsigned int stream_channels,
    unsigned int stream_rate,
    struct pcm_config *best_pcmconfig)
{
    /* get best matching USB config for active pcm config
     * matching sequence
     * first select best format,
     * second select channels using selected format.
     * third select sample rate using selected format & channels
     */
    ALOGI("proxy-%s: stream config SR(%d) CH(%d) FMT(%d)", __func__,
        stream_rate,
        stream_channels,
        stream_format);

    best_pcmconfig->format = usb_get_best_matching_format(usb_devlist,
                                                    stream_format);
    best_pcmconfig->channels = usb_get_best_matching_channels(usb_devlist,
                                                    best_pcmconfig->format,
                                                    stream_channels);
    best_pcmconfig->rate = usb_get_best_matching_samplerate(usb_devlist,
                                                    best_pcmconfig->format,
                                                    best_pcmconfig->channels,
                                                    stream_rate);

    ALOGI("proxy-%s: USB best matching config SR(%d) CH(%d) FMT(%d)", __func__,
            best_pcmconfig->rate,
            best_pcmconfig->channels,
            best_pcmconfig->format);

    return true;
}

static unsigned int usb_get_max_channel(struct listnode *usb_devlist)
{
    struct listnode *node;
    struct usb_device_info *devinfo_node;
    unsigned int selected_channels = 0;
    unsigned cur_channels = 0;

    list_for_each(node, usb_devlist)
    {
        devinfo_node = node_to_item(node, struct usb_device_info, node);
        if (devinfo_node) {
            cur_channels = devinfo_node->channels;
            if (cur_channels > selected_channels)
                selected_channels = cur_channels;
        }
    }

    ALOGI("%s: max channel count ch(%d)", __func__, selected_channels);

    return selected_channels;
}

static unsigned int usb_get_min_channel(struct listnode *usb_devlist)
{
    struct listnode *node;
    struct usb_device_info *devinfo_node;
    unsigned int selected_channels = FCC_8;
    unsigned cur_channels = 0;

    list_for_each(node, usb_devlist)
    {
        devinfo_node = node_to_item(node, struct usb_device_info, node);
        if (devinfo_node) {
            cur_channels = devinfo_node->channels;
            if (cur_channels < selected_channels)
                selected_channels = cur_channels;
        }
    }

    ALOGI("%s: min channel count ch(%d)", __func__, selected_channels);

    return selected_channels;
}

char * usb_get_format_strs(const unsigned int formats_mask)
{
    /* if we assume that format strings are about 24 characters (AUDIO_FORMAT_PCM_16_BIT is 23),
     * plus ~1 for a delimiter "|" this buffer has room for about 10 format strings which seems
     *  like way too much, but it's a stack variable so only temporary.
     */
    char buffer[256];
    buffer[0] = '\0';
    size_t buffSize = ARRAY_SIZE(buffer);
    size_t curStrLen = 0;
    int idx = 0;
    unsigned int mask = formats_mask;
    unsigned int count = _MIN(MAX_NUM_USB_FORMAT, (unsigned int)__builtin_popcount(mask));
    size_t numEntries = 0;

    ALOGVV("%s: mask:0x%x count:%d", __func__, mask, count);

    while (count--) {
        idx = __builtin_ffs(mask) - 1;
        // account for both the null, and potentially the bar.
        if (buffSize - curStrLen < strlen(supported_usb_format_strs[idx])
                                   + (numEntries != 0 ? 2 : 1)) {
            /* we don't have room for another, so bail at this point rather than
             * return a malformed rate string
             */
            break;
        }

        if (numEntries++ != 0) {
            strlcat(buffer, "|", buffSize);
        }
        curStrLen = strlcat(buffer, supported_usb_format_strs[idx], buffSize);
        mask &= ~(1<<idx);
    }
    ALOGI("%s: %s", __func__, buffer);
    return strdup(buffer);
}

char * usb_get_channel_count_strs(void* proxy_usb, const unsigned int channels_mask, int direction)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    /*
     * If we assume each channel string is 26 chars ("AUDIO_CHANNEL_INDEX_MASK_8" is 26) + 1 for,
     * the "|" delimiter, then we allocate room for 16 strings.
     */
    char buffer[27 * 16 + 1]; /* caution, may need to be expanded */
    buffer[0] = '\0';
    size_t buffSize = ARRAY_SIZE(buffer);
    size_t curStrLen = 0;
    unsigned int mask = channels_mask;
    struct listnode *usb_devlist = ((direction == USB_OUT) ?
                                    &aproxy_usb->usbplayback_devlist :
                                    &aproxy_usb->usbcapture_devlist);
    unsigned int max = usb_get_max_channel(usb_devlist);
    unsigned int min = usb_get_min_channel(usb_devlist);
    size_t numEntries = 0;
    char *const *const chans_strs = ((direction == USB_OUT) ?
                                            supported_usb_out_channel_strs :
                                            supported_usb_in_channel_strs);
    unsigned int idx = ((direction == USB_OUT) ? FCC_2 : 1); // OUT start from Stereo, IN start from Mono

    ALOGI("%s: mask:0x%x max:min channels[%d:%d]", __func__, mask, max, min);

    for (; idx <= MAX_NUM_USB_CHANNELS; idx++) {
        if (idx >= min && idx <= max) {
            ALOGVV("%s: idx:0x%x channels:%s", __func__, idx, supported_usb_channel_strs[idx]);
            // account for both the null, and potentially the bar.
            if (buffSize - curStrLen < strlen(supported_usb_channel_strs[idx])
                                       + (numEntries != 0 ? 2 : 1)) {
                /* we don't have room for another, so bail at this point rather than
                 * return a malformed rate string
                 */
                break;
            }

            if (idx == 1 || idx == 2) {
                if (numEntries++ != 0) {
                    strlcat(buffer, "|", buffSize);
                }
                curStrLen = strlcat(buffer, chans_strs[idx], buffSize);
            }

            if (numEntries++ != 0) {
                strlcat(buffer, "|", buffSize);
            }
            curStrLen = strlcat(buffer, supported_usb_channel_strs[idx], buffSize);
        }
    }
    ALOGI("%s: %s", __func__, buffer);
    return strdup(buffer);
}

char * usb_get_sample_rate_strs(const unsigned int rates_mask)
{
    /* if we assume that rate strings are about 5 characters (48000 is 5), plus ~1 for a
     * delimiter "|" this buffer has room for about 22 rate strings which seems like
     * way too much, but it's a stack variable so only temporary.
     */
    char buffer[128];
    buffer[0] = '\0';
    size_t buffSize = ARRAY_SIZE(buffer);
    size_t curStrLen = 0;
    int idx = 0;
    unsigned int mask = rates_mask;
    unsigned int count = _MIN(MAX_NUM_USB_SR, (unsigned int)__builtin_popcount(mask));
    size_t numEntries = 0;

    ALOGVV("%s: mask:0x%x count:%d", __func__, mask, count);

    while (count--) {
        idx = __builtin_ffs(mask) - 1;
        ALOGVV("%s: idx:0x%x rate:%s", __func__, idx, supported_usb_samplingrate_strs[idx]);
        // account for both the null, and potentially the bar.
        if (buffSize - curStrLen < strlen(supported_usb_samplingrate_strs[idx])
                                   + (numEntries != 0 ? 2 : 1)) {
            /* we don't have room for another, so bail at this point rather than
             * return a malformed rate string
             */
            break;
        }

        if (numEntries++ != 0) {
            strlcat(buffer, "|", buffSize);
        }
        curStrLen = strlcat(buffer, supported_usb_samplingrate_strs[idx], buffSize);
        mask &= ~(1<<idx);
    }

    ALOGI("%s: %s", __func__, buffer);
    return strdup(buffer);
}

/******************************************************************************/
/**                                                                          **/
/** Local Functions of USB Audio Proxy                                       **/
/**                                                                          **/
/******************************************************************************/
/* Functions should be called with usb_lock mutex */

// This function is to load usb gain mixer paths xml file
static int usb_audio_gain_load_xml(void *proxy, int usb_card)
{
    struct audio_proxy_usb *aproxy_usb = proxy;
    char gain_mixer_path[MAX_USB_PATH_LEN];
    int ret = 0;

    memset(gain_mixer_path, 0, MAX_USB_PATH_LEN);

    // read gain xml based on PID values
    if (aproxy_usb->usb_pid == USB_BUNDLE_WHITE_PID) {
        strcpy(gain_mixer_path, USB_BUNDLE_WHITE_GAIN_XML_MIXER_PATH);
        ALOGI("proxy-%s: USB White Bundle GainControl XML [%s] loading",
            __func__, gain_mixer_path);
    } else {
        strcpy(gain_mixer_path, USB_BUNDLE_GRAY_GAIN_XML_MIXER_PATH);
        ALOGI("proxy-%s: USB Gray Bundle GainControl XML [%s] loading",
            __func__, gain_mixer_path);
    }

    //initialize audio_route with gain xml
    aproxy_usb->usb_ar = audio_route_init(usb_card, gain_mixer_path);
    if (!aproxy_usb->usb_ar) {
        ALOGE("proxy-%s: failed to init audio route for USB Gain usb_card: %d",
            __func__, usb_card);
        ret = -EINVAL;
    }

    return ret;
}

// This function is to load usb gain mixer paths xml file
static void usb_audio_gain_unload_xml(void *proxy)
{
    struct audio_proxy_usb *aproxy_usb = proxy;

    if (aproxy_usb->usb_ar) {
        audio_route_free(aproxy_usb->usb_ar);
        aproxy_usb->usb_ar = NULL;
    }

    return;
}

/* Check to VID (Vendor ID):PID (Product ID) of USB Device and enable gain-control */
static void usb_audio_gain_control_enable(void *proxy)
{
    struct audio_proxy_usb *aproxy_usb = proxy;
    char path[MAX_USB_PATH_LEN];
    char readbuf[USB_READ_SIZE];
    char *endptr;
    int usb_card = -1;
    int fd = -1;
    int ret = 0;

    if (!aproxy_usb->usb_gaincontrol_needed &&
        (aproxy_usb->usb_out_connected || aproxy_usb->usb_in_connected)) {
        //get valid usb card number
        if (aproxy_usb->usb_out_pcm_card != -1 ||
            aproxy_usb->usb_in_pcm_card != -1) {
            usb_card = ((aproxy_usb->usb_out_pcm_card != -1) ?
                        aproxy_usb->usb_out_pcm_card :
                        aproxy_usb->usb_in_pcm_card);
        } else {
            ALOGE("%s: failed get valid usb card", __func__);
            goto err;
        }

        // get VID:PID information from usb device node
        memset(path, 0, sizeof(path));
        ret = snprintf(path, sizeof(path), "/proc/asound/card%u/usbid",
                     usb_card);
        if (ret < 0) {
            ALOGE("%s: snprintf failed ret (%d)", __func__, ret);
            goto err;
        }

        fd = open(path, O_RDONLY);
        if (fd < 0) {
            ALOGE("%s: failed to open usbid file %s error: %d",
                  __func__, path, errno);
            goto err;
        }

        if(read(fd, readbuf, USB_READ_SIZE) < 0) {
           ALOGE("file read error");
           goto err;
       }

        //extract VID and PID from string separated by colon
        aproxy_usb->usb_vid = (int)strtol(readbuf, &endptr, 16);
        if (endptr == NULL || *endptr == '\0' || *endptr != ':') {
            ALOGE("failed to parse USB VID");
            aproxy_usb->usb_vid = -1;
            goto err;
        }
        aproxy_usb->usb_pid = (int)strtol((endptr+1), &endptr, 16);

        ALOGI("proxy-%s: USB Device VID: 0x%x PID: 0x%x", __func__,
            aproxy_usb->usb_vid, aproxy_usb->usb_pid);
        // check VID & PID, for gain-control
        if (aproxy_usb->usb_vid == USB_BUNDLE_VID &&
            (aproxy_usb->usb_pid == USB_BUNDLE_WHITE_PID ||
            aproxy_usb->usb_pid == USB_BUNDLE_GRAY_HEADPHONE_PID ||
            aproxy_usb->usb_pid == USB_BUNDLE_GRAY_HEADSET_PID)) {
            if (!usb_audio_gain_load_xml(aproxy_usb, usb_card)) {
                aproxy_usb->usb_gaincontrol_needed = true;
                ALOGI("proxy-%s: USB GainControl enabled", __func__);
            } else {
                ALOGW("proxy-%s: failed to load USB gain XML", __func__);
            }
        } else {
            ALOGI("proxy-%s: USB GainControl not required", __func__);
        }
    } else {
        if (aproxy_usb->usb_gaincontrol_needed)
            ALOGI("proxy-%s: USB GainControl already enabled", __func__);
        else
            ALOGI("proxy-%s: USB Device not connected", __func__);
    }

err:
    if (fd >= 0) close(fd);
    return;
}

static void usb_audio_gain_control_disable(void *proxy)
{
    struct audio_proxy_usb *aproxy_usb = proxy;

    if (aproxy_usb->usb_gaincontrol_needed &&
        (!aproxy_usb->usb_out_connected && !aproxy_usb->usb_in_connected)) {
            usb_audio_gain_unload_xml(aproxy_usb);
            aproxy_usb->usb_gaincontrol_needed = false;
            ALOGI("proxy-%s: USB GainControl disabled", __func__);
    } else if (aproxy_usb->usb_gaincontrol_needed) {
        ALOGI("proxy-%s: USB Device still in use", __func__);
    }

    return;
}

/* Function should be called with usb_lock mutex */
static void usb_open_out_proxy(struct audio_proxy_usb *aproxy_usb)
{
    char pcm_path[MAX_USB_PATH_LEN];
    unsigned int flags = PCM_OUT | PCM_MONOTONIC;
    struct pcm_config *ppcmconfig = &aproxy_usb->usb_out_active_pcmconfig;
    unsigned int size = 0;
    uint16_t *dummy = NULL;

    if (aproxy_usb && aproxy_usb->usb_out_connected) {
        if (aproxy_usb->usb_out_status == false) {
            /* Update period-size using updated config rate */
            ppcmconfig->period_count = DEFAULT_USB_PERIOD_COUNT;
            ppcmconfig->period_size = (ppcmconfig->rate * DEFAULT_USB_PLAYBACK_DURATION) / 1000;
            ppcmconfig->stop_threshold = UINT_MAX;
            aproxy_usb->usb_out_pcm = pcm_open(aproxy_usb->usb_out_pcm_card,
                                                aproxy_usb->usb_out_pcm_device,
                                                flags, ppcmconfig);
            if (aproxy_usb->usb_out_pcm && !pcm_is_ready(aproxy_usb->usb_out_pcm)) {
                /* pcm_open does always return pcm structure, not NULL */
                ALOGE("%s-%s: PCM Device is not ready with Sampling_Rate(%u) error(%s)!",
                      "usb_out", __func__, ppcmconfig->rate,
                      pcm_get_error(aproxy_usb->usb_out_pcm));
                goto err_open;
            }

            // Dummy write to trigger pcm_prepare
            size = ppcmconfig->period_size;
            dummy = (uint16_t *)calloc(1, size);
            if (dummy && aproxy_usb->usb_out_pcm &&
                pcm_write(aproxy_usb->usb_out_pcm, (void *)dummy, size) == 0) {
                snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                         aproxy_usb->usb_out_pcm_card, aproxy_usb->usb_out_pcm_device, 'p');

                ALOGI("%s-%s: The opened USB Out PCM Device is %s with SR(%d), CC(%d), Format(%d)",
                    "usb-out", __func__, pcm_path,
                    ppcmconfig->rate,
                    ppcmconfig->channels,
                    ppcmconfig->format);

                aproxy_usb->usb_out_status = true;
            } else {
                ALOGE("%s-%s: USB Out PCM Device write failed %s",
                        "usb-out", __func__,
                        ((aproxy_usb->usb_out_pcm) ? pcm_get_error(aproxy_usb->usb_out_pcm) : "Error"));
                goto err_open;
            }

            if (dummy)
                free(dummy);
        }
    }

    return;
err_open:
    if (aproxy_usb->usb_out_pcm) {
        pcm_close(aproxy_usb->usb_out_pcm);
        aproxy_usb->usb_out_pcm = NULL;
    }
    if (dummy)
        free(dummy);

    return;
}

/* Function should be called with usb_lock mutex */
static void usb_close_out_proxy(struct audio_proxy_usb *aproxy_usb)
{
    if (aproxy_usb && aproxy_usb->usb_out_connected) {
        if (aproxy_usb->usb_out_status == true) {
            if (aproxy_usb->usb_out_pcm) {
                pcm_close(aproxy_usb->usb_out_pcm);
                aproxy_usb->usb_out_pcm = NULL;
            }
            ALOGI("proxy-%s: closed USB Out PCM Device", __func__);

            aproxy_usb->usb_out_status = false;
        }
    }

    return ;
}

static void usb_open_in_proxy(struct audio_proxy_usb *aproxy_usb)
{
    char pcm_path[MAX_USB_PATH_LEN];
    unsigned int flags = PCM_IN | PCM_MONOTONIC;
    struct pcm_config *ppcmconfig = &aproxy_usb->usb_in_active_pcmconfig;

    if (aproxy_usb && aproxy_usb->usb_in_connected) {
        /* Update period-size using updated config rate */
        ppcmconfig->period_count = DEFAULT_USB_PERIOD_COUNT;
        ppcmconfig->period_size = (ppcmconfig->rate * DEFAULT_USB_CAPTURE_DURATION) / 1000;
        aproxy_usb->usb_in_pcm = pcm_open(aproxy_usb->usb_in_pcm_card,
                                            aproxy_usb->usb_in_pcm_device,
                                            flags, ppcmconfig);
        if (aproxy_usb->usb_in_pcm && !pcm_is_ready(aproxy_usb->usb_in_pcm)) {
            /* pcm_open does always return pcm structure, not NULL */
            ALOGE("%s-%s: PCM Device is not ready with Sampling_Rate(%u) error(%s)!",
                  "usb_in", __func__, ppcmconfig->rate,
                  pcm_get_error(aproxy_usb->usb_in_pcm));
            goto err_open;
        }

        snprintf(pcm_path, sizeof(pcm_path), "/dev/snd/pcmC%uD%u%c",
                 aproxy_usb->usb_in_pcm_card, aproxy_usb->usb_in_pcm_device, 'c');

        ALOGVV("%s-%s: USB In PCM Device opened %s with SR(%d), CC(%d), Format(%d)",
                "usb-in", __func__, pcm_path,
                ppcmconfig->rate,
                ppcmconfig->channels,
                ppcmconfig->format);

        if (aproxy_usb->usb_in_pcm && pcm_start(aproxy_usb->usb_in_pcm) == 0) {
            ALOGI("%s-%s: USB In PCM Device opened/started %s with SR(%d), CC(%d), Format(%d)",
                    "usb-in", __func__, pcm_path,
                    ppcmconfig->rate,
                    ppcmconfig->channels,
                    ppcmconfig->format);
        } else {
            ALOGE("%s-%s: PCM Device(%s) with SR(%u) CC(%d) Format(%d) cannot be started as error(%s)",
                "usb-in", __func__, pcm_path,
                ppcmconfig->rate,
                ppcmconfig->channels,
                ppcmconfig->format,
                ((aproxy_usb->usb_in_pcm) ? pcm_get_error(aproxy_usb->usb_in_pcm) : "Error"));
            goto err_open;
        }
    }

    return;
err_open:
    if (aproxy_usb->usb_in_pcm) {
        pcm_close(aproxy_usb->usb_in_pcm);
        aproxy_usb->usb_in_pcm = NULL;
    }

    return;
}

static void usb_close_in_proxy(struct audio_proxy_usb *aproxy_usb)
{
    if (aproxy_usb && aproxy_usb->usb_in_connected) {
        if (aproxy_usb->usb_in_pcm) {
            pcm_close(aproxy_usb->usb_in_pcm);
            aproxy_usb->usb_in_pcm = NULL;
        }
        ALOGI("proxy-%s: closed USB In PCM Device", __func__);
    }

    return ;
}

static bool parse_card_device_params(const char *kvpairs, int *card, int *device)
{
    struct str_parms * parms = str_parms_create_str(kvpairs);
    char value[32];
    int param_val;

    // initialize to "undefined" state.
    *card = -1;
    *device = -1;

    param_val = str_parms_get_str(parms, "card", value, sizeof(value));
    if (param_val >= 0) {
        *card = atoi(value);
    }

    param_val = str_parms_get_str(parms, "device", value, sizeof(value));
    if (param_val >= 0) {
        *device = atoi(value);
    }

    str_parms_destroy(parms);

    return *card >= 0 && *device >= 0;
}

/******************************************************************************/
/**                                                                          **/
/** Interface Functions of USB Audio Proxy                                   **/
/**                                                                          **/
/******************************************************************************/
int proxy_is_usb_playback_CPCall_prepared(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return aproxy_usb->usb_out_cpcall_prepared;
}

int proxy_is_usb_playback_device_connected(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return aproxy_usb->usb_out_connected;
}

int proxy_is_usb_capture_device_connected(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return aproxy_usb->usb_in_connected;
}

unsigned int proxy_usb_get_capture_samplerate(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return aproxy_usb->usb_in_active_pcmconfig.rate;
}

unsigned int proxy_usb_get_capture_channels(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return aproxy_usb->usb_in_active_pcmconfig.channels;
}

int proxy_usb_get_capture_format(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return aproxy_usb->usb_in_active_pcmconfig.format;
}

int proxy_usb_get_playback_samplerate(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return aproxy_usb->usb_out_active_pcmconfig.rate;
}

int proxy_usb_get_playback_channels(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return aproxy_usb->usb_out_active_pcmconfig.channels;
}

int proxy_usb_get_playback_format(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return aproxy_usb->usb_out_active_pcmconfig.format;
}

int proxy_usb_get_playback_bitwidth(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    int ret = 0;
    switch (aproxy_usb->usb_out_active_pcmconfig.format) {
    case PCM_FORMAT_S16_LE:  /* 16-bit signed */
        ret = 16;
        break;
    case PCM_FORMAT_S32_LE:      /* 32-bit signed */
        ret = 32;
        break;
    case PCM_FORMAT_S24_LE:      /* 24-bits in 4-bytes */
    case PCM_FORMAT_S24_3LE:     /* 24-bits in 3-bytes */
        ret = 24;
        break;
    case PCM_FORMAT_S8:          /* 8-bit signed */
    default:
        ret = 16;
        break;
    }

    return ret;
}

int proxy_usb_get_playback_highest_supported_channels(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    return usb_get_max_channel(&aproxy_usb->usbplayback_devlist);
}

void proxy_usb_playback_prepare(void *proxy_usb, bool set_default)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    // Configure sample rate based on flag
    // set_default: 'false' means CPCall configuration 48KHz, 16bit, 2CH or supported
    // set_default: 'true' means to configure selected best playback pcmconfig or supported
    if (set_default) {
        /* Use picked stream PCM config and check USB device supported list
         * Check whether picked config is supported by connect USB device or not
         * if supported: use picked config
         * if Not supported: Use default supported config */
        usb_get_best_matching_config(&aproxy_usb->usbplayback_devlist,
            aproxy_usb->active_playback_picked_format,
            aproxy_usb->active_playback_picked_channels,
            aproxy_usb->active_playback_picked_rate,
            &aproxy_usb->usb_out_active_pcmconfig);
        aproxy_usb->usb_out_cpcall_prepared = false;
    } else { // CPCall fixed or supported configuration
        usb_get_best_matching_config(&aproxy_usb->usbplayback_devlist,
            DEFAULT_USB_MEDIA_FORMAT,
            DEFAULT_USB_MEDIA_CHANNELS,
            DEFAULT_USB_MEDIA_SAMPLING_RATE,
            &aproxy_usb->usb_out_active_pcmconfig);
        aproxy_usb->usb_out_cpcall_prepared = true;
    }

    ALOGI("proxy-%s: configured USB Out Proxy SR(%d) CH(%d) FMT(%d)", __func__,
        aproxy_usb->usb_out_active_pcmconfig.rate,
        aproxy_usb->usb_out_active_pcmconfig.channels,
        aproxy_usb->usb_out_active_pcmconfig.format);

    return;
}

int proxy_usb_getparam_playback_stream(void *proxy_usb, void *query_params, void *reply_params)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    struct str_parms *query = (struct str_parms *)query_params;
    struct str_parms *reply = (struct str_parms *)reply_params;

    if (aproxy_usb->usb_out_connected) {
        // supported sample formats
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
            char * format_list = usb_get_format_strs(aproxy_usb->usb_out_formats_mask);
            if (format_list) {
                str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_FORMATS, format_list);
                free(format_list);
            }
        }

        // supported channel counts
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
            char* channels_list = usb_get_channel_count_strs(aproxy_usb, aproxy_usb->usb_out_channels_mask, USB_OUT);
            if (channels_list) {
                str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, channels_list);
                free(channels_list);
            }
        }

        // supported sample rates
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
            char* rates_list = usb_get_sample_rate_strs(aproxy_usb->usb_out_rates_mask);
            if (rates_list) {
                str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES, rates_list);
                free(rates_list);
            }
        }
    }

    return 0;
}

int proxy_usb_setparam_playback_stream(void *proxy_usb, void *parameters)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    int ret = 0;
    int card = -1;
    int device = -1;

    if (!parse_card_device_params((const char *)parameters, &card, &device)) {
        // nothing to do
        return ret;
    }

#if 0 //FIXME: check again when below code is need or not
    alsa_device_profile *profile = &aproxy_usb->usb_out_profile;

    if (card >= 0 && device >= 0) {
        int saved_card = profile->card;
        int saved_device = profile->device;

        if (saved_card != card || saved_device != device) {
            profile->card = card;
            profile->device = device;
            ret = profile_read_device_info(profile) ? 0 : -EINVAL;
            if (ret != 0) {
                profile->card = saved_card;
                profile->device = saved_device;
                ALOGI("%s-%s: updated USB Card %d Device %d", "usb-out",
                      __func__, profile->card, profile->device);
            }
        } else
            ALOGV("%s-%s: requested same USB Card %d Device %d", "usb-out",
                  __func__, profile->card, profile->device);
    }
#endif
    return ret;
}

void proxy_usb_capture_prepare(void *proxy_usb, bool set_default)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;

    // Configure sample rate based on flag
    // set_default: 'false' means to configuration 48KHz, 16bit, 2CH or supported
    // set_default: 'true' means single clock source USB device therefore
    // use USB output configuration
    if (set_default) {
        // Get default or playback Sample rate based on USB clock source
        usb_get_best_matching_config(&aproxy_usb->usbcapture_devlist,
            DEFAULT_USB_MEDIA_FORMAT,
            DEFAULT_USB_MEDIA_CHANNELS,
            ((is_usb_single_clksource() && aproxy_usb->usb_out_connected) ?
            aproxy_usb->usb_out_active_pcmconfig.rate : DEFAULT_USB_MEDIA_SAMPLING_RATE),
            &aproxy_usb->usb_in_active_pcmconfig);
    } else { // CPCall fixed or supported configuration
        usb_get_best_matching_config(&aproxy_usb->usbcapture_devlist,
            DEFAULT_USB_MEDIA_FORMAT,
            DEFAULT_USB_MEDIA_CHANNELS,
            DEFAULT_USB_MEDIA_SAMPLING_RATE,
            &aproxy_usb->usb_in_active_pcmconfig);
    }

    ALOGI("proxy-%s: configured USB InProxy SR(%d) CH(%d) FMT(%d)", __func__,
            aproxy_usb->usb_in_active_pcmconfig.rate,
            aproxy_usb->usb_in_active_pcmconfig.channels,
            aproxy_usb->usb_in_active_pcmconfig.format);

    return;
}

int proxy_usb_getparam_capture_stream(void *proxy_usb, void *query_params, void *reply_params)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    struct str_parms *query = (struct str_parms *)query_params;
    struct str_parms *reply = (struct str_parms *)reply_params;

    if (aproxy_usb->usb_in_connected) {
        // supported sample formats
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
            char* format_list = usb_get_format_strs(aproxy_usb->usb_in_formats_mask);
            if (format_list) {
                str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_FORMATS, format_list);
                free(format_list);
            }
        }

        // supported channel counts
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
            char* channels_list = usb_get_channel_count_strs(aproxy_usb, aproxy_usb->usb_in_channels_mask, USB_IN);
            if (channels_list) {
                str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, channels_list);
                free(channels_list);
            }
        }

        /* supported sample rates */
        if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
            char* rates_list = usb_get_sample_rate_strs(aproxy_usb->usb_in_rates_mask);
            if (rates_list) {
                str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES, rates_list);
                free(rates_list);
            }
        }
    }

    return 0;
}

int proxy_usb_setparam_capture_stream(void *proxy_usb, void *parameters)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    int card = -1;
    int device = -1;
    int ret = 0;

    if (!parse_card_device_params((const char *)parameters, &card, &device)) {
        // nothing to do
        return ret;
    }

#if 0 //FIXME: Check again whether below code is required or not
    alsa_device_profile *profile = &aproxy_usb->usb_in_profile;

    if (card >= 0 && device >= 0) {
        int saved_card = profile->card;
        int saved_device = profile->device;

        if (saved_card != card || saved_device != device) {
            profile->card = card;
            profile->device = device;
            ret = profile_read_device_info(profile) ? 0 : -EINVAL;
            if (ret != 0) {
                profile->card = saved_card;
                profile->device = saved_device;
                ALOGE("%s-%s: failed to read device info", "usb-in", __func__);
            } else {
                ALOGI("%s-%s: USB Capture device initialized for Card %d Device %d",
                      "usb-in", __func__, profile->card, profile->device);

                //prepare_capture_usbproxy(apstream);
            }
        } else
            ALOGV("%s-%s: requested same USB Card %d Device %d", "usb-in",
                  __func__, profile->card, profile->device);
    }
#endif
    return ret;
}

bool proxy_usb_out_pick_best_pcmconfig(
    void *proxy_usb,
    struct pcm_config cur_pcmconfig)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    bool is_updated = false;

    ALOGI("%s: current config rate[%d] format[%d] channels[%d]",
        __func__, cur_pcmconfig.rate,
        cur_pcmconfig.format,
        cur_pcmconfig.channels);

    /* select best configs compared to usb best picked config */
    if (cur_pcmconfig.rate > aproxy_usb->active_playback_picked_rate) {
        aproxy_usb->active_playback_picked_rate = cur_pcmconfig.rate;
        is_updated = true;
    }

    if (IS_HIGHEST_PCMFORMAT(cur_pcmconfig.format, aproxy_usb->active_playback_picked_format)) {
        aproxy_usb->active_playback_picked_format = cur_pcmconfig.format;
        is_updated = true;
    }

    if (cur_pcmconfig.channels > aproxy_usb->active_playback_picked_channels) {
        aproxy_usb->active_playback_picked_channels = cur_pcmconfig.channels;
        is_updated = true;
    }

    ALOGI_IF(is_updated, "%s: Selected config rate[%d] format[%d] channels[%d]",
        __func__, aproxy_usb->active_playback_picked_rate,
        aproxy_usb->active_playback_picked_format,
        aproxy_usb->active_playback_picked_channels);

    return is_updated;
}

bool proxy_usb_out_reconfig_needed(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    bool reconfig_needed = false;
    struct pcm_config sup_pcmconfig;

    pthread_mutex_lock(&aproxy_usb->usb_lock);

    /* get usb matching for selected stream config */
    usb_get_best_matching_config(&aproxy_usb->usbplayback_devlist,
    aproxy_usb->active_playback_picked_format,
    aproxy_usb->active_playback_picked_channels,
    aproxy_usb->active_playback_picked_rate,
    &sup_pcmconfig);

    if (aproxy_usb->usb_out_connected) {
        /* check whether best pcmconfig is supported by USB device */
        if (aproxy_usb->usb_out_active_pcmconfig.rate != sup_pcmconfig.rate ||
            aproxy_usb->usb_out_active_pcmconfig.format != sup_pcmconfig.format ||
            aproxy_usb->usb_out_active_pcmconfig.channels != sup_pcmconfig.channels)
            reconfig_needed = true;
    }

    ALOGI_IF(reconfig_needed, "%s: need reconfig rate[%d] format[%d] channels[%d]",
        __func__, aproxy_usb->active_playback_picked_rate,
        aproxy_usb->active_playback_picked_format,
        aproxy_usb->active_playback_picked_channels);

    pthread_mutex_unlock(&aproxy_usb->usb_lock);

    return reconfig_needed;
}

void proxy_usb_out_reset_config(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    pthread_mutex_lock(&aproxy_usb->usb_lock);

    //Initialize playback picked pcm config to default values
    aproxy_usb->active_playback_picked_rate = DEFAULT_USB_MEDIA_SAMPLING_RATE;
    aproxy_usb->active_playback_picked_format = DEFAULT_USB_MEDIA_FORMAT;
    aproxy_usb->active_playback_picked_channels = DEFAULT_USB_MEDIA_CHANNELS;

    ALOGI("%s-%s: reset rate[%d] format[%d] channels[%d]", "usb-out", __func__,
        aproxy_usb->active_playback_picked_rate,
        aproxy_usb->active_playback_picked_format,
        aproxy_usb->active_playback_picked_channels);

    pthread_mutex_unlock(&aproxy_usb->usb_lock);

    return;
}

void proxy_usb_open_out_proxy(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    pthread_mutex_lock(&aproxy_usb->usb_lock);

    usb_open_out_proxy(aproxy_usb);

    pthread_mutex_unlock(&aproxy_usb->usb_lock);

    return;
}
void proxy_usb_close_out_proxy(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    pthread_mutex_lock(&aproxy_usb->usb_lock);

    usb_close_out_proxy(aproxy_usb);

    pthread_mutex_unlock(&aproxy_usb->usb_lock);

    return;
}
void proxy_usb_open_in_proxy(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    pthread_mutex_lock(&aproxy_usb->usb_lock);

    usb_open_in_proxy(aproxy_usb);

    pthread_mutex_unlock(&aproxy_usb->usb_lock);

    return;
}
void proxy_usb_close_in_proxy(void *proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    pthread_mutex_lock(&aproxy_usb->usb_lock);

    usb_close_in_proxy(aproxy_usb);

    pthread_mutex_unlock(&aproxy_usb->usb_lock);

    return;
}

void proxy_usb_set_gain(void *proxy_usb, char *path_name)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    char gain_name[MAX_USB_PATH_LEN];

    if (!aproxy_usb->usb_gaincontrol_needed)
        return ;

    strlcpy(gain_name, path_name, MAX_USB_PATH_LEN);
    strlcat(gain_name, "-gain", MAX_USB_PATH_LEN);
    audio_route_apply_and_update_path(aproxy_usb->usb_ar, gain_name);
    ALOGI("proxy-%s: routed to %s", __func__, gain_name);

    return;
}

void proxy_usb_reset_gain(void *proxy_usb, char *path_name)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    char gain_name[MAX_USB_PATH_LEN];

    if (!aproxy_usb->usb_gaincontrol_needed)
        return ;

    strlcpy(gain_name, path_name, MAX_USB_PATH_LEN);
    strlcat(gain_name, "-gain", MAX_USB_PATH_LEN);
    audio_route_reset_and_update_path(aproxy_usb->usb_ar, gain_name);
    ALOGI("proxy-%s: routed to %s", __func__, gain_name);

    return;
}

int proxy_usb_set_parameters(void *proxy_usb, void *parameters)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    struct str_parms *parms = (struct str_parms *)parameters;
    int val;
    int ret = 0;     // for parameter handling
    int status = 0;  // for return value

    ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_CONNECT, &val);
    if (ret >= 0) {
        if ((audio_devices_t)val == AUDIO_DEVICE_OUT_USB_DEVICE ||
            (audio_devices_t)val == AUDIO_DEVICE_OUT_USB_HEADSET) {
            int card = -1, device = -1;

            ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_CARD, &val);
            if (ret >= 0)
                card = val;

            ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_DEVICE, &val);
            if (ret >= 0)
                device = val;

            ALOGI("proxy-%s: connected USB Out Device with card %d / device %d", __func__, card, device);

            if (!aproxy_usb->usb_out_connected && (card != -1 && device != -1)) {
                pthread_mutex_lock(&aproxy_usb->usb_lock);
                aproxy_usb->usb_out_connected = true;
                aproxy_usb->usb_out_pcm_card = card;
                aproxy_usb->usb_out_pcm_device = device;

                // reset mask values before updating
                aproxy_usb->usb_out_formats_mask = 0;
                aproxy_usb->usb_out_channels_mask = 0;
                aproxy_usb->usb_out_rates_mask = 0;

                /* get usb output profile information */
                usb_get_profile_capability(proxy_usb, USB_OUT);
                usb_print_device_info(proxy_usb, USB_OUT);
                usb_get_best_matching_config(&aproxy_usb->usbplayback_devlist,
                    aproxy_usb->active_playback_picked_format,
                    aproxy_usb->active_playback_picked_channels,
                    aproxy_usb->active_playback_picked_rate,
                    &aproxy_usb->usb_out_active_pcmconfig);
                //check and enable gain-control for connected USB-Device
                usb_audio_gain_control_enable(aproxy_usb);
                pthread_mutex_unlock(&aproxy_usb->usb_lock);
            }
        } else if ((audio_devices_t)val == AUDIO_DEVICE_IN_USB_DEVICE ||
                   (audio_devices_t)val == AUDIO_DEVICE_IN_USB_HEADSET) {
            int card = -1, device = -1;

            ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_CARD, &val);
            if (ret >= 0)
                card = val;

            ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_DEVICE, &val);
            if (ret >= 0)
                device = val;

            ALOGI("proxy-%s: connected USB In Device with card %d / device %d", __func__, card, device);

            if (!aproxy_usb->usb_in_connected && (card != -1 && device != -1)) {
                pthread_mutex_lock(&aproxy_usb->usb_lock);
                aproxy_usb->usb_in_connected = true;
                aproxy_usb->usb_in_pcm_card = card;
                aproxy_usb->usb_in_pcm_device = device;

                // reset mask values before updating
                aproxy_usb->usb_in_formats_mask = 0;
                aproxy_usb->usb_in_channels_mask = 0;
                aproxy_usb->usb_in_rates_mask = 0;

                /* get usb input profile information */
                usb_get_profile_capability(proxy_usb, USB_IN);
                usb_print_device_info(proxy_usb, USB_IN);
                usb_get_best_matching_config(&aproxy_usb->usbcapture_devlist,
                    DEFAULT_USB_MEDIA_FORMAT,
                    DEFAULT_USB_MEDIA_CHANNELS,
                    DEFAULT_USB_MEDIA_SAMPLING_RATE,
                    &aproxy_usb->usb_in_active_pcmconfig);
                //check and enable gain-control for connected USB-Device
                usb_audio_gain_control_enable(aproxy_usb);
                pthread_mutex_unlock(&aproxy_usb->usb_lock);
            }
        }

        // Check and update usb device clock source information
        if (aproxy_usb->usb_out_connected ||
            aproxy_usb->usb_in_connected) {
            update_usb_clksource_info(true);
        }
    }

    ret = str_parms_get_int(parms, AUDIO_PARAMETER_DEVICE_DISCONNECT, &val);
    if (ret >= 0) {
        if ((audio_devices_t)val == AUDIO_DEVICE_OUT_USB_DEVICE ||
            (audio_devices_t)val == AUDIO_DEVICE_OUT_USB_HEADSET) {
            ALOGI("proxy-%s: disconnected USB Out Device with card %d / device %d", __func__,
                aproxy_usb->usb_out_pcm_card, aproxy_usb->usb_out_pcm_device);

            if (aproxy_usb->usb_out_connected) {
                pthread_mutex_lock(&aproxy_usb->usb_lock);
                usb_close_out_proxy(aproxy_usb);
                usb_remove_device_info(proxy_usb, USB_OUT);

                aproxy_usb->usb_out_pcm_card = -1;
                aproxy_usb->usb_out_pcm_device = -1;
                aproxy_usb->usb_out_connected = false;
                aproxy_usb->usb_out_formats_mask = 0;
                aproxy_usb->usb_out_channels_mask = 0;
                aproxy_usb->usb_out_rates_mask = 0;

                //check and enable gain-control for connected USB-Device
                usb_audio_gain_control_disable(aproxy_usb);
                pthread_mutex_unlock(&aproxy_usb->usb_lock);
            }
        } else if ((audio_devices_t)val == AUDIO_DEVICE_IN_USB_DEVICE ||
                   (audio_devices_t)val == AUDIO_DEVICE_IN_USB_HEADSET) {
            ALOGI("proxy-%s: disconnected USB In Device with card %d / device %d", __func__,
                aproxy_usb->usb_in_pcm_card, aproxy_usb->usb_in_pcm_device);

            if (aproxy_usb->usb_in_connected) {
                pthread_mutex_lock(&aproxy_usb->usb_lock);
                usb_close_in_proxy(aproxy_usb);
                usb_remove_device_info(proxy_usb, USB_IN);

                aproxy_usb->usb_in_pcm_card = -1;
                aproxy_usb->usb_in_pcm_device = -1;
                aproxy_usb->usb_in_connected = false;
                aproxy_usb->usb_in_formats_mask = 0;
                aproxy_usb->usb_in_channels_mask = 0;
                aproxy_usb->usb_in_rates_mask = 0;

                //check and enable gain-control for connected USB-Device
                usb_audio_gain_control_disable(aproxy_usb);
                pthread_mutex_unlock(&aproxy_usb->usb_lock);
            }
        }

        // Check and update usb device clock source information
        if (((audio_devices_t)val == AUDIO_DEVICE_OUT_USB_DEVICE ||
            (audio_devices_t)val == AUDIO_DEVICE_OUT_USB_HEADSET ||
            (audio_devices_t)val == AUDIO_DEVICE_IN_USB_DEVICE ||
            (audio_devices_t)val == AUDIO_DEVICE_IN_USB_HEADSET) &&
            (!aproxy_usb->usb_out_connected && !aproxy_usb->usb_in_connected)) {
            update_usb_clksource_info(false);
        }
    }

    return status;
}

void * proxy_usb_init(void)
{
    struct audio_proxy_usb *aproxy_usb;

    /* Get audio_proxy_usb singleton instance*/
    aproxy_usb = getUSBInstance();
    if (!aproxy_usb) {
        ALOGE("proxy-%s: failed to create for audio_proxy_usb", __func__);
        return NULL;
    }
    // USB PCM Devices
    pthread_mutex_init(&aproxy_usb->usb_lock, (const pthread_mutexattr_t *) NULL);

    pthread_mutex_lock(&aproxy_usb->usb_lock);
    aproxy_usb->usb_out_connected = false;
    aproxy_usb->usb_out_status = false;
    aproxy_usb->usb_out_pcm_card = -1;
    aproxy_usb->usb_out_pcm_device = -1;
    aproxy_usb->usb_out_cpcall_prepared = false;

    aproxy_usb->usb_in_connected = false;
    aproxy_usb->usb_in_pcm_card = -1;
    aproxy_usb->usb_in_pcm_device = -1;

    //Initialize gain-control varibles
    aproxy_usb->usb_gaincontrol_needed = false;
    aproxy_usb->usb_vid = -1;
    aproxy_usb->usb_pid = -1;

    //Initialize playback picked pcm config to default values
    aproxy_usb->active_playback_picked_rate = DEFAULT_USB_MEDIA_SAMPLING_RATE;
    aproxy_usb->active_playback_picked_channels = DEFAULT_USB_MEDIA_CHANNELS;
    aproxy_usb->active_playback_picked_format = DEFAULT_USB_MEDIA_FORMAT;

    list_init(&aproxy_usb->usbplayback_devlist);
    list_init(&aproxy_usb->usbcapture_devlist);
    aproxy_usb->usb_out_pcm = NULL;
    aproxy_usb->usb_in_pcm = NULL;
    aproxy_usb->usb_out_formats_mask = 0;
    aproxy_usb->usb_out_channels_mask = 0;
    aproxy_usb->usb_out_rates_mask = 0;
    aproxy_usb->usb_in_formats_mask = 0;
    aproxy_usb->usb_in_channels_mask = 0;
    aproxy_usb->usb_in_rates_mask = 0;

    pthread_mutex_unlock(&aproxy_usb->usb_lock);

    ALOGI("proxy-%s: opened & initialized USB Audio Proxy", __func__);

    return (void *)aproxy_usb;
}

void proxy_usb_deinit(void* proxy_usb)
{
    struct audio_proxy_usb *aproxy_usb = (struct audio_proxy_usb *)proxy_usb;
    pthread_mutex_destroy(&aproxy_usb->usb_lock);

    destroyUSBInstance();
    ALOGI("proxy-%s: audio_proxy_usb instance destroyed", __func__);

    return ;
}
