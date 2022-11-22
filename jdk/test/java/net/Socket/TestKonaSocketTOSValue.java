/*
 * Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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
 */

/*
 * @test
 * @summary Test the property of -Dkona.socket.tos.value
 * @library /lib/testlibrary
 * @run main/othervm TestKonaSocketTOSValue
 */

import java.net.*;
import java.nio.*;
import java.nio.channels.*;
import javax.net.*;
import javax.net.ssl.*;
import jdk.net.Sockets;

import jdk.testlibrary.OutputAnalyzer;
import jdk.testlibrary.ProcessTools;

public class TestKonaSocketTOSValue {
    public static void main(String ...args) throws Exception {
        testOptions(null, "0");
        testOptions("-Dkona.socket.tos.value=96", "96");
        testOptions("-Dkona.socket.tos.value=104", "104");
        testOptions("-Dkona.socket.tos.value=0", "0");

        testFailure("-Dkona.socket.tos.value=-1");
        testFailure("-Dkona.socket.tos.value=256");

        testSetTraffic("-Dkona.socket.tos.value=96", "128");
        testSetTraffic("-Dkona.socket.tos.value=0", "96");
    }

    static void testFailure(String arg) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Djdk.internal.VTSocket=off", arg, Test.class.getName());
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Invalid IP_TOS value");
        output.shouldContain("Exception");
    }

    static void testSetTraffic(String arg, String traffic) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Djdk.internal.VTSocket=off", arg, Test.class.getName(), traffic);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("socket: " + traffic);
        output.shouldContain("server socket: " + traffic);
        output.shouldContain("socket channel: " + traffic);
        output.shouldContain("server socket channel: " + traffic);
        output.shouldContain("datagram socket: " + traffic);
        output.shouldContain("datagram channel: " + traffic);
        output.shouldContain("socket fatory: " + traffic);
        output.shouldContain("serversocket fatory: " + traffic);
        output.shouldContain("SSL socket: " + traffic);
        output.shouldContain("Proxy socket: " + traffic);
        output.shouldHaveExitValue(0);
    }

    static void testOptions(String arg, String tos) throws Exception {
        ProcessBuilder pb;
        if (arg != null) {
            pb = ProcessTools.createJavaProcessBuilder("-Djdk.internal.VTSocket=off", arg, Test.class.getName());
        } else {
            pb = ProcessTools.createJavaProcessBuilder("-Djdk.internal.VTSocket=off", Test.class.getName());
        }

        // check the output
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("socket: " + tos);
        output.shouldContain("server socket: " + tos);
        output.shouldContain("socket channel: " + tos);
        output.shouldContain("server socket channel: " + tos);
        output.shouldContain("datagram socket: " + tos);
        output.shouldContain("datagram channel: " + tos);
        output.shouldContain("socket fatory: " + tos);
        output.shouldContain("serversocket fatory: " + tos);
        output.shouldContain("SSL socket: " + tos);
        output.shouldContain("Proxy socket: " + tos);
        output.shouldHaveExitValue(0);
    }

    static class Test {
        public static void main(String ...args) throws Exception {
            Socket s = new Socket();
            if (args.length > 0) {
                s.setTrafficClass(Integer.parseInt(args[0]));
            }
            System.out.println("socket: " + s.getTrafficClass());

            ServerSocket ss = new ServerSocket();
            if (args.length > 0) {
                Sockets.setOption(ss, StandardSocketOptions.IP_TOS, Integer.parseInt(args[0]));
            }
            System.out.println("server socket: " + Sockets.getOption(ss, StandardSocketOptions.IP_TOS));

            SocketChannel sc = SocketChannel.open();
            if (args.length > 0) {
                sc.socket().setTrafficClass(Integer.parseInt(args[0]));
            }
            System.out.println("socket channel: " + sc.socket().getTrafficClass());

            ServerSocketChannel ssc = ServerSocketChannel.open();
            if (args.length > 0) {
                Sockets.setOption(ssc.socket(), StandardSocketOptions.IP_TOS, Integer.parseInt(args[0]));
            }
            System.out.println("server socket channel: " + Sockets.getOption(ssc.socket(), StandardSocketOptions.IP_TOS));

            DatagramSocket ds = new DatagramSocket();
            if (args.length > 0) {
                ds.setTrafficClass(Integer.parseInt(args[0]));
            }
            System.out.println("datagram socket: " + ds.getTrafficClass());

            DatagramChannel dc = DatagramChannel.open();
            if (args.length > 0) {
                dc.socket().setTrafficClass(Integer.parseInt(args[0]));
            }
            System.out.println("datagram channel: " + dc.socket().getTrafficClass());

            SocketFactory socketFactory = SocketFactory.getDefault();
            Socket fs = socketFactory.createSocket();
            if (args.length > 0) {
                fs.setTrafficClass(Integer.parseInt(args[0]));
            }
            System.out.println("socket fatory: " + fs.getTrafficClass());

            ServerSocketFactory serverSocketFactory = ServerSocketFactory.getDefault();
            ServerSocket sfs = serverSocketFactory.createServerSocket();
            if (args.length > 0) {
                Sockets.setOption(sfs, StandardSocketOptions.IP_TOS, Integer.parseInt(args[0]));
            }
            System.out.println("serversocket fatory: " + Sockets.getOption(sfs, StandardSocketOptions.IP_TOS));

            SSLSocketFactory ssf = (SSLSocketFactory) SSLSocketFactory.getDefault();
            SSLSocket ssls = (SSLSocket) ssf.createSocket();
            if (args.length > 0) {
                ssls.setTrafficClass(Integer.parseInt(args[0]));
            }
            System.out.println("SSL socket: " + ssls.getTrafficClass());

            SocketAddress addr = new InetSocketAddress("www.tencent.com", 9999);
            Proxy proxy = new Proxy(Proxy.Type.SOCKS, addr);
            Socket sp = new Socket(proxy);
            if (args.length > 0) {
                sp.setTrafficClass(Integer.parseInt(args[0]));
            }
            System.out.println("Proxy socket: " + sp.getTrafficClass());
        }
    }
}
