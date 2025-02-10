/*
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.JDKToolFinder;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import sun.misc.VM;

public class LimitDirectMemoryTestBasic {
    public static void main(String[] args) throws Exception {
        int pid = ProcessTools.getProcessId();
        String new_size = args[0];
        int alloc_size = Integer.parseInt(args[1]);

        long max_direct_memory = VM.maxDirectMemory();
        System.out.println("initial max direct memory = " + max_direct_memory);
        resize(pid, new_size);
        max_direct_memory = VM.maxDirectMemory();
        System.out.println("after resize, max direct memory = " + max_direct_memory);

        try {
            // alloc direct memory
            int single_alloc_size = 1 * 1024 * 1024;
            ByteBuffer[] buffers = new ByteBuffer[alloc_size];
            for (int i = 0; i < alloc_size; i++) {
                buffers[i] = ByteBuffer.allocateDirect(single_alloc_size);
            }
        } catch (OutOfMemoryError e) {
            System.out.println(e);
            throw e;
        }
        System.out.println("allocation finish!");   
    }

    static void resize(int pid, String new_size) {
        try {
            Process process = Runtime.getRuntime().exec(JDKToolFinder.getJDKTool("jcmd") + " " + pid + " GC.elastic_max_direct_memory " + new_size);
            BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String line;
            while ((line = reader.readLine()) != null) {
                System.out.println(line);
            }
            reader.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}