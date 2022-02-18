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
package org.lineageos.spenactions;

import android.view.KeyEvent;

public enum SPenMode {
    NAVIGATION, CAMERA, MEDIA;

    public int getButtonKey() {
        switch (this) {
            case NAVIGATION:
                return KeyEvent.KEYCODE_ENTER;
            case CAMERA:
                return KeyEvent.KEYCODE_CAMERA;
            case MEDIA:
                return KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE;
            default:
                return KeyEvent.KEYCODE_UNKNOWN;
        }
    }

    public int getGestureKey(SPenDirection direction) {
        switch (this) {
            case NAVIGATION:
                switch (direction) {
                    case POSITIVE_X:
                        return KeyEvent.KEYCODE_DPAD_RIGHT;
                    case NEGATIVE_X:
                        return KeyEvent.KEYCODE_DPAD_LEFT;
                    case NEGATIVE_Y:
                        return KeyEvent.KEYCODE_DPAD_UP;
                    case POSITIVE_Y:
                        return KeyEvent.KEYCODE_DPAD_DOWN;
                    default:
                        return KeyEvent.KEYCODE_UNKNOWN;
                }
            case MEDIA:
                switch (direction) {
                    case POSITIVE_X:
                        return KeyEvent.KEYCODE_MEDIA_NEXT;
                    case NEGATIVE_X:
                        return KeyEvent.KEYCODE_MEDIA_PREVIOUS;
                    case NEGATIVE_Y:
                        return KeyEvent.KEYCODE_VOLUME_UP;
                    case POSITIVE_Y:
                        return KeyEvent.KEYCODE_VOLUME_DOWN;
                    default:
                        return KeyEvent.KEYCODE_UNKNOWN;
                }
            default:
                return KeyEvent.KEYCODE_UNKNOWN;
        }
    }

    public static SPenMode valueOfPref(String prefValue) {
        return SPenMode.values()[Integer.parseInt(prefValue)];
    }
}
