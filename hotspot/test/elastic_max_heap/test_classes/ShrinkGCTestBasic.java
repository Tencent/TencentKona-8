/*
 * Copyright (C) 2023, Tencent. All rights reserved.
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

public class ShrinkGCTestBasic extends TestBase{
    static Object[] root_array;
    public static void main(String[] args) throws Exception {
        int pid = ProcessTools.getProcessId();
        String new_size = args[0];
        boolean young_gen_larger = Boolean.parseBoolean(args[1]);
        long size = 1024L * 1024L * 1024L;
        alloc_and_free(size, young_gen_larger);
        resize(pid, new_size);
    }

    static void alloc_and_free(long size, boolean young_gen_larger) {
        int root_len = (int)(size / 1024L);
        root_array = new Object[root_len];
        for (int i = 0; i < root_len; i++) {
            root_array[i] = new int[254];
            if (young_gen_larger) {
                root_array[i] = null;
            }
        }
        root_array = null; // release
    }
    static void resize(int pid, String new_size) {
        try {
            Process process = Runtime.getRuntime().exec(JDKToolFinder.getJDKTool("jcmd") + " " + pid + " GC.elastic_max_heap " + new_size);
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
