/*
 * Copyright (C) 2023, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. Tencent designates
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
 */

import java.io.*;
import java.lang.instrument.*;
import java.security.*;

class TestTransformer implements ClassFileTransformer {
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined, ProtectionDomain protectionDomain, byte[] classfileBuffer) {
        if (!className.equals("TestRedefine")) {
            return null;
        }

        byte[] newBuf = new byte[classfileBuffer.length];
        System.arraycopy(classfileBuffer, 0, newBuf, 0, classfileBuffer.length);

        return newBuf;
    }
}

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
        boolean found = false, found2 = false;
        Class targetClass = null, targetClass2 = null;
        Class objectClass = null;
        Class classes[] = inst.getAllLoadedClasses();
        for (int i = 0; i < classes.length; i++) {
            if (classes[i].getName().equals("TestRedefine")) {
                found = true;
                targetClass = classes[i];
            }
            if (classes[i].getName().equals("RedefineInterface")) {
                found2 = true;
                targetClass2 = classes[i];
            }
            if (classes[i].getName().equals("java.lang.Object")) {
                objectClass = classes[i];
            }
        }
        if (!found || !found2) {
            System.out.println("No TestRedefine or RedefineInterface class found, agent exit");
            return;
        }
        if (objectClass == null) {
            System.out.println("No java.lang.Object is found, agent exit.");
            return;
        }

        inst.addTransformer(new TestTransformer(), true);
        inst.retransformClasses(targetClass);
        inst.retransformClasses(objectClass);

        redefineClass(inst, "redefined-classes/RedefineInterface.class", targetClass2);
    }
}
