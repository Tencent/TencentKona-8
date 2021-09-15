/*
 * Copyright Amazon.com Inc. or its affiliates. All Rights Reserved.
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
 *
 */

#include "precompiled.hpp"
#include "runtime/atomic.hpp"
#include "runtime/logAsyncWriter.hpp"
#include "utilities/ostream.hpp"

class AsyncLogWriter::AsyncLogLocker : public StackObj {
 public:
  AsyncLogLocker() {
    assert(_instance != NULL, "AsyncLogWriter::_lock is unavailable");
    _instance->_lock.wait();
  }

  ~AsyncLogLocker() {
    _instance->_lock.signal();
  }
};

void AsyncLogWriter::enqueue_locked(const AsyncLogMessage& msg) {
  if (_buffer.size() >= _buffer_max_size)  {
    // drop the enqueueing message.
    os::free(msg.message());
    return;
  }

  assert(_buffer.size() < _buffer_max_size, "_buffer is over-sized.");
  _buffer.push_back(msg);
  _sem.signal();
}

void AsyncLogWriter::enqueue(const char* msg) {
  AsyncLogMessage m(os::strdup(msg));

  { // critical area
    AsyncLogLocker locker;
    enqueue_locked(m);
  }
}

AsyncLogWriter::AsyncLogWriter()
  : NamedThread(), _initialized(false), _buffer_max_size(AsyncLogBufferSize),
    _lock(1), _sem(0), _io_sem(1) {
  if (os::create_thread(this, os::vm_thread)) {
    _initialized = true;
    set_name("AsyncLog Thread");
  } else {
    if (PrintAsyncGCLog) {
      tty->print_cr("AsyncLogging failed to create thread. Falling back to synchronous logging.");
    }
  }
  if (PrintAsyncGCLog) {
    tty->print_cr("The maximum entries of AsyncLogBuffer: " SIZE_FORMAT ", estimated memory use: " SIZE_FORMAT " bytes",
                  _buffer_max_size, AsyncLogBufferSize);
  }
}

void AsyncLogWriter::write() {
  // Use kind of copy-and-swap idiom here.
  // Empty 'logs' swaps the content with _buffer.
  // Along with logs destruction, all processed messages are deleted.
  //
  // The operation 'pop_all()' is done in O(1). All I/O jobs are then performed without
  // lock protection. This guarantees I/O jobs don't block logsites.
  AsyncLogBuffer logs;
  bool own_io = false;

  { // critical region
    AsyncLogLocker locker;

    _buffer.pop_all(&logs);
    own_io = _io_sem.trywait();
  }

  LinkedListIterator<AsyncLogMessage> it(logs.head());

  if (!own_io) {
    _io_sem.wait();
  }

  while (!it.is_empty()) {
    AsyncLogMessage* e = it.next();
    if (e != NULL) {
      char* msg = e->message();

      if (msg != NULL) {
        ((gcLogFileStream*)gclog_or_tty)->write_blocking(msg, strlen(msg));
        os::free(msg);
      }
    }
  }
  _io_sem.signal();
}

void AsyncLogWriter::run() {
  while (true) {
    // The value of a semphore cannot be negative. Therefore, the current thread falls asleep
    // when its value is zero. It will be waken up when new messages are enqueued.
    _sem.wait();
    write();
  }
}

AsyncLogWriter* AsyncLogWriter::_instance = NULL;

void AsyncLogWriter::initialize() {
  if (!UseAsyncGCLog) return;

  assert(_instance == NULL, "initialize() should only be invoked once.");

  AsyncLogWriter* self = new AsyncLogWriter();
  if (self->_initialized) {
    OrderAccess::release_store_ptr(&AsyncLogWriter::_instance, self);
    os::start_thread(self);
    if (PrintAsyncGCLog) {
      tty->print_cr("Async logging thread started.");
    }
  }
}

AsyncLogWriter* AsyncLogWriter::instance() {
  return _instance;
}

// write() acquires and releases _io_sem even _buffer is empty.
// This guarantees all logging I/O of dequeued messages are done when it returns.
void AsyncLogWriter::flush() {
  if (_instance != NULL) {
    _instance->write();
  }
}
