/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef __EXYNOS_AUDIOPROXY_PCM_H__
#define __EXYNOS_AUDIOPROXY_PCM_H__

#include <tinyalsa/asoundlib.h>
#include <tinycompress/tinycompress.h>
#include <compress_params.h>


/* Actual HW DMA mapped Sound Card & Device Definition */
#define SOUND_CARD0                     0

// Sound Devices mapped for A-Box RDMA
#define SOUND_DEVICE_ABOX_RDMA0         0       // A-Box RDMA0
#define SOUND_DEVICE_ABOX_RDMA1         1       // A-Box RDMA1
#define SOUND_DEVICE_ABOX_RDMA2         2       // A-Box RDMA2
#define SOUND_DEVICE_ABOX_RDMA3         3       // A-Box RDMA3
#define SOUND_DEVICE_ABOX_RDMA4         4       // A-Box RDMA4
#define SOUND_DEVICE_ABOX_RDMA5         5       // A-Box RDMA5
#define SOUND_DEVICE_ABOX_RDMA6         6       // A-Box RDMA6
#define SOUND_DEVICE_ABOX_RDMA7         7       // A-Box RDMA7
#define SOUND_DEVICE_ABOX_RDMA8         8       // A-Box RDMA8
#define SOUND_DEVICE_ABOX_RDMA9         9       // A-Box RDMA9
#define SOUND_DEVICE_ABOX_RDMA10        10      // A-Box RDMA10
#define SOUND_DEVICE_ABOX_RDMA11        11      // A-Box RDMA11

// Sound Devices mapped for A-Box WDMA
#define SOUND_DEVICE_ABOX_WDMA0         12      // A-Box WDMA0
#define SOUND_DEVICE_ABOX_WDMA1         13      // A-Box WDMA1
#define SOUND_DEVICE_ABOX_WDMA2         14      // A-Box WDMA2
#define SOUND_DEVICE_ABOX_WDMA3         15      // A-Box WDMA3
#define SOUND_DEVICE_ABOX_WDMA4         16      // A-Box WDMA4
#define SOUND_DEVICE_ABOX_WDMA5         17      // A-Box WDMA5
#define SOUND_DEVICE_ABOX_WDMA6         18      // A-Box WDMA6
#define SOUND_DEVICE_ABOX_WDMA7         19      // A-Box WDMA7

// Sound Devices mapped for other DMA
// Devices 20 & 21 are used by VTS driver
#define SOUND_DEVICE_AUX                22      // Aux Digital Device for DP Audio

/* Vitural PCM DAI Sound Card & Device Definition */
#define SOUND_CARD1                     1

// Sound Devices using Virtual PCM DAIs with sound-card1
// Playback devices
#define SOUND_DEVICE_VIRT_PRIMARY_PLAYBACK       0       // primary playback virtual device
// Capture devices
#define SOUND_DEVICE_VIRT_PRIMARY_CAPTURE        20      // primary capture virtual device
#define SOUND_DEVICE_VIRT_FM_RECORD              21      // WDMA for FM Radio Recording

/* Virtual DMA mapped Sound Card & Device Definition */
#define SOUND_CARD2                     2

// Device  0 ~ 12 : used for A-Box DMA Dump
// Device 13 ~ 15 : used for Compress Offload Dump
// Device 16 ~ 18 : used for OEM Analysis Dump
// From Device 19
#define SOUND_DEVICE_CALL_RECORD        24      // WDMA for Call Recording
#define SOUND_DEVICE_TELEPHONYRX_RECORD 34      // TelephonyRx Recording virtual pcm

#define SOUND_DEVICE_UNDEFINE           99

/* Default values for Media PCM Configuration */
#define DEFAULT_CAPTURE_CHANNELS                1                   // Mono
#define DEFAULT_MEDIA_CHANNELS                  2                   // Stereo
#define DEFAULT_MEDIA_SAMPLING_RATE             48000               // 48KHz
#define DEFAULT_MEDIA_FORMAT                    PCM_FORMAT_S16_LE   // 16bit PCM
#define DEFAULT_MEDIA_24_ZEROPAD_FORMAT         PCM_FORMAT_S24_LE   // 24bit PCM
#define DEFAULT_MEDIA_32_FORMAT                 PCM_FORMAT_S32_LE   // 32bit PCM

// Definition for MMAP Stream
#define MMAP_PERIOD_SIZE (DEFAULT_MEDIA_SAMPLING_RATE/1000)
#define MMAP_PERIOD_COUNT_MIN 32
#define MMAP_PERIOD_COUNT_MAX 512
#define MMAP_PERIOD_COUNT_DEFAULT (MMAP_PERIOD_COUNT_MAX)

// Channel count definitions
#define MEDIA_1_CHANNEL                         1
/* 4Channels is required for SPK AMP/Quad Mic TDM */
#define MEDIA_4_CHANNELS                        4
/* 8Channels is required for Direct output stream */
#define MEDIA_8_CHANNELS                        8

/* Default values for Voice PCM Configuration */
#define SAMPLING_RATE_NB                8000                // 8KHz(Narrow Band)
#define SAMPLING_RATE_WB                16000               // 16KHz(Wide Band)
#define SAMPLING_RATE_SWB               32000               // 32KHz(Wide Band)
#define SAMPLING_RATE_FB                48000               // 48KHz(Full Band)

#define DEFAULT_VOICE_CHANNELS          2                   // Stereo
#define DEFAULT_VOICE_SAMPLING_RATE     SAMPLING_RATE_SWB
#define DEFAULT_VOICE_FORMAT            PCM_FORMAT_S16_LE   // 16bit PCM

/* Default values for CP Voice Recording PCM Configuration */
#define DEFAULT_VOICE_REC_CHANNELS      2                   // Stereo
#define DEFAULT_VOICE_REC_SAMPLINGRATE  SAMPLING_RATE_SWB   // 32KHz
#define DEFAULT_VOICE_REC_PERIODSIZE    2048                // Sync with A-Box Firmware
#define DEFAULT_VOICE_REC_PERIODCOUNT   2
#define DEFAULT_VOICE_REC_FORMAT        PCM_FORMAT_S16_LE   // 16bit PCM

/* Default values for FM Recording PCM Configuration */
#define DEFAULT_FM_REC_CHANNELS         2                   // Stereo
#define DEFAULT_FM_REC_SAMPLINGRATE     48000               // 48KHz
#define DEFAULT_FM_REC_PERIODSIZE       480                // Sync with WDMA7 config
#define DEFAULT_FM_REC_PERIODCOUNT      4
#define DEFAULT_FM_REC_FORMAT           PCM_FORMAT_S16_LE   // 16bit PCM

#define UHQA_MEDIA_FORMAT               PCM_FORMAT_S24_LE   // 24bit PCM
#define UHQA_MEDIA_SAMPLING_RATE        192000

#define SUHQA_MEDIA_FORMAT              PCM_FORMAT_S32_LE   // 32bit PCM
#define SUHQA_MEDIA_SAMPLING_RATE       384000

