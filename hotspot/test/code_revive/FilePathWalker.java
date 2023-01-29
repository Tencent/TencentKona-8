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
 * @summary Test if class FilePathWalker works properly.
 * @library /testlibrary
 * @run main/othervm FilePathWalker
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.io.File;

public class FilePathWalker {
    private static void createFile(String path) throws Exception {
        File file = new File(path);
        if (!file.getParentFile().exists()) {
            file.getParentFile().mkdirs();
        }
        file.createNewFile();
    }

    public static void main(String[] args) throws Exception {
        String[] expect_outputs = {
            "/1.csa", "/2.csa", "/Dir1/3.csa", "/Dir1/Dir2/Dir3/4.csa",
            "/Dir4/5.csa", "/Dir4/Dir5/6.csa", "No compatible CSA file after fingerprint check"
        };
        String pwd = System.getProperty("user.dir");
        String csa_path = pwd + "/csas";
        for (int i = 0; i < expect_outputs.length; i++) {
            createFile(csa_path + expect_outputs[i]);
        }

        Test(expect_outputs,
             "-XX:CodeReviveOptions=merge,disable_check_dir,log=merge=trace,file=a.csa,input_files=" + csa_path,
             "-version");
    }

    static void Test(String[] expect_outputs, String... command) throws Exception {
        // dump aot
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldHaveExitValue(1); // merge fail no valid csa file
        for (String s : expect_outputs) {
            output.shouldContain(s);
        }
    }
}
