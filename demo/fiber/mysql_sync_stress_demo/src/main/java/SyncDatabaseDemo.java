/*
 *
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

import javax.print.attribute.standard.PresentationDirection;
import java.awt.image.DirectColorModel;
import java.sql.DriverManager;
import java.sql.*;
import java.sql.ResultSet;
import java.sql.SQLException;

import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

public class SyncDatabaseDemo {
    private static ExecutorService db_executor;
    private static ExecutorService e;

    private static int threadCount;
    private static int requestCount;
    private static int testOption;
    private static int statsInterval;

    private static final int useFiber = 0;
    private static final int useThreadDirect = 1;
    private static final int useThreadAndThreadPool = 2;
    private static final int useAsync = 3;

    public static String execQuery(String sql) throws InterruptedException, ExecutionException {
        String queryResult = "";
        try {
            ConnectionNode node;
            do {
                node = ConnectionPool.getConnection();
            } while (node == null);
            ResultSet rs = node.stm.executeQuery(sql);

            while (rs.next()) {
                int id = rs.getInt("id");
                String hello = rs.getString("hello");
                String response = rs.getString("response");

                queryResult += "id: " + id + " hello:" + hello + " response: "+ response + "\n";
            }

            rs.close();
            ConnectionPool.releaseConnection(node);
        } catch (Exception e) {
            e.printStackTrace();
        }

        return queryResult;
    }

    public static String submitQuery(String sql) throws InterruptedException, ExecutionException {
        CompletableFuture<String> future = new CompletableFuture<>();

        Runnable r = new Runnable() {
            @Override
            public void run() {
                try {
                    future.complete(execQuery(sql));
                } catch (Exception e) {

                }
            }
        };
        db_executor.execute(r);

        return future.get();
    }

    public static void testAsyncQuery() throws Exception {
        CountDownLatch startSignal = new CountDownLatch(1);
        CountDownLatch doneSignal = new CountDownLatch(requestCount);
        AtomicLong count = new AtomicLong();
        AtomicLong statsTimes = new AtomicLong();

        for (int i = 0; i < requestCount; i++) {
            // Execute async operation
            CompletableFuture<String> cf = CompletableFuture.supplyAsync(() -> {
                String result = null;
                try {
                    startSignal.await();
                    result = execQuery("select * from hello");
                } catch (Exception e) {
                }

                return result;
            }, e);

            // async operation is done, update statistics
            cf.thenAccept(result -> {
                long val = count.addAndGet(1);
                if ((val % statsInterval) == 0) {
                    long time = System.currentTimeMillis();
                    long prev = statsTimes.getAndSet(time);
                    System.out.println("interval " + val + " throughput " + statsInterval/((time - prev)/1000.0));
                }
                doneSignal.countDown();
            });
        }

        long before = System.currentTimeMillis();
        statsTimes.set(before);
        startSignal.countDown();
        doneSignal.await();

        long after = System.currentTimeMillis();
        long duration = (after - before);
        System.out.println("finish " + count.get() + " time " + duration + "ms throughput " + (count.get()/(duration/1000.0)));

        e.shutdown();
        e.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
    }

    public static void testSyncQuery() throws Exception {
        CountDownLatch startSignal = new CountDownLatch(1);
        CountDownLatch doneSignal = new CountDownLatch(requestCount);
        AtomicLong count = new AtomicLong();
        AtomicLong statsTimes = new AtomicLong();

        Runnable r = new Runnable() {
            @Override
            public void run() {
                try {
                    startSignal.await();
                    String sql = "select * from hello";
                    String result;
                    if (testOption == useFiber || testOption == useThreadAndThreadPool) {
                        // submit query to an independent thread pool;
                        result = submitQuery(sql);
                    } else {
                        // execute query direct(use current thread)
                        result = execQuery(sql);
                    }
                    //System.out.println("execute sql result is " + result);

                    long val = count.addAndGet(1);
                    if ((val % statsInterval) == 0) {
                        long time = System.currentTimeMillis();
                        long prev = statsTimes.getAndSet(time);
                        System.out.println("interval " + val + " throughput " + statsInterval/((time - prev)/1000.0));
                    }
                    doneSignal.countDown();
                } catch (Exception e) {

                }
            }
        };

        for (int i = 0; i < requestCount; i++) {
            e.execute(r);
        }

        long before = System.currentTimeMillis();
        statsTimes.set(before);
        startSignal.countDown();
        doneSignal.await();

        long after = System.currentTimeMillis();
        long duration = (after - before);
        System.out.println("finish " + count.get() + " time " + duration + "ms throughput " + (count.get()/(duration/1000.0)));

        e.shutdown();
        e.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);

        if (testOption == useFiber || testOption == useThreadAndThreadPool) {
            db_executor.shutdown();
        }
    }

    public static void initExecutor() {
        ThreadFactory factory;
        if (testOption == useFiber) {
            factory = Thread.ofVirtual().factory();
        } else {
            factory = Thread.ofPlatform().factory();
        }

        if (testOption == useAsync) {
            // thread count is equal to available processors when useAsync
            threadCount = Runtime.getRuntime().availableProcessors();
            e = Executors.newWorkStealingPool(threadCount);
        } else {
            e = Executors.newFixedThreadPool(threadCount, factory);
        }

        if (testOption == useFiber || testOption == useThreadAndThreadPool) {
            // an independent thread pool which has 16 threads
            db_executor = Executors.newFixedThreadPool(Runtime.getRuntime().availableProcessors() * 2);
        }
    }

    public static void main(String[] args) throws Exception {
        threadCount = Integer.parseInt(args[0]);
        requestCount = Integer.parseInt(args[1]);
        testOption = Integer.parseInt(args[2]);
        statsInterval = requestCount / 10;

        initExecutor();

        ConnectionPool.initConnectionPool();
        if (testOption == useAsync) {
            testAsyncQuery();
        } else {
            testSyncQuery();
        }
        ConnectionPool.closeConnection();
    }
}