/* Default values for USB Playback or Capture PCM Configuration */
#define DEFAULT_MEDIA_BITWIDTH              24   // 24bit PCM
#define ABOX_UNSUPPORTED_CHANNELS           6
#define ABOX_SUPPORTED_MAX_CHANNELS         8
//----------------------------------------------------------------------------------------------//
// For Playback (Speaker) Path

/* PCM Configurations */
// PCM Configurations for Primary Playback Stream
#define PRIMARY_PLAYBACK_CARD           SOUND_CARD0
#define PRIMARY_PLAYBACK_DEVICE         SOUND_DEVICE_ABOX_RDMA0

#define PRIMARY_PLAYBACK_CHANNELS       DEFAULT_MEDIA_CHANNELS
#define PRIMARY_PLAYBACK_SAMPLING_RATE  DEFAULT_MEDIA_SAMPLING_RATE
#define PRIMARY_PLAYBACK_PERIOD_SIZE    960
#define PRIMARY_PLAYBACK_PERIOD_COUNT   4
#define PRIMARY_PLAYBACK_FORMAT         DEFAULT_MEDIA_FORMAT
#define PRIMARY_PLAYBACK_START          PRIMARY_PLAYBACK_PERIOD_SIZE
#define PRIMARY_PLAYBACK_STOP           UINT_MAX

struct pcm_config pcm_config_primary_playback = {
    .channels        = PRIMARY_PLAYBACK_CHANNELS,
    .rate            = PRIMARY_PLAYBACK_SAMPLING_RATE,
    .period_size     = PRIMARY_PLAYBACK_PERIOD_SIZE,
    .period_count    = PRIMARY_PLAYBACK_PERIOD_COUNT,
    .format          = PRIMARY_PLAYBACK_FORMAT,
    .start_threshold = PRIMARY_PLAYBACK_START,
    .stop_threshold  = PRIMARY_PLAYBACK_STOP,
};

// PCM Configurations for Virtual Primary playback Stream
#define VIRTUAL_PRIMARY_PLAYBACK_CARD           SOUND_CARD1
#define VIRTUAL_PRIMARY_PLAYBACK_DEVICE         SOUND_DEVICE_VIRT_PRIMARY_PLAYBACK

// PCM Configurations for Fast Playback Stream
#define FAST_PLAYBACK_CARD              SOUND_CARD0
#define FAST_PLAYBACK_DEVICE            SOUND_DEVICE_ABOX_RDMA1

#define FAST_PLAYBACK_CHANNELS          DEFAULT_MEDIA_CHANNELS
#define FAST_PLAYBACK_SAMPLING_RATE     DEFAULT_MEDIA_SAMPLING_RATE
#define FAST_PLAYBACK_PERIOD_SIZE       192
#define FAST_PLAYBACK_PERIOD_COUNT      2
#define FAST_PLAYBACK_FORMAT            DEFAULT_MEDIA_FORMAT
#define FAST_PLAYBACK_START             (FAST_PLAYBACK_PERIOD_SIZE * FAST_PLAYBACK_PERIOD_COUNT)
#define FAST_PLAYBACK_STOP              UINT_MAX

struct pcm_config pcm_config_fast_playback = {
    .channels        = FAST_PLAYBACK_CHANNELS,
    .rate            = FAST_PLAYBACK_SAMPLING_RATE,
    .period_size     = FAST_PLAYBACK_PERIOD_SIZE,
    .period_count    = FAST_PLAYBACK_PERIOD_COUNT,
    .format          = FAST_PLAYBACK_FORMAT,
    .start_threshold = FAST_PLAYBACK_START,
    .stop_threshold  = FAST_PLAYBACK_STOP,
};

// PCM Configurations for Low Latency Playback Stream
#define LOW_PLAYBACK_CARD               SOUND_CARD0
#define LOW_PLAYBACK_DEVICE             SOUND_DEVICE_ABOX_RDMA1

#define LOW_PLAYBACK_CHANNELS           DEFAULT_MEDIA_CHANNELS
#define LOW_PLAYBACK_SAMPLING_RATE      DEFAULT_MEDIA_SAMPLING_RATE
#define LOW_PLAYBACK_PERIOD_SIZE        96
#define LOW_PLAYBACK_PERIOD_COUNT       4
#define LOW_PLAYBACK_FORMAT             DEFAULT_MEDIA_FORMAT
#define LOW_PLAYBACK_START              LOW_PLAYBACK_PERIOD_SIZE
#define LOW_PLAYBACK_STOP               UINT_MAX

struct pcm_config pcm_config_low_playback = {
    .channels        = LOW_PLAYBACK_CHANNELS,
    .rate            = LOW_PLAYBACK_SAMPLING_RATE,
    .period_size     = LOW_PLAYBACK_PERIOD_SIZE,
    .period_count    = LOW_PLAYBACK_PERIOD_COUNT,
    .format          = LOW_PLAYBACK_FORMAT,
    .start_threshold = LOW_PLAYBACK_START,
    .stop_threshold  = LOW_PLAYBACK_STOP,
};

// PCM Configurations for MMAP Playback Stream
#define MMAP_PLAYBACK_CARD               SOUND_CARD0
#define MMAP_PLAYBACK_DEVICE             SOUND_DEVICE_ABOX_RDMA2

#define MMAP_PLAYBACK_CHANNELS           DEFAULT_MEDIA_CHANNELS
#define MMAP_PLAYBACK_SAMPLING_RATE      DEFAULT_MEDIA_SAMPLING_RATE
#define MMAP_PLAYBACK_PERIOD_SIZE        MMAP_PERIOD_SIZE
#define MMAP_PLAYBACK_PERIOD_COUNT       MMAP_PERIOD_COUNT_DEFAULT
#define MMAP_PLAYBACK_FORMAT             DEFAULT_MEDIA_FORMAT
#define MMAP_PLAYBACK_START              (MMAP_PLAYBACK_PERIOD_SIZE * 8)
#define MMAP_PLAYBACK_STOP               UINT_MAX

struct pcm_config pcm_config_mmap_playback = {
    .channels        = MMAP_PLAYBACK_CHANNELS,
    .rate            = MMAP_PLAYBACK_SAMPLING_RATE,
    .period_size     = MMAP_PLAYBACK_PERIOD_SIZE,
    .period_count    = MMAP_PLAYBACK_PERIOD_COUNT,
    .format          = MMAP_PLAYBACK_FORMAT,
    .start_threshold = MMAP_PLAYBACK_START,
    .stop_threshold  = MMAP_PLAYBACK_STOP,
};

// PCM Configurations for DeepBuffer Playback Stream
#define DEEP_PLAYBACK_CARD              SOUND_CARD0
#define DEEP_PLAYBACK_DEVICE            SOUND_DEVICE_ABOX_RDMA3

