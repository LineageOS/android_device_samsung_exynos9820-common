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

public class MotionEvent {

    public enum Action {
        MOVE;
    }

    private final Action mAction;
    private final short mDx;
    private final short mDy;

    private MotionEvent(Action action, short dx, short dy) {
        this.mAction = action;
        this.mDx = dx;
        this.mDy = dy;
    }

    public Action getAction() {
        return mAction;
    }

    public short getDX() {
        return mDx;
    }

    public short getDY() {
        return mDy;
    }

    private static short extractShortValue(byte[] bArr, int i) {
        return (short) ((bArr[i] & 255) | ((bArr[i + 1] & 255) << 8));
    }

    public static MotionEvent fromTypeData(int type, byte[] value) {
        switch (type) {
            case 15:
                return new MotionEvent(Action.MOVE, extractShortValue(value, 1), extractShortValue(value, 3));
            default:
                return null;
        }
    }
}
