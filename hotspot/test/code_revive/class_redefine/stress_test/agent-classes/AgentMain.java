/*
 *
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

import java.io.*;
import java.lang.instrument.*;
import java.security.*;

public class AgentMain {
    private static void redefineClass(Instrumentation inst, String classFile, Class targetClass) {
        try {
            // read redefined class
            File file = new File(classFile);
            int fileSize = (int)file.length();
            byte[] buffer = new byte[fileSize];

            FileInputStream fis = new FileInputStream(file);
            fis.read(buffer, 0, fileSize);
            fis.close();

            ClassDefinition classDef = new ClassDefinition(targetClass, buffer);
            inst.redefineClasses(classDef);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void agentmain(String agentArgs, Instrumentation inst) throws Exception {
        boolean found = false;
        Class targetClass = null;
        Class classes[] = inst.getAllLoadedClasses();
        for (int i = 0; i < classes.length; i++) {
            if (classes[i].getName().equals("TimedExecutor")) {
                found = true;
                targetClass = classes[i];
            }
        }
        if (!found) {
            System.out.println("No TimedExecutor class found, agent exit");
            return;
        }

        redefineClass(inst, "redefined-classes/TimedExecutor.class", targetClass);
    }
}
