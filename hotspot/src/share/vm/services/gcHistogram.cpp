/*
 *
 * Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "gcHistogram.hpp"
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

int GCHistogram::binary_search(long key) {
   int low = 0, high = ALL_LEVEL, mid = 0;

   while (low < high) {
      mid = (high + low + 1) / 2;
      if (_bucket_limits[mid] > key) {
        high = mid - 1;
      }else {
        low = mid;
      }
   }

   return ((_bucket_limits[mid] <= key) ? low : (ALL_LEVEL - 2));
}

int GCHistogram::binary_search(long *array, int size, long key) {
    int first = 0, count = size, step;
    int it = 0;

    while(count > 0) {
        it = first;
        step = count / 2;
        it += step;
        if (!(key < array[it])) {
          first = ++it;
          count -= step +1;
        }else {
          count = step;
        }
    }
   
    return (0 == first ? first : first -1);
}

int GCHistogram::search(long key) {
  //fast path
  int result = (int)(key / 100);
  if (result < LEVEL1) {
    return result;
  }

  //slow path
  return binary_search(_bucket_limits, ALL_LEVEL, key);
}

GCHistogram::GCHistogram() : _bucket_limits(init_default_buckets()) { clear(); }

long* GCHistogram::init_default_buckets_inner() {
  long* result = new long[ALL_LEVEL];
  for (int i = 0; i < ALL_LEVEL; i++) {
    if (i < LEVEL1) {//100,200,300......19000
      result[i] = i * 100;
    } else if (i < LEVEL2) { //2000, 3000, 4000......510000
      result[i] = (i + 2 - LEVEL1) * 10000;
    }else { //1020000, 1530000, 2040000.....25500000
      result[i] = result[LEVEL2 - 1] * (i - LEVEL2 + 2);
    }
  }
  //set the last element max int
  result[ALL_LEVEL - 1] = INT_MAX;
  
  return result;
}

long* GCHistogram::init_default_buckets() {
  long* default_bucket_limits = init_default_buckets_inner();
  return default_bucket_limits;
}

// Create a histogram with a custom set of bucket limits,
GCHistogram::GCHistogram(long* custom_bucket_limits)
    : _custom_bucket_limits(custom_bucket_limits),
      _bucket_limits(_custom_bucket_limits) {
  clear();
}

void GCHistogram::clear() {
  _min = _bucket_limits[ALL_LEVEL - 1];
  _max = 0;
  _num = 0;
  _sum = 0;
  _sum_squares = 0;
  _buckets = new long[ALL_LEVEL];
  for (size_t i = 0; i < ALL_LEVEL; i++) {
    _buckets[i] = 0;
  }
}

void GCHistogram::add(long value) {
 int b = search(value);
  _buckets[b] += 1;
  if (_min > value) _min = value;
  if (_max < value) _max = value;
  _num++;
  _sum += value;
  _sum_squares += (value * value);
}

long GCHistogram::median() const { return percentile(50); }

long GCHistogram::remap(double x, long x0, long x1, long y0,
                        long y1) const {
  //assert((x0 != x1 && y0 != y1), "sanity check");
  return (long)(y0 + (x - x0) / (x1 - x0) * (y1 - y0));
}

long GCHistogram::percentile(double p) const {
  if (0 == _num) return 0;

  double threshold = (_num * (p / 100.0));
  long cumsum_prev = 0;
  for (int i = 0; i < ALL_LEVEL; i++) {
    long cumsum = cumsum_prev + _buckets[i];

    // search the bucket that cumsum >= threshold
    if (cumsum >= threshold) {
      if (cumsum == cumsum_prev) {
        continue;
      }

      // compute the lower bound
      long lhs = (0 == i || 0 == cumsum_prev) ? _min : _bucket_limits[i];
      lhs = (lhs > _min ? lhs : _min);//std::max(lhs, _min);

      // compute the upper bound
      long rhs = _bucket_limits[(i == ALL_LEVEL - 1) ? i : i + 1];
      rhs = (rhs > _max ? _max : rhs);//std::min(rhs, _max);

      long weight = remap(threshold, cumsum_prev, cumsum, lhs, rhs);
      return weight;
    }

    cumsum_prev = cumsum;
  }
  return _max;
}

double GCHistogram::average() const {
  if (0 == _num) return 0.0;
  return (_sum * 1.0) / _num;
}

long GCHistogram::max() const {
  return _max;
}

long GCHistogram::min() const {
  return _min;
}

double GCHistogram::standard_deviation() const {
  if (0 == _num) return 0.0;
  double variance = (_sum_squares * _num * 1.0 - _sum * _sum) / (_num * _num);
  return sqrt(variance);
}
