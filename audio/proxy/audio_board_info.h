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

#ifndef _AUDIO_BOARD_INFO_H_
#define _AUDIO_BOARD_INFO_H_

 /* Audio Board Device's Information */

#define BOARD_INFO_XML_PATH     "vendor/etc/audio_board_info.xml"

#define AUDIO_STRING_TO_ENUM(X) {#X, X}
#define ARRAY_SIZE(x)           (sizeof((x))/sizeof((x)[0]) )

typedef enum {
    INFO_NONE,
    MICROPHONE_CHARACTERISTIC,
} set_information;

static set_information set_info;

struct audio_string_to_enum {
    char *name;
    int  value;
};

struct audio_string_to_enum device_in_type[] = {
    AUDIO_STRING_TO_ENUM(AUDIO_DEVICE_IN_BUILTIN_MIC),
    AUDIO_STRING_TO_ENUM(AUDIO_DEVICE_IN_BACK_MIC),
};

struct audio_string_to_enum microphone_location[AUDIO_MICROPHONE_LOCATION_CNT] = {
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_LOCATION_UNKNOWN),
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_LOCATION_MAINBODY),
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_LOCATION_MAINBODY_MOVABLE),
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_LOCATION_PERIPHERAL),
};

struct audio_string_to_enum microphone_directionality[AUDIO_MICROPHONE_DIRECTIONALITY_CNT] = {
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_DIRECTIONALITY_UNKNOWN),
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_DIRECTIONALITY_OMNI),
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_DIRECTIONALITY_BI_DIRECTIONAL),
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_DIRECTIONALITY_CARDIOID),
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_DIRECTIONALITY_HYPER_CARDIOID),
    AUDIO_STRING_TO_ENUM(AUDIO_MICROPHONE_DIRECTIONALITY_SUPER_CARDIOID),
};

#endif  // _AUDIO_BOARD_INFO_H_
