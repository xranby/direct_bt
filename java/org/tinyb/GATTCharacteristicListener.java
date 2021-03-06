/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
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
 */

package org.tinyb;

import java.lang.ref.WeakReference;

/**
 * {@link BluetoothGattCharacteristic} event listener for notification and indication events.
 * <p>
 * A listener instance may be attached to a {@link BluetoothGattCharacteristic} via
 * {@link BluetoothGattCharacteristic#addCharacteristicListener(GATTCharacteristicListener)} to listen to its events.
 * </p>
 * <p>
 * A listener instance may be attached to a {@link BluetoothDevice} via
 * {@link BluetoothDevice#addCharacteristicListener(GATTCharacteristicListener, BluetoothGattCharacteristic)}
 * to listen to all events of the device or the matching filtered events.
 * </p>
 * <p>
 * One {@link GATTCharacteristicListener} instance can only be attached to a listener receiver once at a time,
 * i.e. you cannot attach the same instance more than once to a {@link BluetoothDevice}
 * or {@link BluetoothGattCharacteristic}.
 * <br>
 * To attach multiple listener, one instance per attachment must be created.
 * <br>
 * This restriction is due to implementation semantics of strictly associating
 * one Java {@link GATTCharacteristicListener} instance to one C++ {@code GATTCharacteristicListener} instance.
 * The latter will be added to the native list of listeners.
 * This class's {@code nativeInstance} field links the Java instance to mentioned C++ listener.
 * <br>
 * Since the listener receiver maintains a unique set of listener instances without duplicates,
 * this restriction is more esoteric.
 * </p>
 */
public abstract class GATTCharacteristicListener {
    @SuppressWarnings("unused")
    private long nativeInstance;
    private final WeakReference<BluetoothGattCharacteristic> associatedCharacteristic;

    /**
     * Returns the weakly associated {@link BluetoothGattCharacteristic} to this listener instance.
     * <p>
     * Returns {@code null} if no association has been made
     * or if the associated {@link BluetoothGattCharacteristic} has been garbage collected.
     * </p>
     */
    public final BluetoothGattCharacteristic getAssociatedCharacteristic() {
        return null != associatedCharacteristic ? associatedCharacteristic.get() : null;
    }

    /**
     * @param associatedCharacteristic weakly associates this listener instance to one {@link BluetoothGattCharacteristic},
     *        may be {@code null} for no association.
     * @see #getAssociatedCharacteristic()
     */
    public GATTCharacteristicListener(final BluetoothGattCharacteristic associatedCharacteristic) {
        if( null != associatedCharacteristic ) {
            this.associatedCharacteristic = new WeakReference<BluetoothGattCharacteristic>(associatedCharacteristic);
        } else {
            this.associatedCharacteristic = null;
        }
    }

    /**
     * Called from native BLE stack, initiated by a received notification associated
     * with the given {@link BluetoothGattCharacteristic}.
     * @param charDecl {@link BluetoothGattCharacteristic} related to this notification
     * @param value the notification value
     * @param timestamp the indication monotonic timestamp, see {@link BluetoothUtils#getCurrentMilliseconds()}
     */
    public void notificationReceived(final BluetoothGattCharacteristic charDecl,
                                     final byte[] value, final long timestamp) {
    }

    /**
     * Called from native BLE stack, initiated by a received indication associated
     * with the given {@link BluetoothGattCharacteristic}.
     * @param charDecl {@link BluetoothGattCharacteristic} related to this indication
     * @param value the indication value
     * @param timestamp the indication monotonic timestamp, see {@link BluetoothUtils#getCurrentMilliseconds()}
     * @param confirmationSent if true, the native stack has sent the confirmation, otherwise user is required to do so.
     */
    public void indicationReceived(final BluetoothGattCharacteristic charDecl,
                                   final byte[] value, final long timestamp,
                                   final boolean confirmationSent) {
    }

    public String toString() {
        final BluetoothGattCharacteristic c = getAssociatedCharacteristic();
        final String cs = null != c ? c.toString() : "null";
        return "GATTCharacteristicListener[associated "+cs+"]";
    }
};
