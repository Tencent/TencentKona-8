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

import java.sql.*;

import java.util.concurrent.locks.*;

public class ConnectionPool {
    private static Lock lock = new ReentrantLock();

    static ConnectionNode head = null;
    static final int connectionCount = 16;

    public static ConnectionNode getConnection() {
        lock.lock();
        if (head != null) {
            ConnectionNode target = head;
            head = head.next;
            lock.unlock();
            return target;
        }

        lock.unlock();

        try {
            Thread.sleep(100);
        } catch (Exception e) {
        }

        return null;
    }

    public static void releaseConnection(ConnectionNode node) {
        addConnectionNode(node);
    }

    public static void addConnectionNode(ConnectionNode current) {
        lock.lock();

        current.next = head;
        head = current;
        lock.unlock();
    }

    public static void initConnectionPool() {
        try {
            for (int i = 0; i < connectionCount; i++) {
                ConnectionNode current = new ConnectionNode();
                addConnectionNode(current);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void closeConnection() {
        try {
            for (int i = 0; i < connectionCount; i++) {
                ConnectionNode node;
                do {
                    node = getConnection();
                } while (node == null);

                if (node.stm != null) {
                    node.stm.close();
                }

                if (node.con != null) {
                    node.con.close();
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