#define DEEP_PLAYBACK_CHANNELS          DEFAULT_MEDIA_CHANNELS
#define DEEP_PLAYBACK_SAMPLING_RATE     DEFAULT_MEDIA_SAMPLING_RATE
#define DEEP_PLAYBACK_PERIOD_SIZE       960
#define DEEP_PLAYBACK_PERIOD_COUNT      4
#define DEEP_PLAYBACK_FORMAT            DEFAULT_MEDIA_24_ZEROPAD_FORMAT
#define DEEP_PLAYBACK_START             DEEP_PLAYBACK_PERIOD_SIZE
#define DEEP_PLAYBACK_STOP              (DEEP_PLAYBACK_PERIOD_SIZE * DEEP_PLAYBACK_PERIOD_COUNT)

struct pcm_config pcm_config_deep_playback = {
    .channels        = DEEP_PLAYBACK_CHANNELS,
    .rate            = DEEP_PLAYBACK_SAMPLING_RATE,
    .period_size     = DEEP_PLAYBACK_PERIOD_SIZE,
    .period_count    = DEEP_PLAYBACK_PERIOD_COUNT,
    .format          = DEEP_PLAYBACK_FORMAT,
    .start_threshold = DEEP_PLAYBACK_START,
    .stop_threshold  = DEEP_PLAYBACK_STOP,
};

// PCM Configurations for Deep UHQA Playback Stream
#define DEEP_PLAYBACK_UHQA_FORMAT            UHQA_MEDIA_FORMAT
#define DEEP_PLAYBACK_UHQA_SAMPLING_RATE     UHQA_MEDIA_SAMPLING_RATE

#define DEEP_PLAYBACK_SUHQA_FORMAT           SUHQA_MEDIA_FORMAT
#define DEEP_PLAYBACK_SUHQA_SAMPLING_RATE    SUHQA_MEDIA_SAMPLING_RATE

struct pcm_config pcm_config_deep_playback_wide_res = {
    .channels        = DEEP_PLAYBACK_CHANNELS,
    .rate            = DEEP_PLAYBACK_SAMPLING_RATE,
    .period_size     = DEEP_PLAYBACK_PERIOD_SIZE,
    .period_count    = DEEP_PLAYBACK_PERIOD_COUNT,
    .format          = DEEP_PLAYBACK_UHQA_FORMAT,
};

struct pcm_config pcm_config_deep_playback_uhqa = {
    .channels        = DEEP_PLAYBACK_CHANNELS,
    .rate            = DEEP_PLAYBACK_UHQA_SAMPLING_RATE,
    .period_size     = DEEP_PLAYBACK_PERIOD_SIZE * 4,
    .period_count    = DEEP_PLAYBACK_PERIOD_COUNT,
    .format          = DEEP_PLAYBACK_UHQA_FORMAT,
};

struct pcm_config pcm_config_deep_playback_suhqa = {
    .channels        = DEEP_PLAYBACK_CHANNELS,
    .rate            = DEEP_PLAYBACK_SUHQA_SAMPLING_RATE,
    .period_size     = DEEP_PLAYBACK_PERIOD_SIZE * 8,
    .period_count    = DEEP_PLAYBACK_PERIOD_COUNT,
    .format          = DEEP_PLAYBACK_SUHQA_FORMAT,
};

// PCM Configurations for Voice RX Playback Stream
#define VRX_PLAYBACK_CARD               SOUND_CARD0
#define VRX_PLAYBACK_DEVICE             SOUND_DEVICE_ABOX_RDMA4

#define VRX_PLAYBACK_CHANNELS           DEFAULT_VOICE_CHANNELS
#define VRX_PLAYBACK_SAMPLING_RATE      DEFAULT_VOICE_SAMPLING_RATE
#define VRX_PLAYBACK_PERIOD_SIZE        480
#define VRX_PLAYBACK_PERIOD_COUNT       4
#define VRX_PLAYBACK_FORMAT             DEFAULT_VOICE_FORMAT
#define VRX_PLAYBACK_START              VRX_PLAYBACK_PERIOD_SIZE
#define VRX_PLAYBACK_STOP               UINT_MAX

struct pcm_config pcm_config_voicerx_playback = {
    .channels        = VRX_PLAYBACK_CHANNELS,
    .rate            = VRX_PLAYBACK_SAMPLING_RATE,
    .period_size     = VRX_PLAYBACK_PERIOD_SIZE,
    .period_count    = VRX_PLAYBACK_PERIOD_COUNT,
    .format          = VRX_PLAYBACK_FORMAT,
    .start_threshold = VRX_PLAYBACK_START,
    .stop_threshold  = VRX_PLAYBACK_STOP,
};

// PCM Configurations for Compress Offload Playback Stream
#define OFFLOAD_PLAYBACK_CARD           SOUND_CARD0
#define OFFLOAD_PLAYBACK_DEVICE         SOUND_DEVICE_ABOX_RDMA5

/*
 * These values are based on HW Decoder: Max Buffer Size = FRAGMENT_SIZE * NUM_FRAGMENTS
 * 0 means that we will use the predefined value by device driver
 */
// Offload Buffer Size is set to Maximum possible buffer size,
// as offload Pause issue after Drain notification is resolved from kernel side
#define OFFLOAD_PLAYBACK_BUFFER_SIZE  (1024 * 64)  // fragment_size is fixed 64KBytes = 64 * 1024
#define OFFLOAD_PLAYBACK_BUFFER_COUNT 5            // fragment is fixed 5

#define OFFLOAD_OFFLOAD_FRAGMENT_SIZE OFFLOAD_PLAYBACK_BUFFER_SIZE
#define OFFLOAD_OFFLOAD_NUM_FRAGMENTS OFFLOAD_PLAYBACK_BUFFER_COUNT

struct compr_config compr_config_offload_playback = {
    .fragment_size  = OFFLOAD_OFFLOAD_FRAGMENT_SIZE,
    .fragments      = OFFLOAD_OFFLOAD_NUM_FRAGMENTS,
    .codec          = NULL,
};

// Internal loopback related PCM node configurations
// PCM Configurations for BT-SCO Playback Stream
#define BTSCO_PLAYBACK_CARD             SOUND_CARD0
#define BTSCO_PLAYBACK_DEVICE           SOUND_DEVICE_ABOX_RDMA6

#define BTSCO_PLAYBACK_CHANNELS         DEFAULT_MEDIA_CHANNELS
#define BTSCO_PLAYBACK_SAMPLING_RATE    DEFAULT_MEDIA_SAMPLING_RATE
#define BTSCO_PLAYBACK_PERIOD_SIZE      480
#define BTSCO_PLAYBACK_PERIOD_COUNT     4
#define BTSCO_PLAYBACK_FORMAT           DEFAULT_MEDIA_32_FORMAT
#define BTSCO_PLAYBACK_START            BTSCO_PLAYBACK_PERIOD_SIZE
#define BTSCO_PLAYBACK_STOP             UINT_MAX

