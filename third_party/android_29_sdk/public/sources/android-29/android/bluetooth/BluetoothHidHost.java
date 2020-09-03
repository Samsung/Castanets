/*
 * Copyright (C) 2011 The Android Open Source Project
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

package android.bluetooth;

import android.annotation.SdkConstant;
import android.annotation.SdkConstant.SdkConstantType;
import android.content.Context;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;


/**
 * This class provides the public APIs to control the Bluetooth Input
 * Device Profile.
 *
 * <p>BluetoothHidHost is a proxy object for controlling the Bluetooth
 * Service via IPC. Use {@link BluetoothAdapter#getProfileProxy} to get
 * the BluetoothHidHost proxy object.
 *
 * <p>Each method is protected with its appropriate permission.
 *
 * @hide
 */
public final class BluetoothHidHost implements BluetoothProfile {
    private static final String TAG = "BluetoothHidHost";
    private static final boolean DBG = true;
    private static final boolean VDBG = false;

    /**
     * Intent used to broadcast the change in connection state of the Input
     * Device profile.
     *
     * <p>This intent will have 3 extras:
     * <ul>
     * <li> {@link #EXTRA_STATE} - The current state of the profile. </li>
     * <li> {@link #EXTRA_PREVIOUS_STATE}- The previous state of the profile.</li>
     * <li> {@link BluetoothDevice#EXTRA_DEVICE} - The remote device. </li>
     * </ul>
     *
     * <p>{@link #EXTRA_STATE} or {@link #EXTRA_PREVIOUS_STATE} can be any of
     * {@link #STATE_DISCONNECTED}, {@link #STATE_CONNECTING},
     * {@link #STATE_CONNECTED}, {@link #STATE_DISCONNECTING}.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH} permission to
     * receive.
     */
    @SdkConstant(SdkConstantType.BROADCAST_INTENT_ACTION)
    public static final String ACTION_CONNECTION_STATE_CHANGED =
            "android.bluetooth.input.profile.action.CONNECTION_STATE_CHANGED";

    /**
     * @hide
     */
    @SdkConstant(SdkConstantType.BROADCAST_INTENT_ACTION)
    public static final String ACTION_PROTOCOL_MODE_CHANGED =
            "android.bluetooth.input.profile.action.PROTOCOL_MODE_CHANGED";

    /**
     * @hide
     */
    @SdkConstant(SdkConstantType.BROADCAST_INTENT_ACTION)
    public static final String ACTION_HANDSHAKE =
            "android.bluetooth.input.profile.action.HANDSHAKE";

    /**
     * @hide
     */
    @SdkConstant(SdkConstantType.BROADCAST_INTENT_ACTION)
    public static final String ACTION_REPORT =
            "android.bluetooth.input.profile.action.REPORT";

    /**
     * @hide
     */
    @SdkConstant(SdkConstantType.BROADCAST_INTENT_ACTION)
    public static final String ACTION_VIRTUAL_UNPLUG_STATUS =
            "android.bluetooth.input.profile.action.VIRTUAL_UNPLUG_STATUS";

    /**
     * @hide
     */
    @SdkConstant(SdkConstantType.BROADCAST_INTENT_ACTION)
    public static final String ACTION_IDLE_TIME_CHANGED =
            "android.bluetooth.input.profile.action.IDLE_TIME_CHANGED";

    /**
     * Return codes for the connect and disconnect Bluez / Dbus calls.
     *
     * @hide
     */
    public static final int INPUT_DISCONNECT_FAILED_NOT_CONNECTED = 5000;

    /**
     * @hide
     */
    public static final int INPUT_CONNECT_FAILED_ALREADY_CONNECTED = 5001;

    /**
     * @hide
     */
    public static final int INPUT_CONNECT_FAILED_ATTEMPT_FAILED = 5002;

    /**
     * @hide
     */
    public static final int INPUT_OPERATION_GENERIC_FAILURE = 5003;

    /**
     * @hide
     */
    public static final int INPUT_OPERATION_SUCCESS = 5004;

    /**
     * @hide
     */
    public static final int PROTOCOL_REPORT_MODE = 0;

    /**
     * @hide
     */
    public static final int PROTOCOL_BOOT_MODE = 1;

    /**
     * @hide
     */
    public static final int PROTOCOL_UNSUPPORTED_MODE = 255;

    /*  int reportType, int reportType, int bufferSize */
    /**
     * @hide
     */
    public static final byte REPORT_TYPE_INPUT = 1;

    /**
     * @hide
     */
    public static final byte REPORT_TYPE_OUTPUT = 2;

    /**
     * @hide
     */
    public static final byte REPORT_TYPE_FEATURE = 3;

    /**
     * @hide
     */
    public static final int VIRTUAL_UNPLUG_STATUS_SUCCESS = 0;

    /**
     * @hide
     */
    public static final int VIRTUAL_UNPLUG_STATUS_FAIL = 1;

