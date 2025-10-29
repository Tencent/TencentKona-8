/*
 * Copyright (C) 2020, 2023, Tencent. All rights reserved.
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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.management.ManagementFactory;
import java.util.ArrayList;
import java.util.Random;
import java.util.regex.Pattern;
import com.oracle.java.testlibrary.Asserts;

/**
 * @test TestFreeHeapPhysicalMemory
 * @key gc
 * @summary test free heap physical memory when FreeHeapPhysicalMemory is on
 * @requires (os.family == "linux")
 * @library /testlibrary
 * @run main/othervm -Xms100m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseConcMarkSweepGC TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseConcMarkSweepGC TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:+UseParallelOldGC TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:+UseParallelOldGC TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseG1GC TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseG1GC TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseConcMarkSweepGC -XX:PeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseConcMarkSweepGC -XX:PeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:PeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:PeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:+UseParallelOldGC -XX:PeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:+UseParallelOldGC -XX:PeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseG1GC -XX:PeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseG1GC -XX:PeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseConcMarkSweepGC -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseConcMarkSweepGC -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:+UseParallelOldGC -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:+UseParallelOldGC -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseG1GC -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn40m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseG1GC -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseConcMarkSweepGC -XX:PeriodicGCInterval=100 -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseConcMarkSweepGC -XX:PeriodicGCInterval=100 -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:PeriodicGCInterval=100 -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:PeriodicGCInterval=100 -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:+UseParallelOldGC -XX:PeriodicGCInterval=100 -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseParallelGC -XX:+UseParallelOldGC -XX:PeriodicGCInterval=100 -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms100m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseG1GC -XX:PeriodicGCInterval=100 -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 * @run main/othervm -Xms60m -Xmx100m -Xmn10m -XX:MetaspaceSize=10m -XX:SurvivorRatio=4 -XX:+FreeHeapPhysicalMemory -XX:+UseG1GC -XX:PeriodicGCInterval=100 -XX:ForcePeriodicGCInterval=100 TestFreeHeapPhysicalMemory
 */
public class TestFreeHeapPhysicalMemory {
	public static final int treeDepth = 18;
	public static final int longLivedTreeDepth = 16;
	public static ArrayList<Node> longLivedList =new ArrayList<Node>();
	public	static int count = 0;

	// Nodes used by a tree of a given size
	static int TreeSize(int i) {
		return ((1 << (i + 1)) - 1);
	}

	// Number of iterations to use for a given tree depth
	static int NumIters(int i) {
		return 2 * TreeSize(treeDepth) / TreeSize(i);
	}

	// Build tree bottom-up
	static Node MakeTree(int iDepth) {
                Node node = null;
		if (iDepth <= 0) {
			return new Node();
		} else {
			node = new Node(MakeTree(iDepth - 1), MakeTree(iDepth - 1));
                        if (++count % 500 == 0) {
                          longLivedList.add(node);
                        }
                        return node;
		}
	}
	private static int sum() {
		int res = 0;
		for (int i = 0; i < longLivedList.size(); i++) {
			res += longLivedList.get(i).i;
			res += longLivedList.get(i).j;
		}
		return res;
	}

	private static String currentProcessPid() {
		String name = ManagementFactory.getRuntimeMXBean().getName();
		String pid = name.split("@")[0];
		return pid;
	}

	private static long getVmRSS(String command) {
		Pattern pattern = Pattern.compile("[^0-9]");
		long res = 0;
		try {
			Process  process = Runtime.getRuntime().exec(command);
			BufferedReader input = new BufferedReader(new InputStreamReader(process.getInputStream()));
			String line = "";
			while ((line = input.readLine()) != null) {
				if (line.startsWith("VmRSS", 0)) {
				  res = Long.parseLong(pattern.matcher(line).replaceAll(""));
				  break;
                                }
			}
			input.close();
		} catch (IOException e) {
			e.printStackTrace();
		}

		return res;
	}

	public static void main(String[] args) throws InterruptedException {
		String pid = currentProcessPid();
		String command = "cat /proc/" + pid + "/status";
		Node root = MakeTree(20);
		int sum1 = sum();
		long before = getVmRSS(command);
                root = null;
		System.gc();
                Thread.sleep(500);
		long after = getVmRSS(command);
		int sum2 = sum();
                Asserts.assertEQ(sum1, sum2);
	}
}

class Node {
	public static Random r = new Random();
	Node left, right;
	int i, j;

	Node(Node l, Node r) {
		left = l;
		right = r;
		j = left.r.nextInt() % 11;
		i = left.r.nextInt() % 11;
	}
	Node() {}
}
