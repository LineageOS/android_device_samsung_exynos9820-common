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

#ifndef AUDIO_USB_PROXY_INTERFACE_H
#define AUDIO_USB_PROXY_INTERFACE_H

/* USB Proxy interface function prototypes */
int proxy_is_usb_playback_CPCall_prepared(void *proxy_usb);
int proxy_is_usb_playback_device_connected(void *proxy_usb);
int proxy_is_usb_capture_device_connected(void *proxy_usb);
unsigned int proxy_usb_get_capture_samplerate(void *proxy_usb);
unsigned int proxy_usb_get_capture_channels(void *proxy_usb);
int proxy_usb_get_capture_format(void *proxy_usb);
int proxy_usb_get_playback_samplerate(void *proxy_usb);
int proxy_usb_get_playback_channels(void *proxy_usb);
int proxy_usb_get_playback_format(void *proxy_usb);
int proxy_usb_get_playback_bitwidth(void *proxy_usb);
int proxy_usb_get_playback_highest_supported_channels(void *proxy_usb);

// Audio Stream USB Proxy Playback Functions
void proxy_usb_playback_prepare(void *proxy_usb, bool set_default);
int  proxy_usb_getparam_playback_stream(void *proxy_usb, void *query_params, void *reply_params);
int  proxy_usb_setparam_playback_stream(void *proxy_usb, void *parameters);

// Audio Stream USB Proxy Capture Functions
void proxy_usb_capture_prepare(void *proxy_usb, bool set_default);
int  proxy_usb_getparam_capture_stream(void *proxy_usb, void *query_params, void *reply_params);
int  proxy_usb_setparam_capture_stream(void *proxy_usb, void *parameters);

// Audio USB Device Proxy Functions
bool proxy_usb_out_pick_best_pcmconfig(void *proxy_usb, struct pcm_config cur_pcmconfig);
int proxy_usb_out_reconfig_needed(void *proxy_usb);
void proxy_usb_out_reset_config(void *proxy_usb);
void proxy_usb_open_out_proxy(void *proxy_usb);
void proxy_usb_close_out_proxy(void *proxy_usb);
void proxy_usb_open_in_proxy(void *proxy_usb);
void proxy_usb_close_in_proxy(void *proxy_usb);

// set parameters function carries USB configuration inforamtion
void proxy_usb_set_gain(void *proxy_usb, char *path_name);
void proxy_usb_reset_gain(void *proxy_usb, char *path_name);
int proxy_usb_set_parameters(void *proxy_usb, void *parameters);
void *proxy_usb_init(void);
void proxy_usb_deinit(void* proxy_usb);

#endif /* AUDIO_USB_PROXY_INTERFACE_H */