    /**
     * @hide
     */
    public static final String EXTRA_PROTOCOL_MODE =
            "android.bluetooth.BluetoothHidHost.extra.PROTOCOL_MODE";

    /**
     * @hide
     */
    public static final String EXTRA_REPORT_TYPE =
            "android.bluetooth.BluetoothHidHost.extra.REPORT_TYPE";

    /**
     * @hide
     */
    public static final String EXTRA_REPORT_ID =
            "android.bluetooth.BluetoothHidHost.extra.REPORT_ID";

    /**
     * @hide
     */
    public static final String EXTRA_REPORT_BUFFER_SIZE =
            "android.bluetooth.BluetoothHidHost.extra.REPORT_BUFFER_SIZE";

    /**
     * @hide
     */
    public static final String EXTRA_REPORT = "android.bluetooth.BluetoothHidHost.extra.REPORT";

    /**
     * @hide
     */
    public static final String EXTRA_STATUS = "android.bluetooth.BluetoothHidHost.extra.STATUS";

    /**
     * @hide
     */
    public static final String EXTRA_VIRTUAL_UNPLUG_STATUS =
            "android.bluetooth.BluetoothHidHost.extra.VIRTUAL_UNPLUG_STATUS";

    /**
     * @hide
     */
    public static final String EXTRA_IDLE_TIME =
            "android.bluetooth.BluetoothHidHost.extra.IDLE_TIME";

    private BluetoothAdapter mAdapter;
    private final BluetoothProfileConnector<IBluetoothHidHost> mProfileConnector =
            new BluetoothProfileConnector(this, BluetoothProfile.HID_HOST,
                    "BluetoothHidHost", IBluetoothHidHost.class.getName()) {
                @Override
                public IBluetoothHidHost getServiceInterface(IBinder service) {
                    return IBluetoothHidHost.Stub.asInterface(Binder.allowBlocking(service));
                }
    };

    /**
     * Create a BluetoothHidHost proxy object for interacting with the local
     * Bluetooth Service which handles the InputDevice profile
     */
    /*package*/ BluetoothHidHost(Context context, ServiceListener listener) {
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        mProfileConnector.connect(context, listener);
    }

    /*package*/ void close() {
        if (VDBG) log("close()");
        mProfileConnector.disconnect();
    }

    private IBluetoothHidHost getService() {
        return mProfileConnector.getService();
    }

