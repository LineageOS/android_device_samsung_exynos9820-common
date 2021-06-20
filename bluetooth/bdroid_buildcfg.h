/*
 * Copyright (C) 2021 The LineageOS Project
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

#ifndef _BDROID_BUILDCFG_H
#define _BDROID_BUILDCFG_H

#pragma push_macro("PROPERTY_VALUE_MAX")

#if !defined(OS_GENERIC)
#include <cutils/properties.h>
#include <string.h>

static inline const char* getBTDefaultName()
{
    char device[PROPERTY_VALUE_MAX];
    property_get("ro.product.device", device, "");

    if (!strcmp("beyond0lte", device))
        return "Galaxy S10e";

    if (!strcmp("beyond1lte", device))
        return "Galaxy S10";

    if (!strcmp("beyond2lte", device))
        return "Galaxy S10+";

    if (!strcmp("beyondx", device))
        return "Galaxy S10 5G";

    if (!strcmp("d1", device))
        return "Galaxy Note10";

    if (!strcmp("d2s", device))
        return "Galaxy Note10+";

    if (!strcmp("d2x", device))
        return "Galaxy Note10+ 5G";

    return "";
}

#define BTM_DEF_LOCAL_NAME getBTDefaultName()
#endif /* OS_GENERIC */

/*
 * Toggles support for vendor specific extensions such as RPA offloading,
 * feature discovery, multi-adv etc.
 */
#define BLE_VND_INCLUDED TRUE

/* 'strings libbluetooth.so' */
#define BTA_AV_SINK_INCLUDED TRUE

#define BTM_WBS_INCLUDED TRUE       /* Enable WBS */
#define BTIF_HF_WBS_PREFERRED FALSE /* Don't prefer WBS    */

#define BTM_SCO_ENHANCED_SYNC_ENABLED FALSE

#pragma pop_macro("PROPERTY_VALUE_MAX")

#endif /* _BDROID_BUILDCFG_H */
