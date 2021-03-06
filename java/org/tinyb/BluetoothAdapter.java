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
import java.util.UUID;

/**
  * Provides access to Bluetooth adapters. Follows the BlueZ adapter API
  * available at: http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/adapter-api.txt
  */
public interface BluetoothAdapter extends BluetoothObject
{
    @Override
    public BluetoothAdapter clone();

    /** Find a BluetoothDevice. If parameters name and address are not null,
      * the returned object will have to match them.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter name optionally specify the name of the object you are
      * waiting for
      * @parameter address optionally specify the MAC address of the device you are
      * waiting for
      * @parameter timeoutMS the function will return after timeout time in milliseconds, a
      * value of zero means wait forever. If object is not found during this time null will be returned.
      * @return An object matching the name and address or null if not found before
      * timeout expires.
      */
    public BluetoothDevice find(String name, String address, long timeoutMS);

    /** Find a BluetoothDevice. If parameters name and address are not null,
      * the returned object will have to match them.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter name optionally specify the name of the object you are
      * waiting for
      * @parameter address optionally specify the MAC address of the device you are
      * waiting for
      * @return An object matching the name and address.
      */
    public BluetoothDevice find(String name, String address);

    /* Bluetooth specific API */

    /**
     * Returns true, if the adapter's device is already whitelisted.
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus
     */
    public boolean isDeviceWhitelisted(final String address);

    /**
     * Add the given device to the adapter's autoconnect whitelist.
     * <p>
     * The given LE connection parameter will be uploaded to the kernel for the given device first,
     * if the device is of type {@link BluetoothAddressType#BDADDR_LE_PUBLIC} or {@link BluetoothAddressType#BDADDR_LE_RANDOM}.
     * </p>
     * <p>
     * Method will reject duplicate devices, in which case it should be removed first.
     * </p>
     *
     * @param address
     * @param address_type
     * @param ctype
     * @param conn_interval_min default value 0x000F
     * @param conn_interval_max default value 0x000F
     * @param conn_latency default value 0x0000
     * @param timeout in units of 10ms, default value 1000 for 10000ms or 10s.
     * @return {@code true} if successful, otherwise {@code false}.
     *
     * @see #addDeviceToWhitelist(String, BluetoothAddressType, HCIWhitelistConnectType)
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus
     */
    public boolean addDeviceToWhitelist(final String address, final BluetoothAddressType address_type,
                                        final HCIWhitelistConnectType ctype,
                                        final short conn_interval_min, final short conn_interval_max,
                                        final short conn_latency, final short timeout);

    /**
     * Add the given device to the adapter's autoconnect whitelist.
     * <p>
     * This variant of {@link #addDeviceToWhitelist(String, BluetoothAddressType, HCIWhitelistConnectType, short, short, short, short)}
     * uses default connection parameter, which will be uploaded to the kernel for the given device first.
     * </p>
     * <p>
     * Method will reject duplicate devices, in which case it should be removed first.
     * </p>
     *
     * @param address
     * @param address_type
     * @param ctype
     * @return {@code true} if successful, otherwise {@code false}.
     *
     * @see #addDeviceToWhitelist(String, BluetoothAddressType, HCIWhitelistConnectType, short, short, short, short)
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus
     */
    public boolean addDeviceToWhitelist(final String address, final BluetoothAddressType address_type,
                                        final HCIWhitelistConnectType ctype);


    /**
     * Remove the given device from the adapter's autoconnect whitelist.
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus
     */
    public boolean removeDeviceFromWhitelist(final String address, final BluetoothAddressType address_type);


    /** Turns on device discovery if it is disabled.
      * @return TRUE if discovery was successfully enabled
      * @deprecated since 2.0.0, use {@link #startDiscovery(boolean)}.
      */
    @Deprecated
    public boolean startDiscovery() throws BluetoothException;

