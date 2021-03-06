/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
 *
 * Author: Andrei Vasiliu <andrei.vasiliu@intel.com>
 * Copyright (c) 2016 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
package org.tinyb;

import java.util.List;
import java.util.Map;

/**
  * Provides access to Bluetooth adapters. Follows the BlueZ adapter API
  * available at: http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/device-api.txt
  */
public interface BluetoothDevice extends BluetoothObject
{
    @Override
    public BluetoothDevice clone();

    /** Find a BluetoothGattService. If parameter UUID is not null,
      * the returned object will have to match it.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter UUID optionally specify the UUID of the BluetoothGattService you are
      * waiting for
      * @parameter timeoutMS the function will return after timeout time in milliseconds, a
      * value of zero means wait forever. If object is not found during this time null will be returned.
      * @return An object matching the UUID or null if not found before
      * timeout expires or event is canceled.
      */
    BluetoothGattService find(String UUID, long timeoutMS);

    /** Find a BluetoothGattService. If parameter UUID is not null,
      * the returned object will have to match it.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter UUID optionally specify the UUID of the BluetoothGattService you are
      * waiting for
      * @return An object matching the UUID or null if not found before
      * timeout expires or event is canceled.
      */
    BluetoothGattService find(String UUID);

    /* Bluetooth method calls: */

    /**
     * <b>direct_bt.tinyb</b>: Disconnect the LE or BREDR peer's GATT and HCI connection.
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.6 Disconnect command
     * </p>
     * <p>
     * The actual disconnect event will be delivered asynchronous and
     * the connection event can be caught via AdapterStatusListener::deviceDisconnected(..).
     * If unacceptable, {@link HCIStatusCode#UNACCEPTABLE_CONNECTION_PARAM} is being returned.
     * </p>
     * <p>
     * The device will be removed from the managing adapter's connected devices
     * when {@link AdapterStatusListener#deviceDisconnected(BluetoothDevice, HCIStatusCode, long)} has been received.
     * </p>
     * <p>
     * Any open GATT connection will be closed as well.
     * </p>
     * <p>
     * <b>tinyb.dbus</b> The connection to this device is removed, removing all connected
     * profiles.
     * </p>
     * @return {@link HCIStatusCode#SUCCESS} if the command has been accepted, otherwise {@link HCIStatusCode} may disclose reason for rejection.
     * @since 2.1.0 change API, i.e. return value from boolean to HCIStatusCode in favor of <i>direct_bt</i>
     */
    HCIStatusCode disconnect() throws BluetoothException;

    /**
     * <b>direct_bt.tinyb</b>: Establish a default HCI connection to this device, using certain default parameter.
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.12 LE Create Connection command <br>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.5 Create Connection command
     * </p>
     * <p>
     * Depending on this device's addressType,
     * either a BREDR (BDADDR_BREDR) or LE (BDADDR_LE_PUBLIC, BDADDR_LE_RANDOM) connection is attempted.<br>
     * If unacceptable, {@link HCIStatusCode#UNACCEPTABLE_CONNECTION_PARAM} is being returned.
     * </p>
     * <p>
     * The actual new connection handle will be delivered asynchronous and
     * the connection event can be caught via {@link AdapterStatusListener#deviceConnected(BluetoothDevice, long)},
     * or if failed via {@link AdapterStatusListener#deviceDisconnected(BluetoothDevice, HCIStatusCode, long)}.
     * </p>
     * <p>
     * The device is tracked by the managing adapter.
     * </p>
     * <p>
     * <b>tinyb.dbus</b>: A connection to this device is established, connecting each profile
     * flagged as auto-connectable.
     * </p>
     * @return {@link HCIStatusCode#SUCCESS} if the command has been accepted, otherwise {@link HCIStatusCode} may disclose reason for rejection.
     * @see #connect(short, short, short, short, short, short)
     * @since 2.1.0 change API, i.e. return value from boolean to HCIStatusCode in favor of <i>direct_bt</i>
     */
    HCIStatusCode connect() throws BluetoothException;

