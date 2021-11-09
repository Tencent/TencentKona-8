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

#ifndef SHARE_VM_RUNTIME_LOGASYNCWRITER_HPP
#define SHARE_VM_RUNTIME_LOGASYNCWRITER_HPP

#include "memory/resourceArea.hpp"
#include "runtime/semaphore.hpp"
#include "runtime/thread.hpp"
#include "utilities/linkedlist.hpp"

template <typename E, MEMFLAGS F>
class LinkedListDeque : private LinkedListImpl<E, ResourceObj::C_HEAP, F> {
 private:
  LinkedListNode<E>* _tail;
  size_t _size;

 public:
  LinkedListDeque() : _tail(NULL), _size(0) {}
  void push_back(const E& e) {
    if (!_tail) {
      _tail = this->add(e);
    } else {
      _tail = this->insert_after(e, _tail);
    }

    ++_size;
  }

  // pop all elements to logs.
  void pop_all(LinkedList<E>* logs) {
    logs->move(static_cast<LinkedList<E>* >(this));
    _tail = NULL;
    _size = 0;
  }

  void pop_all(LinkedListDeque<E, F>* logs) {
    logs->_size = _size;
    logs->_tail = _tail;
    pop_all(static_cast<LinkedList<E>* >(logs));
  }

  void pop_front() {
    LinkedListNode<E>* h = this->unlink_head();
    if (h == _tail) {
      _tail = NULL;
    }

    if (h != NULL) {
      --_size;
      this->delete_node(h);
    }
  }

  size_t size() const { return _size; }

  const E* front() const {
    return this->_head == NULL ? NULL : this->_head->peek();
  }

  const E* back() const {
    return _tail == NULL ? NULL : _tail->peek();
  }

  LinkedListNode<E>* head() const {
    return this->_head;
  }
};

class AsyncLogMessage {
  char* _message;

public:
  AsyncLogMessage(char* msg):
    _message(msg) {}

  // placeholder for LinkedListImpl.
  bool equals(const AsyncLogMessage& o) const { return false; }

  char* message() const { return _message; }
};

typedef LinkedListDeque<AsyncLogMessage, mtInternal> AsyncLogBuffer;

//  
// ASYNC LOGGING SUPPORT
//
// Summary:
// Async Logging is working on the basis of singleton AsyncLogWriter, which manages an intermediate buffer and a flushing thread.
//
// Interface:
//
// initialize() is called once when JVM is initialized. It creates and initializes the singleton instance of AsyncLogWriter.
// Once async logging is established, there's no way to turn it off.
//
// instance() is MT-safe and returns the pointer of the singleton instance if and only if async logging is enabled and has well
// initialized. Clients can use its return value to determine async logging is established or not.
//
// The basic operation of AsyncLogWriter is enqueue(). 2 overloading versions of it are provided to match LogOutput::write().
// They are both MT-safe and non-blocking. Derived classes of LogOutput can invoke the corresponding enqueue() in write() and
// return 0. AsyncLogWriter is responsible of copying neccessary data.
//
// The static member function flush() is designated to flush out all pending messages when JVM is terminating.
// In normal JVM termination, flush() is invoked in LogConfiguration::finalize(). flush() is MT-safe and can be invoked arbitrary
// times. It is no-op if async logging is not established.
//
class AsyncLogWriter : public NamedThread {
  class AsyncLogLocker;

  static AsyncLogWriter* _instance;

  // _lock(1) denotes a critional region.
  Semaphore _lock;
  // _sem is a semaphore whose value denotes how many messages have been enqueued.
  // It decreases in AsyncLogWriter::run()
  Semaphore _sem;
  // A lock of IO
  Semaphore _io_sem;

  volatile bool _initialized;
  AsyncLogBuffer _buffer;

  // The memory use of each AsyncLogMessage (payload) consists of itself and a variable-length c-str message.
  // A regular logging message is smaller than vwrite_buffer_size
  const size_t _buffer_max_size;

  AsyncLogWriter();
  void enqueue_locked(const AsyncLogMessage& msg);
  void write();
  virtual void run();

 public:
  void enqueue(const char* msg);

  static AsyncLogWriter* instance();
  static void initialize();
  static void flush();
};

#endif // SHARE_VM_RUNTIME_LOGASYNCWRITER_HPP
