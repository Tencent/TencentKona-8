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

import sun.misc.SharedSecrets;

import java.util.concurrent.locks.LockSupport;
import java.util.concurrent.locks.ReentrantLock;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.SocketChannel;
import java.nio.channels.ServerSocketChannel;
import java.security.AccessController;
import java.security.PrivilegedExceptionAction;
import static java.util.concurrent.TimeUnit.*;
import sun.net.www.ParseUtil;

public class VTSocketImpl extends SocketImpl {
    private boolean isServerSocket;
    private boolean isSocksSocket;
    private int timeout;

    private final ReentrantLock readLock = new ReentrantLock();
    private final ReentrantLock writeLock = new ReentrantLock();
    private final ReentrantLock stateLock = new ReentrantLock();

    private volatile int state = ST_NEW;  // need stateLock to change
    private static final int ST_NEW = 0;
    private static final int ST_CLOSE = 1;

    private static final int OP_OTHER = 0;
    private static final int OP_READ = 1;
    private static final int OP_WRITE = 2;
    // flags to indicate if the connection is shutdown for input and output
    private volatile boolean isInputClosed;
    private volatile boolean isOutputClosed;

    protected SocketChannelImpl sc = null;
    protected ServerSocketChannelImpl ssc = null;

    /* indicates connection reset state */
    private boolean connectionReset;

    public VTSocketImpl(boolean isServerSocket, boolean isSocksSocket) {
        this.isServerSocket = isServerSocket;
        this.isSocksSocket = isSocksSocket;
    }

    private boolean registerEvent(int fd, int event, int op) throws IOException {
        stateLock.lock();
        try {
            if (state == ST_CLOSE
                || (op == OP_READ && isInputClosed)
                || (op == OP_WRITE && isOutputClosed)) {
                return false;
            }

            Poller.register(fd, event);
        } finally {
            stateLock.unlock();
        }

        return true;
    }

    private void deregisterEvent(int fd, int event, int op) {
        stateLock.lock();
        try {
            if (state == ST_CLOSE
                || (op == OP_READ && isInputClosed)
                || (op == OP_WRITE && isOutputClosed)) {
                return;
            }
            Poller.deregister(fd, event);
        } finally {
            stateLock.unlock();
        }
    }

    private void park(int fd, int event, long nanos, int op) throws IOException {
        try {
            if (!registerEvent(fd, event, op)) {
                return;
            }

            if (nanos == 0) {
                LockSupport.park();
            } else {
                LockSupport.parkNanos(nanos);
            }
        } finally {
            deregisterEvent(fd, event, op);
        }
    }

    private void connect0(SocketAddress remote, int timeout) throws IOException {
        final SocketChannelImpl ch = getSocketChannelImpl();
        readLock.lock();
        try {
            if (ch.connect(remote)) return;

            long timeoutNanos = NANOSECONDS.convert(timeout, MILLISECONDS);
            long startTime = System.nanoTime();

            do {
                park(ch.getFDVal(), Net.POLLOUT, timeoutNanos, OP_OTHER);

                if (timeout > 0 && timeoutNanos <= System.nanoTime() - startTime) {
                    throw new SocketTimeoutException("time out");
                }
            } while (!ch.finishConnect());
        } catch (Exception x) {
            try {
                Net.translateException(x, true);
            } catch (IOException e) {
                close();
                throw e;
            }
        } finally {
            readLock.unlock();

            if (ch.isBlocking())
                ch.configureBlocking(false);
        }
    }

