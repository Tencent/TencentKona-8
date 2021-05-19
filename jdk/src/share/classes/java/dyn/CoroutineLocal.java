/*
 * Copyright (c) 1997, 2012, Oracle and/or its affiliates. All rights reserved.
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

import java.lang.Object;
import java.lang.Thread;
import java.lang.UnsupportedOperationException;
import java.lang.ref.*;
import java.util.concurrent.atomic.AtomicInteger;

@SuppressWarnings({ "rawtypes", "unchecked" })
public class CoroutineLocal<T> {
	private final int coroutineLocalHashCode = nextHashCode();
	private static AtomicInteger nextHashCode = new AtomicInteger();
	private static final int HASH_INCREMENT = 0x61c88647;

	private static int nextHashCode() {
		return nextHashCode.getAndAdd(HASH_INCREMENT);
	}

	protected T initialValue() {
		return null;
	}

	public CoroutineLocal() {
	}

	public T get() {
		assert Thread.currentThread().getCoroutineSupport() != null;
		CoroutineBase t = Thread.currentThread().getCoroutineSupport().getCurrent();
		CoroutineLocalMap map = getMap(t);
		if (map != null) {
			CoroutineLocalMap.Entry e = map.getEntry(this);
			if (e != null)
				return (T) e.value;
		}
		return setInitialValue();
	}

	/**
	 * Variant of set() to establish initialValue. Used instead of set() in case user has overridden the set() method.
	 * 
	 * @return the initial value
	 */
	private T setInitialValue() {
		T value = initialValue();
		assert Thread.currentThread().getCoroutineSupport() != null;
		CoroutineBase t = Thread.currentThread().getCoroutineSupport().getCurrent();
		CoroutineLocalMap map = getMap(t);
		if (map != null)
			map.set(this, value);
		else
			createMap(t, value);
		return value;
	}

	/**
	 * Sets the current thread's copy of this thread-local variable to the specified value. Most subclasses will have no need to override
	 * this method, relying solely on the {@link #initialValue} method to set the values of thread-locals.
	 * 
	 * @param value the value to be stored in the current thread's copy of this thread-local.
	 */
	public void set(T value) {
		assert Thread.currentThread().getCoroutineSupport() != null;
		CoroutineBase t = Thread.currentThread().getCoroutineSupport().getCurrent();
		CoroutineLocalMap map = getMap(t);
		if (map != null)
			map.set(this, value);
		else
			createMap(t, value);
	}

	/**
	 * Removes the current thread's value for this thread-local variable. If this thread-local variable is subsequently {@linkplain #get
	 * read} by the current thread, its value will be reinitialized by invoking its {@link #initialValue} method, unless its value is
	 * {@linkplain #set set} by the current thread in the interim. This may result in multiple invocations of the <tt>initialValue</tt>
	 * method in the current thread.
	 * 
	 * @since 1.5
	 */
	public void remove() {
		assert Thread.currentThread().getCoroutineSupport() != null;
		CoroutineLocalMap m = getMap(Thread.currentThread().getCoroutineSupport().getCurrent());
		if (m != null)
			m.remove(this);
	}

	CoroutineLocalMap getMap(CoroutineBase t) {
		return t.coroutineLocals;
	}

	/**
	 * Create the map associated with a ThreadLocal. Overridden in InheritableThreadLocal.
	 * 
	 * @param t the current thread
	 * @param firstValue value for the initial entry of the map
	 * @param map the map to store.
	 */
	void createMap(CoroutineBase t, T firstValue) {
		t.coroutineLocals = new CoroutineLocalMap(this, firstValue);
	}

	/**
	 * Factory method to create map of inherited thread locals. Designed to be called only from Thread constructor.
	 * 
	 * @param parentMap the map associated with parent thread
	 * @return a map containing the parent's inheritable bindings
	 */
	static CoroutineLocalMap createInheritedMap(CoroutineLocalMap parentMap) {
		return new CoroutineLocalMap(parentMap);
	}

	/**
	 * Method childValue is visibly defined in subclass InheritableThreadLocal, but is internally defined here for the sake of providing
	 * createInheritedMap factory method without needing to subclass the map class in InheritableThreadLocal. This technique is preferable
	 * to the alternative of embedding instanceof tests in methods.
	 */
	T childValue(T parentValue) {
		throw new UnsupportedOperationException();
	}

	/**
	 * ThreadLocalMap is a customized hash map suitable only for maintaining thread local values. No operations are exported outside of the
	 * ThreadLocal class. The class is package private to allow declaration of fields in class Thread. To help deal with very large and
	 * long-lived usages, the hash table entries use WeakReferences for keys. However, since reference queues are not used, stale entries
	 * are guaranteed to be removed only when the table starts running out of space.
	 */
	static class CoroutineLocalMap {

		/**
		 * The entries in this hash map extend WeakReference, using its main ref field as the key (which is always a ThreadLocal object).
		 * Note that null keys (i.e. entry.get() == null) mean that the key is no longer referenced, so the entry can be expunged from
		 * table. Such entries are referred to as "stale entries" in the code that follows.
		 */
		static class Entry extends WeakReference<CoroutineLocal> {
			/** The value associated with this ThreadLocal. */
			Object value;

			Entry(CoroutineLocal k, Object v) {
				super(k);
				value = v;
			}
		}

		/**
		 * The initial capacity -- MUST be a power of two.
		 */
		private static final int INITIAL_CAPACITY = 16;

		/**
		 * The table, resized as necessary. table.length MUST always be a power of two.
		 */
		private Entry[] table;

		/**
		 * The number of entries in the table.
		 */
		private int size = 0;

		/**
		 * The next size value at which to resize.
		 */
		private int threshold; // Default to 0

		/**
		 * Set the resize threshold to maintain at worst a 2/3 load factor.
		 */
		private void setThreshold(int len) {
			threshold = len * 2 / 3;
		}

		/**
		 * Increment i modulo len.
		 */
		private static int nextIndex(int i, int len) {
			return ((i + 1 < len) ? i + 1 : 0);
		}

		/**
		 * Decrement i modulo len.
		 */
		private static int prevIndex(int i, int len) {
			return ((i - 1 >= 0) ? i - 1 : len - 1);
		}

		/**
		 * Construct a new map initially containing (firstKey, firstValue). ThreadLocalMaps are constructed lazily, so we only create one
		 * when we have at least one entry to put in it.
		 */
		CoroutineLocalMap(CoroutineLocal firstKey, Object firstValue) {
			table = new Entry[INITIAL_CAPACITY];
			int i = firstKey.coroutineLocalHashCode & (INITIAL_CAPACITY - 1);
			table[i] = new Entry(firstKey, firstValue);
			size = 1;
			setThreshold(INITIAL_CAPACITY);
		}

		/**
		 * Construct a new map including all Inheritable ThreadLocals from given parent map. Called only by createInheritedMap.
		 * 
		 * @param parentMap the map associated with parent thread.
		 */
		private CoroutineLocalMap(CoroutineLocalMap parentMap) {
			Entry[] parentTable = parentMap.table;
			int len = parentTable.length;
			setThreshold(len);
			table = new Entry[len];

			for (int j = 0; j < len; j++) {
				Entry e = parentTable[j];
				if (e != null) {
					CoroutineLocal key = e.get();
					if (key != null) {
						Object value = key.childValue(e.value);
						Entry c = new Entry(key, value);
						int h = key.coroutineLocalHashCode & (len - 1);
						while (table[h] != null)
							h = nextIndex(h, len);
						table[h] = c;
						size++;
					}
				}
			}
		}

		/**
		 * Get the entry associated with key. This method itself handles only the fast path: a direct hit of existing key. It otherwise
		 * relays to getEntryAfterMiss. This is designed to maximize performance for direct hits, in part by making this method readily
		 * inlinable.
		 * 
		 * @param key the thread local object
		 * @return the entry associated with key, or null if no such
		 */
		private Entry getEntry(CoroutineLocal key) {
			int i = key.coroutineLocalHashCode & (table.length - 1);
			Entry e = table[i];
			if (e != null && e.get() == key)
				return e;
			else
				return getEntryAfterMiss(key, i, e);
		}

		/**
		 * Version of getEntry method for use when key is not found in its direct hash slot.
		 * 
		 * @param key the thread local object
		 * @param i the table index for key's hash code
		 * @param e the entry at table[i]
		 * @return the entry associated with key, or null if no such
		 */
		private Entry getEntryAfterMiss(CoroutineLocal key, int i, Entry e) {
			Entry[] tab = table;
			int len = tab.length;

			while (e != null) {
				CoroutineLocal k = e.get();
				if (k == key)
					return e;
				if (k == null)
					expungeStaleEntry(i);
				else
					i = nextIndex(i, len);
				e = tab[i];
			}
			return null;
		}

		/**
		 * Set the value associated with key.
		 * 
		 * @param key the thread local object
		 * @param value the value to be set
		 */
		private void set(CoroutineLocal key, Object value) {

			// We don't use a fast path as with get() because it is at
			// least as common to use set() to create new entries as
			// it is to replace existing ones, in which case, a fast
			// path would fail more often than not.

			Entry[] tab = table;
			int len = tab.length;
			int i = key.coroutineLocalHashCode & (len - 1);

			for (Entry e = tab[i]; e != null; e = tab[i = nextIndex(i, len)]) {
				CoroutineLocal k = e.get();

				if (k == key) {
					e.value = value;
					return;
				}

				if (k == null) {
					replaceStaleEntry(key, value, i);
					return;
				}
			}

			tab[i] = new Entry(key, value);
			int sz = ++size;
			if (!cleanSomeSlots(i, sz) && sz >= threshold)
				rehash();
		}

		/**
		 * Remove the entry for key.
		 */
		private void remove(CoroutineLocal key) {
			Entry[] tab = table;
			int len = tab.length;
			int i = key.coroutineLocalHashCode & (len - 1);
			for (Entry e = tab[i]; e != null; e = tab[i = nextIndex(i, len)]) {
				if (e.get() == key) {
					e.clear();
					expungeStaleEntry(i);
					return;
				}
			}
		}

		/**
		 * Replace a stale entry encountered during a set operation with an entry for the specified key. The value passed in the value
		 * parameter is stored in the entry, whether or not an entry already exists for the specified key.
		 * 
		 * As a side effect, this method expunges all stale entries in the "run" containing the stale entry. (A run is a sequence of entries
		 * between two null slots.)
		 * 
		 * @param key the key
		 * @param value the value to be associated with key
		 * @param staleSlot index of the first stale entry encountered while searching for key.
		 */
		private void replaceStaleEntry(CoroutineLocal key, Object value, int staleSlot) {
			Entry[] tab = table;
			int len = tab.length;
			Entry e;

			// Back up to check for prior stale entry in current run.
			// We clean out whole runs at a time to avoid continual
			// incremental rehashing due to garbage collector freeing
			// up refs in bunches (i.e., whenever the collector runs).
			int slotToExpunge = staleSlot;
			for (int i = prevIndex(staleSlot, len); (e = tab[i]) != null; i = prevIndex(i, len))
				if (e.get() == null)
					slotToExpunge = i;

			// Find either the key or trailing null slot of run, whichever
			// occurs first
			for (int i = nextIndex(staleSlot, len); (e = tab[i]) != null; i = nextIndex(i, len)) {
				CoroutineLocal k = e.get();

				// If we find key, then we need to swap it
				// with the stale entry to maintain hash table order.
				// The newly stale slot, or any other stale slot
				// encountered above it, can then be sent to expungeStaleEntry
				// to remove or rehash all of the other entries in run.
				if (k == key) {
					e.value = value;

					tab[i] = tab[staleSlot];
					tab[staleSlot] = e;

					// Start expunge at preceding stale entry if it exists
					if (slotToExpunge == staleSlot)
						slotToExpunge = i;
					cleanSomeSlots(expungeStaleEntry(slotToExpunge), len);
					return;
				}

				// If we didn't find stale entry on backward scan, the
				// first stale entry seen while scanning for key is the
				// first still present in the run.
				if (k == null && slotToExpunge == staleSlot)
					slotToExpunge = i;
			}

			// If key not found, put new entry in stale slot
			tab[staleSlot].value = null;
			tab[staleSlot] = new Entry(key, value);

			// If there are any other stale entries in run, expunge them
			if (slotToExpunge != staleSlot)
				cleanSomeSlots(expungeStaleEntry(slotToExpunge), len);
		}

		/**
		 * Expunge a stale entry by rehashing any possibly colliding entries lying between staleSlot and the next null slot. This also
		 * expunges any other stale entries encountered before the trailing null. See Knuth, Section 6.4
		 * 
		 * @param staleSlot index of slot known to have null key
		 * @return the index of the next null slot after staleSlot (all between staleSlot and this slot will have been checked for
		 *         expunging).
		 */
		private int expungeStaleEntry(int staleSlot) {
			Entry[] tab = table;
			int len = tab.length;

			// expunge entry at staleSlot
			tab[staleSlot].value = null;
			tab[staleSlot] = null;
			size--;

			// Rehash until we encounter null
			Entry e;
			int i;
			for (i = nextIndex(staleSlot, len); (e = tab[i]) != null; i = nextIndex(i, len)) {
				CoroutineLocal k = e.get();
				if (k == null) {
					e.value = null;
					tab[i] = null;
					size--;
				}
				else {
					int h = k.coroutineLocalHashCode & (len - 1);
					if (h != i) {
						tab[i] = null;

						// Unlike Knuth 6.4 Algorithm R, we must scan until
						// null because multiple entries could have been stale.
						while (tab[h] != null)
							h = nextIndex(h, len);
						tab[h] = e;
					}
				}
			}
			return i;
		}

		/**
		 * Heuristically scan some cells looking for stale entries. This is invoked when either a new element is added, or another stale one
		 * has been expunged. It performs a logarithmic number of scans, as a balance between no scanning (fast but retains garbage) and a
		 * number of scans proportional to number of elements, that would find all garbage but would cause some insertions to take O(n)
		 * time.
		 * 
		 * @param i a position known NOT to hold a stale entry. The scan starts at the element after i.
		 * 
		 * @param n scan control: <tt>log2(n)</tt> cells are scanned, unless a stale entry is found, in which case
		 *            <tt>log2(table.length)-1</tt> additional cells are scanned. When called from insertions, this parameter is the number
		 *            of elements, but when from replaceStaleEntry, it is the table length. (Note: all this could be changed to be either
		 *            more or less aggressive by weighting n instead of just using straight log n. But this version is simple, fast, and
		 *            seems to work well.)
		 * 
		 * @return true if any stale entries have been removed.
		 */
		private boolean cleanSomeSlots(int i, int n) {
			boolean removed = false;
			Entry[] tab = table;
			int len = tab.length;
			do {
				i = nextIndex(i, len);
				Entry e = tab[i];
				if (e != null && e.get() == null) {
					n = len;
					removed = true;
					i = expungeStaleEntry(i);
				}
			}
			while ((n >>>= 1) != 0);
			return removed;
		}

		/**
		 * Re-pack and/or re-size the table. First scan the entire table removing stale entries. If this doesn't sufficiently shrink the
		 * size of the table, double the table size.
		 */
		private void rehash() {
			expungeStaleEntries();

			// Use lower threshold for doubling to avoid hysteresis
			if (size >= threshold - threshold / 4)
				resize();
		}

		/**
		 * Double the capacity of the table.
		 */
		private void resize() {
			Entry[] oldTab = table;
			int oldLen = oldTab.length;
			int newLen = oldLen * 2;
			Entry[] newTab = new Entry[newLen];
			int count = 0;

			for (int j = 0; j < oldLen; ++j) {
				Entry e = oldTab[j];
				if (e != null) {
					CoroutineLocal k = e.get();
					if (k == null) {
						e.value = null; // Help the GC
					}
					else {
						int h = k.coroutineLocalHashCode & (newLen - 1);
						while (newTab[h] != null)
							h = nextIndex(h, newLen);
						newTab[h] = e;
						count++;
					}
				}
			}

			setThreshold(newLen);
			size = count;
			table = newTab;
		}

		/**
		 * Expunge all stale entries in the table.
		 */
		private void expungeStaleEntries() {
			Entry[] tab = table;
			int len = tab.length;
			for (int j = 0; j < len; j++) {
				Entry e = tab[j];
				if (e != null && e.get() == null)
					expungeStaleEntry(j);
			}
		}
	}
}
