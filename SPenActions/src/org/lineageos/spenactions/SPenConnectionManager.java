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
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;

import vendor.samsung.hardware.spen.ISPen;

public class SPenConnectionManager extends BroadcastReceiver {

    private static final String LOG_TAG = "SPenActions/SPenConnectionManager";

    private final Context mContext;
    private ISPen mSPenHAL;
    private BluetoothAdapter mAdapter;
    private BluetoothDevice mSpen;
    private BluetoothGatt mGatt;
    private String mCurBleSpenAddr;

    public SPenConnectionManager(Context context) {
        this.mContext = context;
        this.mSPenHAL = ISPen.Stub.asInterface(ServiceManager.waitForDeclaredService("vendor.samsung.hardware.spen.ISPen/default"));

        context.registerReceiver(this, new IntentFilter(BluetoothDevice.ACTION_PAIRING_REQUEST));
    }

    public void connect() throws RemoteException {
        String blespenAddr = mSPenHAL.getMACAddress();
        Log.i(LOG_TAG, "Connecting! SPen BLE address: " + blespenAddr);

        mCurBleSpenAddr = blespenAddr;
        BluetoothManager bluetoothManager = (BluetoothManager) mContext.getSystemService(Context.BLUETOOTH_SERVICE);
        mAdapter = bluetoothManager.getAdapter();

        if (!mAdapter.isEnabled())
            mAdapter.enable();

        mSpen = mAdapter.getRemoteDevice(blespenAddr);

        if (!mSPenHAL.isCharging())
            mSPenHAL.setCharging(true);

        if (mGatt == null || bluetoothManager.getConnectionState(mSpen, BluetoothProfile.GATT_SERVER)
                == BluetoothProfile.STATE_DISCONNECTED)
            mGatt = mSpen.connectGatt(mContext, false, new SPenGattCallback(this, mContext), BluetoothDevice.TRANSPORT_LE);
    }

    public void disconnect() throws RemoteException {
        if (mGatt != null) {
            mGatt.disconnect();
            mGatt.close();
        }
        if (mAdapter != null && mSpen != null) mAdapter.disconnectAllEnabledProfiles(mSpen);

        mSPenHAL.setCharging(false);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);

        if (mSpen != null && mSpen.getAddress().equals(device.getAddress())) {
            device.setPairingConfirmation(true);
            abortBroadcast(); // Prevent Settings from receiving the broadcast message
        }
    }

}