    /**
     * Check proxy is set by System.setProperty() or not.
     * Code is referred to SocksSocketImpl::connect(SocketAddress endpoint, int timeout). 
     *
     * @param remote end point of connect
     * @throws UnsupportedOperationException if proxy has set by System.setProperty()
     */
    private void checkProxyPropertyHasSet(SocketAddress remote) throws IOException {
        ProxySelector sel = java.security.AccessController.doPrivileged(
            new java.security.PrivilegedAction<ProxySelector>() {
                public ProxySelector run() {
                        return ProxySelector.getDefault();
                    }
                });
        if (sel == null) {
            return;
        }

        InetSocketAddress epoint = (InetSocketAddress)remote;
        URI uri;
        // Use getHostString() to avoid reverse lookups
        String host = epoint.getHostString();
        // IPv6 litteral?
        if (epoint.getAddress() instanceof Inet6Address &&
            (!host.startsWith("[")) && (host.indexOf(':') >= 0)) {
            host = "[" + host + "]";
        }
        try {
            uri = new URI("socket://" + ParseUtil.encodePath(host) + ":"+ epoint.getPort());
        } catch (URISyntaxException e) {
            // This shouldn't happen
            assert false : e;
            uri = null;
        }
        Proxy p = null;
        IOException savedExc = null;
        java.util.Iterator<Proxy> iProxy = null;
        iProxy = sel.select(uri).iterator();
        if (iProxy == null || !(iProxy.hasNext())) {
            return;
        }

        while (iProxy.hasNext()) {
            p = iProxy.next();
            if (p == null || p.type() != Proxy.Type.SOCKS) {
                return;
            }

            if (!(p.address() instanceof InetSocketAddress))
                throw new SocketException("Unknown address type for proxy: " + p);

            throw new UnsupportedOperationException();
        }
    }

    @Override
    public void connect(SocketAddress remote, int timeout) throws IOException {
        if (remote == null)
            throw new IllegalArgumentException("connect: The address can't be null");
        if (timeout < 0)
            throw new IllegalArgumentException("connect: timeout can't be negative");

        if (isSocksSocket) {
            checkProxyPropertyHasSet(remote);
        }
        connect0(remote, timeout);
    }

    private InputStream socketInputStream = null;

    @Override
    public InputStream getInputStream() throws IOException {
        if (socketInputStream == null) {
            try {
                socketInputStream = AccessController.doPrivileged(
                    new PrivilegedExceptionAction<InputStream>() {
                        public InputStream run() throws IOException {
                            return new InputStream() {
                                protected final SocketChannelImpl ch = getSocketChannelImpl();
                                private ByteBuffer bb = null;
                                // Invoker's previous array
                                private byte[] bs = null;
                                private byte[] b1 = null;

                                @Override
                                public int read() throws IOException {
                                    if (b1 == null) {
                                        b1 = new byte[1];
                                    }
                                    int n = this.read(b1);
                                    if (n == 1)
                                        return b1[0] & 0xff;
                                    return -1;
                                }

                                @Override
                                public int read(byte[] bs, int off, int len)
                                        throws IOException {
                                    if (len <= 0 || off < 0 || off + len > bs.length) {
                                        if (len == 0) {
                                            return 0;
                                        }
                                        throw new ArrayIndexOutOfBoundsException();
                                    }

                                    ByteBuffer bb = ((this.bs == bs) ? this.bb : ByteBuffer.wrap(bs));

                                    bb.limit(Math.min(off + len, bb.capacity()));
                                    bb.position(off);
                                    this.bb = bb;
                                    this.bs = bs;
                                    return read(bb);
                                }

                                private int read(ByteBuffer bb) throws IOException {
                                    try {
                                        readLock.lock();
                                        return read0(bb);
                                    } catch (ClosedChannelException x) {
                                        throw new SocketException("Socket closed");
                                    } catch (IOException e) {
                                        if (e.getLocalizedMessage().startsWith("Connection reset by peer")) {
                                            connectionReset = true;
                                            throw new SocketException("Connection reset");
                                        } else {
                                            throw e;
                                        }
                                    } finally {
                                        readLock.unlock();
                                    }
                                }

                                private int read0(ByteBuffer bb)
                                        throws IOException {
                                    int n;

                                    if (connectionReset)
                                        throw new SocketException("Connection reset");

                                    if ((n = ch.read(bb)) != 0) {
                                        return n;
                                    }

                                    long timeoutNanos = NANOSECONDS.convert(timeout, MILLISECONDS);
                                    long startTime = System.nanoTime();

                                    do {
                                        park(ch.getFDVal(), Net.POLLIN, timeoutNanos, OP_READ);

                                        if (timeout > 0 && timeoutNanos <= System.nanoTime() - startTime) {
                                            throw new SocketTimeoutException("time out");
                                        }
                                    } while ((n = ch.read(bb)) == 0);

                                    return n;
                                }

                                @Override
                                public int available() throws IOException {
                                    return VTSocketImpl.this.available();
                                }

                                @Override
                                public void close() throws IOException {
                                    VTSocketImpl.this.close();
                                }
                            };
                        }
                    });
            } catch (java.security.PrivilegedActionException e) {
                throw (IOException)e.getException();
            }
        }
        return socketInputStream;
    }

