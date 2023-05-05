/*
 * Copyright (c) 2023, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
package jdk.jfr.event.runtime;
import static jdk.test.lib.Asserts.assertEquals;
import java.util.List;
import java.lang.reflect.Field;
import sun.misc.Unsafe;
import jdk.jfr.Recording;
import jdk.jfr.consumer.RecordedEvent;
import jdk.test.lib.jfr.EventNames;
import jdk.test.lib.jfr.Events;
/**
 * @test
 * @key jfr

 * @library /lib /
 * @run main/othervm jdk.jfr.event.runtime.TestNativeMemEvent
 */
public class TestNativeMemEvent {
    
    private final static String ALLOC_EVENT_NAME = EventNames.JavaNativeAllocation;
    private final static String FREE_EVENT_NAME = EventNames.JavaNativeFree;
    private final static String REMALLOC_EVENT_NAME = EventNames.JavaNativeReallocate;
    public static void main(String[] args) throws Throwable {
        Recording recording = new Recording();
        recording.enable(ALLOC_EVENT_NAME);
        recording.enable(FREE_EVENT_NAME);
        recording.enable(REMALLOC_EVENT_NAME);
        recording.start();
        long[] addrs = unsafeMemTest();
        recording.stop();
        List<RecordedEvent> events = Events.fromRecording(recording);
        Events.hasEvents(events);
        for (RecordedEvent event : events) {
            System.out.println("Event:" + event);
            if (ALLOC_EVENT_NAME.equals(event.getEventType().getName())) {
                long addr = Events.assertField(event, "addr").atLeast(1L).getValue();
                assertEquals(addr,addrs[0]);
                long allocationSize = Events.assertField(event, "allocationSize").atLeast(1L).getValue();
                assertEquals(allocationSize,80L);
            } else if(FREE_EVENT_NAME.equals(event.getEventType().getName())){
                long addr = Events.assertField(event, "addr").atLeast(1L).getValue();
                assertEquals(addr,addrs[1]);;
            } else if(REMALLOC_EVENT_NAME.equals(event.getEventType().getName())){
                long freeAddr = Events.assertField(event, "freeAddr").atLeast(1L).getValue();
                assertEquals(freeAddr,addrs[0]);
                long allocAddr = Events.assertField(event, "allocAddr").atLeast(1L).getValue();
                assertEquals(allocAddr,addrs[1]);
                long allocationSize = Events.assertField(event, "allocationSize").atLeast(1L).getValue();
                assertEquals(allocationSize,1000L);
            }
        }
    }
    public static long[] unsafeMemTest() throws Exception {
        long[] addrs = new long[2];
        Field field = Unsafe.class.getDeclaredField("theUnsafe");
        field.setAccessible(true);
        Unsafe unsafe = (Unsafe) field.get(null);
        long addr = unsafe.allocateMemory(80);
        addrs[0] = addr;
        long reallocAddr = unsafe.reallocateMemory(addr, 1000);
        addrs[1] = reallocAddr;
        unsafe.freeMemory(reallocAddr);
        return addrs;
    }
}