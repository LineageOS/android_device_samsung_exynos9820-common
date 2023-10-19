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

#ifndef __EXYNOS_AUDIOPROXY_ABOX_H__
#define __EXYNOS_AUDIOPROXY_ABOX_H__

#include <linux/ioctl.h>
#include <linux/types.h>

/* A-Box HW limitations */

// Supported Sampling Rate
#define MAX_NUM_PLAYBACK_SR     8
unsigned int supported_playback_samplingrate[MAX_NUM_PLAYBACK_SR] = {8000, 16000, 32000, 44100, 48000, 96000, 192000, 384000};

/* In spite of A-Box spec, we need to fix 48KHz recording only to support some solution limitation */
#define MAX_NUM_CAPTURE_SR      1
unsigned int supported_capture_samplingrate[MAX_NUM_CAPTURE_SR] = {48000};

// Supported Channel Mask
#define MAX_NUM_PLAYBACK_CM     2
audio_channel_mask_t supported_playback_channelmask[MAX_NUM_PLAYBACK_CM] = {AUDIO_CHANNEL_OUT_MONO, AUDIO_CHANNEL_OUT_STEREO};

// Supported direct Channel Mask
#define MAX_NUM_DIRECT_PLAYBACK_CM     3
audio_channel_mask_t supported_direct_playback_channelmask[MAX_NUM_DIRECT_PLAYBACK_CM] = {AUDIO_CHANNEL_OUT_5POINT1, AUDIO_CHANNEL_OUT_6POINT1, AUDIO_CHANNEL_OUT_7POINT1};

#define MAX_NUM_CAPTURE_CM      2
audio_channel_mask_t supported_capture_channelmask[MAX_NUM_CAPTURE_CM] = {AUDIO_CHANNEL_IN_STEREO, AUDIO_CHANNEL_IN_FRONT_BACK};

// Supported PCM Format
#define MAX_NUM_PLAYBACK_PF     3
audio_format_t supported_playback_pcmformat[MAX_NUM_PLAYBACK_PF] = {AUDIO_FORMAT_PCM_16_BIT, AUDIO_FORMAT_PCM_8_24_BIT, AUDIO_FORMAT_PCM_32_BIT};

#define MAX_NUM_CAPTURE_PF      2
audio_format_t supported_capture_pcmformat[MAX_NUM_CAPTURE_PF] = {AUDIO_FORMAT_PCM_16_BIT, AUDIO_FORMAT_PCM_8_24_BIT};

// Supported Audio Format
#define MAX_NUM_PLAYBACK_AF     1
audio_format_t supported_playback_audioformat[MAX_NUM_PLAYBACK_AF] = {AUDIO_FORMAT_MP3};

/* Calliope Firmware Dump */
#define CALLIOPE_LOG_BUFFERSIZE     (4 * 1024)

#define CALLIOPE_DBG_PATH  "/sys/kernel/debug/abox/"
#define CALLIOPE_LOG       "log-00"
#define SYSFS_PREFIX       "/sys"
#define ABOX_DEV           "/devices/platform/18c50000.abox/"
#define ABOX_REGMAP_PATH   "/d/regmap/18c50000.abox/"
#define ABOX_DEBUG         "0.abox-debug/"
#define ABOX_SRAM          "calliope_sram"
#define ABOX_DRAM          "calliope_dram"
#define ABOX_REG_FILE      "registers"
#define ABOX_DUMP          "/data/vendor/log/abox/"
#define ABOX_DUMP_LIMIT (10)
#define ABOX_GPR           "gpr"

// ION Memory MMAP FD retreiving interface
struct snd_pcm_mmap_fd {
    int32_t dir;
    int32_t fd;
    int32_t size;
    int32_t actual_size;
};

#define SNDRV_PCM_IOCTL_MMAP_DATA_FD    _IOWR('U', 0xd2, struct snd_pcm_mmap_fd)

#endif  // __EXYNOS_AUDIOPROXY_ABOX_H__
