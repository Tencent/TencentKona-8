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

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * This is a test utility class for common AOT test functionality.
 *
 * Various methods use (String ...) for passing VM options. Note that the order
 * of the VM options are important in certain cases. Many methods take arguments like
 *
 *    (String prefix[], String suffix[], String... opts)
 *
 * Note that the order of the VM options is:
 *
 *    prefix + opts + suffix
 */
public class TestCommon {
    private static final String JSA_FILE_PREFIX = System.getProperty("user.dir") +
        File.separator + "aot-";

    private static final SimpleDateFormat timeStampFormat =
        new SimpleDateFormat("HH'h'mm'm'ss's'SSS");

    private static String currentArchiveName;

    // Call this method to start new archive with new unique name
    public static void startNewArchiveName() {
        deletePriorArchives();
        if (currentArchiveName == null) {
          currentArchiveName = JSA_FILE_PREFIX + timeStampFormat.format(new Date()) + ".csa";
        }
    }

    // Call this method to get current archive name
    public static String getCurrentArchiveName() {
        return currentArchiveName;
    }

    public static void setCurrentArchiveName(String archiveName) {
        currentArchiveName = archiveName;
    }

    // Attempt to clean old archives to preserve space
    // Archives are large artifacts (20Mb or more), and much larger than
    // most other artifacts created in jtreg testing.
    // Therefore it is a good idea to clean the old archives when they are not needed.
    // In most cases the deletion attempt will succeed; on rare occasion the
    // delete operation will fail since the system or VM process still holds a handle
    // to the file; in such cases the File.delete() operation will silently fail, w/o
    // throwing an exception, thus allowing testing to continue.
    public static void deletePriorArchives() {
        File dir = new File(System.getProperty("user.dir"));
        String files[] = dir.list();
        for (String name : files) {
            if (name.startsWith("aot-") && name.endsWith(".csa")) {
                if (!(new File(dir, name)).delete())
                    System.out.println("deletePriorArchives(): delete failed for file " + name);
            }
        }
    }

    public static void preservePriorArchives() {
        File dir = new File(System.getProperty("user.dir"));
        String files[] = dir.list();
        for (String name : files) {
            if (name.startsWith("aot-") && name.endsWith(".csa")) {
                File newName = new File("PRESERVED-" + name);
                if (!(new File(dir, name)).renameTo(newName))
                    System.out.println("preservePriorArchives(): rename failed for file " + name);
            }
        }
    }


    // Create AppCDS archive using most common args - convenience method
    public static OutputAnalyzer createArchive(String appJar, String appClasses[], String codeReviveOptions,
                                               String... suffix) throws Exception {
        AOTOptions opts = (new AOTOptions()).setAppJar(appJar)
            .setAppClasses(appClasses);
        opts.addSuffix(suffix);
        opts.setCodeReviveOptions(codeReviveOptions);
        return createArchive(opts);
    }

    public static String[] makeCommandLineForAOT(String... args) throws Exception {
        return args;
    }

