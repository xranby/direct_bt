/*
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

package direct_bt.tinyb;

import org.tinyb.BluetoothCallback;
import org.tinyb.BluetoothEvent;
import org.tinyb.BluetoothType;

public class DBTEvent implements BluetoothEvent
{
    private long nativeInstance;

    @Override
    public native BluetoothType getType();
    @Override
    public native String getName();
    @Override
    public native String getIdentifier();
    @Override
    public native boolean executeCallback();
    @Override
    public native boolean hasCallback();

    private native void init(BluetoothType type, String name, String identifier,
                            DBTObject parent, BluetoothCallback cb, Object data);
    private native void delete();

    public DBTEvent(final BluetoothType type, final String name, final String identifier,
                            final DBTObject parent, final BluetoothCallback cb, final Object data)
    {
        init(type, name, identifier, parent, cb, data);
    }

    @Override
    protected void finalize()
    {
        delete();
    }
}
