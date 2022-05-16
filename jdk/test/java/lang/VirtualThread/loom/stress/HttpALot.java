/*
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
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

/**
 * @test
 * @summary Stress test the HTTP protocol handler and HTTP server
 * @modules java.base/java.util.concurrent:open
 * @compile HttpALot.java
 * @run main/othervm/timeout=120
 *     -Dsun.net.client.defaultConnectTimeout=50000
 *     -Dsun.net.client.defaultReadTimeout=50000
 *     HttpALot
 */

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.net.URL;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicInteger;
import com.sun.net.httpserver.HttpServer;

public class HttpALot {
    public static void main(String[] args) throws Exception {
        int requests = 1_000;
        if (args.length > 0) {
            requests = Integer.parseInt(args[0]);
        }

        AtomicInteger requestsHandled = new AtomicInteger();

        // Create HTTP on the loopback address. The server will reply to
        // the requests to GET /hello. It uses virtual threads.
        InetAddress lb = InetAddress.getLoopbackAddress();
        HttpServer server = HttpServer.create(new InetSocketAddress(lb, 0), 1024);

        ExecutorService e_server = Executors.newWorkStealingPool(1);
        ThreadFactory factory = Thread.ofVirtual().scheduler(e_server).name("writer-", 0).factory();
        server.setExecutor(Executors.newFixedThreadPool(requests, factory));
        server.createContext("/hello", e -> {
            byte[] response = "Hello".getBytes("UTF-8");
            e.sendResponseHeaders(200, response.length);
            try (OutputStream out = e.getResponseBody()) {
                Thread.sleep(1000);
                out.write(response);
            } catch (Exception ecp) {
                System.out.println("### exception is " + ecp);
            }
        });

        // URL for hello service
        InetSocketAddress address = server.getAddress();
        URL url = new URL("http://" + address.getHostName() + ":" + address.getPort() + "/hello");

        // go
        server.start();
        try {
            ExecutorService e_client = Executors.newWorkStealingPool(1);
            factory = Thread.ofVirtual().scheduler(e_client).name("fetcher-", 0).factory();
            ExecutorService executor = Executors.newFixedThreadPool(requests, factory);
            for (int i = 1; i <= requests; i++) {
                executor.submit(() -> { 
                    try {
                        fetch(url);
                        requestsHandled.incrementAndGet();
                    } catch (Exception e) {
                        System.out.println("1 exception is " + e);
                    }
                });
            }

            while (requestsHandled.get() != requests) {
                System.out.println("handle request is " + requestsHandled.get());
                Thread.sleep(1000);
            }

            executor.shutdown();
        } finally {
            server.stop(1);
        }

        if (requestsHandled.get() < requests) {
            throw new RuntimeException(requestsHandled.get() + " handled, expected " + requests);
        }
    }

    private static String fetch(URL url) throws IOException {
        try (InputStream in = url.openConnection(Proxy.NO_PROXY).getInputStream()) {
            byte[] bytes = new byte[2000];
            in.read(bytes, 0, 2000);
            return new String(bytes, "UTF-8");
        } catch (Exception e) {
            System.out.println("2 exception is " + e);
            e.printStackTrace();
        }

        return null;
    }
}