    /**
     * Turns on device discovery if it is disabled.
     * <pre>
     * + --+-------+--------+-----------+----------------------------------------------------+
     * | # | meta  | native | keepAlive | Note
     * +---+-------+--------+-----------+----------------------------------------------------+
     * | 1 | true  | true   | false     | -
     * | 2 | false | false  | false     | -
     * +---+-------+--------+-----------+----------------------------------------------------+
     * | 3 | true  | true   | true      | -
     * | 4 | true  | false  | true      | temporarily disabled -> startDiscoveryBackground()
     * | 5 | false | false  | true      | [4] -> [5] requires manual DISCOVERING event
     * +---+-------+--------+-----------+----------------------------------------------------+
     * </pre>
     *
     * @param keepAlive if {@code true}, indicates that discovery shall be restarted
     *        if stopped by the underlying Bluetooth implementation (BlueZ, ..).
     *        Using {@link #startDiscovery(boolean) startDiscovery}({@code keepAlive=true})
     *        and {@link #stopDiscovery()} is the recommended workflow
     *        for a reliable discovery process.
     * @return TRUE if discovery was successfully enabled
     * @throws BluetoothException
     * @since 2.0.0
     * @implNote {@code keepAlive} not implemented in tinyb.dbus
     * @see #getDiscovering()
     */
    public boolean startDiscovery(final boolean keepAlive) throws BluetoothException;

    /** Turns off device discovery if it is enabled.
      * @return TRUE if discovery was successfully disabled
      */
    public boolean stopDiscovery() throws BluetoothException;

    /** Returns a list of BluetoothDevices visible from this adapter.
      * @return A list of BluetoothDevices visible on this adapter,
      * NULL if an error occurred
      */
    public List<BluetoothDevice> getDevices();

    /**
     * Remove all the known devices
     *
     * @return The number of devices removed from internal list
     * @throws BluetoothException
     */
    public int removeDevices() throws BluetoothException;

    /* D-Bus property accessors: */
    /** Returns the hardware address of this adapter.
      * @return The hardware address of this adapter.
      */
    public String getAddress();

    /** Returns the system name of this adapter.
      * @return The system name of this adapter.
      */
    public String getName();

    /** Returns the friendly name of this adapter.
      * @return The friendly name of this adapter, or NULL if not set.
      */
    public String getAlias();

    /** Sets the friendly name of this adapter.
      */
    public void setAlias(String value);

    /** Returns the Bluetooth class of the adapter.
      * @return The Bluetooth class of the adapter.
      */
    public long getBluetoothClass();

    /**
     * Returns whether this adapter is enabled and usable.
     * <p>
     * An inactive adapter could be blocked or rfkill'ed.
     * </p>
     * @return {@code true} if this adapter is enabled and usable, otherwise {@code false}.
     * @since 2.0.0
     */
    public boolean isEnabled();

    /** Returns the power state the adapter.
      * @return The power state of the adapter.
      */
    public boolean getPowered();

    /**
     * Enables notifications for the powered property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the powered
     * property.
     */
    public void enablePoweredNotifications(BluetoothNotification<Boolean> callback);

    /**
     * Disables notifications of the powered property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    public void disablePoweredNotifications();

    /** Sets the power state the adapter.
      */
    public void setPowered(boolean value);

    /** Returns the discoverable state the adapter.
      * @return The discoverable state of the adapter.
      */
    public boolean getDiscoverable();

    /**
     * Enables notifications for the discoverable property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the discoverable
     * property.
     */
    public void enableDiscoverableNotifications(BluetoothNotification<Boolean> callback);
    /**
     * Disables notifications of the discoverable property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    public void disableDiscoverableNotifications();

    /** Sets the discoverable state the adapter.
      */
    public void setDiscoverable(boolean value);

    /** Returns the discoverable timeout the adapter.
      * @return The discoverable timeout of the adapter.
      */
    public long getDiscoverableTimeout();

    /** Sets the discoverable timeout the adapter. A value of 0 disables
      * the timeout.
      */
    public void setDiscoverableTimout(long value);

    /**
     * This method connects to device without need of
     * performing General Discovery. Connection mechanism is
     * similar to Connect method from Device1 interface with
     * exception that this method returns success when physical
     * connection is established. After this method returns,
     * services discovery will continue and any supported
     * profile will be connected. There is no need for calling
     * Connect on Device1 after this call. If connection was
     * successful this method returns object path to created
     * device object.
     *
     * @param address The Bluetooth device address of the remote
     *                device. This parameter is mandatory.
     * @param addressType The Bluetooth device Address Type. This is
     *                address type that should be used for initial
     *                connection. If this parameter is not present
     *                BR/EDR device is created.
     *                Possible values:
     *                <ul>
     *                <li>{@code public} - Public address</li>
     *                <li>{@code random} - Random address</li>
     *                </ul>
     */
    public BluetoothDevice connectDevice(String address, String addressType);

