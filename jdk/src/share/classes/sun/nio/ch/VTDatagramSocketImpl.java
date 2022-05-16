/*
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package sun.nio.ch;

import java.io.IOException;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.DatagramChannel;
import java.util.concurrent.locks.LockSupport;
import java.util.concurrent.locks.ReentrantLock;
import static java.util.concurrent.TimeUnit.*;

import sun.misc.SharedSecrets;

public class VTDatagramSocketImpl extends DatagramSocketImpl {
    private final ReentrantLock readLock = new ReentrantLock();
    private final ReentrantLock writeLock = new ReentrantLock();
    private final ReentrantLock stateLock = new ReentrantLock();

    private volatile int state = ST_NEW;  // need stateLock to change
    private static final int ST_NEW = 0;
    private static final int ST_CLOSE = 1;

    // Timeout "option" value for receives
    private volatile int timeout;
    private DatagramChannelImpl dc;

    private boolean registerEvent(int fd, int event) throws IOException {
        stateLock.lock();
        try {
            if (state == ST_CLOSE) {
                return false;
            }
            Poller.register(fd, event);
        } finally {
            stateLock.unlock();
        }

        return true;
    }

    private void deregisterEvent(int fd, int event) {
        stateLock.lock();
        try {
            if (state == ST_CLOSE) {
                return;
            }
            Poller.deregister(fd, event);
        } finally {
            stateLock.unlock();
        }
    }

    private void park(int fd, int event, long nanos) throws IOException {
        try {
            if (!registerEvent(fd, event)) {
                return;
            }

            if (nanos == 0) {
                LockSupport.park();
            } else {
                LockSupport.parkNanos(nanos);
            }
        } finally {
            deregisterEvent(fd, event);
        }
    }

    private SocketAddress receive0(ByteBuffer bb) throws IOException {
        final DatagramChannelImpl ch = getChannelImpl();
        SocketAddress sa;

        if ((sa = ch.receive(bb)) != null)
            return sa;

        long timeoutNanos = NANOSECONDS.convert(timeout, MILLISECONDS);
        long startTime = System.nanoTime();

        do {
            park(ch.getFDVal(), Net.POLLIN, timeoutNanos);

            if (timeout > 0 && timeoutNanos <= System.nanoTime() - startTime) {
                throw new SocketTimeoutException("time out");
            }
        } while ((sa = ch.receive(bb)) == null);

        return sa;
    }

    private SocketAddress receive(ByteBuffer bb) throws IOException {
        assert this != null;
        try {
            readLock.lock();
            return receive0(bb);
        } catch (ClosedChannelException e) {
            throw new SocketException("Socket Closed");
        } finally {
            readLock.unlock();
        }
    }

    public int receive(DatagramPacket p, int len)  throws IOException {
        int dataLen = 0;
        try {
            ByteBuffer bb = ByteBuffer.wrap(p.getData(), p.getOffset(), len);
            SocketAddress sender = receive(bb);
            p.setSocketAddress(sender);
            dataLen = bb.position() - p.getOffset();
        } catch (IOException x) {
            Net.translateException(x);
        }
        return dataLen;
    }

    private void send0(DatagramPacket p) throws IOException {
        final DatagramChannelImpl ch = getChannelImpl();
        try {
            ByteBuffer bb = ByteBuffer.wrap(p.getData(),
                    p.getOffset(),
                    p.getLength());

            boolean useWrite = ch.isConnected() && p.getAddress() == null;
            if (useWrite) {
                InetSocketAddress isa = (InetSocketAddress)ch.remoteAddress();
                p.setPort(isa.getPort());
                p.setAddress(isa.getAddress());
            }

            int writeLen = bb.remaining();
            if ((useWrite ? ch.write(bb) : ch.send(bb, p.getSocketAddress())) == writeLen)
                return;

            long timeoutNanos = NANOSECONDS.convert(timeout, MILLISECONDS);
            long startTime = System.nanoTime();

            do {
                park(ch.getFDVal(), Net.POLLOUT, timeoutNanos);

                if (timeout > 0 && timeoutNanos <= System.nanoTime() - startTime) {
                    throw new SocketTimeoutException("time out");
                }

                if (useWrite) {
                    ch.write(bb);
                } else {
                    ch.send(bb, p.getSocketAddress());
                }
            } while (bb.remaining() > 0);
        } catch (IOException x) {
            Net.translateException(x);
        }
    }

    public void send(DatagramPacket p) throws IOException {
        try {
            writeLock.lock();
            send0(p);
        } finally {
            writeLock.unlock();
        }
    }

    private DatagramChannelImpl getChannelImpl() throws SocketException {
        if (dc == null) {
            try {
                dc = (DatagramChannelImpl)DatagramChannel.open();
                dc.configureBlocking(false);
            } catch (IOException e) {
                throw new SocketException(e.getMessage());
            } 
        }
        return dc;
    }

    private void tryClose(int fd) throws IOException {
        stateLock.lock();
        try {
            if (state == ST_CLOSE) {
                return;
            }
            state = ST_CLOSE;
            Poller.stopPoll(fd);
        } finally {
            stateLock.unlock();
        }
    }

    public void close() {
        try {
            tryClose(getChannelImpl().getFDVal());
            getChannelImpl().close();
        } catch (IOException x) {
            throw new Error(x);
        }
    }

    protected void create() throws SocketException {
    }

    private void setIntOption(SocketOption<Integer> name, int value) throws SocketException {
        try {
            getChannelImpl().setOption(name, value);
        } catch (IOException x) {
            Net.translateToSocketException(x);
        }
    }

    private int getIntOption(SocketOption<Integer> name) throws SocketException {
        try {
            return getChannelImpl().getOption(name).intValue();
        } catch (IOException x) {
            Net.translateToSocketException(x);
            return -1;          // keep compiler happy
        }
    }

    private void setBooleanOption(SocketOption<Boolean> name, boolean value)
        throws SocketException
    {
        try {
            getChannelImpl().setOption(name, value);
        } catch (IOException x) {
            Net.translateToSocketException(x);
        }
    }

    private boolean getBooleanOption(SocketOption<Boolean> name) throws SocketException {
        try {
            return getChannelImpl().getOption(name).booleanValue();
        } catch (IOException x) {
            Net.translateToSocketException(x);
            return false;       // keep compiler happy
        }
    }

    public void setOption(int optID, Object o) throws SocketException {
        switch (optID) {
        case SO_TIMEOUT:
            if (o == null || (!(o instanceof Integer)))
                throw new SocketException("Bad parameter for SO_TIMEOUT");
            int tmp = ((Integer) o).intValue();
            if (tmp < 0)
                throw new IllegalArgumentException("timeout < 0");
            timeout = tmp;
            break;
        case SO_SNDBUF:
            if (((Integer) o).intValue() <= 0)
                throw new IllegalArgumentException("Invalid send size");
            setIntOption(StandardSocketOptions.SO_SNDBUF, ((Integer) o).intValue());
            break;
        case SO_RCVBUF:
            if (((Integer) o).intValue() <= 0)
                throw new IllegalArgumentException("Invalid receive size");
            setIntOption(StandardSocketOptions.SO_RCVBUF, ((Integer) o).intValue());
            break;
        case SO_REUSEADDR:
            setBooleanOption(StandardSocketOptions.SO_REUSEADDR, ((Boolean)o).booleanValue());
            break;
        case SO_BROADCAST:
            setBooleanOption(StandardSocketOptions.SO_BROADCAST, ((Boolean)o).booleanValue());
            break;
        case IP_TOS:
            setIntOption(StandardSocketOptions.IP_TOS, ((Integer) o).intValue());
            break;
        }
        
    }

    public Object getOption(int optID) throws SocketException {
        switch (optID) {
        case SO_BINDADDR:
            InetSocketAddress local = (InetSocketAddress)getChannelImpl().localAddress();
            if (local == null)
                local = new InetSocketAddress(0);
            InetAddress result = local.getAddress();
            SecurityManager sm = System.getSecurityManager();
            if (sm != null) {
                try {
                    sm.checkConnect(result.getHostAddress(), -1);
                } catch (SecurityException x) {
                    return new InetSocketAddress(0).getAddress();
                }
            }
            return result;
        case SO_TIMEOUT:
            return timeout;
        case SO_SNDBUF:
            return getIntOption(StandardSocketOptions.SO_SNDBUF);
        case SO_RCVBUF:
            return getIntOption(StandardSocketOptions.SO_RCVBUF);
        case SO_REUSEADDR:
            return getBooleanOption(StandardSocketOptions.SO_REUSEADDR);
        case SO_BROADCAST:
            return getBooleanOption(StandardSocketOptions.SO_BROADCAST);
        case IP_TOS:
            return getIntOption(StandardSocketOptions.IP_TOS);
        }

        return null;
    }

    public int getLocalPort() {
        try {
            InetSocketAddress local = (InetSocketAddress)getChannelImpl().localAddress();
            if (local != null) {
                return local.getPort();
            }
        } catch (Exception x) {
        }
        return 0;
    }

    public void connect(InetAddress address, int port)
        throws SocketException
    {
        try {
            getChannelImpl().connect(new InetSocketAddress(address, port));
        } catch (ClosedChannelException e) {
            // ignore
        } catch (Exception x) {
            Net.translateToSocketException(x);
        }
    }

    public void disconnect() {
        try {
            getChannelImpl().disconnect();
        } catch (IOException x) {
            throw new Error(x);
        }
    }

    protected void bind(int lport, InetAddress laddr) throws SocketException {
        try {
            getChannelImpl().bind(new InetSocketAddress(laddr, lport));
        } catch (Exception x) {
            Net.translateToSocketException(x);
        }
    }

    protected int peek(InetAddress i) throws IOException { return 0; }

    protected int peekData(DatagramPacket p) throws IOException { return 0; }

    protected void receive(DatagramPacket p) throws IOException {}

    @Deprecated
    protected void setTTL(byte ttl) throws IOException {}

    @Deprecated
    protected byte getTTL() throws IOException { return 0; }

    protected void setTimeToLive(int ttl) throws IOException {}

    protected int getTimeToLive() throws IOException { return 0;}

    protected void join(InetAddress inetaddr) throws IOException {}

    protected void leave(InetAddress inetaddr) throws IOException {}

    protected void joinGroup(SocketAddress mcastaddr,
                             NetworkInterface netIf) throws IOException {}

    protected void leaveGroup(SocketAddress mcastaddr,
                              NetworkInterface netIf) throws IOException {}
}