struct pcm_config pcm_config_btsco_playback = {
    .channels        = BTSCO_PLAYBACK_CHANNELS,
    .rate            = BTSCO_PLAYBACK_SAMPLING_RATE,
    .period_size     = BTSCO_PLAYBACK_PERIOD_SIZE,
    .period_count    = BTSCO_PLAYBACK_PERIOD_COUNT,
    .format          = BTSCO_PLAYBACK_FORMAT,
    .start_threshold = BTSCO_PLAYBACK_START,
    .stop_threshold  = BTSCO_PLAYBACK_STOP,
};

// Internal loopback related PCM node configurations
// PCM Configurations for BT-A2DP Playback Stream
#define BTA2DP_PLAYBACK_CARD             SOUND_CARD0
#define BTA2DP_PLAYBACK_DEVICE           SOUND_DEVICE_ABOX_RDMA6

#define BTA2DP_PLAYBACK_CHANNELS         DEFAULT_MEDIA_CHANNELS
#define BTA2DP_PLAYBACK_SAMPLING_RATE    DEFAULT_MEDIA_SAMPLING_RATE
#define BTA2DP_PLAYBACK_PERIOD_SIZE      480
#define BTA2DP_PLAYBACK_PERIOD_COUNT     4
#define BTA2DP_PLAYBACK_FORMAT           DEFAULT_MEDIA_FORMAT
#define BTA2DP_PLAYBACK_START            BTA2DP_PLAYBACK_PERIOD_SIZE
#define BTA2DP_PLAYBACK_STOP             UINT_MAX

struct pcm_config pcm_config_bta2dp_playback = {
    .channels        = BTA2DP_PLAYBACK_CHANNELS,
    .rate            = BTA2DP_PLAYBACK_SAMPLING_RATE,
    .period_size     = BTA2DP_PLAYBACK_PERIOD_SIZE,
    .period_count    = BTA2DP_PLAYBACK_PERIOD_COUNT,
    .format          = BTA2DP_PLAYBACK_FORMAT,
    .start_threshold = BTA2DP_PLAYBACK_START,
    .stop_threshold  = BTA2DP_PLAYBACK_STOP,
};

// PCM Configurations for A2DP Mute Playback Stream
#define A2DPMUTE_PLAYBACK_CARD          SOUND_CARD0
#define A2DPMUTE_PLAYBACK_DEVICE        SOUND_DEVICE_ABOX_RDMA11

#define A2DPMUTE_PLAYBACK_CHANNELS      DEFAULT_MEDIA_CHANNELS
#define A2DPMUTE_PLAYBACK_SAMPLING_RATE DEFAULT_MEDIA_SAMPLING_RATE
#define A2DPMUTE_PLAYBACK_PERIOD_SIZE   480
#define A2DPMUTE_PLAYBACK_PERIOD_COUNT  4
#define A2DPMUTE_PLAYBACK_FORMAT        DEFAULT_MEDIA_FORMAT
#define A2DPMUTE_PLAYBACK_START         A2DPMUTE_PLAYBACK_PERIOD_SIZE
#define A2DPMUTE_PLAYBACK_STOP          UINT_MAX

struct pcm_config pcm_config_a2dp_mute_playback = {
    .channels        = A2DPMUTE_PLAYBACK_CHANNELS,
    .rate            = A2DPMUTE_PLAYBACK_SAMPLING_RATE,
    .period_size     = A2DPMUTE_PLAYBACK_PERIOD_SIZE,
    .period_count    = A2DPMUTE_PLAYBACK_PERIOD_COUNT,
    .format          = A2DPMUTE_PLAYBACK_FORMAT,
    .start_threshold = A2DPMUTE_PLAYBACK_START,
    .stop_threshold  = A2DPMUTE_PLAYBACK_STOP,
};

// PCM Configurations for SpeakerAMP Playback Stream
#define SPKAMP_PLAYBACK_CARD            SOUND_CARD0
#define SPKAMP_PLAYBACK_DEVICE          SOUND_DEVICE_ABOX_RDMA7

#define SPKAMP_PLAYBACK_CHANNELS        MEDIA_4_CHANNELS
#define SPKAMP_PLAYBACK_SAMPLING_RATE   DEFAULT_MEDIA_SAMPLING_RATE
#define SPKAMP_PLAYBACK_PERIOD_SIZE     480
#define SPKAMP_PLAYBACK_PERIOD_COUNT    4
#define SPKAMP_PLAYBACK_FORMAT          DEFAULT_MEDIA_32_FORMAT
#define SPKAMP_PLAYBACK_START           SPKAMP_PLAYBACK_PERIOD_SIZE
#define SPKAMP_PLAYBACK_STOP            UINT_MAX

struct pcm_config pcm_config_spkamp_playback = {
    .channels        = SPKAMP_PLAYBACK_CHANNELS,
    .rate            = SPKAMP_PLAYBACK_SAMPLING_RATE,
    .period_size     = SPKAMP_PLAYBACK_PERIOD_SIZE,
    .period_count    = SPKAMP_PLAYBACK_PERIOD_COUNT,
    .format          = SPKAMP_PLAYBACK_FORMAT,
    .start_threshold = SPKAMP_PLAYBACK_START,
    .stop_threshold  = SPKAMP_PLAYBACK_STOP,
};

// PCM Configurations for FM Radio Playback Stream
#define FMRADIO_PLAYBACK_CARD           SOUND_CARD0
#define FMRADIO_PLAYBACK_DEVICE         SOUND_DEVICE_ABOX_RDMA8

#define FMRADIO_PLAYBACK_CHANNELS       DEFAULT_MEDIA_CHANNELS
#define FMRADIO_PLAYBACK_SAMPLING_RATE  DEFAULT_MEDIA_SAMPLING_RATE
#define FMRADIO_PLAYBACK_PERIOD_SIZE    480
#define FMRADIO_PLAYBACK_PERIOD_COUNT   4
#define FMRADIO_PLAYBACK_FORMAT         DEFAULT_MEDIA_FORMAT
#define FMRADIO_PLAYBACK_START          FMRADIO_PLAYBACK_PERIOD_SIZE
#define FMRADIO_PLAYBACK_STOP           UINT_MAX

struct pcm_config pcm_config_fmradio_playback = {
    .channels        = FMRADIO_PLAYBACK_CHANNELS,
    .rate            = FMRADIO_PLAYBACK_SAMPLING_RATE,
    .period_size     = FMRADIO_PLAYBACK_PERIOD_SIZE,
    .period_count    = FMRADIO_PLAYBACK_PERIOD_COUNT,
    .format          = FMRADIO_PLAYBACK_FORMAT,
    .start_threshold = FMRADIO_PLAYBACK_START,
    .stop_threshold  = FMRADIO_PLAYBACK_STOP,
};

// PCM Configurations for Direct Playback Stream
#define DIRECT_PLAYBACK_CARD            SOUND_CARD0
#define DIRECT_PLAYBACK_DEVICE          SOUND_DEVICE_ABOX_RDMA9

