/*
 *
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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
/*
 * @test
 * @summary Test whether class identity is correctly generated.
 * @library /testlibrary
 * @compile test-classes/CreateTypeArray.java
 * @run main/othervm ClassIdentity
 */

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.io.File;
import java.io.FileWriter;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.ArrayList;

public class ClassIdentity {
    private static Pattern p = Pattern.compile("CodeRevive Identity for class (.*) is (\\w*)", Pattern.MULTILINE);

    private static String generateInterface(String className, String superClass, String methodName,
                                            boolean needDefault) throws Exception {
        StringBuilder sb = new StringBuilder();
        sb.append("public interface " + className);
        if (superClass != null) {
            sb.append(" extends " + superClass);
        }
        sb.append(" {");
        if (needDefault) {
            sb.append("default void defaultMethod" + className + "() {");
            sb.append("    System.out.println(\"DefaultMethod\");");
            sb.append("}");
        }
        sb.append("int " + methodName + "();");
        sb.append("}");

        return sb.toString();
    }

    private static String generateClass(String className, String superClass, String interFace, String fieldName,
                                        String fieldValue, String methodName, boolean needMain) throws Exception {
        StringBuilder sb = new StringBuilder();
        sb.append("public class " + className + " ");
        if (superClass != null) {
            sb.append(" extends " + superClass);
        }
        if (interFace != null) {
            sb.append(" implements " + interFace);
        }
        sb.append("{");
        sb.append("    private int " + fieldName + " = " + fieldValue + ";");
        if (needMain) {
	    sb.append("    public static void main(String[] args) {");
            sb.append("        System.out.println(\"Identity Test\"\n);");
            sb.append("        int total = 0;");
            sb.append("       " + className + " foo = new " + className + "();");
            sb.append("        for (int i = 0; i < 2000; i++) {");
            sb.append("            total += foo." + methodName + "();");
            sb.append("        }");
            sb.append("        System.out.println(total);");
            sb.append("    }");
        }
        sb.append("    public int " + methodName + "() {");
        sb.append("        int total = 0;");
        sb.append("        for (int i = 0; i < 2000; i ++) {");
        sb.append("            " + className + "[][][] arr = new " + className + "[1][1][1];");
        sb.append("            total += arr.length;");
        sb.append("        }");
        sb.append("        return total;");
        sb.append("    }");
        sb.append("    public void run() {}");
        sb.append("}");

        return sb.toString();
    }

    private static void compileClass(String className, String superClass, String interFace,
                                     String fieldName, String fieldValue, String methodName,
                                     boolean isClass, boolean needMain, boolean needDefault) throws Exception {
        String code;
        if (isClass) {
            code = generateClass(className, superClass, interFace, fieldName, fieldValue, methodName, needMain);
        } else {
            code = generateInterface(className, superClass, methodName, needDefault);
        }

        File file = new File(className + ".java");
        FileWriter fw = new FileWriter(file);
        fw.write(code);
        fw.close();

        String[] cmd = { System.getProperty("test.jdk") + "/bin/javac", "-cp", ".", className + ".java" };
        Process process = Runtime.getRuntime().exec(cmd);
        if (process.waitFor() != 0) {
             throw new RuntimeException("Error in compile");
        }
    }

    private static ArrayList<String> getClassIdentity(String superClass, String interFace, String fieldName,
                                           String fieldValue, String methodName) throws Exception {
        compileClass("IdentityTest", superClass, interFace, fieldName, fieldValue, methodName, true, true, false);
        String[] cmdLine = { "-XX:-Inline", "-cp", ".", "-XX:CodeReviveOptions=save,file=test.csa,log=save=info", "IdentityTest" };
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, cmdLine);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
	ArrayList<String> ids = new ArrayList<String>();

        Matcher m = p.matcher(output.getOutput());
        while (m.find()) {
            if (m.group(1).equals("IdentityTest") || m.group(1).equals("[LIdentityTest;") ||
                m.group(1).equals("[[LIdentityTest;") || m.group(1).equals("[[[LIdentityTest;")) {
                ids.add(m.group(2));
            }
        }

