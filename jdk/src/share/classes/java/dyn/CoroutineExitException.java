/*
 * Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

package java.dyn;

public class CoroutineExitException extends RuntimeException {
	private static final long serialVersionUID = -2651365020938997924L;

	public CoroutineExitException() {
	}

	public CoroutineExitException(String message, Throwable cause) {
		super(message, cause);
	}

	public CoroutineExitException(String message) {
		super(message);
	}

	public CoroutineExitException(Throwable cause) {
		super(cause);
	}
	
	
	static public CoroutineExitException searchException(Throwable e) {
		Throwable trace = e;
		if (trace instanceof CoroutineExitException) {
			return (CoroutineExitException) trace;
		}
		while (trace.getCause() != null) {
			if (trace.getCause() instanceof CoroutineExitException) {
				return (CoroutineExitException) trace.getCause();
			}
			trace = trace.getCause();
		}
		for (Throwable idx : trace.getSuppressed()) {
			if (idx instanceof CoroutineExitException) {
				return (CoroutineExitException) idx;
			}
		}
		return null;
	}
	
}
