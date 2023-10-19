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

#ifndef AUDIO_USB_PROXY_H
#define AUDIO_USB_PROXY_H

#include <system/audio.h>
#include <hardware/hardware.h>
#include <hardware/audio.h>
#include <audio_route/audio_route.h>
#include <cutils/list.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define DEFAULT_USB_PERIOD_COUNT        4
#define DEFAULT_USB_PLAYBACK_DURATION   10 //10ms
#define DEFAULT_USB_CAPTURE_DURATION    10 //10ms

// Supported formats
int supported_usb_formats[] = {PCM_FORMAT_S32_LE, PCM_FORMAT_S24_3LE, PCM_FORMAT_S24_LE, PCM_FORMAT_S16_LE, PCM_FORMAT_S8};
static const uint32_t MAX_NUM_USB_FORMAT = ARRAY_SIZE(supported_usb_formats);
char *  const supported_usb_format_strs[] = {
    "AUDIO_FORMAT_PCM_32_BIT",
    "AUDIO_FORMAT_PCM_24_BIT_PACKED",
    "AUDIO_FORMAT_PCM_8_24_BIT",
    "AUDIO_FORMAT_PCM_16_BIT",
    "AUDIO_FORMAT_PCM_8_BIT"};

// Supported channels
#define MAX_NUM_USB_CHANNELS            8
char * const supported_usb_channel_strs[] = {
    /* 0 */"AUDIO_CHANNEL_NONE", /* will never be taken as this is a terminator */
    /* 1 */"AUDIO_CHANNEL_INDEX_MASK_1",
    /* 2 */"AUDIO_CHANNEL_INDEX_MASK_2",
    /* 3 */"AUDIO_CHANNEL_INDEX_MASK_3",
    /* 4 */"AUDIO_CHANNEL_INDEX_MASK_4",
    /* 5 */"AUDIO_CHANNEL_INDEX_MASK_5",
    /* 6 */"AUDIO_CHANNEL_INDEX_MASK_6",
    /* 7 */"AUDIO_CHANNEL_INDEX_MASK_7",
    /* 8 */"AUDIO_CHANNEL_INDEX_MASK_8",
};

char * const supported_usb_out_channel_strs[] = {
    /* 0 */"AUDIO_CHANNEL_NONE", /* will never be taken as this is a terminator */
    /* 1 */"AUDIO_CHANNEL_OUT_MONO",
    /* 2 */"AUDIO_CHANNEL_OUT_STEREO",
};

char * const supported_usb_in_channel_strs[] = {
    /* 0 */"AUDIO_CHANNEL_NONE", /* will never be taken as this is a terminator */
    /* 1 */"AUDIO_CHANNEL_IN_MONO",
    /* 2 */"AUDIO_CHANNEL_IN_STEREO",
};

// Supported Sampling Rate
unsigned int supported_usb_samplingrates[] = {384000, 192000, 96000, 48000, 44100, 32000, 16000, 8000};

static const uint32_t MAX_NUM_USB_SR = ARRAY_SIZE(supported_usb_samplingrates);
char *  const supported_usb_samplingrate_strs[] = {
    "384000",
    "192000",
    "96000",
    "48000",
    "44100",
    "32000",
    "16000",
    "8000"};

typedef enum usb_direction_type{
    USB_OUT = 0,
    USB_IN,
} usb_direction_type_t;

struct usb_device_info
{
    struct listnode node;
    enum pcm_format format;
    unsigned int bit_width;
    unsigned int channels;
    unsigned int rate_size;
    unsigned int rates[MAX_NUM_USB_SR];
};

struct audio_proxy_usb
{
    pthread_mutex_t usb_lock;

    struct listnode usbplayback_devlist;
    int  usb_out_pcm_card;
    int  usb_out_pcm_device;
    unsigned int usb_out_formats_mask;
    unsigned int usb_out_channels_mask;
    unsigned int usb_out_rates_mask;
    struct pcm_config usb_out_active_pcmconfig;
    struct pcm *usb_out_pcm;
    bool usb_out_connected;
    bool usb_out_status;
    bool usb_out_cpcall_prepared;

    struct listnode usbcapture_devlist;
    int  usb_in_pcm_card;
    int  usb_in_pcm_device;
    unsigned int usb_in_formats_mask;
    unsigned int usb_in_channels_mask;
    unsigned int usb_in_rates_mask;
    struct pcm_config usb_in_active_pcmconfig;
    struct pcm *usb_in_pcm;
    bool usb_in_connected;

    bool usb_gaincontrol_needed;
    int usb_vid;
    int usb_pid;
    struct audio_route *usb_ar;

    // active Playback streams best PCM config
    unsigned int active_playback_picked_rate;
    unsigned int active_playback_picked_channels;
    enum pcm_format active_playback_picked_format;
};

/* Default values for Media PCM Configuration */
#define DEFAULT_USB_CAPTURE_CHANNELS        1                   // Mono
#define DEFAULT_USB_MEDIA_CHANNELS          2                   // Stereo
#define DEFAULT_USB_MEDIA_SAMPLING_RATE     48000               // 48KHz
#define DEFAULT_USB_MEDIA_FORMAT            PCM_FORMAT_S16_LE   // 16bit PCM

#define MAX_USB_PATH_LEN                    256
#define USB_READ_SIZE                       128


#define AUDIO_PARAMETER_DEVICE_CARD   "card"
#define AUDIO_PARAMETER_DEVICE_DEVICE "device"

/* USB Bundle Device VID (Vendor ID): PID (Product ID) definitions */
#define USB_BUNDLE_VID                      0x04e8
#define USB_BUNDLE_WHITE_PID                0xa037
#define USB_BUNDLE_GRAY_HEADPHONE_PID       0xa04b
#define USB_BUNDLE_GRAY_HEADSET_PID         0xa04c

/* USB Device VID (Vendor ID): PID (Product ID) definitions */
#define USB_BUNDLE_WHITE_GAIN_XML_MIXER_PATH    "/vendor/etc/mixer_usb_white.xml"
#define USB_BUNDLE_GRAY_GAIN_XML_MIXER_PATH     "/vendor/etc/mixer_usb_gray.xml"

extern void update_usb_clksource_info(bool flag);
extern bool is_usb_single_clksource();

/* PCM format in increasing preference order */
static const int pcm_format_order_weight[] = {
    2,  /* PCM_FORMAT_S16_LE, 16-bit signed */
    5,  /* PCM_FORMAT_S32_LE, 32-bit signed */
    1,  /* PCM_FORMAT_S8, 8-bit signed */
    3,  /* PCM_FORMAT_S24_LE, 24-bits in 4-bytes */
    4,  /* PCM_FORMAT_S24_3LE, 24-bits in 3-bytes */
};

#define IS_HIGHEST_PCMFORMAT(a, b) (pcm_format_order_weight[a] > pcm_format_order_weight[b])

#endif /* AUDIO_USB_PROXY_H */
