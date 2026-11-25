/*
 * Copyright (C) 2026, Tencent. All rights reserved.
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

package jdk.jfr.event.oldobject;

import java.lang.management.ManagementFactory;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import jdk.jfr.Recording;
import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordedObject;
import jdk.jfr.internal.test.WhiteBox;
import jdk.test.lib.jfr.EventNames;
import jdk.test.lib.jfr.Events;

/**
 * @test
 * @bug 8284549
 * @key jfr
 * @summary The FieldTable used while emitting old object chains must release
 *          its native hash table (NMT category Tracing) when the writer is done.
 *
 * @library /test/lib /
 *
 * @run main/othervm -Xint -XX:TLABSize=2k -XX:NativeMemoryTracking=summary
 *                   jdk.jfr.event.oldobject.TestFieldTableLeak
 */
public class TestFieldTableLeak {

    private static final String FIELD_NAME = "testField";
    private static final int WARMUP_ITERATIONS = 20;
    private static final int ITERATIONS = 100;
    private static final int OBJECT_COUNT = 50;

    // Every dump of the old objects with a path to the GC roots goes through
    // a new ObjectSampleWriter. The reference chain starts from this static
    // field, so the writer has to create a FieldTable.
    public static final Object[] testField = new Object[OBJECT_COUNT];

    public static void main(String[] args) throws Exception {
        WhiteBox.setWriteAllObjectSamples(true);

        for (int i = 0; i < WARMUP_ITERATIONS; i++) {
            emitOldObjects();
        }
        long before = tracingMallocKB();

        for (int i = 0; i < ITERATIONS; i++) {
            emitOldObjects();
        }
        long after = tracingMallocKB();

        long growth = after - before;
        System.out.println("NMT Tracing malloc: before=" + before + "KB, after=" + after
                           + "KB, growth=" + growth + "KB");

        // Without the fix, each iteration leaks the hash table buckets and
        // entries of one FieldTable, so the growth is proportional to ITERATIONS.
        // Allow for some noise from other JFR activities.
        long limit = ITERATIONS / 4;
        if (growth > limit) {
            throw new RuntimeException("Native memory of NMT category Tracing grew by "
                                       + growth + "KB, limit is " + limit + "KB");
        }
    }

    // Emits the old objects with their reference chains and checks that a chain
    // containing the field 'testField' was written. Otherwise no FieldTable
    // would have been created, and the memory check would pass vacuously.
    private static void emitOldObjects() throws Exception {
        // Dead samples left by previous iterations must be cleaned up first,
        // otherwise most of the following iterations record no samples at all.
        System.gc();
        try (Recording recording = new Recording()) {
            recording.enable(EventNames.OldObjectSample)
                     .withoutStackTrace()
                     .with("cutoff", "infinity");
            recording.start();
            for (int i = 0; i < testField.length; i++) {
                // Allocate array to trigger sampling code path for interpreter / c1
                testField[i] = new Object[10_000];
            }
            recording.stop();

            List<RecordedEvent> events = Events.fromRecording(recording);
            for (RecordedEvent e : events) {
                if (hasTestFieldReferrer(e)) {
                    return;
                }
            }
            throw new RuntimeException("No old object with a reference chain through field '"
                                       + FIELD_NAME + "' found, " + events.size() + " events");
        }
    }

    private static boolean hasTestFieldReferrer(RecordedEvent e) throws Exception {
        RecordedObject object = e.getValue("object");
        Set<Long> visited = new HashSet<>();
        while (object != null) {
            Long address = object.getValue("address");
            if (!visited.add(address)) {
                return false;
            }
            RecordedObject referrer = object.getValue("referrer");
            RecordedObject field = referrer != null ? referrer.getValue("field") : null;
            if (field != null && FIELD_NAME.equals(field.getValue("name"))) {
                return true;
            }
            object = referrer != null ? referrer.getValue("object") : null;
        }
        return false;
    }

    private static long tracingMallocKB() throws Exception {
        System.gc();
        MBeanServer server = ManagementFactory.getPlatformMBeanServer();
        ObjectName name = new ObjectName("com.sun.management:type=DiagnosticCommand");
        String output = (String) server.invoke(name, "vmNativeMemory",
                                               new Object[] { new String[] { "summary" } },
                                               new String[] { String[].class.getName() });
        Matcher m = Pattern.compile("Tracing \\(reserved=\\d+KB, committed=\\d+KB\\)\\s+\\(malloc=(\\d+)KB").matcher(output);
        if (!m.find()) {
            throw new RuntimeException("Cannot find Tracing in NMT output:\n" + output);
        }
        return Long.parseLong(m.group(1));
    }
}