        if (ids.size() != 4) {
            throw new RuntimeException("Can't find all identities for class IdentityClass");
        }
        return ids;
    }

    private static void getTypeArrayIdentity(ArrayList<String> ids) throws Exception {
        String[] cmdLine = { "-XX:CodeReviveOptions=save,file=test.csa,log=save=info", "CreateTypeArray" };
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, cmdLine);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());

	int count = 0;
        Matcher m = p.matcher(output.getOutput());
        while (m.find()) {
            String className = m.group(1);
            if (className.length() <= 4 && className.charAt(0) == '[') {
                count++;
                ids.add(m.group(2));
            }
        }

        if (count != 16) {
            throw new RuntimeException("Can't find all identities for TypeArray classes");
        }
    }

    private static void compare(boolean same, String s1, String s2) throws Exception {
        if (same && !s1.equals(s2)) {
            throw new RuntimeException("should be same: " + s1 + " " + s2);
        }
        if (!same && s1.equals(s2)) {
            throw new RuntimeException("should be not same: " + s1 + " " + s2);
        }
    }

    public static void main(String[] args) throws Exception {
        ArrayList<String> id1 = getClassIdentity(null, null, "foo", "1", "test");
        ArrayList<String> id2 = getClassIdentity(null, null, "foo", "1", "test");
        for (int i = 0; i < id1.size(); i++) {
            compare(true, id1.get(i), id2.get(i));
        }

        ArrayList<String> ids = new ArrayList<String>();
        ids.addAll(id1);
        ids.addAll(getClassIdentity("Thread", null, "foo", "1", "test"));
        ids.addAll(getClassIdentity(null, "Runnable", "foo", "1", "test"));
        ids.addAll(getClassIdentity(null, null, "bar", "1", "test"));
        ids.addAll(getClassIdentity(null, null, "foo", "12345", "test"));
        ids.addAll(getClassIdentity(null, null, "foo", "1", "bar"));

        // build super class
        compileClass("IdentitySuperSuper", null, null, "superSuperFoo", "2", "superSuperTest", true, false, false);
        compileClass("IdentitySuper", "IdentitySuperSuper", null, "superFoo", "2", "superTest", true, false, false);
        ids.addAll(getClassIdentity("IdentitySuper", null, "foo", "1", "test"));
        // change super super
        compileClass("IdentitySuperSuper", null, null, "superSuperFoo", "222", "superSuperTest", true, false, false);
        ids.addAll(getClassIdentity("IdentitySuper", null, "foo", "1", "test"));
        // change super
        compileClass("IdentitySuper", "IdentitySuperSuper", null, "superFoo", "222", "superTest", true, false, false);
        ids.addAll(getClassIdentity("IdentitySuper", null, "foo", "1", "test"));

        // build interface
        compileClass("IdentitySuperInterface", null, null, null, null, "test", false, false, false);
        compileClass("IdentityInterface", "IdentitySuperInterface", null, null, null, "test", false, false, false);
        ids.addAll(getClassIdentity(null, "IdentityInterface", "foo", "1", "test"));

        // change super interface
        compileClass("IdentitySuperInterface", null, null, null, null, "test", false, false, true);
        ids.addAll(getClassIdentity(null, "IdentityInterface", "foo", "1", "test"));
        // change interface
        compileClass("IdentityInterface", "IdentitySuperInterface", null, null, null, "test", false, false, true);
        ids.addAll(getClassIdentity(null, "IdentityInterface", "foo", "1", "test"));

        getTypeArrayIdentity(ids);

        // all generated identities should be different.
        for (int i = 0; i < ids.size(); i++) {
            System.out.println("Generated Identity: " + ids.get(i));
            for (int j = i + 1; j < ids.size(); j++) {
                compare(false, ids.get(i), ids.get(j));
            }
        }
    }
}

