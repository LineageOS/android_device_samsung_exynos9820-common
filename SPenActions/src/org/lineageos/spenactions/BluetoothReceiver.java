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

import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.RemoteException;
import android.util.Log;

import org.lineageos.spenactions.settings.SettingsUtils;

public class BluetoothReceiver extends BroadcastReceiver {

    private final Context mContext;
    private final SPenConnectionManager mConnectionManager;

    public BluetoothReceiver(Context context) {
        this.mContext = context;
        this.mConnectionManager = new SPenConnectionManager(context);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        try {
            int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);

            if (state == BluetoothAdapter.STATE_ON) {
                if (SettingsUtils.isEnabled(mContext, SettingsUtils.SPEN_BLUETOOTH_ENABLE, false)) {
                    mConnectionManager.disconnect();
                    mConnectionManager.connect();
                }
            } else if (state == BluetoothAdapter.STATE_OFF) {
                mConnectionManager.disconnect();
            }
        } catch (RemoteException ex) {
            ex.printStackTrace();
        }
    }

}
