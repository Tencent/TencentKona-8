
/*
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
package org.example;

import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.results.format.ResultFormatType;
import org.openjdk.jmh.runner.Runner;
import org.openjdk.jmh.runner.RunnerException;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;

import java.util.concurrent.*;
import java.util.function.Predicate;


@Fork(3)
@Warmup(iterations = 20, time = 1)
@Measurement(iterations = 20, time = 1)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@BenchmarkMode(Mode.AverageTime)
@State(Scope.Thread)
@SuppressWarnings("preview")
public class Ring {

	public static class Worker<T> implements Runnable {
		private final Channel<T> source;
		private final Channel<T> sink;
		private final Predicate<T> finisher;

		public Worker(Channel<T> source, Channel<T> sink, Predicate<T> finisher) {
			this.source = source;
			this.sink = sink;
			this.finisher = finisher;
		}

		@Override
		public void run() {
			boolean endOfWork = false;
			do {
				T msg = source.receive();
				endOfWork = finisher.test(msg);
				sink.send(msg);
			} while (!endOfWork);
		}
	}

	@Param({"10"})
	int THREADS;

	@Param({"sq"})
	public String queue;

	@Param({"1", "4", "16", "64", "256"})
	public int stackDepth;

	@Param({"1", "4", "8"})
	public int stackFrame;

	@Param({"true", "false"})
	public boolean allocalot;

	@Param({"true", "false"})
	public boolean singleshot;

	@Setup
	@SuppressWarnings("unchecked")
	public void setup() {
		msg = 42;
		Channel<Integer>[] chans = new Channel[THREADS + 1];
		for (int i = 0; i < chans.length; i++) {
			chans[i] = getChannel();
		}
		head = chans[0];
		tail = chans[chans.length - 1];
		Predicate<Integer> finalCondition = singleshot ? (x -> true) : (x -> (x < 0));
		workers = new Worker[chans.length - 1];
		for (int i = 0; i < chans.length - 1; i++) {
			workers[i] = new Worker<>(chans[i], chans[i + 1], finalCondition);
		}
		if (!singleshot) {
			startAll();
		}
	}

	private void startAll() {
		for (Worker<Integer> w : workers) {
			Thread.ofVirtual().scheduler(Executors.newSingleThreadExecutor()).start(w);
		}
	}

	Worker<Integer>[] workers;
	Channel<Integer> head;
	Channel<Integer> tail;
	Integer msg;

	@TearDown
	public void tearDown() {
		if (!singleshot) {
			head.send(-1);
			tail.receive();
		}
	}

	@Benchmark
	public Object trip() {
		if (singleshot) {
			startAll();
		}
		head.send(msg);
		return tail.receive();
	}

	public static <T> BlockingQueue<T> getQueue(String queue) {
		switch (queue) {
			case "lbq":
				return new LinkedBlockingQueue<>();
			case "abq":
				return new ArrayBlockingQueue<>(1);
			case "sq":
				return new SynchronousQueue<>();
		}
		return null;
	}

	Channel<Integer> getChannel() {
		switch (stackFrame) {
			case 1:
				return new Channels.ChannelFixedStackR1<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
			case 2:
				return new Channels.ChannelFixedStackR2<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
			case 4:
				return new Channels.ChannelFixedStackR4<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
			case 8:
				return new Channels.ChannelFixedStackR8<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
			default:
				throw new RuntimeException("Illegal stack parameter value: " + stackFrame + " (allowed: 1,2,4,8)");
		}
	}


	public static void main(String[] args) throws RunnerException {
		Options opt = new OptionsBuilder()
				.include(Ring.class.getSimpleName())
				.result("res.json")
				.resultFormat(ResultFormatType.JSON)
				.build();

		new Runner(opt).run();
	}
}