    // Create AOT archive using AOT options
    public static OutputAnalyzer createArchive(AOTOptions opts)
        throws Exception {

        ArrayList<String> cmd = new ArrayList<String>();

        startNewArchiveName();

        if (opts.appJar != null) {
            cmd.add("-cp");
            cmd.add(opts.appJar);
        }

        if (opts.archiveName == null)
            opts.archiveName = getCurrentArchiveName();

        if ( opts.codeReviveOption == null) {
            cmd.add("-XX:CodeReviveOptions=save,disable_check_dir,file=" + opts.archiveName);
        } else {
            cmd.add("-XX:CodeReviveOptions=save,disable_check_dir,file=" + opts.archiveName + "," + opts.codeReviveOption);
        }

        for (String s : opts.suffix) cmd.add(s);

        cmd.add(opts.appClasses[0]);

        String[] cmdLine = cmd.toArray(new String[cmd.size()]);
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, makeCommandLineForAOT(cmdLine));
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        return output;
    }

    public static OutputAnalyzer runWithArchive(String appJar, String appClasses[], String codeReviveOptions,
                                                String... suffix) throws Exception {
        AOTOptions opts = (new AOTOptions()).setAppJar(appJar)
            .setAppClasses(appClasses);
        opts.addSuffix(suffix);
        opts.setCodeReviveOptions(codeReviveOptions);
        return runWithArchive(opts);
    }

    public static OutputAnalyzer runWithArchive(String appClasses[], String codeReviveOptions,
                                                String... suffix) throws Exception {
        return runWithArchive(null, appClasses, codeReviveOptions, suffix);
    }


    // Execute JVM using AOT archive with specified AOTOptions
    public static OutputAnalyzer runWithArchive(AOTOptions opts)
        throws Exception {

        ArrayList<String> cmd = new ArrayList<String>();

        if (opts.appJar != null) {
            cmd.add("-cp");
            cmd.add(opts.appJar);
        }

        if (opts.archiveName == null)
            opts.archiveName = getCurrentArchiveName();

        if (opts.codeReviveOption == null) {
            cmd.add("-XX:CodeReviveOptions=restore,disable_check_dir,file=" + opts.archiveName);
        } else {
            cmd.add("-XX:CodeReviveOptions=restore,disable_check_dir,file=" + opts.archiveName + "," + opts.codeReviveOption);
        }

        cmd.add("-XX:+UnlockDiagnosticVMOptions");
        cmd.add("-XX:+PrintNMethods");

        for (String s : opts.suffix) cmd.add(s);

        cmd.add(opts.appClasses[0]);

        String[] cmdLine = cmd.toArray(new String[cmd.size()]);
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, makeCommandLineForAOT(cmdLine));
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        return output;
    }

    public static OutputAnalyzer mergeCSA(String appJar, String codeReviveOptions,
                                                String... suffix) throws Exception {
        AOTOptions opts = (new AOTOptions()).setAppJar(appJar);
        opts.addSuffix(suffix);
        opts.setCodeReviveOptions(codeReviveOptions);
        return mergeCSA(opts);
    }

    // Merge CSA with specified AOTOptions
    public static OutputAnalyzer mergeCSA(AOTOptions opts)
        throws Exception {

        ArrayList<String> cmd = new ArrayList<String>();

        if (opts.appJar != null) {
            cmd.add("-cp");
            cmd.add(opts.appJar);
        }

        if (opts.archiveName == null)
            opts.archiveName = getCurrentArchiveName();

        cmd.add("-XX:CodeReviveOptions=merge,disable_check_dir,file=" + opts.archiveName + "," + opts.codeReviveOption);

        for (String s : opts.suffix) cmd.add(s);

        String[] cmdLine = cmd.toArray(new String[cmd.size()]);
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true, makeCommandLineForAOT(cmdLine));
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        return output;
    }


    // A common operation: dump, then check results
    public static OutputAnalyzer testDump(String appJar, String appClasses[], String codeReviveOptions,
                                          String... suffix) throws Exception {
        OutputAnalyzer output = createArchive(appJar, appClasses, codeReviveOptions, suffix);
        return output;
    }

    public static OutputAnalyzer testDump(String appClasses[], String codeReviveOptions,
                                          String... suffix) throws Exception {
        OutputAnalyzer output = createArchive(null, appClasses, codeReviveOptions, suffix);
        return output;
    }


    public static OutputAnalyzer testRunWithAOT(String appJar, String appClasses[], String codeReviveOptions, String... args)
        throws Exception {
        OutputAnalyzer output = runWithArchive(appJar, appClasses, codeReviveOptions, args);
        return output;
    }

    public static OutputAnalyzer testRunWithAOT(String appClasses[], String codeReviveOptions, String... args)
        throws Exception {
        OutputAnalyzer output = runWithArchive(null, appClasses, codeReviveOptions, args);
        return output;
    }

    public static OutputAnalyzer testMergeCSA(String appJar, String codeReviveOptions, String... args)
        throws Exception {
        OutputAnalyzer output = mergeCSA(appJar, codeReviveOptions, args);
        return output;
    }


    public static boolean containAOTMethod(OutputAnalyzer output, String expectedMethod[]) {
        List<String> lines = output.asLines();
        for (String method : expectedMethod) {
            boolean found = false;
            for (String data : lines) {
                if (data.contains(method) && data.contains("Compiled method (c2 aot)")) {
                   found = true;
                   break;
                }
            }
            if (found == false) {
                throw new RuntimeException("'" + method + "' not load from AOT \n");
            }
        }
        return true;
    }
}