    /**
     * Initiate connection to a profile of the remote bluetooth device.
     *
     * <p> The system supports connection to multiple input devices.
     *
     * <p> This API returns false in scenarios like the profile on the
     * device is already connected or Bluetooth is not turned on.
     * When this API returns true, it is guaranteed that
     * connection state intent for the profile will be broadcasted with
     * the state. Users can get the connection state of the profile
     * from this intent.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN}
     * permission.
     *
     * @param device Remote Bluetooth Device
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean connect(BluetoothDevice device) {
        if (DBG) log("connect(" + device + ")");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.connect(device);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    /**
     * Initiate disconnection from a profile
     *
     * <p> This API will return false in scenarios like the profile on the
     * Bluetooth device is not in connected state etc. When this API returns,
     * true, it is guaranteed that the connection state change
     * intent will be broadcasted with the state. Users can get the
     * disconnection state of the profile from this intent.
     *
     * <p> If the disconnection is initiated by a remote device, the state
     * will transition from {@link #STATE_CONNECTED} to
     * {@link #STATE_DISCONNECTED}. If the disconnect is initiated by the
     * host (local) device the state will transition from
     * {@link #STATE_CONNECTED} to state {@link #STATE_DISCONNECTING} to
     * state {@link #STATE_DISCONNECTED}. The transition to
     * {@link #STATE_DISCONNECTING} can be used to distinguish between the
     * two scenarios.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN}
     * permission.
     *
     * @param device Remote Bluetooth Device
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean disconnect(BluetoothDevice device) {
        if (DBG) log("disconnect(" + device + ")");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.disconnect(device);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public List<BluetoothDevice> getConnectedDevices() {
        if (VDBG) log("getConnectedDevices()");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled()) {
            try {
                return service.getConnectedDevices();
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return new ArrayList<BluetoothDevice>();
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return new ArrayList<BluetoothDevice>();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        if (VDBG) log("getDevicesMatchingStates()");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled()) {
            try {
                return service.getDevicesMatchingConnectionStates(states);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return new ArrayList<BluetoothDevice>();
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return new ArrayList<BluetoothDevice>();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public int getConnectionState(BluetoothDevice device) {
        if (VDBG) log("getState(" + device + ")");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.getConnectionState(device);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return BluetoothProfile.STATE_DISCONNECTED;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return BluetoothProfile.STATE_DISCONNECTED;
    }

    /**
     * Set priority of the profile
     *
     * <p> The device should already be paired.
     * Priority can be one of {@link #PRIORITY_ON} or
     * {@link #PRIORITY_OFF},
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN}
     * permission.
     *
     * @param device Paired bluetooth device
     * @param priority
     * @return true if priority is set, false on error
     * @hide
     */
    public boolean setPriority(BluetoothDevice device, int priority) {
        if (DBG) log("setPriority(" + device + ", " + priority + ")");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            if (priority != BluetoothProfile.PRIORITY_OFF
                    && priority != BluetoothProfile.PRIORITY_ON) {
                return false;
            }
            try {
                return service.setPriority(device, priority);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    /**
     * Get the priority of the profile.
     *
     * <p> The priority can be any of:
     * {@link #PRIORITY_AUTO_CONNECT}, {@link #PRIORITY_OFF},
     * {@link #PRIORITY_ON}, {@link #PRIORITY_UNDEFINED}
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH} permission.
     *
     * @param device Bluetooth device
     * @return priority of the device
     * @hide
     */
    public int getPriority(BluetoothDevice device) {
        if (VDBG) log("getPriority(" + device + ")");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.getPriority(device);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return BluetoothProfile.PRIORITY_OFF;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return BluetoothProfile.PRIORITY_OFF;
    }

    private boolean isEnabled() {
        return mAdapter.getState() == BluetoothAdapter.STATE_ON;
    }

    private static boolean isValidDevice(BluetoothDevice device) {
        return device != null && BluetoothAdapter.checkBluetoothAddress(device.getAddress());
    }

    /**
     * Initiate virtual unplug for a HID input device.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN} permission.
     *
     * @param device Remote Bluetooth Device
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean virtualUnplug(BluetoothDevice device) {
        if (DBG) log("virtualUnplug(" + device + ")");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.virtualUnplug(device);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }

        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;

    }

    /**
     * Send Get_Protocol_Mode command to the connected HID input device.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN} permission.
     *
     * @param device Remote Bluetooth Device
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean getProtocolMode(BluetoothDevice device) {
        if (VDBG) log("getProtocolMode(" + device + ")");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.getProtocolMode(device);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    /**
     * Send Set_Protocol_Mode command to the connected HID input device.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN} permission.
     *
     * @param device Remote Bluetooth Device
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean setProtocolMode(BluetoothDevice device, int protocolMode) {
        if (DBG) log("setProtocolMode(" + device + ")");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.setProtocolMode(device, protocolMode);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    /**
     * Send Get_Report command to the connected HID input device.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN} permission.
     *
     * @param device Remote Bluetooth Device
     * @param reportType Report type
     * @param reportId Report ID
     * @param bufferSize Report receiving buffer size
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean getReport(BluetoothDevice device, byte reportType, byte reportId,
            int bufferSize) {
        if (VDBG) {
            log("getReport(" + device + "), reportType=" + reportType + " reportId=" + reportId
                    + "bufferSize=" + bufferSize);
        }
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.getReport(device, reportType, reportId, bufferSize);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    /**
     * Send Set_Report command to the connected HID input device.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN} permission.
     *
     * @param device Remote Bluetooth Device
     * @param reportType Report type
     * @param report Report receiving buffer size
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean setReport(BluetoothDevice device, byte reportType, String report) {
        if (VDBG) log("setReport(" + device + "), reportType=" + reportType + " report=" + report);
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.setReport(device, reportType, report);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    /**
     * Send Send_Data command to the connected HID input device.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN} permission.
     *
     * @param device Remote Bluetooth Device
     * @param report Report to send
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean sendData(BluetoothDevice device, String report) {
        if (DBG) log("sendData(" + device + "), report=" + report);
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.sendData(device, report);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    /**
     * Send Get_Idle_Time command to the connected HID input device.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN} permission.
     *
     * @param device Remote Bluetooth Device
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean getIdleTime(BluetoothDevice device) {
        if (DBG) log("getIdletime(" + device + ")");
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.getIdleTime(device);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    /**
     * Send Set_Idle_Time command to the connected HID input device.
     *
     * <p>Requires {@link android.Manifest.permission#BLUETOOTH_ADMIN} permission.
     *
     * @param device Remote Bluetooth Device
     * @param idleTime Idle time to be set on HID Device
     * @return false on immediate error, true otherwise
     * @hide
     */
    public boolean setIdleTime(BluetoothDevice device, byte idleTime) {
        if (DBG) log("setIdletime(" + device + "), idleTime=" + idleTime);
        final IBluetoothHidHost service = getService();
        if (service != null && isEnabled() && isValidDevice(device)) {
            try {
                return service.setIdleTime(device, idleTime);
            } catch (RemoteException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }
        if (service == null) Log.w(TAG, "Proxy not attached to service");
        return false;
    }

    private static void log(String msg) {
        Log.d(TAG, msg);
    }
}
