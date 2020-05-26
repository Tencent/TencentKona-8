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

#ifndef SHARE_VM_SERVICES_HISTOGRAM_HPP
#define SHARE_VM_SERVICES_HISTOGRAM_HPP

//This class provides a framework for collecting percentile statistics.
class Histogram {
 public:
  enum BucketLevel {
    LEVEL1 = 200,
    LEVEL2 = 250,
    LEVEL3 = 300,
    ALL_LEVEL = LEVEL3
  };

  // Create a histogram with a default set of bucket boundaries.
  Histogram();

  // Create a histogram with a custom set of bucket boundaries,
  explicit Histogram(long* custom_bucket_limits);

  ~Histogram() {}

  void clear();
  void add(long value);

  // Return the median of the values in the histogram
  long median() const;

  // Return the "p"th percentile [0.0..100.0] of the values in the
  // distribution
  long percentile(double p) const;

  long min() const;
  long max() const;
  int search(long);
  int binary_search(long);
  int binary_search(long *array, int size, long key);
  long* init_default_buckets_inner();
  long* init_default_buckets();

  // Return the average value of the distribution
  double average() const;

  // Return the standard deviation of values in the distribution
  double standard_deviation() const;

 private:
  long _min;
  long _max;
  long _num;
  long _sum;
  long _sum_squares;

  long* _custom_bucket_limits;
  long* _bucket_limits;
  //the element of _buckets will be modifyed by vmthread frequently.
  //should be set volatile
  volatile  long* _buckets;

  long remap(double x, long x0, long x1, long y0, long y1) const;
};

#endif