    @Override
    protected int available() throws IOException {
        stateLock.lock();
        try {
            if (!getSocketChannelImpl().isOpen())
                throw new ClosedChannelException();
            if (isInputClosed) {
                return 0;
            } else {
                return Net.available(getSocketChannelImpl().getFD());
            }
        } finally {
            stateLock.unlock();
        }
    }

    @Override
    public OutputStream getOutputStream() throws IOException {
        try {
            return AccessController.doPrivileged(
                new PrivilegedExceptionAction<OutputStream>() {
                    public OutputStream run() throws IOException {
                        return new OutputStream() {
                            protected final SocketChannelImpl ch = getSocketChannelImpl();
                            private ByteBuffer bb = null;
                            // Invoker's previous array
                            private byte[] bs = null;
                            private byte[] b1 = null;


                            @Override
                            public void write(int b) throws IOException {
                                if (b1 == null) {
                                    b1 = new byte[1];
                                }
                                b1[0] = (byte) b;
                                this.write(b1);
                            }

                            @Override
                            public void write(byte[] bs, int off, int len)
                                    throws IOException
                            {
                                if (len <= 0 || off < 0 || off + len > bs.length) {
                                    if (len == 0) {
                                        return;
                                    }
                                    throw new ArrayIndexOutOfBoundsException();
                                }
                                ByteBuffer bb = ((this.bs == bs) ? this.bb : ByteBuffer.wrap(bs));
                                bb.limit(Math.min(off + len, bb.capacity()));
                                bb.position(off);
                                this.bb = bb;
                                this.bs = bs;

                                write(bb);
                            }

                            private void write(ByteBuffer bb) throws IOException {
                                try {
                                    writeLock.lock();
                                    write0(bb);
                                } catch (Exception x) {
                                    throw new SocketException("Socket closed");
                                } finally {
                                    writeLock.unlock();
                                }
                            }

                            private void write0(ByteBuffer bb)
                                    throws IOException {

                                int writeLen = bb.remaining();
                                if (ch.write(bb) == writeLen) {
                                    return;
                                }

                                long timeoutNanos = NANOSECONDS.convert(timeout, MILLISECONDS);
                                long startTime = System.nanoTime();
                                do {
                                    park(ch.getFDVal(), Net.POLLOUT, timeoutNanos, OP_WRITE);
                                    if (timeout > 0 && timeoutNanos <= System.nanoTime() - startTime) {
                                        throw new SocketTimeoutException("time out");
                                    }
                                    ch.write(bb);
                                } while (bb.remaining() > 0);
                            }

                            @Override
                            public void close() throws IOException {
                                VTSocketImpl.this.close();
                            }
                        };
                    }
                });
        } catch (java.security.PrivilegedActionException e) {
            throw (IOException)e.getException();
        }
    }

    private SocketChannelImpl getSocketChannelImpl() {
        if (sc == null) {
            try {
                sc = (SocketChannelImpl)SocketChannel.open();
                sc.configureBlocking(false);
            } catch (IOException e) {
            }
        }
        return sc;
    }