#define DIRECT_PLAYBACK_CHANNELS        MEDIA_8_CHANNELS
#define DIRECT_PLAYBACK_SAMPLING_RATE   DEFAULT_MEDIA_SAMPLING_RATE
#define DIRECT_PLAYBACK_PERIOD_SIZE     480
#define DIRECT_PLAYBACK_PERIOD_COUNT    4
#define DIRECT_PLAYBACK_FORMAT          DEFAULT_MEDIA_24_ZEROPAD_FORMAT
#define DIRECT_PLAYBACK_START           DIRECT_PLAYBACK_PERIOD_SIZE
#define DIRECT_PLAYBACK_STOP            (DIRECT_PLAYBACK_PERIOD_SIZE * DIRECT_PLAYBACK_PERIOD_COUNT)

struct pcm_config pcm_config_direct_playback = {
    .channels        = DIRECT_PLAYBACK_CHANNELS,
    .rate            = DIRECT_PLAYBACK_SAMPLING_RATE,
    .period_size     = DIRECT_PLAYBACK_PERIOD_SIZE,
    .period_count    = DIRECT_PLAYBACK_PERIOD_COUNT,
    .format          = DIRECT_PLAYBACK_FORMAT,
};

// PCM Configurations for USB Output Loopback Stream
#define USBOUT_LOOPBACK_CARD             SOUND_CARD0
#define USBOUT_LOOPBACK_DEVICE           SOUND_DEVICE_ABOX_WDMA6

#define USBOUT_LOOPBACK_CHANNELS         DEFAULT_MEDIA_CHANNELS
#define USBOUT_LOOPBACK_SAMPLING_RATE    DEFAULT_MEDIA_SAMPLING_RATE
#define USBOUT_LOOPBACK_PERIOD_SIZE      480
#define USBOUT_LOOPBACK_PERIOD_COUNT     2
#define USBOUT_LOOPBACK_FORMAT           DEFAULT_MEDIA_FORMAT
#define USBOUT_LOOPBACK_START            USBOUT_LOOPBACK_PERIOD_SIZE
#define USBOUT_LOOPBACK_STOP             UINT_MAX

struct pcm_config pcm_config_usb_out_loopback = {
    .channels        = USBOUT_LOOPBACK_CHANNELS,
    .rate            = USBOUT_LOOPBACK_SAMPLING_RATE,
    .period_size     = USBOUT_LOOPBACK_PERIOD_SIZE,
    .period_count    = USBOUT_LOOPBACK_PERIOD_COUNT,
    .format          = USBOUT_LOOPBACK_FORMAT,
    .start_threshold = USBOUT_LOOPBACK_START,
    .stop_threshold  = USBOUT_LOOPBACK_STOP,
};

// PCM Configurations for USB Input Loopback Stream
#define USBIN_LOOPBACK_CARD             SOUND_CARD0
#define USBIN_LOOPBACK_DEVICE           SOUND_DEVICE_ABOX_RDMA10

#define USBIN_LOOPBACK_CHANNELS         DEFAULT_MEDIA_CHANNELS
#define USBIN_LOOPBACK_SAMPLING_RATE    DEFAULT_MEDIA_SAMPLING_RATE
#define USBIN_LOOPBACK_PERIOD_SIZE      480
#define USBIN_LOOPBACK_PERIOD_COUNT     4
#define USBIN_LOOPBACK_FORMAT           DEFAULT_MEDIA_FORMAT
#define USBIN_LOOPBACK_START            USBIN_LOOPBACK_PERIOD_SIZE
#define USBIN_LOOPBACK_STOP             UINT_MAX

struct pcm_config pcm_config_usb_in_loopback = {
    .channels        = USBIN_LOOPBACK_CHANNELS,
    .rate            = USBIN_LOOPBACK_SAMPLING_RATE,
    .period_size     = USBIN_LOOPBACK_PERIOD_SIZE,
    .period_count    = USBIN_LOOPBACK_PERIOD_COUNT,
    .format          = USBIN_LOOPBACK_FORMAT,
    .start_threshold = USBIN_LOOPBACK_START,
    .stop_threshold  = USBIN_LOOPBACK_STOP,
};

// AUX doesn't use A-Box pcm node
// PCM Configurations for AUX Digital(HDMI / DisplayPort) Playback Stream
#define AUX_PLAYBACK_CARD               SOUND_CARD0
#define AUX_PLAYBACK_DEVICE             SOUND_DEVICE_AUX

#define AUX_PLAYBACK_CHANNELS           DEFAULT_MEDIA_CHANNELS
#define AUX_PLAYBACK_SAMPLING_RATE      DEFAULT_MEDIA_SAMPLING_RATE
#define AUX_PLAYBACK_PERIOD_SIZE        960
#define AUX_PLAYBACK_PERIOD_COUNT       2
#define AUX_PLAYBACK_FORMAT             DEFAULT_MEDIA_FORMAT
#define AUX_PLAYBACK_START              AUX_PLAYBACK_PERIOD_SIZE
#define AUX_PLAYBACK_STOP               UINT_MAX

struct pcm_config pcm_config_aux_playback = {
    .channels        = AUX_PLAYBACK_CHANNELS,
    .rate            = AUX_PLAYBACK_SAMPLING_RATE,
    .period_size     = AUX_PLAYBACK_PERIOD_SIZE,
    .period_count    = AUX_PLAYBACK_PERIOD_COUNT,
    .format          = AUX_PLAYBACK_FORMAT,
    .start_threshold = AUX_PLAYBACK_START,
    .stop_threshold  = AUX_PLAYBACK_STOP,
};

//----------------------------------------------------------------------------------------------//
// For Capture (MIC) Path

// FIXME: Currently unused configuration
// PCM Configurations for Mixed Capture Stream
#define MIXED_CAPTURE_CARD              SOUND_CARD0
#define MIXED_CAPTURE_DEVICE            SOUND_DEVICE_ABOX_WDMA0

#define MIXED_CAPTURE_CHANNELS          DEFAULT_MEDIA_CHANNELS
#define MIXED_CAPTURE_SAMPLING_RATE     DEFAULT_MEDIA_SAMPLING_RATE
#define MIXED_CAPTURE_PERIOD_SIZE       480
#define MIXED_CAPTURE_PERIOD_COUNT      4
#define MIXED_CAPTURE_FORMAT            DEFAULT_MEDIA_FORMAT
#define MIXED_CAPTURE_START             MIXED_CAPTURE_PERIOD_SIZE
#define MIXED_CAPTURE_STOP              UINT_MAX

struct pcm_config pcm_config_mixed_capture = {
    .channels        = MIXED_CAPTURE_CHANNELS,
    .rate            = MIXED_CAPTURE_SAMPLING_RATE,
    .period_size     = MIXED_CAPTURE_PERIOD_SIZE,
    .period_count    = MIXED_CAPTURE_PERIOD_COUNT,
    .format          = MIXED_CAPTURE_FORMAT,
    .start_threshold = MIXED_CAPTURE_START,
    .stop_threshold  = MIXED_CAPTURE_STOP,
};