    /**
     * Establish a HCI BDADDR_LE_PUBLIC or BDADDR_LE_RANDOM connection to this device.
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.12 LE Create Connection command
     * </p>
     * <p>
     * If this device's {@link #getAddressType()} is not BDADDR_LE_PUBLIC or BDADDR_LE_RANDOM,
     * {@link HCIStatusCode#UNACCEPTABLE_CONNECTION_PARAM} is being returned.
     * </p>
     * <p>
     * The actual new connection handle will be delivered asynchronous and
     * the connection event can be caught via {@link AdapterStatusListener#deviceConnected(BluetoothDevice, long)},
     * or if failed via {@link AdapterStatusListener#deviceDisconnected(BluetoothDevice, HCIStatusCode, long)}.
     * </p>
     * <p>
     * The device is tracked by the managing adapter.
     * </p>
     * <p>
     * Default parameter values are chosen for using public address resolution
     * and usual connection latency, interval etc.
     * </p>
     * <p>
     * Set window to the same value as the interval, enables continuous scanning.
     * </p>
     *
     * @param le_scan_interval in units of 0.625ms, default value 48 for 30ms, min value 4 for 2.5ms -> 0x4000 for 10.24s
     * @param le_scan_window in units of 0.625ms, default value 48 for 30ms,  min value 4 for 2.5ms -> 0x4000 for 10.24s. Shall be <= le_scan_interval
     * @param conn_interval_min in units of 1.25ms, default value 15 for 19.75ms
     * @param conn_interval_max in units of 1.25ms, default value 15 for 19.75ms
     * @param conn_latency slave latency in units of connection events, default value 0
     * @param supervision_timeout in units of 10ms, default value 1000 for 10000ms or 10s.
     * @return {@link HCIStatusCode#SUCCESS} if the command has been accepted, otherwise {@link HCIStatusCode} may disclose reason for rejection.
     *
     * @see #connect()
     * @since 2.1.0 change API, i.e. return value from boolean to HCIStatusCode in favor of <i>direct_bt</i>
     * @implNote not implemented in <b>tinyb.dbus</b>
     */
    HCIStatusCode connect(final short le_scan_interval, final short le_scan_window,
                          final short conn_interval_min, final short conn_interval_max,
                          final short conn_latency, final short timeout);


    /** Connects a specific profile available on the device, given by UUID
      * @param arg_UUID The UUID of the profile to be connected
      * @return TRUE if the profile connected successfully
      */
    boolean connectProfile(String arg_UUID) throws BluetoothException;

    /** Disconnects a specific profile available on the device, given by UUID
      * @param arg_UUID The UUID of the profile to be disconnected
      * @return TRUE if the profile disconnected successfully
      */
    boolean disconnectProfile(String arg_UUID) throws BluetoothException;

    /** A connection to this device is established, and the device is then
      * paired.
      * @return TRUE if the device connected and paired
      */
    boolean pair() throws BluetoothException;

    /** Remove this device from the system (like an unpair).
      * @return TRUE if the device has been removed
      * @throws BluetoothException
      */
    boolean remove() throws BluetoothException;

    /** Cancels an initiated pairing operation
      * @return TRUE if the paring is cancelled successfully
      */
    boolean cancelPairing() throws BluetoothException;

    /** Returns a list of BluetoothGattServices available on this device.
      * @return A list of BluetoothGattServices available on this device,
      * NULL if an error occurred
      */
    List<BluetoothGattService> getServices();

    /**
     * Issues a GATT ping to the device, validating whether it is still reachable.
     * <p>
     * This method could be periodically utilized to shorten the underlying OS disconnect period
     * after turning the device off, which lies within 7-13s.
     * </p>
     * <p>
     * In case the device is no more reachable, disconnect will be initiated due to the occurring IO error.
     * </p>
     * <p>
     * GATT services must have been initialized via {@link #getServices()}, otherwise {@code false} is being returned.
     * </p>
     * @return {@code true} if successful or not implemented, otherwise false in case no GATT services exists or is not connected..
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus.
     */
    boolean pingGATT();

    /**
     * Returns the timestamp in monotonic milliseconds when this device instance has been created,
     * either via its initial discovery or its initial direct connection.
     *
     * @see BluetoothUtils#getCurrentMilliseconds()
     * @since 2.0.0
     */
    long getCreationTimestamp();

    /**
     * Returns the timestamp in monotonic milliseconds when this device instance has
     * discovered or connected directly the last time.
     *
     * @see BluetoothUtils#getCurrentMilliseconds()
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus, returns {@link #getCreationTimestamp()}
     */
    long getLastDiscoveryTimestamp();

    /**
     * Returns the timestamp in monotonic milliseconds when this device instance underlying data
     * has been updated the last time.
     *
     * @see BluetoothUtils#getCurrentMilliseconds()
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus, returns {@link #getCreationTimestamp()}
     */
    long getLastUpdateTimestamp();

    /** Returns the hardware address of this device.
      * @return The hardware address of this device.
      */
    String getAddress();