    private ServerSocketChannelImpl getServerSocketChannelImpl() {
        if (ssc == null) {
            try {
                ssc = (ServerSocketChannelImpl)ServerSocketChannel.open();
                ssc.configureBlocking(false);
            } catch (IOException e) {
            }
        }
        return ssc;
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

    public void close() throws IOException {
        if (isServerSocket) {
            tryClose(getServerSocketChannelImpl().getFDVal());
            getServerSocketChannelImpl().close();
        } else {
            tryClose(getSocketChannelImpl().getFDVal());
            getSocketChannelImpl().close();
        }
    }

    protected void bind(InetAddress host, int port) throws IOException {
        InetSocketAddress localAddress;
        try {
            if (isServerSocket) {
                getServerSocketChannelImpl().bind(new InetSocketAddress(host, port));
                localAddress = getServerSocketChannelImpl().localAddress();
            } else {
                getSocketChannelImpl().bind(new InetSocketAddress(host, port));
                localAddress = getSocketChannelImpl().localAddress();
            }

            if (localAddress == null) {
                this.localport = -1;
                this.address = null;
            } else {
                this.localport = localAddress.getPort();
                this.address = Net.getRevealedLocalAddress(localAddress).getAddress();
            }
        } catch (Exception x) {
            Net.translateException(x);
        }
    }

    protected boolean supportsUrgentData () {
        return true;
    }

    protected void sendUrgentData (int data) throws IOException {
        int n = getSocketChannelImpl().sendOutOfBandData((byte) data);
        if (n == 0)
            throw new IOException("Socket buffer full");
    }

    public void setSocketChannel(SocketChannel target) {
        sc = (SocketChannelImpl)target;
    }

    protected void accept(SocketImpl s) throws IOException {
        try {
            readLock.lock();
            accept0(s);
        } catch (ClosedChannelException x) {
            throw new SocketException("Socket closed");
        } finally {
            readLock.unlock();
        }
    }

    private void accept0(SocketImpl s) throws IOException {
        final ServerSocketChannelImpl ch = getServerSocketChannelImpl();
        try {
            SocketChannel res;

            long timeoutNanos = NANOSECONDS.convert(timeout, MILLISECONDS);
            long startTime = System.nanoTime();

            while ((res = ch.accept()) == null) {
                park(ch.getFDVal(), Net.POLLIN, timeoutNanos, OP_OTHER);
                if (timeout > 0 && timeoutNanos <= System.nanoTime() - startTime) {
                    throw new SocketTimeoutException("Accept timed out");
                }
            }
            res.configureBlocking(false);
            ((VTSocketImpl)s).setSocketChannel(res);
        } catch (Exception x) {
            Net.translateException(x, true);
        }
    }

    protected void listen(int backlog) throws IOException {
    }

    protected void connect(String host, int port) throws IOException {
        assert false;
    }

    protected void connect(InetAddress address, int port) throws IOException {
        assert false;
    }

    protected void create(boolean stream) throws IOException {
    }

    private boolean getBooleanOption(SocketOption<Boolean> name) throws SocketException {
        try {
            if (isServerSocket) {
                return getServerSocketChannelImpl().getOption(name).booleanValue();
            } else {
                return getSocketChannelImpl().getOption(name).booleanValue();
            }
        } catch (IOException x) {
            Net.translateToSocketException(x);
            return false;       // keep compiler happy
        }
    }

    private void setBooleanOption(SocketOption<Boolean> name, boolean value) throws SocketException {
        try {
            if (isServerSocket) {
                getServerSocketChannelImpl().setOption(name, value);
            } else {
                getSocketChannelImpl().setOption(name, value);
            }
        } catch (IOException x) {
            Net.translateToSocketException(x);
        }
    }

    private int getIntOption(SocketOption<Integer> name) throws SocketException {
        try {
            if (isServerSocket) {
                return getServerSocketChannelImpl().getOption(name).intValue();
            } else {
                return getSocketChannelImpl().getOption(name).intValue();
            }
        } catch (IOException x) {
            Net.translateToSocketException(x);
            return -1;          // keep compiler happy
        }
    }

    private void setIntOption(SocketOption<Integer> name, int value) throws SocketException {
        try {
            if (isServerSocket) {
                getServerSocketChannelImpl().setOption(name, value);
            } else {
                getSocketChannelImpl().setOption(name, value);
            }
        } catch (IOException x) {
            Net.translateToSocketException(x);
        }
    }

    
    public Object getOption(int optID) throws SocketException {
        switch (optID) {
        case SO_OOBINLINE:
            try {
                return getSocketChannelImpl().getOption(ExtendedSocketOption.SO_OOBINLINE);
            } catch (IOException x) {
                Net.translateToSocketException(x);
            }
            break;
        case SO_TIMEOUT:
            return timeout;
        case SO_LINGER:
            try {
                return getSocketChannelImpl().getOption(StandardSocketOptions.SO_LINGER).intValue();
            } catch (IOException x) {
                Net.translateToSocketException(x);
                return null;          // keep compiler happy
            }
        case TCP_NODELAY:
            return getBooleanOption(StandardSocketOptions.TCP_NODELAY);
        case SO_BINDADDR:
            if (getSocketChannelImpl().isOpen()) {
                InetSocketAddress local = getSocketChannelImpl().localAddress();
                if (local != null) {
                    return Net.getRevealedLocalAddress(local).getAddress();
                }
            }
            return new InetSocketAddress(0).getAddress();
        case IP_TOS:
            try {
                return getSocketChannelImpl().getOption(StandardSocketOptions.IP_TOS).intValue();
            } catch (IOException x) {
                Net.translateToSocketException(x);
                return -1;          // keep compiler happy
            }
        case SO_REUSEADDR:
            return getBooleanOption(StandardSocketOptions.SO_REUSEADDR);
        case SO_KEEPALIVE:
            return getBooleanOption(StandardSocketOptions.SO_KEEPALIVE);
        case SO_SNDBUF:
            return getIntOption(StandardSocketOptions.SO_SNDBUF);
        case SO_RCVBUF:
            return getIntOption(StandardSocketOptions.SO_RCVBUF);
        default:
            throw new SocketException("Unknown option " + optID);
        }

        return null; // keep compiler happy
    }

    @Override
    public <T> void setOption(SocketOption<T> name, T value) throws IOException {
        if (isServerSocket) {
            getServerSocketChannelImpl().setOption(name, value);
        } else {
            getSocketChannelImpl().setOption(name, value);
        }
    }

    @Override
    public <T> T getOption(SocketOption<T> name) throws IOException {
        if (isServerSocket) {
            if (name == StandardSocketOptions.IP_TOS) {
                throw new UnsupportedOperationException("'" + name + "' not supported when add option -Djdk.internal.VTSocket=true(default is true)");
            }
            return getServerSocketChannelImpl().getOption(name);
        } else {
            return getSocketChannelImpl().getOption(name);
        }
    }

    public void setOption(int optID, Object o) throws SocketException {
        switch (optID) {
        case SO_OOBINLINE:
            setBooleanOption(ExtendedSocketOption.SO_OOBINLINE, ((Boolean)o).booleanValue());
            break;
        case SO_TIMEOUT:
            if (o == null || (!(o instanceof Integer)))
                throw new SocketException("Bad parameter for SO_TIMEOUT");
            int tmp = ((Integer) o).intValue();
            if (tmp < 0)
                throw new IllegalArgumentException("timeout < 0");
            timeout = tmp;
            break;
        case SO_LINGER:
            int linger;
            if (o == null || (!(o instanceof Integer) && !(o instanceof Boolean)))
                throw new SocketException("Bad parameter for option");
            if (o instanceof Boolean) {
                /* true only if disabling - enabling should be Integer */
                linger = -1;
            } else {
                linger = ((Integer)o).intValue();
            }
            setIntOption(StandardSocketOptions.SO_LINGER, linger);
            break;
        case TCP_NODELAY:
            setBooleanOption(StandardSocketOptions.TCP_NODELAY, ((Boolean)o).booleanValue());
            break;
        case IP_TOS:
            setIntOption(StandardSocketOptions.IP_TOS, ((Integer)o).intValue());
            break;
        case SO_REUSEADDR:
            setBooleanOption(StandardSocketOptions.SO_REUSEADDR, ((Boolean)o).booleanValue());
            break;
        case SO_KEEPALIVE:
            setBooleanOption(StandardSocketOptions.SO_KEEPALIVE, ((Boolean)o).booleanValue());
            break;
        case SO_SNDBUF:
            // size 0 valid for SocketChannel, invalid for Socket
            if (((Integer)o).intValue() <= 0)
                throw new IllegalArgumentException("Invalid send size");
            setIntOption(StandardSocketOptions.SO_SNDBUF, ((Integer)o).intValue());
            break;
        case SO_RCVBUF:
            // size 0 valid for SocketChannel, invalid for Socket
            if (((Integer)o).intValue() <= 0)
                throw new IllegalArgumentException("Invalid receive size");
            setIntOption(StandardSocketOptions.SO_RCVBUF, ((Integer)o).intValue());
            break;
        default:
            throw new SocketException("Unknown option " + optID);
        }
    }

    public InetAddress getInetAddress() {
        if (isServerSocket) {
            InetSocketAddress local = getServerSocketChannelImpl().localAddress();
            if (local == null) {
                return null;
            } else {
                return Net.getRevealedLocalAddress(local).getAddress();
            }
        } else {
            InetSocketAddress remote = (InetSocketAddress)(getSocketChannelImpl().remoteAddress());
            if (remote == null) {
                return null;
            } else {
                return remote.getAddress();
            }
        }
    }

    public int getPort() {
        if (isServerSocket) {
            InetSocketAddress local = getServerSocketChannelImpl().localAddress();
            if (local == null) {
                return -1;
            } else {
                return local.getPort();
            }
        } else {
            InetSocketAddress remote = (InetSocketAddress)(getSocketChannelImpl().remoteAddress());
            if (remote == null) {
                return 0;
            } else {
                return remote.getPort();
            }
        }
    }

    protected void shutdownInput() throws IOException {
        assert isServerSocket == false;
        try {
            stateLock.lock();
            try {
                if (state != ST_CLOSE && !isInputClosed) {
                    isInputClosed = true;
                    Poller.stopPoll(getSocketChannelImpl().getFDVal(), Net.POLLIN);
                }
            } finally {
                stateLock.unlock();
            }
            getSocketChannelImpl().shutdownInput();
        } catch (Exception x) {
            Net.translateException(x);
        }
    }

    protected void shutdownOutput() throws IOException {
        assert isServerSocket == false;
        try {
            try {
                stateLock.lock();
                if (state != ST_CLOSE && !isOutputClosed) {
                    isOutputClosed = true;
                    Poller.stopPoll(getSocketChannelImpl().getFDVal(), Net.POLLOUT);
                }
            } finally {
                stateLock.unlock();
            }
            getSocketChannelImpl().shutdownOutput();
        } catch (Exception x) {
            Net.translateException(x);
        }
    }

    public int getLocalPort() {
        if (isServerSocket) {
            InetSocketAddress local = getServerSocketChannelImpl().localAddress();
            if (local == null) {
                return -1;
            } else {
                return local.getPort();
            }
        } else {
            InetSocketAddress local = getSocketChannelImpl().localAddress();
            if (local == null) {
                return -1;
            } else {
                return local.getPort();
            }
        }
    }   
}