// PCM Configurations for Primary Capture Stream
#define PRIMARY_CAPTURE_CARD            SOUND_CARD0
#define PRIMARY_CAPTURE_DEVICE          SOUND_DEVICE_ABOX_WDMA1

#define PRIMARY_CAPTURE_CHANNELS        DEFAULT_MEDIA_CHANNELS
#define PRIMARY_CAPTURE_SAMPLING_RATE   DEFAULT_MEDIA_SAMPLING_RATE
#define PRIMARY_CAPTURE_PERIOD_SIZE     960
#define PRIMARY_CAPTURE_PERIOD_COUNT    4
#define PRIMARY_CAPTURE_FORMAT          DEFAULT_MEDIA_FORMAT
#define PRIMARY_CAPTURE_START           PRIMARY_CAPTURE_PERIOD_SIZE
#define PRIMARY_CAPTURE_STOP            UINT_MAX

struct pcm_config pcm_config_primary_capture = {
    .channels        = PRIMARY_CAPTURE_CHANNELS,
    .rate            = PRIMARY_CAPTURE_SAMPLING_RATE,
    .period_size     = PRIMARY_CAPTURE_PERIOD_SIZE,
    .period_count    = PRIMARY_CAPTURE_PERIOD_COUNT,
    .format          = PRIMARY_CAPTURE_FORMAT,
    .start_threshold = PRIMARY_CAPTURE_START,
    .stop_threshold  = PRIMARY_CAPTURE_STOP,
};

#ifdef SUPPORT_QUAD_MIC
// PCM Configurations for Primary Quad-Mic 4 channel Capture Stream
#define PRIMARY_QUAD_CAPTURE_CHANNELS       MEDIA_4_CHANNELS
#define PRIMARY_QUAD_CAPTURE_SAMPLING_RATE  DEFAULT_MEDIA_SAMPLING_RATE
#define PRIMARY_QUAD_CAPTURE_PERIOD_SIZE    960
#define PRIMARY_QUAD_CAPTURE_PERIOD_COUNT   4
#define PRIMARY_QUAD_CAPTURE_FORMAT         DEFAULT_MEDIA_FORMAT
#define PRIMARY_QUAD_CAPTURE_START          PRIMARY_QUAD_CAPTURE_PERIOD_SIZE
#define PRIMARY_QUAD_CAPTURE_STOP           UINT_MAX

struct pcm_config pcm_config_primary_quad_mic_capture = {
    .channels        = PRIMARY_QUAD_CAPTURE_CHANNELS,
    .rate            = PRIMARY_QUAD_CAPTURE_SAMPLING_RATE,
    .period_size     = PRIMARY_QUAD_CAPTURE_PERIOD_SIZE,
    .period_count    = PRIMARY_QUAD_CAPTURE_PERIOD_COUNT,
    .format          = PRIMARY_QUAD_CAPTURE_FORMAT,
    .start_threshold = PRIMARY_QUAD_CAPTURE_START,
    .stop_threshold  = PRIMARY_QUAD_CAPTURE_STOP,
};
#endif

// PCM Configurations for Virtual Primary Capture Stream
#define VIRTUAL_PRIMARY_CAPTURE_CARD            SOUND_CARD1
#define VIRTUAL_PRIMARY_CAPTURE_DEVICE          SOUND_DEVICE_VIRT_PRIMARY_CAPTURE

// PCM Configurations for Low Latency Capture Stream
#define LOW_CAPTURE_CARD                SOUND_CARD0
#define LOW_CAPTURE_DEVICE              SOUND_DEVICE_ABOX_WDMA1

#define LOW_CAPTURE_CHANNELS            DEFAULT_MEDIA_CHANNELS
#define LOW_CAPTURE_SAMPLING_RATE       DEFAULT_MEDIA_SAMPLING_RATE
#define LOW_CAPTURE_PERIOD_SIZE         FAST_PLAYBACK_PERIOD_SIZE
#define LOW_CAPTURE_PERIOD_COUNT        FAST_PLAYBACK_PERIOD_COUNT
#define LOW_CAPTURE_FORMAT              DEFAULT_MEDIA_FORMAT
#define LOW_CAPTURE_START               LOW_CAPTURE_PERIOD_SIZE
#define LOW_CAPTURE_STOP                UINT_MAX

struct pcm_config pcm_config_low_capture = {
    .channels        = LOW_CAPTURE_CHANNELS,
    .rate            = LOW_CAPTURE_SAMPLING_RATE,
    .period_size     = LOW_CAPTURE_PERIOD_SIZE,
    .period_count    = LOW_CAPTURE_PERIOD_COUNT,
    .format          = LOW_CAPTURE_FORMAT,
    .start_threshold = LOW_CAPTURE_START,
    .stop_threshold  = LOW_CAPTURE_STOP,
};

// PCM Configurations for MMAP Capture Stream
#define MMAP_CAPTURE_CARD               SOUND_CARD0
#define MMAP_CAPTURE_DEVICE             SOUND_DEVICE_ABOX_WDMA1

#define MMAP_CAPTURE_CHANNELS           DEFAULT_MEDIA_CHANNELS
#define MMAP_CAPTURE_SAMPLING_RATE      DEFAULT_MEDIA_SAMPLING_RATE
#define MMAP_CAPTURE_PERIOD_SIZE        MMAP_PERIOD_SIZE
#define MMAP_CAPTURE_PERIOD_COUNT       MMAP_PERIOD_COUNT_DEFAULT
#define MMAP_CAPTURE_FORMAT             DEFAULT_MEDIA_FORMAT
#define MMAP_CAPTURE_START              MMAP_CAPTURE_PERIOD_SIZE
#define MMAP_CAPTURE_STOP               UINT_MAX

struct pcm_config pcm_config_mmap_capture = {
    .channels        = MMAP_CAPTURE_CHANNELS,
    .rate            = MMAP_CAPTURE_SAMPLING_RATE,
    .period_size     = MMAP_CAPTURE_PERIOD_SIZE,
    .period_count    = MMAP_CAPTURE_PERIOD_COUNT,
    .format          = MMAP_CAPTURE_FORMAT,
    .start_threshold = MMAP_CAPTURE_START,
    .stop_threshold  = MMAP_CAPTURE_STOP,
};

// PCM Configurations for Voice TX Capture Stream
#define VTX_CAPTURE_CARD                SOUND_CARD0
#define VTX_CAPTURE_DEVICE              SOUND_DEVICE_ABOX_WDMA2

#define VTX_CAPTURE_CHANNELS            DEFAULT_VOICE_CHANNELS
#define VTX_CAPTURE_SAMPLING_RATE       DEFAULT_VOICE_SAMPLING_RATE
#define VTX_CAPTURE_PERIOD_SIZE         480
#define VTX_CAPTURE_PERIOD_COUNT        4
#define VTX_CAPTURE_FORMAT              DEFAULT_VOICE_FORMAT
#define VTX_CAPTURE_START               VTX_CAPTURE_PERIOD_SIZE
#define VTX_CAPTURE_STOP                UINT_MAX