    /**
     * Returns the {@link BluetoothAddressType},
     * determining whether the device is {@link BluetoothAddressType#BDADDR_BREDR}
     * or an LE device, {@link BluetoothAddressType#BDADDR_LE_PUBLIC} or {@link BluetoothAddressType#BDADDR_LE_RANDOM}.
     * @since 2.0.0
     * @implNote not implemented in <b>tinyb.dbus</b>, returns {@link BluetoothAddressType#BDADDR_LE_PUBLIC}
     */
    BluetoothAddressType getAddressType();

    /**
     * Returns the {@link BLERandomAddressType}.
     * <p>
     * If {@link #getAddressType()} is {@link BluetoothAddressType#BDADDR_LE_RANDOM},
     * method shall return a valid value other than {@link BLERandomAddressType#UNDEFINED}.
     * </p>
     * <p>
     * If {@link #getAddressType()} is not {@link BluetoothAddressType#BDADDR_LE_RANDOM},
     * method shall return {@link BLERandomAddressType#UNDEFINED}.
     * </p>
     * @since 2.0.0
     * @implNote not implemented in <b>tinyb.dbus</b>, returns {@link BLERandomAddressType#UNDEFINED}
     */
    BLERandomAddressType getBLERandomAddressType();

    /** Returns the remote friendly name of this device.
      * @return The remote friendly name of this device, or NULL if not set.
      */
    String getName();

    /** Returns an alternative friendly name of this device.
      * @return The alternative friendly name of this device, or NULL if not set.
      */
    String getAlias();

    /** Sets an alternative friendly name of this device.
      */
    void setAlias(String value);

    /** Returns the Bluetooth class of the device.
      * @return The Bluetooth class of the device.
      */
    int getBluetoothClass();

    /** Returns the appearance of the device, as found by GAP service.
      * @return The appearance of the device, as found by GAP service.
      */
    short getAppearance();

    /** Returns the proposed icon name of the device.
      * @return The proposed icon name, or NULL if not set.
      */
    String getIcon();

    /** Returns the paired state the device.
      * @return The paired state of the device.
      */
    boolean getPaired();

    /**
     * Enables notifications for the paired property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the paired
     * property.
     */
    void enablePairedNotifications(BluetoothNotification<Boolean> callback);

    /**
     * Disables notifications of the paired property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disablePairedNotifications();

    /** Returns the trusted state the device.
      * @return The trusted state of the device.
      */
    boolean getTrusted();

    /**
     * Enables notifications for the trusted property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the trusted
     * property.
     */
    void enableTrustedNotifications(BluetoothNotification<Boolean> callback);

    /**
     * Disables notifications of the trusted property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableTrustedNotifications();

    /** Sets the trusted state the device.
      */
    void setTrusted(boolean value);

    /** Returns the blocked state the device.
      * @return The blocked state of the device.
      */
    boolean getBlocked();

    /**
     * Enables notifications for the blocked property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the blocked
     * property.
     */
    void enableBlockedNotifications(BluetoothNotification<Boolean> callback);

    /**
     * Disables notifications of the blocked property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableBlockedNotifications();

    /** Sets the blocked state the device.
      */
    void setBlocked(boolean value);

    /** Returns if device uses only pre-Bluetooth 2.1 pairing mechanism.
      * @return True if device uses only pre-Bluetooth 2.1 pairing mechanism.
      */
    boolean getLegacyPairing();

    /** Returns the Received Signal Strength Indicator of the device.
      * @return The Received Signal Strength Indicator of the device.
      */
    short getRSSI();

    /**
     * Enables notifications for the RSSI property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Short> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the RSSI
     * property.
     */
    void enableRSSINotifications(BluetoothNotification<Short> callback);

    /**
     * Disables notifications of the RSSI property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableRSSINotifications();

    /** Returns the connected state of the device.
      * @return The connected state of the device.
      */
    boolean getConnected();

    /**
     * Return the HCI connection handle to the LE or BREDR peer, zero if not connected.
     * @since 2.1.0
     * @implNote not implemented in <b>tinyb.dbus</b>
     */
    short getConnectionHandle();

    /**
     * Enables notifications for the connected property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the connected
     * property.
     */
    void enableConnectedNotifications(BluetoothNotification<Boolean> callback);

    /**
     * Disables notifications of the connected property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableConnectedNotifications();

    /** Returns the UUIDs of the device.
      * @return Array containing the UUIDs of the device, ends with NULL.
      */
    String[] getUUIDs();

