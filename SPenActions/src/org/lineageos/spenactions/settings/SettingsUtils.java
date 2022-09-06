/*
 * Copyright (c) 2021-2022 The LineageOS Project
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
package org.lineageos.spenactions.settings;

import android.content.Context;
import android.content.SharedPreferences.Editor;
import android.provider.Settings;

import androidx.preference.PreferenceManager;

public class SettingsUtils {

    public static final String SPEN_BLUETOOTH_ENABLE = "spen_bluetooth_enable";
    public static final String SPEN_MODE = "spen_mode";
    public static final String ACTION_BUTTONS = "action_buttons";

    public static boolean isEnabled(Context context, String pref, boolean defValue) {
        return PreferenceManager.getDefaultSharedPreferences(context)
                .getBoolean(pref, defValue);
    }

    public static String getSwitchPreference(Context context, String pref, String defValue) {
        return PreferenceManager.getDefaultSharedPreferences(context)
                .getString(pref, defValue);
    }
}