struct pcm_config pcm_config_voicetx_capture = {
    .channels        = VTX_CAPTURE_CHANNELS,
    .rate            = VTX_CAPTURE_SAMPLING_RATE,
    .period_size     = VTX_CAPTURE_PERIOD_SIZE,
    .period_count    = VTX_CAPTURE_PERIOD_COUNT,
    .format          = VTX_CAPTURE_FORMAT,
    .start_threshold = VTX_CAPTURE_START,
    .stop_threshold  = VTX_CAPTURE_STOP,
};

#ifdef SUPPORT_QUAD_MIC
// PCM Configurations for Quad-mic Voice TX Capture Stream
#define VTX_QUAD_MIC_CAPTURE_CHANNELS           MEDIA_4_CHANNELS
#define VTX_QUAD_MIC_CAPTURE_SAMPLING_RATE      DEFAULT_VOICE_SAMPLING_RATE
#define VTX_QUAD_MIC_CAPTURE_PERIOD_SIZE        480
#define VTX_QUAD_MIC_CAPTURE_PERIOD_COUNT       4
#define VTX_QUAD_MIC_CAPTURE_FORMAT             DEFAULT_VOICE_FORMAT
#define VTX_QUAD_MIC_CAPTURE_START              VTX_QUAD_MIC_CAPTURE_PERIOD_SIZE
#define VTX_QUAD_MIC_CAPTURE_STOP               UINT_MAX

struct pcm_config pcm_config_quad_mic_voicetx_capture = {
    .channels        = VTX_QUAD_MIC_CAPTURE_CHANNELS,
    .rate            = VTX_QUAD_MIC_CAPTURE_SAMPLING_RATE,
    .period_size     = VTX_QUAD_MIC_CAPTURE_PERIOD_SIZE,
    .period_count    = VTX_QUAD_MIC_CAPTURE_PERIOD_COUNT,
    .format          = VTX_QUAD_MIC_CAPTURE_FORMAT,
    .start_threshold = VTX_QUAD_MIC_CAPTURE_START,
    .stop_threshold  = VTX_QUAD_MIC_CAPTURE_STOP,
};
#endif

// PCM Configurations for FM Radio/Voice Call Capture Stream
#define VC_FMRADIO_CAPTURE_CARD            SOUND_CARD0
#define VC_FMRADIO_CAPTURE_DEVICE          SOUND_DEVICE_ABOX_WDMA7

#define VC_FMRADIO_CAPTURE_CHANNELS        DEFAULT_MEDIA_CHANNELS
#define VC_FMRADIO_CAPTURE_SAMPLING_RATE   DEFAULT_MEDIA_SAMPLING_RATE
#define VC_FMRADIO_CAPTURE_PERIOD_SIZE     480
#define VC_FMRADIO_CAPTURE_PERIOD_COUNT    4
#define VC_FMRADIO_CAPTURE_FORMAT          DEFAULT_MEDIA_FORMAT
#define VC_FMRADIO_CAPTURE_START           VC_FMRADIO_CAPTURE_PERIOD_SIZE
#define VC_FMRADIO_CAPTURE_STOP            UINT_MAX

struct pcm_config pcm_config_vc_fmradio_capture = {
    .channels        = VC_FMRADIO_CAPTURE_CHANNELS,
    .rate            = VC_FMRADIO_CAPTURE_SAMPLING_RATE,
    .period_size     = VC_FMRADIO_CAPTURE_PERIOD_SIZE,
    .period_count    = VC_FMRADIO_CAPTURE_PERIOD_COUNT,
    .format          = VC_FMRADIO_CAPTURE_FORMAT,
    .start_threshold = VC_FMRADIO_CAPTURE_START,
    .stop_threshold  = VC_FMRADIO_CAPTURE_STOP,
};

#ifdef SUPPORT_QUAD_MIC
// PCM Configuration for Quad-Mic direct Capture Stream
#define QUAD_MIC_CAPTURE_CHANNELS        MEDIA_4_CHANNELS
#define QUAD_MIC_CAPTURE_SAMPLING_RATE   DEFAULT_MEDIA_SAMPLING_RATE
#define QUAD_MIC_CAPTURE_PERIOD_SIZE     480
#define QUAD_MIC_CAPTURE_PERIOD_COUNT    4
#define QUAD_MIC_CAPTURE_FORMAT          DEFAULT_MEDIA_FORMAT
#define QUAD_MIC_CAPTURE_START           QUAD_MIC_CAPTURE_PERIOD_SIZE
#define QUAD_MIC_CAPTURE_STOP            UINT_MAX

struct pcm_config pcm_config_vc_quad_mic_capture = {
    .channels        = QUAD_MIC_CAPTURE_CHANNELS,
    .rate            = QUAD_MIC_CAPTURE_SAMPLING_RATE,
    .period_size     = QUAD_MIC_CAPTURE_PERIOD_SIZE,
    .period_count    = QUAD_MIC_CAPTURE_PERIOD_COUNT,
    .format          = QUAD_MIC_CAPTURE_FORMAT,
    .start_threshold = QUAD_MIC_CAPTURE_START,
    .stop_threshold  = QUAD_MIC_CAPTURE_STOP,
};
#endif

// PCM Configurations for ERAP In Stream
#define ERAP_IN_CARD                    SOUND_CARD0
#define ERAP_IN_DEVICE                  SOUND_DEVICE_ABOX_WDMA3

#define ERAP_IN_CHANNELS                DEFAULT_MEDIA_CHANNELS
#define ERAP_IN_SAMPLING_RATE           DEFAULT_MEDIA_SAMPLING_RATE
#define ERAP_IN_PERIOD_SIZE             480
#define ERAP_IN_PERIOD_COUNT            4
#define ERAP_IN_FORMAT                  DEFAULT_MEDIA_32_FORMAT
#define ERAP_IN_START                   ERAP_IN_PERIOD_SIZE
#define ERAP_IN_STOP                    UINT_MAX

struct pcm_config pcm_config_erap_in = {
    .channels        = ERAP_IN_CHANNELS,
    .rate            = ERAP_IN_SAMPLING_RATE,
    .period_size     = ERAP_IN_PERIOD_SIZE,
    .period_count    = ERAP_IN_PERIOD_COUNT,
    .format          = ERAP_IN_FORMAT,
    .start_threshold = ERAP_IN_START,
    .stop_threshold  = ERAP_IN_STOP,
};

// PCM Configurations for Speaker AMP Reference Stream
#define SPKAMP_REFERENCE_CARD           SOUND_CARD0
#define SPKAMP_REFERENCE_DEVICE         SOUND_DEVICE_ABOX_WDMA4

#define SPKAMP_REFERENCE_CHANNELS       MEDIA_4_CHANNELS
#define SPKAMP_REFERENCE_SAMPLING_RATE  DEFAULT_MEDIA_SAMPLING_RATE
#define SPKAMP_REFERENCE_PERIOD_SIZE    480
#define SPKAMP_REFERENCE_PERIOD_COUNT   4
#define SPKAMP_REFERENCE_FORMAT         DEFAULT_MEDIA_32_FORMAT
#define SPKAMP_REFERENCE_START          SPKAMP_REFERENCE_PERIOD_SIZE
#define SPKAMP_REFERENCE_STOP           UINT_MAX

