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

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.IBinder;
import android.os.UserHandle;
import android.util.Log;

public class SPenActionsService extends Service {

    private static final String LOG_TAG = "SPenActions/Service";

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(LOG_TAG, "Service started");

        IntentFilter bluetoothFilter = new IntentFilter();
        bluetoothFilter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
        registerReceiver(new BluetoothReceiver(this), bluetoothFilter);

        // Just started - manually send current bluetooth state
        Intent bluetoothState = new Intent(BluetoothAdapter.ACTION_STATE_CHANGED);
        bluetoothState.setPackage(getPackageName());
        bluetoothState.putExtra(BluetoothAdapter.EXTRA_STATE,
                ((BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE)).getAdapter().getState());
        sendBroadcastAsUser(bluetoothState, UserHandle.CURRENT);

        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        Log.i(LOG_TAG, "Service stopping");

        // Force stop by faking bluetooth turn off
        Intent bluetoothState = new Intent(BluetoothAdapter.ACTION_STATE_CHANGED);
        bluetoothState.setPackage(getPackageName());
        bluetoothState.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_OFF);
        sendBroadcastAsUser(bluetoothState, UserHandle.CURRENT);
    }
}