    /** Returns the local ID of the adapter.
      * @return The local ID of the adapter.
      */
    String getModalias();

    /** Returns the adapter on which this device was discovered or
      * connected.
      * @return The adapter.
      */
    BluetoothAdapter getAdapter();

    /** Returns a map containing manufacturer specific advertisement data.
      * An entry has a uint16_t key and an array of bytes.
      * @return manufacturer specific advertisement data.
      */
    Map<Short, byte[]> getManufacturerData();

    /**
     * Enables notifications for the manufacturer data property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Map<Short, byte[]> > object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the manufacturer data
     * property.
     */
    void enableManufacturerDataNotifications(
            BluetoothNotification<Map<Short, byte[]>> callback);

    /**
     * Disables notifications of the manufacturer data property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableManufacturerDataNotifications();

    /** Returns a map containing service advertisement data.
      * An entry has a UUID string key and an array of bytes.
      * @return service advertisement data.
      */
    Map<String, byte[]> getServiceData();

    /**
     * Enables notifications for the service data property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Map<String, byte[]> > object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the service data
     * property.
     */
    void enableServiceDataNotifications(
            BluetoothNotification<Map<String, byte[]>> callback);

    /**
     * Disables notifications of the service data property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableServiceDataNotifications();

    /** Returns the transmission power level (0 means unknown).
      * @return the transmission power level (0 means unknown).
      */
    short getTxPower();

    /** Returns true if service discovery has ended.
      * @return true if the service discovery has ended.
      */
    boolean getServicesResolved();

    /**
     * Enables notifications for the services resolved property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the services resolved
     * property.
     */
    void enableServicesResolvedNotifications(
            BluetoothNotification<Boolean> callback);

    /**
     * Disables notifications of the services resolved property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableServicesResolvedNotifications();

    /**
     * Add the given {@link GATTCharacteristicListener} to the listener list if not already present.
     * <p>
     * To enable the actual BLE notification and/or indication, one needs to call
     * {@link BluetoothGattCharacteristic#configNotificationIndication(boolean, boolean, boolean[])}
     * or {@link BluetoothGattCharacteristic#enableNotificationOrIndication(boolean[])}.
     * </p>
     * @param listener A {@link GATTCharacteristicListener} instance, listening to all {@link BluetoothGattCharacteristic} events of this device
     * @return true if the given listener is not element of the list and has been newly added, otherwise false.
     * @throws IllegalStateException if the {@link BluetoothDevice}'s GATTHandler is null, i.e. not connected
     * @throws IllegalStateException if the given {@link GATTCharacteristicListener} is already in use, i.e. added.
     * @see BluetoothGattCharacteristic#configNotificationIndication(boolean, boolean, boolean[])
     * @see BluetoothGattCharacteristic#enableNotificationOrIndication(boolean[])
     * @since 2.0.0
     * @implNote not implemented in <b>tinyb.dbus</b>
     */
    public boolean addCharacteristicListener(final GATTCharacteristicListener listener)
        throws IllegalStateException;

    /**
     * Remove the given {@link GATTCharacteristicListener} from the listener list.
     * <p>
     * If the {@link BluetoothDevice}'s GATTHandler is null, i.e. not connected, {@code false} is being returned.
     * </p>
     * @param listener A {@link GATTCharacteristicListener} instance
     * @return true if the given listener is an element of the list and has been removed, otherwise false.
     * @since 2.0.0
     * @implNote not implemented in <b>tinyb.dbus</b>
     */
    public boolean removeCharacteristicListener(final GATTCharacteristicListener l);

    /**
     * Remove all {@link GATTCharacteristicListener} from the list, which are associated to the given {@link BluetoothGattCharacteristic}.
     * <p>
     * Implementation tests all listener's {@link GATTCharacteristicListener#getAssociatedCharacteristic()}
     * to match with the given associated characteristic.
     * </p>
     * @param associatedCharacteristic the match criteria to remove any GATTCharacteristicListener from the list
     * @return number of removed listener.
     * @since 2.0.0
     * @implNote not implemented in <b>tinyb.dbus</b>
     */
    public int removeAllAssociatedCharacteristicListener(final BluetoothGattCharacteristic associatedCharacteristic);

    /**
     * Remove all {@link GATTCharacteristicListener} from the list.
     * @return number of removed listener.
     * @since 2.0.0
     * @implNote not implemented in <b>tinyb.dbus</b>
     */
    public int removeAllCharacteristicListener();
}