    /** Returns the pairable state the adapter.
      * @return The pairable state of the adapter.
      */
    public boolean getPairable();

    /**
     * Enables notifications for the pairable property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the pairable
     * property.
     */
    public void enablePairableNotifications(BluetoothNotification<Boolean> callback);

    /**
     * Disables notifications of the pairable property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    public void disablePairableNotifications();

    /** Sets the discoverable state the adapter.
      */
    public void setPairable(boolean value);

    /** Returns the timeout in seconds after which pairable state turns off
      * automatically, 0 means never.
      * @return The pairable timeout of the adapter.
      */
    public long getPairableTimeout();

    /** Sets the timeout after which pairable state turns off automatically, 0 means never.
      */
    public void setPairableTimeout(long value);

    /**
     * Returns the meta discovering state (of the adapter).
     * It can be modified through
     * start_discovery/stop_discovery functions.
     * @return The discovering state of the adapter.
     * @see #startDiscovery(boolean)
     */
    public boolean getDiscovering();

    /**
     * Add the given {@link AdapterStatusListener} to the list if not already present.
     * @param listener A {@link AdapterStatusListener} instance
     * @param deviceMatch Optional {@link BluetoothDevice} to be matched before calling any
     *        {@link AdapterStatusListener} {@code device*} methods. Pass {@code null} for no filtering.
     * @return true if the given listener is not element of the list and has been newly added, otherwise false.
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus
     */
    public boolean addStatusListener(final AdapterStatusListener listener, final BluetoothDevice deviceMatch);

    /**
     * Remove the given {@link AdapterStatusListener} from the list.
     * @param listener A {@link AdapterStatusListener} instance
     * @return true if the given listener is an element of the list and has been removed, otherwise false.
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus
     */
    public boolean removeStatusListener(final AdapterStatusListener l);

    /**
     * Remove all {@link AdapterStatusListener} from the list.
     * @return number of removed listener.
     * @since 2.0.0
     * @implNote not implemented in tinyb.dbus
     */
    public int removeAllStatusListener();

    /**
     * Enables notifications for the discovering property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the discovering
     * property.
     */
    public void enableDiscoveringNotifications(BluetoothNotification<Boolean> callback);

    /**
     * Disables notifications of the discovering property and unregisters the discovering
     * object passed through the corresponding enable method.
     */
    public void disableDiscoveringNotifications();

    /** Returns the UUIDs of the adapter.
      * @return Array containing the UUIDs of the adapter, ends with NULL.
      */
    public String[] getUUIDs();

    /** Returns the local ID of the adapter.
      * @return The local ID of the adapter.
      */
    public String getModalias();

    /** This method sets the device discovery filter for the caller. When this method is called
     * with no filter parameter, filter is removed.
     * <p>
     * When a remote device is found that advertises any UUID from UUIDs, it will be reported if:
     * <ul><li>Pathloss and RSSI are both empty.</li>
     * <li>only Pathloss param is set, device advertise TX pwer, and computed pathloss is less than Pathloss param.</li>
     * <li>only RSSI param is set, and received RSSI is higher than RSSI param.</li>
     * </ul>
     * <p>
     * If one or more discovery filters have been set, the RSSI delta-threshold,
     * that is imposed by StartDiscovery by default, will not be applied.
     * <p>
     * If "auto" transport is requested, scan will use LE, BREDR, or both, depending on what's
     * currently enabled on the controller.
     *
     * @param uuids a list of device UUIDs
     * @param rssi a rssi value
     * @param pathloss a pathloss value
     */
    public void setDiscoveryFilter(List<UUID> uuids, int rssi, int pathloss, TransportType transportType);

    /** Returns the interface name of the adapter.
     * @return The interface name of the adapter.
     */
    public String getInterfaceName();
}