struct pcm_config pcm_config_spkamp_reference = {
    .channels        = SPKAMP_REFERENCE_CHANNELS,
    .rate            = SPKAMP_REFERENCE_SAMPLING_RATE,
    .period_size     = SPKAMP_REFERENCE_PERIOD_SIZE,
    .period_count    = SPKAMP_REFERENCE_PERIOD_COUNT,
    .format          = SPKAMP_REFERENCE_FORMAT,
    .start_threshold = SPKAMP_REFERENCE_START,
    .stop_threshold  = SPKAMP_REFERENCE_STOP,
};

// PCM Configurations for BT A2DP Output Loopback Stream
#define BTA2DP_OUT_LOOPBACK_CARD             SOUND_CARD0
#define BTA2DP_OUT_LOOPBACK_DEVICE           SOUND_DEVICE_ABOX_WDMA5

#define BTA2DP_OUT_LOOPBACK_CHANNELS         DEFAULT_MEDIA_CHANNELS
#define BTA2DP_OUT_LOOPBACK_SAMPLING_RATE    DEFAULT_MEDIA_SAMPLING_RATE
#define BTA2DP_OUT_LOOPBACK_PERIOD_SIZE      480
#define BTA2DP_OUT_LOOPBACK_PERIOD_COUNT     4
#define BTA2DP_OUT_LOOPBACK_FORMAT           DEFAULT_MEDIA_FORMAT
#define BTA2DP_OUT_LOOPBACK_START            BTA2DP_OUT_LOOPBACK_PERIOD_SIZE
#define BTA2DP_OUT_LOOPBACK_STOP             UINT_MAX

struct pcm_config pcm_config_bta2dp_out_loopback = {
    .channels        = BTA2DP_OUT_LOOPBACK_CHANNELS,
    .rate            = BTA2DP_OUT_LOOPBACK_SAMPLING_RATE,
    .period_size     = BTA2DP_OUT_LOOPBACK_PERIOD_SIZE,
    .period_count    = BTA2DP_OUT_LOOPBACK_PERIOD_COUNT,
    .format          = BTA2DP_OUT_LOOPBACK_FORMAT,
    .start_threshold = BTA2DP_OUT_LOOPBACK_START,
    .stop_threshold  = BTA2DP_OUT_LOOPBACK_STOP,
};

// PCM Configurations for Voice Call/TelephonyRx Recording Stream
#define TELERX_RECORD_CARD              SOUND_CARD2
#define TELERX_RECORD_DEVICE            SOUND_DEVICE_TELEPHONYRX_RECORD

#define CALL_RECORD_CARD                SOUND_CARD2
#define CALL_RECORD_DEVICE              SOUND_DEVICE_CALL_RECORD

#define CALL_RECORD_CHANNELS            DEFAULT_VOICE_REC_CHANNELS
#define CALL_RECORD_SAMPLING_RATE       DEFAULT_VOICE_REC_SAMPLINGRATE
#define CALL_RECORD_PERIOD_SIZE         DEFAULT_VOICE_REC_PERIODSIZE
#define CALL_RECORD_PERIOD_COUNT        DEFAULT_VOICE_REC_PERIODCOUNT
#define CALL_RECORD_FORMAT              DEFAULT_VOICE_REC_FORMAT
#define CALL_RECORD_START               CALL_RECORD_PERIOD_SIZE
#define CALL_RECORD_STOP                UINT_MAX

struct pcm_config pcm_config_call_record = {
    .channels        = CALL_RECORD_CHANNELS,
    .rate            = CALL_RECORD_SAMPLING_RATE,
    .period_size     = CALL_RECORD_PERIOD_SIZE,
    .period_count    = CALL_RECORD_PERIOD_COUNT,
    .format          = CALL_RECORD_FORMAT,
    .start_threshold = CALL_RECORD_START,
    .stop_threshold  = CALL_RECORD_STOP,
};

// PCM Configurations for FM Radio Recording Stream
#define FM_RECORD_CARD                  SOUND_CARD1
#define FM_RECORD_DEVICE                SOUND_DEVICE_VIRT_FM_RECORD

#define FM_RECORD_CHANNELS              DEFAULT_FM_REC_CHANNELS
#define FM_RECORD_SAMPLING_RATE         DEFAULT_FM_REC_SAMPLINGRATE
#define FM_RECORD_PERIOD_SIZE           DEFAULT_FM_REC_PERIODSIZE
#define FM_RECORD_PERIOD_COUNT          DEFAULT_FM_REC_PERIODCOUNT
#define FM_RECORD_FORMAT                DEFAULT_FM_REC_FORMAT
#define FM_RECORD_START                 FM_RECORD_PERIOD_SIZE
#define FM_RECORD_STOP                  FM_RECORD_PERIOD_SIZE * FM_RECORD_PERIOD_COUNT

struct pcm_config pcm_config_fm_record = {
    .channels        = FM_RECORD_CHANNELS,
    .rate            = FM_RECORD_SAMPLING_RATE,
    .period_size     = FM_RECORD_PERIOD_SIZE,
    .period_count    = FM_RECORD_PERIOD_COUNT,
    .format          = FM_RECORD_FORMAT,
    .start_threshold = FM_RECORD_START,
    .stop_threshold  = FM_RECORD_STOP,
};


#ifdef SUPPORT_STHAL_INTERFACE
// PCM Configurations for hotword capture Stream
// Note: Should be matching with STHAL pcm configuration
#define DEFAULT_HOTWORD_CHANNELS                1       // Mono
#define DEFAULT_HOTWORD_SAMPLING_RATE           16000
#define HOTWORD_PERIOD_SIZE                     480     // 480 frames, 30ms in case of 16KHz Stream
#define HOTWORD_PERIOD_COUNT                    128     // Buffer count => Total  122880 Bytes = 480 * 1(Mono) * 2(16bit PCM) * 128(Buffer count)

struct pcm_config pcm_config_hotword_capture = {
    .channels = DEFAULT_HOTWORD_CHANNELS,
    .rate = DEFAULT_HOTWORD_SAMPLING_RATE,
    .period_size = HOTWORD_PERIOD_SIZE,
    .period_count = HOTWORD_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};
#endif

#define MAX_PCM_PATH_LEN 256

// Duration for DP Playback
#define PREDEFINED_DP_PLAYBACK_DURATION     20  // 20ms

// Duration for MMAP pcm configurations
#define PREDEFINED_MMAP_CAPTURE_DURATION    1   // 1ms

// Duration for Remote-Mic Playback/Capture loopback node configuration
#define PREDEFINED_REMOTE_MIC_DURATION      20  // 20ms

#endif  // __EXYNOS_AUDIOPROXY_PCM_H__
