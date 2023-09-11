## How to Run the Test
### prerequisites
Java 1.8.0_362_fiber is needed for this project, so make sure you have it installed.

### build the project
Go to the directory where the project is located.
Run the command `mvn clean package` to build and package the project.

### run the test
Run the command `java -jar ./target/oneshot_test.jar` to run the JMH test.


## Detailed Test Results
### KonaFiber
```
# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 0.00% complete, ETA 00:10:00
# Fork: 1 of 1
# Warmup Iteration   1: 183.530 ns/op
# Warmup Iteration   2: 175.869 ns/op
# Warmup Iteration   3: 186.089 ns/op
# Warmup Iteration   4: 191.532 ns/op
# Warmup Iteration   5: 204.310 ns/op
Iteration   1: 203.763 ns/op
Iteration   2: 192.537 ns/op
Iteration   3: 192.667 ns/op
Iteration   4: 187.708 ns/op
Iteration   5: 190.063 ns/op


Result "org.example.OneShot.noYield":
  193.347 ±(99.9%) 23.752 ns/op [Average]
  (min, avg, max) = (187.708, 193.347, 203.763), stdev = 6.168
  CI (99.9%): [169.595, 217.100] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 1.67% complete, ETA 00:10:14
# Fork: 1 of 1
# Warmup Iteration   1: 196.154 ns/op
# Warmup Iteration   2: 189.412 ns/op
# Warmup Iteration   3: 184.654 ns/op
# Warmup Iteration   4: 185.829 ns/op
# Warmup Iteration   5: 179.676 ns/op
Iteration   1: 192.163 ns/op
Iteration   2: 186.343 ns/op
Iteration   3: 186.882 ns/op
Iteration   4: 194.482 ns/op
Iteration   5: 194.099 ns/op


Result "org.example.OneShot.noYield":
  190.794 ±(99.9%) 15.101 ns/op [Average]
  (min, avg, max) = (186.343, 190.794, 194.482), stdev = 3.922
  CI (99.9%): [175.693, 205.895] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 3.33% complete, ETA 00:10:03
# Fork: 1 of 1
# Warmup Iteration   1: 211.337 ns/op
# Warmup Iteration   2: 206.527 ns/op
# Warmup Iteration   3: 205.787 ns/op
# Warmup Iteration   4: 206.720 ns/op
# Warmup Iteration   5: 199.886 ns/op
Iteration   1: 201.900 ns/op
Iteration   2: 197.578 ns/op
Iteration   3: 205.150 ns/op
Iteration   4: 203.752 ns/op
Iteration   5: 199.175 ns/op


Result "org.example.OneShot.noYield":
  201.511 ±(99.9%) 12.077 ns/op [Average]
  (min, avg, max) = (197.578, 201.511, 205.150), stdev = 3.136
  CI (99.9%): [189.435, 213.588] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 5.00% complete, ETA 00:09:53
# Fork: 1 of 1
# Warmup Iteration   1: 538.501 ns/op
# Warmup Iteration   2: 533.792 ns/op
# Warmup Iteration   3: 513.321 ns/op
# Warmup Iteration   4: 519.327 ns/op
# Warmup Iteration   5: 503.315 ns/op
Iteration   1: 495.340 ns/op
Iteration   2: 516.305 ns/op
Iteration   3: 503.650 ns/op
Iteration   4: 523.679 ns/op
Iteration   5: 518.093 ns/op


Result "org.example.OneShot.noYield":
  511.413 ±(99.9%) 44.641 ns/op [Average]
  (min, avg, max) = (495.340, 511.413, 523.679), stdev = 11.593
  CI (99.9%): [466.772, 556.055] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 6.67% complete, ETA 00:09:42
# Fork: 1 of 1
# Warmup Iteration   1: 234.571 ns/op
# Warmup Iteration   2: 229.260 ns/op
# Warmup Iteration   3: 225.292 ns/op
# Warmup Iteration   4: 226.777 ns/op
# Warmup Iteration   5: 220.710 ns/op
Iteration   1: 224.004 ns/op
Iteration   2: 228.223 ns/op
Iteration   3: 220.812 ns/op
Iteration   4: 228.885 ns/op
Iteration   5: 230.652 ns/op


Result "org.example.OneShot.noYield":
  226.515 ±(99.9%) 15.460 ns/op [Average]
  (min, avg, max) = (220.812, 226.515, 230.652), stdev = 4.015
  CI (99.9%): [211.055, 241.975] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 8.33% complete, ETA 00:09:32
# Fork: 1 of 1
# Warmup Iteration   1: 276.897 ns/op
# Warmup Iteration   2: 259.755 ns/op
# Warmup Iteration   3: 266.779 ns/op
# Warmup Iteration   4: 260.052 ns/op
# Warmup Iteration   5: 270.747 ns/op
Iteration   1: 273.654 ns/op
Iteration   2: 268.863 ns/op
Iteration   3: 271.901 ns/op
Iteration   4: 271.025 ns/op
Iteration   5: 269.981 ns/op


Result "org.example.OneShot.noYield":
  271.085 ±(99.9%) 7.055 ns/op [Average]
  (min, avg, max) = (268.863, 271.085, 273.654), stdev = 1.832
  CI (99.9%): [264.030, 278.140] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 10.00% complete, ETA 00:09:21
# Fork: 1 of 1
# Warmup Iteration   1: 371.067 ns/op
# Warmup Iteration   2: 367.575 ns/op
# Warmup Iteration   3: 336.772 ns/op
# Warmup Iteration   4: 363.537 ns/op
# Warmup Iteration   5: 351.877 ns/op
Iteration   1: 356.266 ns/op
Iteration   2: 359.751 ns/op
Iteration   3: 373.018 ns/op
Iteration   4: 370.802 ns/op
Iteration   5: 385.111 ns/op


Result "org.example.OneShot.noYield":
  368.990 ±(99.9%) 44.183 ns/op [Average]
  (min, avg, max) = (356.266, 368.990, 385.111), stdev = 11.474
  CI (99.9%): [324.806, 413.173] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 11.67% complete, ETA 00:09:11
# Fork: 1 of 1
# Warmup Iteration   1: 1032.733 ns/op
# Warmup Iteration   2: 1026.362 ns/op
# Warmup Iteration   3: 1019.331 ns/op
# Warmup Iteration   4: 1020.523 ns/op
# Warmup Iteration   5: 969.350 ns/op
Iteration   1: 1026.320 ns/op
Iteration   2: 1021.395 ns/op
Iteration   3: 1014.788 ns/op
Iteration   4: 1017.552 ns/op
Iteration   5: 995.400 ns/op


Result "org.example.OneShot.noYield":
  1015.091 ±(99.9%) 45.551 ns/op [Average]
  (min, avg, max) = (995.400, 1015.091, 1026.320), stdev = 11.829
  CI (99.9%): [969.540, 1060.642] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 13.33% complete, ETA 00:09:01
# Fork: 1 of 1
# Warmup Iteration   1: 255.110 ns/op
# Warmup Iteration   2: 251.558 ns/op
# Warmup Iteration   3: 244.999 ns/op
# Warmup Iteration   4: 242.475 ns/op
# Warmup Iteration   5: 245.461 ns/op
Iteration   1: 253.287 ns/op
Iteration   2: 243.246 ns/op
Iteration   3: 247.401 ns/op
Iteration   4: 258.417 ns/op
Iteration   5: 243.539 ns/op


Result "org.example.OneShot.noYield":
  249.178 ±(99.9%) 25.278 ns/op [Average]
  (min, avg, max) = (243.246, 249.178, 258.417), stdev = 6.565
  CI (99.9%): [223.900, 274.456] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 15.00% complete, ETA 00:08:50
# Fork: 1 of 1
# Warmup Iteration   1: 321.069 ns/op
# Warmup Iteration   2: 309.766 ns/op
# Warmup Iteration   3: 322.020 ns/op
# Warmup Iteration   4: 333.512 ns/op
# Warmup Iteration   5: 317.415 ns/op
Iteration   1: 321.661 ns/op
Iteration   2: 317.342 ns/op
Iteration   3: 337.892 ns/op
Iteration   4: 335.542 ns/op
Iteration   5: 335.282 ns/op


Result "org.example.OneShot.noYield":
  329.544 ±(99.9%) 36.000 ns/op [Average]
  (min, avg, max) = (317.342, 329.544, 337.892), stdev = 9.349
  CI (99.9%): [293.544, 365.543] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 16.67% complete, ETA 00:08:40
# Fork: 1 of 1
# Warmup Iteration   1: 465.874 ns/op
# Warmup Iteration   2: 467.371 ns/op
# Warmup Iteration   3: 471.530 ns/op
# Warmup Iteration   4: 456.637 ns/op
# Warmup Iteration   5: 447.067 ns/op
Iteration   1: 462.873 ns/op
Iteration   2: 469.568 ns/op
Iteration   3: 473.967 ns/op
Iteration   4: 474.572 ns/op
Iteration   5: 465.855 ns/op


Result "org.example.OneShot.noYield":
  469.367 ±(99.9%) 19.522 ns/op [Average]
  (min, avg, max) = (462.873, 469.367, 474.572), stdev = 5.070
  CI (99.9%): [449.845, 488.889] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.noYield
# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 18.33% complete, ETA 00:08:29
# Fork: 1 of 1
# Warmup Iteration   1: 1550.774 ns/op
# Warmup Iteration   2: 1460.484 ns/op
# Warmup Iteration   3: 1555.778 ns/op
# Warmup Iteration   4: 1557.748 ns/op
# Warmup Iteration   5: 1501.706 ns/op
Iteration   1: 1536.644 ns/op
Iteration   2: 1538.024 ns/op
Iteration   3: 1539.794 ns/op
Iteration   4: 1530.784 ns/op
Iteration   5: 1505.565 ns/op


Result "org.example.OneShot.noYield":
  1530.162 ±(99.9%) 54.525 ns/op [Average]
  (min, avg, max) = (1505.565, 1530.162, 1539.794), stdev = 14.160
  CI (99.9%): [1475.637, 1584.687] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 20.00% complete, ETA 00:08:19
# Fork: 1 of 1
# Warmup Iteration   1: 419.773 ns/op
# Warmup Iteration   2: 399.167 ns/op
# Warmup Iteration   3: 410.087 ns/op
# Warmup Iteration   4: 400.082 ns/op
# Warmup Iteration   5: 395.456 ns/op
Iteration   1: 396.634 ns/op
Iteration   2: 407.362 ns/op
Iteration   3: 417.490 ns/op
Iteration   4: 407.722 ns/op
Iteration   5: 387.826 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  403.407 ±(99.9%) 43.957 ns/op [Average]
  (min, avg, max) = (387.826, 403.407, 417.490), stdev = 11.416
  CI (99.9%): [359.450, 447.364] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 21.67% complete, ETA 00:08:09
# Fork: 1 of 1
# Warmup Iteration   1: 646.982 ns/op
# Warmup Iteration   2: 620.911 ns/op
# Warmup Iteration   3: 634.413 ns/op
# Warmup Iteration   4: 608.418 ns/op
# Warmup Iteration   5: 617.956 ns/op
Iteration   1: 595.551 ns/op
Iteration   2: 591.933 ns/op
Iteration   3: 622.414 ns/op
Iteration   4: 626.758 ns/op
Iteration   5: 617.246 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  610.780 ±(99.9%) 61.477 ns/op [Average]
  (min, avg, max) = (591.933, 610.780, 626.758), stdev = 15.965
  CI (99.9%): [549.304, 672.257] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 23.33% complete, ETA 00:07:58
# Fork: 1 of 1
# Warmup Iteration   1: 1106.420 ns/op
# Warmup Iteration   2: 1066.532 ns/op
# Warmup Iteration   3: 1068.506 ns/op
# Warmup Iteration   4: 1041.816 ns/op
# Warmup Iteration   5: 1048.924 ns/op
Iteration   1: 1067.229 ns/op
Iteration   2: 1048.068 ns/op
Iteration   3: 1099.427 ns/op
Iteration   4: 1027.780 ns/op
Iteration   5: 1108.724 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  1070.246 ±(99.9%) 131.096 ns/op [Average]
  (min, avg, max) = (1027.780, 1070.246, 1108.724), stdev = 34.045
  CI (99.9%): [939.149, 1201.342] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 25.00% complete, ETA 00:07:48
# Fork: 1 of 1
# Warmup Iteration   1: 4789.777 ns/op
# Warmup Iteration   2: 4735.030 ns/op
# Warmup Iteration   3: 4624.935 ns/op
# Warmup Iteration   4: 4680.530 ns/op
# Warmup Iteration   5: 4579.603 ns/op
Iteration   1: 4498.869 ns/op
Iteration   2: 4653.350 ns/op
Iteration   3: 4704.845 ns/op
Iteration   4: 5037.313 ns/op
Iteration   5: 4563.598 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  4691.595 ±(99.9%) 804.697 ns/op [Average]
  (min, avg, max) = (4498.869, 4691.595, 5037.313), stdev = 208.977
  CI (99.9%): [3886.898, 5496.292] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 26.67% complete, ETA 00:07:37
# Fork: 1 of 1
# Warmup Iteration   1: 425.550 ns/op
# Warmup Iteration   2: 417.003 ns/op
# Warmup Iteration   3: 406.077 ns/op
# Warmup Iteration   4: 419.450 ns/op
# Warmup Iteration   5: 412.526 ns/op
Iteration   1: 416.221 ns/op
Iteration   2: 396.262 ns/op
Iteration   3: 419.709 ns/op
Iteration   4: 417.193 ns/op
Iteration   5: 413.483 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  412.573 ±(99.9%) 36.146 ns/op [Average]
  (min, avg, max) = (396.262, 412.573, 419.709), stdev = 9.387
  CI (99.9%): [376.428, 448.719] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 28.33% complete, ETA 00:07:27
# Fork: 1 of 1
# Warmup Iteration   1: 658.652 ns/op
# Warmup Iteration   2: 628.872 ns/op
# Warmup Iteration   3: 615.427 ns/op
# Warmup Iteration   4: 613.004 ns/op
# Warmup Iteration   5: 607.661 ns/op
Iteration   1: 594.007 ns/op
Iteration   2: 592.286 ns/op
Iteration   3: 643.006 ns/op
Iteration   4: 614.112 ns/op
Iteration   5: 621.200 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  612.922 ±(99.9%) 80.739 ns/op [Average]
  (min, avg, max) = (592.286, 612.922, 643.006), stdev = 20.968
  CI (99.9%): [532.183, 693.661] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 30.00% complete, ETA 00:07:16
# Fork: 1 of 1
# Warmup Iteration   1: 1065.654 ns/op
# Warmup Iteration   2: 1058.429 ns/op
# Warmup Iteration   3: 1071.276 ns/op
# Warmup Iteration   4: 1066.734 ns/op
# Warmup Iteration   5: 995.324 ns/op
Iteration   1: 1073.386 ns/op
Iteration   2: 1036.048 ns/op
Iteration   3: 1052.869 ns/op
Iteration   4: 1069.018 ns/op
Iteration   5: 1092.471 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  1064.759 ±(99.9%) 82.276 ns/op [Average]
  (min, avg, max) = (1036.048, 1064.759, 1092.471), stdev = 21.367
  CI (99.9%): [982.482, 1147.035] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 31.67% complete, ETA 00:07:06
# Fork: 1 of 1
# Warmup Iteration   1: 4985.935 ns/op
# Warmup Iteration   2: 4956.972 ns/op
# Warmup Iteration   3: 4854.199 ns/op
# Warmup Iteration   4: 4689.261 ns/op
# Warmup Iteration   5: 4686.692 ns/op
Iteration   1: 4637.396 ns/op
Iteration   2: 4670.587 ns/op
Iteration   3: 4803.703 ns/op
Iteration   4: 4565.182 ns/op
Iteration   5: 4731.575 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  4681.689 ±(99.9%) 349.937 ns/op [Average]
  (min, avg, max) = (4565.182, 4681.689, 4803.703), stdev = 90.878
  CI (99.9%): [4331.751, 5031.626] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 33.33% complete, ETA 00:06:55
# Fork: 1 of 1
# Warmup Iteration   1: 414.628 ns/op
# Warmup Iteration   2: 419.038 ns/op
# Warmup Iteration   3: 408.192 ns/op
# Warmup Iteration   4: 421.337 ns/op
# Warmup Iteration   5: 409.328 ns/op
Iteration   1: 405.173 ns/op
Iteration   2: 420.407 ns/op
Iteration   3: 431.361 ns/op
Iteration   4: 414.045 ns/op
Iteration   5: 421.576 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  418.512 ±(99.9%) 37.332 ns/op [Average]
  (min, avg, max) = (405.173, 418.512, 431.361), stdev = 9.695
  CI (99.9%): [381.181, 455.844] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 35.00% complete, ETA 00:06:45
# Fork: 1 of 1
# Warmup Iteration   1: 677.688 ns/op
# Warmup Iteration   2: 657.958 ns/op
# Warmup Iteration   3: 632.706 ns/op
# Warmup Iteration   4: 674.751 ns/op
# Warmup Iteration   5: 643.823 ns/op
Iteration   1: 650.932 ns/op
Iteration   2: 654.669 ns/op
Iteration   3: 658.307 ns/op
Iteration   4: 640.761 ns/op
Iteration   5: 672.064 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  655.347 ±(99.9%) 43.943 ns/op [Average]
  (min, avg, max) = (640.761, 655.347, 672.064), stdev = 11.412
  CI (99.9%): [611.404, 699.289] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 36.67% complete, ETA 00:06:35
# Fork: 1 of 1
# Warmup Iteration   1: 1108.156 ns/op
# Warmup Iteration   2: 1092.221 ns/op
# Warmup Iteration   3: 1063.934 ns/op
# Warmup Iteration   4: 1100.177 ns/op
# Warmup Iteration   5: 1070.555 ns/op
Iteration   1: 1058.586 ns/op
Iteration   2: 1108.766 ns/op
Iteration   3: 1110.086 ns/op
Iteration   4: 1089.341 ns/op
Iteration   5: 1171.286 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  1107.613 ±(99.9%) 158.758 ns/op [Average]
  (min, avg, max) = (1058.586, 1107.613, 1171.286), stdev = 41.229
  CI (99.9%): [948.855, 1266.370] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldAfterEachCall
# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 38.33% complete, ETA 00:06:24
# Fork: 1 of 1
# Warmup Iteration   1: 4730.393 ns/op
# Warmup Iteration   2: 4678.695 ns/op
# Warmup Iteration   3: 4690.274 ns/op
# Warmup Iteration   4: 4549.787 ns/op
# Warmup Iteration   5: 4514.138 ns/op
Iteration   1: 4747.241 ns/op
Iteration   2: 4535.939 ns/op
Iteration   3: 4761.224 ns/op
Iteration   4: 5471.328 ns/op
Iteration   5: 4899.929 ns/op


Result "org.example.OneShot.yieldAfterEachCall":
  4883.132 ±(99.9%) 1361.582 ns/op [Average]
  (min, avg, max) = (4535.939, 4883.132, 5471.328), stdev = 353.599
  CI (99.9%): [3521.550, 6244.715] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 40.00% complete, ETA 00:06:14
# Fork: 1 of 1
# Warmup Iteration   1: 589.491 ns/op
# Warmup Iteration   2: 560.279 ns/op
# Warmup Iteration   3: 609.938 ns/op
# Warmup Iteration   4: 590.807 ns/op
# Warmup Iteration   5: 568.003 ns/op
Iteration   1: 582.968 ns/op
Iteration   2: 572.786 ns/op
Iteration   3: 611.373 ns/op
Iteration   4: 593.542 ns/op
Iteration   5: 597.718 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  591.677 ±(99.9%) 56.482 ns/op [Average]
  (min, avg, max) = (572.786, 591.677, 611.373), stdev = 14.668
  CI (99.9%): [535.195, 648.160] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 41.67% complete, ETA 00:06:04
# Fork: 1 of 1
# Warmup Iteration   1: 999.030 ns/op
# Warmup Iteration   2: 993.215 ns/op
# Warmup Iteration   3: 1054.355 ns/op
# Warmup Iteration   4: 1015.505 ns/op
# Warmup Iteration   5: 1040.845 ns/op
Iteration   1: 1007.991 ns/op
Iteration   2: 1044.413 ns/op
Iteration   3: 1003.795 ns/op
Iteration   4: 1047.191 ns/op
Iteration   5: 1009.741 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  1022.626 ±(99.9%) 81.977 ns/op [Average]
  (min, avg, max) = (1003.795, 1022.626, 1047.191), stdev = 21.289
  CI (99.9%): [940.649, 1104.603] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 43.33% complete, ETA 00:05:53
# Fork: 1 of 1
# Warmup Iteration   1: 1816.563 ns/op
# Warmup Iteration   2: 1886.745 ns/op
# Warmup Iteration   3: 1827.989 ns/op
# Warmup Iteration   4: 1815.098 ns/op
# Warmup Iteration   5: 1796.746 ns/op
Iteration   1: 1835.988 ns/op
Iteration   2: 1870.241 ns/op
Iteration   3: 1820.755 ns/op
Iteration   4: 1826.194 ns/op
Iteration   5: 1801.580 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  1830.952 ±(99.9%) 97.383 ns/op [Average]
  (min, avg, max) = (1801.580, 1830.952, 1870.241), stdev = 25.290
  CI (99.9%): [1733.569, 1928.335] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 45.00% complete, ETA 00:05:43
# Fork: 1 of 1
# Warmup Iteration   1: 8284.820 ns/op
# Warmup Iteration   2: 8895.025 ns/op
# Warmup Iteration   3: 8395.587 ns/op
# Warmup Iteration   4: 8267.268 ns/op
# Warmup Iteration   5: 8227.796 ns/op
Iteration   1: 8913.385 ns/op
Iteration   2: 8553.669 ns/op
Iteration   3: 8423.413 ns/op
Iteration   4: 8449.506 ns/op
Iteration   5: 8187.522 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  8505.499 ±(99.9%) 1018.192 ns/op [Average]
  (min, avg, max) = (8187.522, 8505.499, 8913.385), stdev = 264.421
  CI (99.9%): [7487.307, 9523.691] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 46.67% complete, ETA 00:05:32
# Fork: 1 of 1
# Warmup Iteration   1: 655.272 ns/op
# Warmup Iteration   2: 631.303 ns/op
# Warmup Iteration   3: 647.742 ns/op
# Warmup Iteration   4: 677.691 ns/op
# Warmup Iteration   5: 657.327 ns/op
Iteration   1: 673.795 ns/op
Iteration   2: 668.695 ns/op
Iteration   3: 656.270 ns/op
Iteration   4: 667.627 ns/op
Iteration   5: 686.450 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  670.567 ±(99.9%) 42.148 ns/op [Average]
  (min, avg, max) = (656.270, 670.567, 686.450), stdev = 10.946
  CI (99.9%): [628.419, 712.716] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 48.33% complete, ETA 00:05:22
# Fork: 1 of 1
# Warmup Iteration   1: 1052.241 ns/op
# Warmup Iteration   2: 1014.477 ns/op
# Warmup Iteration   3: 957.839 ns/op
# Warmup Iteration   4: 964.557 ns/op
# Warmup Iteration   5: 981.674 ns/op
Iteration   1: 941.991 ns/op
Iteration   2: 996.331 ns/op
Iteration   3: 997.654 ns/op
Iteration   4: 979.529 ns/op
Iteration   5: 1001.714 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  983.444 ±(99.9%) 95.015 ns/op [Average]
  (min, avg, max) = (941.991, 983.444, 1001.714), stdev = 24.675
  CI (99.9%): [888.429, 1078.459] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 50.00% complete, ETA 00:05:12
# Fork: 1 of 1
# Warmup Iteration   1: 1873.992 ns/op
# Warmup Iteration   2: 1850.341 ns/op
# Warmup Iteration   3: 1784.780 ns/op
# Warmup Iteration   4: 1927.619 ns/op
# Warmup Iteration   5: 1798.571 ns/op
Iteration   1: 1857.649 ns/op
Iteration   2: 1840.655 ns/op
Iteration   3: 1893.485 ns/op
Iteration   4: 1931.601 ns/op
Iteration   5: 1875.410 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  1879.760 ±(99.9%) 134.950 ns/op [Average]
  (min, avg, max) = (1840.655, 1879.760, 1931.601), stdev = 35.046
  CI (99.9%): [1744.810, 2014.710] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 51.67% complete, ETA 00:05:01
# Fork: 1 of 1
# Warmup Iteration   1: 8956.296 ns/op
# Warmup Iteration   2: 9015.140 ns/op
# Warmup Iteration   3: 8482.774 ns/op
# Warmup Iteration   4: 8601.406 ns/op
# Warmup Iteration   5: 8276.188 ns/op
Iteration   1: 8687.784 ns/op
Iteration   2: 8803.656 ns/op
Iteration   3: 8656.917 ns/op
Iteration   4: 8718.723 ns/op
Iteration   5: 8679.548 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  8709.325 ±(99.9%) 220.228 ns/op [Average]
  (min, avg, max) = (8656.917, 8709.325, 8803.656), stdev = 57.193
  CI (99.9%): [8489.097, 8929.554] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 53.33% complete, ETA 00:04:51
# Fork: 1 of 1
# Warmup Iteration   1: 645.157 ns/op
# Warmup Iteration   2: 618.376 ns/op
# Warmup Iteration   3: 629.381 ns/op
# Warmup Iteration   4: 621.088 ns/op
# Warmup Iteration   5: 626.955 ns/op
Iteration   1: 621.443 ns/op
Iteration   2: 631.789 ns/op
Iteration   3: 650.203 ns/op
Iteration   4: 628.306 ns/op
Iteration   5: 647.832 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  635.915 ±(99.9%) 48.346 ns/op [Average]
  (min, avg, max) = (621.443, 635.915, 650.203), stdev = 12.555
  CI (99.9%): [587.569, 684.261] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 55.00% complete, ETA 00:04:40
# Fork: 1 of 1
# Warmup Iteration   1: 1007.552 ns/op
# Warmup Iteration   2: 1014.872 ns/op
# Warmup Iteration   3: 1018.751 ns/op
# Warmup Iteration   4: 977.963 ns/op
# Warmup Iteration   5: 982.812 ns/op
Iteration   1: 1017.289 ns/op
Iteration   2: 973.844 ns/op
Iteration   3: 984.510 ns/op
Iteration   4: 1004.604 ns/op
Iteration   5: 1013.125 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  998.674 ±(99.9%) 72.236 ns/op [Average]
  (min, avg, max) = (973.844, 998.674, 1017.289), stdev = 18.760
  CI (99.9%): [926.438, 1070.911] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 56.67% complete, ETA 00:04:30
# Fork: 1 of 1
# Warmup Iteration   1: 1914.173 ns/op
# Warmup Iteration   2: 1822.635 ns/op
# Warmup Iteration   3: 1833.283 ns/op
# Warmup Iteration   4: 1809.697 ns/op
# Warmup Iteration   5: 1769.325 ns/op
Iteration   1: 2073.314 ns/op
Iteration   2: 1916.477 ns/op
Iteration   3: 1850.833 ns/op
Iteration   4: 1844.585 ns/op
Iteration   5: 1771.229 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  1891.288 ±(99.9%) 439.041 ns/op [Average]
  (min, avg, max) = (1771.229, 1891.288, 2073.314), stdev = 114.017
  CI (99.9%): [1452.247, 2330.328] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeAndAfterEachCall
# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 58.33% complete, ETA 00:04:20
# Fork: 1 of 1
# Warmup Iteration   1: 8562.796 ns/op
# Warmup Iteration   2: 8829.066 ns/op
# Warmup Iteration   3: 8775.297 ns/op
# Warmup Iteration   4: 8931.894 ns/op
# Warmup Iteration   5: 8357.396 ns/op
Iteration   1: 8819.912 ns/op
Iteration   2: 8815.652 ns/op
Iteration   3: 8937.933 ns/op
Iteration   4: 8992.408 ns/op
Iteration   5: 8900.866 ns/op


Result "org.example.OneShot.yieldBeforeAndAfterEachCall":
  8893.354 ±(99.9%) 293.804 ns/op [Average]
  (min, avg, max) = (8815.652, 8893.354, 8992.408), stdev = 76.300
  CI (99.9%): [8599.550, 9187.158] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 60.00% complete, ETA 00:04:09
# Fork: 1 of 1
# Warmup Iteration   1: 497.532 ns/op
# Warmup Iteration   2: 435.926 ns/op
# Warmup Iteration   3: 430.438 ns/op
# Warmup Iteration   4: 432.977 ns/op
# Warmup Iteration   5: 428.161 ns/op
Iteration   1: 428.596 ns/op
Iteration   2: 431.523 ns/op
Iteration   3: 430.387 ns/op
Iteration   4: 424.795 ns/op
Iteration   5: 438.661 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  430.792 ±(99.9%) 19.579 ns/op [Average]
  (min, avg, max) = (424.795, 430.792, 438.661), stdev = 5.085
  CI (99.9%): [411.213, 450.371] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 61.67% complete, ETA 00:03:59
# Fork: 1 of 1
# Warmup Iteration   1: 702.327 ns/op
# Warmup Iteration   2: 665.265 ns/op
# Warmup Iteration   3: 669.136 ns/op
# Warmup Iteration   4: 661.375 ns/op
# Warmup Iteration   5: 664.883 ns/op
Iteration   1: 656.085 ns/op
Iteration   2: 649.436 ns/op
Iteration   3: 660.542 ns/op
Iteration   4: 650.515 ns/op
Iteration   5: 673.758 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  658.067 ±(99.9%) 37.914 ns/op [Average]
  (min, avg, max) = (649.436, 658.067, 673.758), stdev = 9.846
  CI (99.9%): [620.153, 695.982] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 63.33% complete, ETA 00:03:48
# Fork: 1 of 1
# Warmup Iteration   1: 1222.872 ns/op
# Warmup Iteration   2: 1167.072 ns/op
# Warmup Iteration   3: 1191.910 ns/op
# Warmup Iteration   4: 1188.040 ns/op
# Warmup Iteration   5: 1154.321 ns/op
Iteration   1: 1212.769 ns/op
Iteration   2: 1199.129 ns/op
Iteration   3: 1202.656 ns/op
Iteration   4: 1166.971 ns/op
Iteration   5: 1201.169 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  1196.539 ±(99.9%) 66.785 ns/op [Average]
  (min, avg, max) = (1166.971, 1196.539, 1212.769), stdev = 17.344
  CI (99.9%): [1129.754, 1263.324] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 65.00% complete, ETA 00:03:38
# Fork: 1 of 1
# Warmup Iteration   1: 5160.037 ns/op
# Warmup Iteration   2: 5153.102 ns/op
# Warmup Iteration   3: 5001.803 ns/op
# Warmup Iteration   4: 4988.762 ns/op
# Warmup Iteration   5: 4648.257 ns/op
Iteration   1: 4675.721 ns/op
Iteration   2: 4806.954 ns/op
Iteration   3: 4734.920 ns/op
Iteration   4: 4595.672 ns/op
Iteration   5: 4664.023 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  4695.458 ±(99.9%) 306.384 ns/op [Average]
  (min, avg, max) = (4595.672, 4695.458, 4806.954), stdev = 79.567
  CI (99.9%): [4389.074, 5001.842] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 66.67% complete, ETA 00:03:28
# Fork: 1 of 1
# Warmup Iteration   1: 420.892 ns/op
# Warmup Iteration   2: 421.277 ns/op
# Warmup Iteration   3: 421.194 ns/op
# Warmup Iteration   4: 435.966 ns/op
# Warmup Iteration   5: 411.471 ns/op
Iteration   1: 425.967 ns/op
Iteration   2: 424.458 ns/op
Iteration   3: 436.825 ns/op
Iteration   4: 426.058 ns/op
Iteration   5: 437.038 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  430.069 ±(99.9%) 24.247 ns/op [Average]
  (min, avg, max) = (424.458, 430.069, 437.038), stdev = 6.297
  CI (99.9%): [405.822, 454.316] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 68.33% complete, ETA 00:03:17
# Fork: 1 of 1
# Warmup Iteration   1: 673.807 ns/op
# Warmup Iteration   2: 633.793 ns/op
# Warmup Iteration   3: 647.094 ns/op
# Warmup Iteration   4: 667.564 ns/op
# Warmup Iteration   5: 642.529 ns/op
Iteration   1: 655.108 ns/op
Iteration   2: 670.393 ns/op
Iteration   3: 649.887 ns/op
Iteration   4: 648.550 ns/op
Iteration   5: 685.100 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  661.808 ±(99.9%) 60.241 ns/op [Average]
  (min, avg, max) = (648.550, 661.808, 685.100), stdev = 15.644
  CI (99.9%): [601.567, 722.049] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 70.00% complete, ETA 00:03:07
# Fork: 1 of 1
# Warmup Iteration   1: 1108.457 ns/op
# Warmup Iteration   2: 1069.532 ns/op
# Warmup Iteration   3: 1117.210 ns/op
# Warmup Iteration   4: 1090.881 ns/op
# Warmup Iteration   5: 1087.992 ns/op
Iteration   1: 1070.066 ns/op
Iteration   2: 1090.164 ns/op
Iteration   3: 1124.437 ns/op
Iteration   4: 1122.641 ns/op
Iteration   5: 1086.616 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  1098.785 ±(99.9%) 91.818 ns/op [Average]
  (min, avg, max) = (1070.066, 1098.785, 1124.437), stdev = 23.845
  CI (99.9%): [1006.967, 1190.603] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 71.67% complete, ETA 00:02:56
# Fork: 1 of 1
# Warmup Iteration   1: 4942.176 ns/op
# Warmup Iteration   2: 4711.566 ns/op
# Warmup Iteration   3: 4523.146 ns/op
# Warmup Iteration   4: 4754.685 ns/op
# Warmup Iteration   5: 4590.989 ns/op
Iteration   1: 4658.331 ns/op
Iteration   2: 4537.258 ns/op
Iteration   3: 4623.861 ns/op
Iteration   4: 4976.510 ns/op
Iteration   5: 4667.308 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  4692.654 ±(99.9%) 642.248 ns/op [Average]
  (min, avg, max) = (4537.258, 4692.654, 4976.510), stdev = 166.790
  CI (99.9%): [4050.406, 5334.902] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 73.33% complete, ETA 00:02:46
# Fork: 1 of 1
# Warmup Iteration   1: 430.474 ns/op
# Warmup Iteration   2: 415.795 ns/op
# Warmup Iteration   3: 401.261 ns/op
# Warmup Iteration   4: 426.882 ns/op
# Warmup Iteration   5: 411.889 ns/op
Iteration   1: 411.520 ns/op
Iteration   2: 407.304 ns/op
Iteration   3: 432.669 ns/op
Iteration   4: 414.293 ns/op
Iteration   5: 430.939 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  419.345 ±(99.9%) 44.892 ns/op [Average]
  (min, avg, max) = (407.304, 419.345, 432.669), stdev = 11.658
  CI (99.9%): [374.452, 464.237] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 75.00% complete, ETA 00:02:36
# Fork: 1 of 1
# Warmup Iteration   1: 645.148 ns/op
# Warmup Iteration   2: 655.573 ns/op
# Warmup Iteration   3: 634.285 ns/op
# Warmup Iteration   4: 641.131 ns/op
# Warmup Iteration   5: 623.019 ns/op
Iteration   1: 625.693 ns/op
Iteration   2: 626.789 ns/op
Iteration   3: 639.807 ns/op
Iteration   4: 628.683 ns/op
Iteration   5: 633.096 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  630.814 ±(99.9%) 22.209 ns/op [Average]
  (min, avg, max) = (625.693, 630.814, 639.807), stdev = 5.768
  CI (99.9%): [608.605, 653.022] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 76.67% complete, ETA 00:02:25
# Fork: 1 of 1
# Warmup Iteration   1: 1114.478 ns/op
# Warmup Iteration   2: 1117.140 ns/op
# Warmup Iteration   3: 1054.933 ns/op
# Warmup Iteration   4: 1074.856 ns/op
# Warmup Iteration   5: 1070.417 ns/op
Iteration   1: 1059.791 ns/op
Iteration   2: 1071.128 ns/op
Iteration   3: 1145.555 ns/op
Iteration   4: 1149.367 ns/op
Iteration   5: 1063.704 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  1097.909 ±(99.9%) 174.964 ns/op [Average]
  (min, avg, max) = (1059.791, 1097.909, 1149.367), stdev = 45.438
  CI (99.9%): [922.945, 1272.873] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldBeforeEachCall
# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 78.33% complete, ETA 00:02:15
# Fork: 1 of 1
# Warmup Iteration   1: 4890.897 ns/op
# Warmup Iteration   2: 4812.172 ns/op
# Warmup Iteration   3: 4822.917 ns/op
# Warmup Iteration   4: 4685.074 ns/op
# Warmup Iteration   5: 4591.855 ns/op
Iteration   1: 4790.287 ns/op
Iteration   2: 4600.196 ns/op
Iteration   3: 5089.382 ns/op
Iteration   4: 4776.302 ns/op
Iteration   5: 4809.214 ns/op


Result "org.example.OneShot.yieldBeforeEachCall":
  4813.076 ±(99.9%) 676.743 ns/op [Average]
  (min, avg, max) = (4600.196, 4813.076, 5089.382), stdev = 175.748
  CI (99.9%): [4136.334, 5489.819] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 80.00% complete, ETA 00:02:04
# Fork: 1 of 1
# Warmup Iteration   1: 245.424 ns/op
# Warmup Iteration   2: 239.484 ns/op
# Warmup Iteration   3: 224.958 ns/op
# Warmup Iteration   4: 215.569 ns/op
# Warmup Iteration   5: 240.470 ns/op
Iteration   1: 227.698 ns/op
Iteration   2: 223.983 ns/op
Iteration   3: 232.554 ns/op
Iteration   4: 231.845 ns/op
Iteration   5: 241.845 ns/op


Result "org.example.OneShot.yieldThenContinue":
  231.585 ±(99.9%) 25.771 ns/op [Average]
  (min, avg, max) = (223.983, 231.585, 241.845), stdev = 6.693
  CI (99.9%): [205.814, 257.356] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 81.67% complete, ETA 00:01:54
# Fork: 1 of 1
# Warmup Iteration   1: 253.578 ns/op
# Warmup Iteration   2: 248.155 ns/op
# Warmup Iteration   3: 252.641 ns/op
# Warmup Iteration   4: 236.974 ns/op
# Warmup Iteration   5: 246.379 ns/op
Iteration   1: 250.738 ns/op
Iteration   2: 246.581 ns/op
Iteration   3: 242.079 ns/op
Iteration   4: 246.029 ns/op
Iteration   5: 254.681 ns/op


Result "org.example.OneShot.yieldThenContinue":
  248.022 ±(99.9%) 18.576 ns/op [Average]
  (min, avg, max) = (242.079, 248.022, 254.681), stdev = 4.824
  CI (99.9%): [229.446, 266.598] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 83.33% complete, ETA 00:01:44
# Fork: 1 of 1
# Warmup Iteration   1: 273.760 ns/op
# Warmup Iteration   2: 258.037 ns/op
# Warmup Iteration   3: 253.211 ns/op
# Warmup Iteration   4: 244.731 ns/op
# Warmup Iteration   5: 252.802 ns/op
Iteration   1: 245.489 ns/op
Iteration   2: 255.213 ns/op
Iteration   3: 255.084 ns/op
Iteration   4: 268.811 ns/op
Iteration   5: 260.998 ns/op


Result "org.example.OneShot.yieldThenContinue":
  257.119 ±(99.9%) 33.057 ns/op [Average]
  (min, avg, max) = (245.489, 257.119, 268.811), stdev = 8.585
  CI (99.9%): [224.062, 290.175] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 85.00% complete, ETA 00:01:33
# Fork: 1 of 1
# Warmup Iteration   1: 594.791 ns/op
# Warmup Iteration   2: 575.865 ns/op
# Warmup Iteration   3: 580.895 ns/op
# Warmup Iteration   4: 582.673 ns/op
# Warmup Iteration   5: 576.637 ns/op
Iteration   1: 582.925 ns/op
Iteration   2: 580.325 ns/op
Iteration   3: 578.503 ns/op
Iteration   4: 572.865 ns/op
Iteration   5: 614.499 ns/op


Result "org.example.OneShot.yieldThenContinue":
  585.823 ±(99.9%) 63.343 ns/op [Average]
  (min, avg, max) = (572.865, 585.823, 614.499), stdev = 16.450
  CI (99.9%): [522.480, 649.167] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 86.67% complete, ETA 00:01:23
# Fork: 1 of 1
# Warmup Iteration   1: 284.682 ns/op
# Warmup Iteration   2: 274.462 ns/op
# Warmup Iteration   3: 249.203 ns/op
# Warmup Iteration   4: 270.433 ns/op
# Warmup Iteration   5: 261.667 ns/op
Iteration   1: 265.690 ns/op
Iteration   2: 278.921 ns/op
Iteration   3: 278.539 ns/op
Iteration   4: 276.817 ns/op
Iteration   5: 263.139 ns/op


Result "org.example.OneShot.yieldThenContinue":
  272.621 ±(99.9%) 29.215 ns/op [Average]
  (min, avg, max) = (263.139, 272.621, 278.921), stdev = 7.587
  CI (99.9%): [243.406, 301.836] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 88.33% complete, ETA 00:01:12
# Fork: 1 of 1
# Warmup Iteration   1: 328.319 ns/op
# Warmup Iteration   2: 309.805 ns/op
# Warmup Iteration   3: 292.060 ns/op
# Warmup Iteration   4: 311.365 ns/op
# Warmup Iteration   5: 287.839 ns/op
Iteration   1: 299.169 ns/op
Iteration   2: 313.573 ns/op
Iteration   3: 307.651 ns/op
Iteration   4: 313.453 ns/op
Iteration   5: 310.601 ns/op


Result "org.example.OneShot.yieldThenContinue":
  308.889 ±(99.9%) 22.920 ns/op [Average]
  (min, avg, max) = (299.169, 308.889, 313.573), stdev = 5.952
  CI (99.9%): [285.969, 331.809] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 90.00% complete, ETA 00:01:02
# Fork: 1 of 1
# Warmup Iteration   1: 399.135 ns/op
# Warmup Iteration   2: 390.840 ns/op
# Warmup Iteration   3: 383.467 ns/op
# Warmup Iteration   4: 380.289 ns/op
# Warmup Iteration   5: 383.029 ns/op
Iteration   1: 386.040 ns/op
Iteration   2: 385.314 ns/op
Iteration   3: 381.153 ns/op
Iteration   4: 388.078 ns/op
Iteration   5: 377.731 ns/op


Result "org.example.OneShot.yieldThenContinue":
  383.663 ±(99.9%) 16.034 ns/op [Average]
  (min, avg, max) = (377.731, 383.663, 388.078), stdev = 4.164
  CI (99.9%): [367.629, 399.697] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 91.67% complete, ETA 00:00:52
# Fork: 1 of 1
# Warmup Iteration   1: 930.176 ns/op
# Warmup Iteration   2: 901.276 ns/op
# Warmup Iteration   3: 887.991 ns/op
# Warmup Iteration   4: 882.405 ns/op
# Warmup Iteration   5: 905.881 ns/op
Iteration   1: 869.843 ns/op
Iteration   2: 887.922 ns/op
Iteration   3: 901.250 ns/op
Iteration   4: 881.385 ns/op
Iteration   5: 912.654 ns/op


Result "org.example.OneShot.yieldThenContinue":
  890.611 ±(99.9%) 64.512 ns/op [Average]
  (min, avg, max) = (869.843, 890.611, 912.654), stdev = 16.754
  CI (99.9%): [826.098, 955.123] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 93.33% complete, ETA 00:00:41
# Fork: 1 of 1
# Warmup Iteration   1: 306.803 ns/op
# Warmup Iteration   2: 277.576 ns/op
# Warmup Iteration   3: 285.499 ns/op
# Warmup Iteration   4: 288.436 ns/op
# Warmup Iteration   5: 283.285 ns/op
Iteration   1: 311.891 ns/op
Iteration   2: 293.598 ns/op
Iteration   3: 282.937 ns/op
Iteration   4: 292.999 ns/op
Iteration   5: 296.318 ns/op


Result "org.example.OneShot.yieldThenContinue":
  295.548 ±(99.9%) 40.249 ns/op [Average]
  (min, avg, max) = (282.937, 295.548, 311.891), stdev = 10.453
  CI (99.9%): [255.299, 335.798] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 95.00% complete, ETA 00:00:31
# Fork: 1 of 1
# Warmup Iteration   1: 347.495 ns/op
# Warmup Iteration   2: 340.763 ns/op
# Warmup Iteration   3: 349.127 ns/op
# Warmup Iteration   4: 348.113 ns/op
# Warmup Iteration   5: 358.341 ns/op
Iteration   1: 356.542 ns/op
Iteration   2: 350.991 ns/op
Iteration   3: 357.678 ns/op
Iteration   4: 350.683 ns/op
Iteration   5: 347.951 ns/op


Result "org.example.OneShot.yieldThenContinue":
  352.769 ±(99.9%) 16.000 ns/op [Average]
  (min, avg, max) = (347.951, 352.769, 357.678), stdev = 4.155
  CI (99.9%): [336.769, 368.769] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 96.67% complete, ETA 00:00:20
# Fork: 1 of 1
# Warmup Iteration   1: 482.327 ns/op
# Warmup Iteration   2: 469.821 ns/op
# Warmup Iteration   3: 492.060 ns/op
# Warmup Iteration   4: 477.711 ns/op
# Warmup Iteration   5: 467.002 ns/op
Iteration   1: 471.425 ns/op
Iteration   2: 472.509 ns/op
Iteration   3: 489.438 ns/op
Iteration   4: 484.559 ns/op
Iteration   5: 488.464 ns/op


Result "org.example.OneShot.yieldThenContinue":
  481.279 ±(99.9%) 33.512 ns/op [Average]
  (min, avg, max) = (471.425, 481.279, 489.438), stdev = 8.703
  CI (99.9%): [447.767, 514.791] (assumes normal distribution)


# JMH version: 1.37
# VM version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
# VM invoker: C:\TencentKona\TencentKona-8.0.13-362\jre\bin\java.exe
# VM options: <none>
# Blackhole mode: full + dont-inline hint (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)
# Warmup: 5 iterations, 1 s each
# Measurement: 5 iterations, 1 s each
# Timeout: 10 min per iteration
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Average time, time/op
# Benchmark: org.example.OneShot.yieldThenContinue
# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 98.33% complete, ETA 00:00:10
# Fork: 1 of 1
# Warmup Iteration   1: 1504.973 ns/op
# Warmup Iteration   2: 1357.623 ns/op
# Warmup Iteration   3: 1417.031 ns/op
# Warmup Iteration   4: 1431.621 ns/op
# Warmup Iteration   5: 1404.878 ns/op
Iteration   1: 1444.303 ns/op
Iteration   2: 1443.860 ns/op
Iteration   3: 1448.521 ns/op
Iteration   4: 1434.081 ns/op
Iteration   5: 1446.787 ns/op


Result "org.example.OneShot.yieldThenContinue":
  1443.510 ±(99.9%) 21.570 ns/op [Average]
  (min, avg, max) = (1434.081, 1443.510, 1448.521), stdev = 5.602
  CI (99.9%): [1421.941, 1465.080] (assumes normal distribution)


# Run complete. Total time: 00:10:24

REMEMBER: The numbers below are just data. To gain reusable insights, you need to follow up on
why the numbers are the way they are. Use profilers (see -prof, -lprof), design factorial
experiments, perform baseline and negative tests that provide experimental control, make sure
the benchmarking environment is safe on JVM/OS/HW level, ask for reviews from the domain experts.
Do not assume the numbers tell you what you want them to tell.
```

### Loom
```
# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 0.00% complete, ETA 00:12:00

# Fork: 1 of 1

# Warmup Iteration   1: 1301.331 ns/op

# Warmup Iteration   2: 1103.963 ns/op

# Warmup Iteration   3: 1080.763 ns/op

# Warmup Iteration   4: 1139.475 ns/op

# Warmup Iteration   5: 1055.219 ns/op

Iteration   1: 1059.249 ns/op
Iteration   2: 1039.204 ns/op
Iteration   3: 1046.993 ns/op
Iteration   4: 1091.392 ns/op
Iteration   5: 1074.650 ns/op


Result "org.example.Main.noYield":
1062.298 ±(99.9%) 81.136 ns/op [Average]
(min, avg, max) = (1039.204, 1062.298, 1091.392), stdev = 21.071
CI (99.9%): [981.161, 1143.434] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 1.39% complete, ETA 00:12:25

# Fork: 1 of 1

# Warmup Iteration   1: 1071.897 ns/op

# Warmup Iteration   2: 1172.869 ns/op

# Warmup Iteration   3: 1291.113 ns/op

# Warmup Iteration   4: 1242.909 ns/op

# Warmup Iteration   5: 1266.198 ns/op

Iteration   1: 1201.836 ns/op
Iteration   2: 1299.943 ns/op
Iteration   3: 1206.949 ns/op
Iteration   4: 1050.754 ns/op
Iteration   5: 1050.376 ns/op


Result "org.example.Main.noYield":
1161.972 ±(99.9%) 419.487 ns/op [Average]
(min, avg, max) = (1050.376, 1161.972, 1299.943), stdev = 108.939
CI (99.9%): [742.485, 1581.458] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 2.78% complete, ETA 00:12:09

# Fork: 1 of 1

# Warmup Iteration   1: 1188.791 ns/op

# Warmup Iteration   2: 1172.360 ns/op

# Warmup Iteration   3: 1156.357 ns/op

# Warmup Iteration   4: 1075.136 ns/op

# Warmup Iteration   5: 1168.925 ns/op

Iteration   1: 1046.882 ns/op
Iteration   2: 1120.703 ns/op
Iteration   3: 1042.947 ns/op
Iteration   4: 1034.797 ns/op
Iteration   5: 1132.554 ns/op


Result "org.example.Main.noYield":
1075.576 ±(99.9%) 180.958 ns/op [Average]
(min, avg, max) = (1034.797, 1075.576, 1132.554), stdev = 46.994
CI (99.9%): [894.619, 1256.534] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 4.17% complete, ETA 00:11:56

# Fork: 1 of 1

# Warmup Iteration   1: 1389.858 ns/op

# Warmup Iteration   2: 1564.943 ns/op

# Warmup Iteration   3: 1425.049 ns/op

# Warmup Iteration   4: 1434.532 ns/op

# Warmup Iteration   5: 1497.358 ns/op

Iteration   1: 1391.875 ns/op
Iteration   2: 1564.206 ns/op
Iteration   3: 1490.717 ns/op
Iteration   4: 1472.422 ns/op
Iteration   5: 1415.051 ns/op


Result "org.example.Main.noYield":
1466.854 ±(99.9%) 261.045 ns/op [Average]
(min, avg, max) = (1391.875, 1466.854, 1564.206), stdev = 67.792
CI (99.9%): [1205.810, 1727.899] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 5.56% complete, ETA 00:11:45

# Fork: 1 of 1

# Warmup Iteration   1: 1148.278 ns/op

# Warmup Iteration   2: 1230.054 ns/op

# Warmup Iteration   3: 1079.552 ns/op

# Warmup Iteration   4: 1077.057 ns/op

# Warmup Iteration   5: 1123.090 ns/op

Iteration   1: 1051.073 ns/op
Iteration   2: 1169.158 ns/op
Iteration   3: 1055.092 ns/op
Iteration   4: 1107.113 ns/op
Iteration   5: 1067.030 ns/op


Result "org.example.Main.noYield":
1089.893 ±(99.9%) 190.795 ns/op [Average]
(min, avg, max) = (1051.073, 1089.893, 1169.158), stdev = 49.549
CI (99.9%): [899.098, 1280.688] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 6.94% complete, ETA 00:11:34

# Fork: 1 of 1

# Warmup Iteration   1: 1124.544 ns/op

# Warmup Iteration   2: 1221.296 ns/op

# Warmup Iteration   3: 1103.172 ns/op

# Warmup Iteration   4: 1119.001 ns/op

# Warmup Iteration   5: 1097.024 ns/op

Iteration   1: 1094.088 ns/op
Iteration   2: 1120.315 ns/op
Iteration   3: 1250.835 ns/op
Iteration   4: 1160.269 ns/op
Iteration   5: 1104.532 ns/op


Result "org.example.Main.noYield":
1146.008 ±(99.9%) 245.568 ns/op [Average]
(min, avg, max) = (1094.088, 1146.008, 1250.835), stdev = 63.773
CI (99.9%): [900.440, 1391.576] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 8.33% complete, ETA 00:11:23

# Fork: 1 of 1

# Warmup Iteration   1: 1172.148 ns/op

# Warmup Iteration   2: 1176.513 ns/op

# Warmup Iteration   3: 1249.279 ns/op

# Warmup Iteration   4: 1221.368 ns/op

# Warmup Iteration   5: 1223.482 ns/op

Iteration   1: 1246.383 ns/op
Iteration   2: 1145.789 ns/op
Iteration   3: 1137.206 ns/op
Iteration   4: 1137.063 ns/op
Iteration   5: 1136.553 ns/op


Result "org.example.Main.noYield":
1160.599 ±(99.9%) 185.247 ns/op [Average]
(min, avg, max) = (1136.553, 1160.599, 1246.383), stdev = 48.108
CI (99.9%): [975.351, 1345.846] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 9.72% complete, ETA 00:11:13

# Fork: 1 of 1

# Warmup Iteration   1: 1657.268 ns/op

# Warmup Iteration   2: 1784.217 ns/op

# Warmup Iteration   3: 1771.527 ns/op

# Warmup Iteration   4: 1745.747 ns/op

# Warmup Iteration   5: 1696.354 ns/op

Iteration   1: 1628.052 ns/op
Iteration   2: 1732.731 ns/op
Iteration   3: 1973.921 ns/op
Iteration   4: 1663.955 ns/op
Iteration   5: 1711.922 ns/op


Result "org.example.Main.noYield":
1742.116 ±(99.9%) 523.224 ns/op [Average]
(min, avg, max) = (1628.052, 1742.116, 1973.921), stdev = 135.880
CI (99.9%): [1218.892, 2265.340] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 11.11% complete, ETA 00:11:02

# Fork: 1 of 1

# Warmup Iteration   1: 1164.035 ns/op

# Warmup Iteration   2: 1239.977 ns/op

# Warmup Iteration   3: 1251.812 ns/op

# Warmup Iteration   4: 1112.043 ns/op

# Warmup Iteration   5: 1119.362 ns/op

Iteration   1: 1146.852 ns/op
Iteration   2: 1355.782 ns/op
Iteration   3: 1202.031 ns/op
Iteration   4: 1308.537 ns/op
Iteration   5: 1094.478 ns/op


Result "org.example.Main.noYield":
1221.536 ±(99.9%) 420.466 ns/op [Average]
(min, avg, max) = (1094.478, 1221.536, 1355.782), stdev = 109.194
CI (99.9%): [801.070, 1642.002] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 12.50% complete, ETA 00:10:52

# Fork: 1 of 1

# Warmup Iteration   1: 1333.213 ns/op

# Warmup Iteration   2: 1159.838 ns/op

# Warmup Iteration   3: 1154.218 ns/op

# Warmup Iteration   4: 1390.189 ns/op

# Warmup Iteration   5: 1146.667 ns/op

Iteration   1: 1347.215 ns/op
Iteration   2: 1168.100 ns/op
Iteration   3: 1240.451 ns/op
Iteration   4: 1161.191 ns/op
Iteration   5: 1184.881 ns/op


Result "org.example.Main.noYield":
1220.368 ±(99.9%) 298.187 ns/op [Average]
(min, avg, max) = (1161.191, 1220.368, 1347.215), stdev = 77.438
CI (99.9%): [922.181, 1518.554] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 13.89% complete, ETA 00:10:41

# Fork: 1 of 1

# Warmup Iteration   1: 1500.822 ns/op

# Warmup Iteration   2: 1396.452 ns/op

# Warmup Iteration   3: 1449.426 ns/op

# Warmup Iteration   4: 1249.838 ns/op

# Warmup Iteration   5: 1315.865 ns/op

Iteration   1: 1287.072 ns/op
Iteration   2: 1293.108 ns/op
Iteration   3: 1256.183 ns/op
Iteration   4: 1465.719 ns/op
Iteration   5: 1253.401 ns/op


Result "org.example.Main.noYield":
1311.096 ±(99.9%) 339.824 ns/op [Average]
(min, avg, max) = (1253.401, 1311.096, 1465.719), stdev = 88.251
CI (99.9%): [971.273, 1650.920] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.noYield

# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 15.28% complete, ETA 00:10:31

# Fork: 1 of 1

# Warmup Iteration   1: 2399.848 ns/op

# Warmup Iteration   2: 2141.054 ns/op

# Warmup Iteration   3: 2129.267 ns/op

# Warmup Iteration   4: 2172.954 ns/op

# Warmup Iteration   5: 2156.883 ns/op

Iteration   1: 2218.819 ns/op
Iteration   2: 2100.285 ns/op
Iteration   3: 2115.085 ns/op
Iteration   4: 2106.748 ns/op
Iteration   5: 2239.206 ns/op


Result "org.example.Main.noYield":
2156.029 ±(99.9%) 258.836 ns/op [Average]
(min, avg, max) = (2100.285, 2156.029, 2239.206), stdev = 67.219
CI (99.9%): [1897.193, 2414.864] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 16.67% complete, ETA 00:10:20

# Fork: 1 of 1

# Warmup Iteration   1: 1282.337 ns/op

# Warmup Iteration   2: 1191.414 ns/op

# Warmup Iteration   3: 1190.361 ns/op

# Warmup Iteration   4: 1188.094 ns/op

# Warmup Iteration   5: 1245.975 ns/op

Iteration   1: 1199.664 ns/op
Iteration   2: 1292.876 ns/op
Iteration   3: 1181.523 ns/op
Iteration   4: 1230.262 ns/op
Iteration   5: 1212.667 ns/op


Result "org.example.Main.yield":
1223.398 ±(99.9%) 164.570 ns/op [Average]
(min, avg, max) = (1181.523, 1223.398, 1292.876), stdev = 42.738
CI (99.9%): [1058.828, 1387.968] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 18.06% complete, ETA 00:10:10

# Fork: 1 of 1

# Warmup Iteration   1: 1310.243 ns/op

# Warmup Iteration   2: 1371.540 ns/op

# Warmup Iteration   3: 1187.404 ns/op

# Warmup Iteration   4: 1282.164 ns/op

# Warmup Iteration   5: 1205.499 ns/op

Iteration   1: 1426.180 ns/op
Iteration   2: 1264.453 ns/op
Iteration   3: 1335.652 ns/op
Iteration   4: 1179.739 ns/op
Iteration   5: 1325.827 ns/op


Result "org.example.Main.yield":
1306.370 ±(99.9%) 351.775 ns/op [Average]
(min, avg, max) = (1179.739, 1306.370, 1426.180), stdev = 91.355
CI (99.9%): [954.595, 1658.146] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 19.44% complete, ETA 00:10:00

# Fork: 1 of 1

# Warmup Iteration   1: 1288.486 ns/op

# Warmup Iteration   2: 1206.561 ns/op

# Warmup Iteration   3: 1202.423 ns/op

# Warmup Iteration   4: 1300.798 ns/op

# Warmup Iteration   5: 1235.243 ns/op

Iteration   1: 1309.373 ns/op
Iteration   2: 1604.914 ns/op
Iteration   3: 1656.769 ns/op
Iteration   4: 1489.170 ns/op
Iteration   5: 1596.360 ns/op


Result "org.example.Main.yield":
1531.317 ±(99.9%) 532.242 ns/op [Average]
(min, avg, max) = (1309.373, 1531.317, 1656.769), stdev = 138.222
CI (99.9%): [999.074, 2063.559] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 20.83% complete, ETA 00:09:49

# Fork: 1 of 1

# Warmup Iteration   1: 2107.484 ns/op

# Warmup Iteration   2: 1924.535 ns/op

# Warmup Iteration   3: 1781.010 ns/op

# Warmup Iteration   4: 1870.295 ns/op

# Warmup Iteration   5: 1845.442 ns/op

Iteration   1: 1753.743 ns/op
Iteration   2: 1807.905 ns/op
Iteration   3: 1829.077 ns/op
Iteration   4: 1852.526 ns/op
Iteration   5: 1827.855 ns/op


Result "org.example.Main.yield":
1814.221 ±(99.9%) 143.709 ns/op [Average]
(min, avg, max) = (1753.743, 1814.221, 1852.526), stdev = 37.321
CI (99.9%): [1670.512, 1957.929] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 22.22% complete, ETA 00:09:39

# Fork: 1 of 1

# Warmup Iteration   1: 1491.113 ns/op

# Warmup Iteration   2: 1485.265 ns/op

# Warmup Iteration   3: 1442.213 ns/op

# Warmup Iteration   4: 1460.346 ns/op

# Warmup Iteration   5: 1527.542 ns/op

Iteration   1: 1460.438 ns/op
Iteration   2: 1369.190 ns/op
Iteration   3: 1243.045 ns/op
Iteration   4: 1259.583 ns/op
Iteration   5: 1465.012 ns/op


Result "org.example.Main.yield":
1359.454 ±(99.9%) 408.243 ns/op [Average]
(min, avg, max) = (1243.045, 1359.454, 1465.012), stdev = 106.019
CI (99.9%): [951.210, 1767.697] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 23.61% complete, ETA 00:09:28

# Fork: 1 of 1

# Warmup Iteration   1: 1364.972 ns/op

# Warmup Iteration   2: 1244.839 ns/op

# Warmup Iteration   3: 1289.288 ns/op

# Warmup Iteration   4: 1316.945 ns/op

# Warmup Iteration   5: 1235.715 ns/op

Iteration   1: 1302.363 ns/op
Iteration   2: 1178.304 ns/op
Iteration   3: 1225.725 ns/op
Iteration   4: 1264.738 ns/op
Iteration   5: 1238.461 ns/op


Result "org.example.Main.yield":
1241.918 ±(99.9%) 177.456 ns/op [Average]
(min, avg, max) = (1178.304, 1241.918, 1302.363), stdev = 46.085
CI (99.9%): [1064.462, 1419.374] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 25.00% complete, ETA 00:09:18

# Fork: 1 of 1

# Warmup Iteration   1: 1355.686 ns/op

# Warmup Iteration   2: 1220.044 ns/op

# Warmup Iteration   3: 1228.794 ns/op

# Warmup Iteration   4: 1258.595 ns/op

# Warmup Iteration   5: 1214.643 ns/op

Iteration   1: 1214.906 ns/op
Iteration   2: 1288.052 ns/op
Iteration   3: 1372.991 ns/op
Iteration   4: 1446.398 ns/op
Iteration   5: 1520.237 ns/op


Result "org.example.Main.yield":
1368.517 ±(99.9%) 468.354 ns/op [Average]
(min, avg, max) = (1214.906, 1368.517, 1520.237), stdev = 121.630
CI (99.9%): [900.162, 1836.871] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 26.39% complete, ETA 00:09:08

# Fork: 1 of 1

# Warmup Iteration   1: 2144.780 ns/op

# Warmup Iteration   2: 2028.625 ns/op

# Warmup Iteration   3: 1863.435 ns/op

# Warmup Iteration   4: 1805.706 ns/op

# Warmup Iteration   5: 1882.636 ns/op

Iteration   1: 1926.317 ns/op
Iteration   2: 2006.228 ns/op
Iteration   3: 1903.900 ns/op
Iteration   4: 1866.118 ns/op
Iteration   5: 1873.755 ns/op


Result "org.example.Main.yield":
1915.263 ±(99.9%) 216.665 ns/op [Average]
(min, avg, max) = (1866.118, 1915.263, 2006.228), stdev = 56.267
CI (99.9%): [1698.598, 2131.929] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 27.78% complete, ETA 00:08:57

# Fork: 1 of 1

# Warmup Iteration   1: 1335.192 ns/op

# Warmup Iteration   2: 1310.390 ns/op

# Warmup Iteration   3: 1186.464 ns/op

# Warmup Iteration   4: 1159.521 ns/op

# Warmup Iteration   5: 1212.120 ns/op

Iteration   1: 1227.276 ns/op
Iteration   2: 1203.451 ns/op
Iteration   3: 1220.650 ns/op
Iteration   4: 1166.918 ns/op
Iteration   5: 1257.351 ns/op


Result "org.example.Main.yield":
1215.129 ±(99.9%) 128.022 ns/op [Average]
(min, avg, max) = (1166.918, 1215.129, 1257.351), stdev = 33.247
CI (99.9%): [1087.107, 1343.151] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 29.17% complete, ETA 00:08:47

# Fork: 1 of 1

# Warmup Iteration   1: 1393.908 ns/op

# Warmup Iteration   2: 1245.224 ns/op

# Warmup Iteration   3: 1201.918 ns/op

# Warmup Iteration   4: 1287.596 ns/op

# Warmup Iteration   5: 1201.953 ns/op

Iteration   1: 1217.073 ns/op
Iteration   2: 1187.444 ns/op
Iteration   3: 1308.950 ns/op
Iteration   4: 1277.816 ns/op
Iteration   5: 1307.298 ns/op


Result "org.example.Main.yield":
1259.716 ±(99.9%) 211.411 ns/op [Average]
(min, avg, max) = (1187.444, 1259.716, 1308.950), stdev = 54.903
CI (99.9%): [1048.305, 1471.128] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 30.56% complete, ETA 00:08:37

# Fork: 1 of 1

# Warmup Iteration   1: 1438.082 ns/op

# Warmup Iteration   2: 1267.377 ns/op

# Warmup Iteration   3: 1315.895 ns/op

# Warmup Iteration   4: 1347.279 ns/op

# Warmup Iteration   5: 1314.381 ns/op

Iteration   1: 1242.336 ns/op
Iteration   2: 1291.737 ns/op
Iteration   3: 1290.086 ns/op
Iteration   4: 1241.582 ns/op
Iteration   5: 1291.015 ns/op


Result "org.example.Main.yield":
1271.351 ±(99.9%) 103.347 ns/op [Average]
(min, avg, max) = (1241.582, 1271.351, 1291.737), stdev = 26.839
CI (99.9%): [1168.004, 1374.699] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yield

# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 31.94% complete, ETA 00:08:26

# Fork: 1 of 1

# Warmup Iteration   1: 1915.018 ns/op

# Warmup Iteration   2: 1677.904 ns/op

# Warmup Iteration   3: 1705.588 ns/op

# Warmup Iteration   4: 1747.117 ns/op

# Warmup Iteration   5: 1742.977 ns/op

Iteration   1: 1655.815 ns/op
Iteration   2: 1650.256 ns/op
Iteration   3: 1658.142 ns/op
Iteration   4: 1654.030 ns/op
Iteration   5: 1671.968 ns/op


Result "org.example.Main.yield":
1658.042 ±(99.9%) 31.964 ns/op [Average]
(min, avg, max) = (1650.256, 1658.042, 1671.968), stdev = 8.301
CI (99.9%): [1626.078, 1690.006] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 33.33% complete, ETA 00:08:16

# Fork: 1 of 1

# Warmup Iteration   1: 1953.658 ns/op

# Warmup Iteration   2: 1786.443 ns/op

# Warmup Iteration   3: 1891.034 ns/op

# Warmup Iteration   4: 1921.874 ns/op

# Warmup Iteration   5: 1912.864 ns/op

Iteration   1: 1744.979 ns/op
Iteration   2: 1880.490 ns/op
Iteration   3: 1872.060 ns/op
Iteration   4: 1946.283 ns/op
Iteration   5: 1871.597 ns/op


Result "org.example.Main.yieldAfterEachCall":
1863.082 ±(99.9%) 281.169 ns/op [Average]
(min, avg, max) = (1744.979, 1863.082, 1946.283), stdev = 73.019
CI (99.9%): [1581.913, 2144.250] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 34.72% complete, ETA 00:08:06

# Fork: 1 of 1

# Warmup Iteration   1: 4712.314 ns/op

# Warmup Iteration   2: 3320.433 ns/op

# Warmup Iteration   3: 3216.891 ns/op

# Warmup Iteration   4: 2592.303 ns/op

# Warmup Iteration   5: 2612.975 ns/op

Iteration   1: 2867.392 ns/op
Iteration   2: 3426.383 ns/op
Iteration   3: 2784.469 ns/op
Iteration   4: 2845.598 ns/op
Iteration   5: 2611.756 ns/op


Result "org.example.Main.yieldAfterEachCall":
2907.120 ±(99.9%) 1182.594 ns/op [Average]
(min, avg, max) = (2611.756, 2907.120, 3426.383), stdev = 307.116
CI (99.9%): [1724.526, 4089.714] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 36.11% complete, ETA 00:07:55

# Fork: 1 of 1

# Warmup Iteration   1: 4775.369 ns/op

# Warmup Iteration   2: 4532.335 ns/op

# Warmup Iteration   3: 4353.730 ns/op

# Warmup Iteration   4: 4313.148 ns/op

# Warmup Iteration   5: 3845.079 ns/op

Iteration   1: 3705.944 ns/op
Iteration   2: 4178.427 ns/op
Iteration   3: 3777.159 ns/op
Iteration   4: 3962.777 ns/op
Iteration   5: 3744.271 ns/op


Result "org.example.Main.yieldAfterEachCall":
3873.716 ±(99.9%) 758.018 ns/op [Average]
(min, avg, max) = (3705.944, 3873.716, 4178.427), stdev = 196.855
CI (99.9%): [3115.697, 4631.734] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 37.50% complete, ETA 00:07:45

# Fork: 1 of 1

# Warmup Iteration   1: 18253.519 ns/op

# Warmup Iteration   2: 15807.651 ns/op

# Warmup Iteration   3: 15945.978 ns/op

# Warmup Iteration   4: 15909.315 ns/op

# Warmup Iteration   5: 18336.854 ns/op

Iteration   1: 16651.142 ns/op
Iteration   2: 15489.780 ns/op
Iteration   3: 16006.631 ns/op
Iteration   4: 16839.953 ns/op
Iteration   5: 15527.793 ns/op


Result "org.example.Main.yieldAfterEachCall":
16103.060 ±(99.9%) 2404.533 ns/op [Average]
(min, avg, max) = (15489.780, 16103.060, 16839.953), stdev = 624.450
CI (99.9%): [13698.527, 18507.593] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 38.89% complete, ETA 00:07:35

# Fork: 1 of 1

# Warmup Iteration   1: 2004.069 ns/op

# Warmup Iteration   2: 1818.613 ns/op

# Warmup Iteration   3: 1773.825 ns/op

# Warmup Iteration   4: 1787.369 ns/op

# Warmup Iteration   5: 1925.256 ns/op

Iteration   1: 1914.280 ns/op
Iteration   2: 1769.524 ns/op
Iteration   3: 1816.525 ns/op
Iteration   4: 1782.024 ns/op
Iteration   5: 1826.405 ns/op


Result "org.example.Main.yieldAfterEachCall":
1821.752 ±(99.9%) 218.814 ns/op [Average]
(min, avg, max) = (1769.524, 1821.752, 1914.280), stdev = 56.825
CI (99.9%): [1602.937, 2040.566] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 40.28% complete, ETA 00:07:24

# Fork: 1 of 1

# Warmup Iteration   1: 2631.145 ns/op

# Warmup Iteration   2: 2716.551 ns/op

# Warmup Iteration   3: 2476.293 ns/op

# Warmup Iteration   4: 2487.635 ns/op

# Warmup Iteration   5: 2786.072 ns/op

Iteration   1: 2421.138 ns/op
Iteration   2: 2416.875 ns/op
Iteration   3: 2415.642 ns/op
Iteration   4: 2594.400 ns/op
Iteration   5: 2472.042 ns/op


Result "org.example.Main.yieldAfterEachCall":
2464.019 ±(99.9%) 294.927 ns/op [Average]
(min, avg, max) = (2415.642, 2464.019, 2594.400), stdev = 76.592
CI (99.9%): [2169.092, 2758.946] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 41.67% complete, ETA 00:07:14

# Fork: 1 of 1

# Warmup Iteration   1: 4198.851 ns/op

# Warmup Iteration   2: 3908.370 ns/op

# Warmup Iteration   3: 3695.024 ns/op

# Warmup Iteration   4: 3697.451 ns/op

# Warmup Iteration   5: 3767.793 ns/op

Iteration   1: 3840.471 ns/op
Iteration   2: 3695.160 ns/op
Iteration   3: 3974.510 ns/op
Iteration   4: 4152.315 ns/op
Iteration   5: 4327.659 ns/op


Result "org.example.Main.yieldAfterEachCall":
3998.023 ±(99.9%) 961.828 ns/op [Average]
(min, avg, max) = (3695.160, 3998.023, 4327.659), stdev = 249.784
CI (99.9%): [3036.195, 4959.851] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 43.06% complete, ETA 00:07:04

# Fork: 1 of 1

# Warmup Iteration   1: 19977.754 ns/op

# Warmup Iteration   2: 20430.434 ns/op

# Warmup Iteration   3: 17607.953 ns/op

# Warmup Iteration   4: 16113.502 ns/op

# Warmup Iteration   5: 17873.185 ns/op

Iteration   1: 16470.476 ns/op
Iteration   2: 16110.981 ns/op
Iteration   3: 16460.063 ns/op
Iteration   4: 16792.958 ns/op
Iteration   5: 18762.072 ns/op


Result "org.example.Main.yieldAfterEachCall":
16919.310 ±(99.9%) 4073.997 ns/op [Average]
(min, avg, max) = (16110.981, 16919.310, 18762.072), stdev = 1058.004
CI (99.9%): [12845.312, 20993.307] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 44.44% complete, ETA 00:06:53

# Fork: 1 of 1

# Warmup Iteration   1: 1932.967 ns/op

# Warmup Iteration   2: 1774.574 ns/op

# Warmup Iteration   3: 1828.527 ns/op

# Warmup Iteration   4: 1879.863 ns/op

# Warmup Iteration   5: 1960.328 ns/op

Iteration   1: 1801.554 ns/op
Iteration   2: 1861.797 ns/op
Iteration   3: 1784.028 ns/op
Iteration   4: 1793.487 ns/op
Iteration   5: 1840.748 ns/op


Result "org.example.Main.yieldAfterEachCall":
1816.323 ±(99.9%) 128.392 ns/op [Average]
(min, avg, max) = (1784.028, 1816.323, 1861.797), stdev = 33.343
CI (99.9%): [1687.931, 1944.714] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 45.83% complete, ETA 00:06:43

# Fork: 1 of 1

# Warmup Iteration   1: 2981.857 ns/op

# Warmup Iteration   2: 2863.672 ns/op

# Warmup Iteration   3: 2680.730 ns/op

# Warmup Iteration   4: 2635.918 ns/op

# Warmup Iteration   5: 2852.067 ns/op

Iteration   1: 2741.693 ns/op
Iteration   2: 2544.839 ns/op
Iteration   3: 2801.513 ns/op
Iteration   4: 2772.503 ns/op
Iteration   5: 2544.473 ns/op


Result "org.example.Main.yieldAfterEachCall":
2681.004 ±(99.9%) 486.156 ns/op [Average]
(min, avg, max) = (2544.473, 2681.004, 2801.513), stdev = 126.253
CI (99.9%): [2194.848, 3167.160] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 47.22% complete, ETA 00:06:33

# Fork: 1 of 1

# Warmup Iteration   1: 4048.746 ns/op

# Warmup Iteration   2: 3932.595 ns/op

# Warmup Iteration   3: 3726.823 ns/op

# Warmup Iteration   4: 3879.607 ns/op

# Warmup Iteration   5: 3965.611 ns/op

Iteration   1: 3761.677 ns/op
Iteration   2: 3896.001 ns/op
Iteration   3: 4024.654 ns/op
Iteration   4: 3749.135 ns/op
Iteration   5: 4026.439 ns/op


Result "org.example.Main.yieldAfterEachCall":
3891.581 ±(99.9%) 520.480 ns/op [Average]
(min, avg, max) = (3749.135, 3891.581, 4026.439), stdev = 135.167
CI (99.9%): [3371.101, 4412.062] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldAfterEachCall

# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 48.61% complete, ETA 00:06:22

# Fork: 1 of 1

# Warmup Iteration   1: 16932.551 ns/op

# Warmup Iteration   2: 16240.756 ns/op

# Warmup Iteration   3: 17380.680 ns/op

# Warmup Iteration   4: 16288.053 ns/op

# Warmup Iteration   5: 16820.729 ns/op

Iteration   1: 16348.272 ns/op
Iteration   2: 17430.331 ns/op
Iteration   3: 19812.650 ns/op
Iteration   4: 17688.399 ns/op
Iteration   5: 15814.403 ns/op


Result "org.example.Main.yieldAfterEachCall":
17418.811 ±(99.9%) 5941.562 ns/op [Average]
(min, avg, max) = (15814.403, 17418.811, 19812.650), stdev = 1543.005
CI (99.9%): [11477.249, 23360.373] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 50.00% complete, ETA 00:06:12

# Fork: 1 of 1

# Warmup Iteration   1: 2527.787 ns/op

# Warmup Iteration   2: 2430.714 ns/op

# Warmup Iteration   3: 2396.522 ns/op

# Warmup Iteration   4: 2498.036 ns/op

# Warmup Iteration   5: 2472.645 ns/op

Iteration   1: 2440.343 ns/op
Iteration   2: 2632.076 ns/op
Iteration   3: 2576.063 ns/op
Iteration   4: 2581.812 ns/op
Iteration   5: 2649.496 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
2575.958 ±(99.9%) 316.263 ns/op [Average]
(min, avg, max) = (2440.343, 2575.958, 2649.496), stdev = 82.132
CI (99.9%): [2259.695, 2892.221] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 51.39% complete, ETA 00:06:02

# Fork: 1 of 1

# Warmup Iteration   1: 4065.886 ns/op

# Warmup Iteration   2: 3832.850 ns/op

# Warmup Iteration   3: 3918.448 ns/op

# Warmup Iteration   4: 3810.223 ns/op

# Warmup Iteration   5: 3907.449 ns/op

Iteration   1: 3753.989 ns/op
Iteration   2: 3791.954 ns/op
Iteration   3: 3875.234 ns/op
Iteration   4: 3809.001 ns/op
Iteration   5: 3932.542 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
3832.544 ±(99.9%) 273.604 ns/op [Average]
(min, avg, max) = (3753.989, 3832.544, 3932.542), stdev = 71.054
CI (99.9%): [3558.940, 4106.148] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 52.78% complete, ETA 00:05:51

# Fork: 1 of 1

# Warmup Iteration   1: 7091.424 ns/op

# Warmup Iteration   2: 6855.850 ns/op

# Warmup Iteration   3: 6894.075 ns/op

# Warmup Iteration   4: 7006.199 ns/op

# Warmup Iteration   5: 6584.375 ns/op

Iteration   1: 6670.407 ns/op
Iteration   2: 7101.806 ns/op
Iteration   3: 6772.385 ns/op
Iteration   4: 6578.256 ns/op
Iteration   5: 7052.477 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
6835.066 ±(99.9%) 893.587 ns/op [Average]
(min, avg, max) = (6578.256, 6835.066, 7101.806), stdev = 232.062
CI (99.9%): [5941.479, 7728.654] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 54.17% complete, ETA 00:05:41

# Fork: 1 of 1

# Warmup Iteration   1: 40446.942 ns/op

# Warmup Iteration   2: 40804.080 ns/op

# Warmup Iteration   3: 39545.131 ns/op

# Warmup Iteration   4: 35798.222 ns/op

# Warmup Iteration   5: 36382.243 ns/op

Iteration   1: 37038.576 ns/op
Iteration   2: 38187.013 ns/op
Iteration   3: 36170.173 ns/op
Iteration   4: 36684.351 ns/op
Iteration   5: 37101.195 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
37036.262 ±(99.9%) 2857.272 ns/op [Average]
(min, avg, max) = (36170.173, 37036.262, 38187.013), stdev = 742.025
CI (99.9%): [34178.989, 39893.534] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 55.56% complete, ETA 00:05:30

# Fork: 1 of 1

# Warmup Iteration   1: 2665.307 ns/op

# Warmup Iteration   2: 2493.496 ns/op

# Warmup Iteration   3: 2416.597 ns/op

# Warmup Iteration   4: 2512.405 ns/op

# Warmup Iteration   5: 2499.367 ns/op

Iteration   1: 2537.641 ns/op
Iteration   2: 2431.720 ns/op
Iteration   3: 2679.362 ns/op
Iteration   4: 2595.009 ns/op
Iteration   5: 2599.032 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
2568.553 ±(99.9%) 352.845 ns/op [Average]
(min, avg, max) = (2431.720, 2568.553, 2679.362), stdev = 91.633
CI (99.9%): [2215.708, 2921.398] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 56.94% complete, ETA 00:05:20

# Fork: 1 of 1

# Warmup Iteration   1: 4207.964 ns/op

# Warmup Iteration   2: 3846.163 ns/op

# Warmup Iteration   3: 3712.446 ns/op

# Warmup Iteration   4: 3762.675 ns/op

# Warmup Iteration   5: 4043.544 ns/op

Iteration   1: 3901.436 ns/op
Iteration   2: 4349.146 ns/op
Iteration   3: 4692.265 ns/op
Iteration   4: 4476.682 ns/op
Iteration   5: 3914.360 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
4266.778 ±(99.9%) 1347.114 ns/op [Average]
(min, avg, max) = (3901.436, 4266.778, 4692.265), stdev = 349.841
CI (99.9%): [2919.663, 5613.892] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 58.33% complete, ETA 00:05:10

# Fork: 1 of 1

# Warmup Iteration   1: 7177.447 ns/op

# Warmup Iteration   2: 8053.406 ns/op

# Warmup Iteration   3: 8950.484 ns/op

# Warmup Iteration   4: 8572.255 ns/op

# Warmup Iteration   5: 8466.206 ns/op

Iteration   1: 8006.059 ns/op
Iteration   2: 8557.715 ns/op
Iteration   3: 8438.970 ns/op
Iteration   4: 8220.330 ns/op
Iteration   5: 8800.458 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
8404.707 ±(99.9%) 1177.629 ns/op [Average]
(min, avg, max) = (8006.059, 8404.707, 8800.458), stdev = 305.827
CI (99.9%): [7227.077, 9582.336] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 59.72% complete, ETA 00:04:59

# Fork: 1 of 1

# Warmup Iteration   1: 55104.197 ns/op

# Warmup Iteration   2: 49434.806 ns/op

# Warmup Iteration   3: 48288.884 ns/op

# Warmup Iteration   4: 45280.910 ns/op

# Warmup Iteration   5: 39393.089 ns/op

Iteration   1: 39534.598 ns/op
Iteration   2: 38975.860 ns/op
Iteration   3: 39874.068 ns/op
Iteration   4: 39131.085 ns/op
Iteration   5: 41592.209 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
39821.564 ±(99.9%) 4044.214 ns/op [Average]
(min, avg, max) = (38975.860, 39821.564, 41592.209), stdev = 1050.270
CI (99.9%): [35777.350, 43865.778] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 61.11% complete, ETA 00:04:49

# Fork: 1 of 1

# Warmup Iteration   1: 2804.436 ns/op

# Warmup Iteration   2: 2404.084 ns/op

# Warmup Iteration   3: 3147.607 ns/op

# Warmup Iteration   4: 2634.517 ns/op

# Warmup Iteration   5: 2483.883 ns/op

Iteration   1: 2523.229 ns/op
Iteration   2: 2440.719 ns/op
Iteration   3: 2428.383 ns/op
Iteration   4: 2536.611 ns/op
Iteration   5: 2494.288 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
2484.646 ±(99.9%) 186.439 ns/op [Average]
(min, avg, max) = (2428.383, 2484.646, 2536.611), stdev = 48.418
CI (99.9%): [2298.206, 2671.085] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 62.50% complete, ETA 00:04:39

# Fork: 1 of 1

# Warmup Iteration   1: 4137.121 ns/op

# Warmup Iteration   2: 3884.824 ns/op

# Warmup Iteration   3: 3959.936 ns/op

# Warmup Iteration   4: 4227.121 ns/op

# Warmup Iteration   5: 4588.824 ns/op

Iteration   1: 3902.130 ns/op
Iteration   2: 4187.989 ns/op
Iteration   3: 3919.216 ns/op
Iteration   4: 4067.538 ns/op
Iteration   5: 3996.987 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
4014.772 ±(99.9%) 451.216 ns/op [Average]
(min, avg, max) = (3902.130, 4014.772, 4187.989), stdev = 117.179
CI (99.9%): [3563.556, 4465.988] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 63.89% complete, ETA 00:04:28

# Fork: 1 of 1

# Warmup Iteration   1: 7785.116 ns/op

# Warmup Iteration   2: 6828.763 ns/op

# Warmup Iteration   3: 7037.561 ns/op

# Warmup Iteration   4: 6960.027 ns/op

# Warmup Iteration   5: 6883.076 ns/op

Iteration   1: 7625.108 ns/op
Iteration   2: 7359.965 ns/op
Iteration   3: 7148.426 ns/op
Iteration   4: 6870.234 ns/op
Iteration   5: 7108.718 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
7222.490 ±(99.9%) 1095.091 ns/op [Average]
(min, avg, max) = (6870.234, 7222.490, 7625.108), stdev = 284.392
CI (99.9%): [6127.399, 8317.581] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeAndAfterEachCall

# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 65.28% complete, ETA 00:04:18

# Fork: 1 of 1

# Warmup Iteration   1: 46019.544 ns/op

# Warmup Iteration   2: 40273.618 ns/op

# Warmup Iteration   3: 40705.789 ns/op

# Warmup Iteration   4: 39690.809 ns/op

# Warmup Iteration   5: 40561.220 ns/op

Iteration   1: 41023.015 ns/op
Iteration   2: 39185.555 ns/op
Iteration   3: 39519.256 ns/op
Iteration   4: 39373.544 ns/op
Iteration   5: 43644.807 ns/op


Result "org.example.Main.yieldBeforeAndAfterEachCall":
40549.236 ±(99.9%) 7232.080 ns/op [Average]
(min, avg, max) = (39185.555, 40549.236, 43644.807), stdev = 1878.148
CI (99.9%): [33317.156, 47781.315] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 66.67% complete, ETA 00:04:08

# Fork: 1 of 1

# Warmup Iteration   1: 2171.641 ns/op

# Warmup Iteration   2: 2106.811 ns/op

# Warmup Iteration   3: 1840.785 ns/op

# Warmup Iteration   4: 1968.614 ns/op

# Warmup Iteration   5: 1956.025 ns/op

Iteration   1: 1981.127 ns/op
Iteration   2: 1943.383 ns/op
Iteration   3: 1859.725 ns/op
Iteration   4: 1975.603 ns/op
Iteration   5: 2046.483 ns/op


Result "org.example.Main.yieldBeforeEachCall":
1961.264 ±(99.9%) 261.818 ns/op [Average]
(min, avg, max) = (1859.725, 1961.264, 2046.483), stdev = 67.993
CI (99.9%): [1699.446, 2223.082] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 68.06% complete, ETA 00:03:57

# Fork: 1 of 1

# Warmup Iteration   1: 2798.620 ns/op

# Warmup Iteration   2: 3214.495 ns/op

# Warmup Iteration   3: 2637.995 ns/op

# Warmup Iteration   4: 2635.311 ns/op

# Warmup Iteration   5: 2660.127 ns/op

Iteration   1: 2664.425 ns/op
Iteration   2: 2631.290 ns/op
Iteration   3: 2727.402 ns/op
Iteration   4: 2691.730 ns/op
Iteration   5: 2596.119 ns/op


Result "org.example.Main.yieldBeforeEachCall":
2662.193 ±(99.9%) 196.821 ns/op [Average]
(min, avg, max) = (2596.119, 2662.193, 2727.402), stdev = 51.114
CI (99.9%): [2465.373, 2859.014] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 69.44% complete, ETA 00:03:47

# Fork: 1 of 1

# Warmup Iteration   1: 4618.439 ns/op

# Warmup Iteration   2: 4436.698 ns/op

# Warmup Iteration   3: 4104.441 ns/op

# Warmup Iteration   4: 4332.501 ns/op

# Warmup Iteration   5: 4304.605 ns/op

Iteration   1: 4162.882 ns/op
Iteration   2: 4294.051 ns/op
Iteration   3: 4099.166 ns/op
Iteration   4: 4682.027 ns/op
Iteration   5: 4150.240 ns/op


Result "org.example.Main.yieldBeforeEachCall":
4277.673 ±(99.9%) 913.365 ns/op [Average]
(min, avg, max) = (4099.166, 4277.673, 4682.027), stdev = 237.198
CI (99.9%): [3364.308, 5191.039] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 70.83% complete, ETA 00:03:37

# Fork: 1 of 1

# Warmup Iteration   1: 24892.623 ns/op

# Warmup Iteration   2: 21949.845 ns/op

# Warmup Iteration   3: 21146.744 ns/op

# Warmup Iteration   4: 21430.201 ns/op

# Warmup Iteration   5: 24556.458 ns/op

Iteration   1: 26882.268 ns/op
Iteration   2: 26133.098 ns/op
Iteration   3: 23062.041 ns/op
Iteration   4: 21751.520 ns/op
Iteration   5: 23362.905 ns/op


Result "org.example.Main.yieldBeforeEachCall":
24238.366 ±(99.9%) 8373.447 ns/op [Average]
(min, avg, max) = (21751.520, 24238.366, 26882.268), stdev = 2174.558
CI (99.9%): [15864.919, 32611.813] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 72.22% complete, ETA 00:03:26

# Fork: 1 of 1

# Warmup Iteration   1: 2132.255 ns/op

# Warmup Iteration   2: 1840.705 ns/op

# Warmup Iteration   3: 1847.015 ns/op

# Warmup Iteration   4: 2033.624 ns/op

# Warmup Iteration   5: 1895.287 ns/op

Iteration   1: 1852.993 ns/op
Iteration   2: 1868.507 ns/op
Iteration   3: 1858.398 ns/op
Iteration   4: 1928.392 ns/op
Iteration   5: 1845.720 ns/op


Result "org.example.Main.yieldBeforeEachCall":
1870.802 ±(99.9%) 128.031 ns/op [Average]
(min, avg, max) = (1845.720, 1870.802, 1928.392), stdev = 33.249
CI (99.9%): [1742.772, 1998.833] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 73.61% complete, ETA 00:03:16

# Fork: 1 of 1

# Warmup Iteration   1: 2706.080 ns/op

# Warmup Iteration   2: 2932.354 ns/op

# Warmup Iteration   3: 2670.564 ns/op

# Warmup Iteration   4: 2579.464 ns/op

# Warmup Iteration   5: 2612.851 ns/op

Iteration   1: 2737.323 ns/op
Iteration   2: 2546.510 ns/op
Iteration   3: 2575.180 ns/op
Iteration   4: 2670.960 ns/op
Iteration   5: 2698.931 ns/op


Result "org.example.Main.yieldBeforeEachCall":
2645.781 ±(99.9%) 314.471 ns/op [Average]
(min, avg, max) = (2546.510, 2645.781, 2737.323), stdev = 81.667
CI (99.9%): [2331.310, 2960.252] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 75.00% complete, ETA 00:03:06

# Fork: 1 of 1

# Warmup Iteration   1: 4425.947 ns/op

# Warmup Iteration   2: 4777.896 ns/op

# Warmup Iteration   3: 4252.648 ns/op

# Warmup Iteration   4: 4120.676 ns/op

# Warmup Iteration   5: 4283.973 ns/op

Iteration   1: 4113.852 ns/op
Iteration   2: 4272.612 ns/op
Iteration   3: 4134.131 ns/op
Iteration   4: 4232.952 ns/op
Iteration   5: 4493.692 ns/op


Result "org.example.Main.yieldBeforeEachCall":
4249.448 ±(99.9%) 584.448 ns/op [Average]
(min, avg, max) = (4113.852, 4249.448, 4493.692), stdev = 151.779
CI (99.9%): [3664.999, 4833.896] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 76.39% complete, ETA 00:02:55

# Fork: 1 of 1

# Warmup Iteration   1: 28152.163 ns/op

# Warmup Iteration   2: 25098.094 ns/op

# Warmup Iteration   3: 23907.843 ns/op

# Warmup Iteration   4: 24305.897 ns/op

# Warmup Iteration   5: 23918.777 ns/op

Iteration   1: 24503.884 ns/op
Iteration   2: 24183.509 ns/op
Iteration   3: 23966.529 ns/op
Iteration   4: 24454.088 ns/op
Iteration   5: 27490.964 ns/op


Result "org.example.Main.yieldBeforeEachCall":
24919.794 ±(99.9%) 5597.228 ns/op [Average]
(min, avg, max) = (23966.529, 24919.794, 27490.964), stdev = 1453.582
CI (99.9%): [19322.566, 30517.023] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 77.78% complete, ETA 00:02:45

# Fork: 1 of 1

# Warmup Iteration   1: 2172.727 ns/op

# Warmup Iteration   2: 1892.656 ns/op

# Warmup Iteration   3: 1875.750 ns/op

# Warmup Iteration   4: 1875.506 ns/op

# Warmup Iteration   5: 2043.329 ns/op

Iteration   1: 1884.304 ns/op
Iteration   2: 1909.862 ns/op
Iteration   3: 1878.581 ns/op
Iteration   4: 1862.783 ns/op
Iteration   5: 2052.399 ns/op


Result "org.example.Main.yieldBeforeEachCall":
1917.586 ±(99.9%) 297.440 ns/op [Average]
(min, avg, max) = (1862.783, 1917.586, 2052.399), stdev = 77.244
CI (99.9%): [1620.146, 2215.026] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 79.17% complete, ETA 00:02:35

# Fork: 1 of 1

# Warmup Iteration   1: 2722.829 ns/op

# Warmup Iteration   2: 2684.219 ns/op

# Warmup Iteration   3: 2628.438 ns/op

# Warmup Iteration   4: 2760.390 ns/op

# Warmup Iteration   5: 2629.070 ns/op

Iteration   1: 2791.028 ns/op
Iteration   2: 2661.773 ns/op
Iteration   3: 2606.762 ns/op
Iteration   4: 2577.976 ns/op
Iteration   5: 2665.128 ns/op


Result "org.example.Main.yieldBeforeEachCall":
2660.533 ±(99.9%) 314.946 ns/op [Average]
(min, avg, max) = (2577.976, 2660.533, 2791.028), stdev = 81.790
CI (99.9%): [2345.588, 2975.479] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 80.56% complete, ETA 00:02:24

# Fork: 1 of 1

# Warmup Iteration   1: 4546.288 ns/op

# Warmup Iteration   2: 4407.105 ns/op

# Warmup Iteration   3: 4193.620 ns/op

# Warmup Iteration   4: 4324.177 ns/op

# Warmup Iteration   5: 4494.987 ns/op

Iteration   1: 4382.751 ns/op
Iteration   2: 4174.462 ns/op
Iteration   3: 4172.327 ns/op
Iteration   4: 4345.744 ns/op
Iteration   5: 4632.990 ns/op


Result "org.example.Main.yieldBeforeEachCall":
4341.655 ±(99.9%) 728.594 ns/op [Average]
(min, avg, max) = (4172.327, 4341.655, 4632.990), stdev = 189.214
CI (99.9%): [3613.060, 5070.249] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldBeforeEachCall

# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 81.94% complete, ETA 00:02:14

# Fork: 1 of 1

# Warmup Iteration   1: 27789.703 ns/op

# Warmup Iteration   2: 25425.475 ns/op

# Warmup Iteration   3: 24281.944 ns/op

# Warmup Iteration   4: 23953.464 ns/op

# Warmup Iteration   5: 24531.901 ns/op

Iteration   1: 24975.436 ns/op
Iteration   2: 24056.954 ns/op
Iteration   3: 25143.116 ns/op
Iteration   4: 24084.831 ns/op
Iteration   5: 24826.520 ns/op


Result "org.example.Main.yieldBeforeEachCall":
24617.372 ±(99.9%) 1969.133 ns/op [Average]
(min, avg, max) = (24056.954, 24617.372, 25143.116), stdev = 511.378
CI (99.9%): [22648.239, 26586.505] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 1, stackDepth = 5)

# Run progress: 83.33% complete, ETA 00:02:04

# Fork: 1 of 1

# Warmup Iteration   1: 1547.013 ns/op

# Warmup Iteration   2: 1338.580 ns/op

# Warmup Iteration   3: 1393.143 ns/op

# Warmup Iteration   4: 1373.106 ns/op

# Warmup Iteration   5: 1392.574 ns/op

Iteration   1: 1474.947 ns/op
Iteration   2: 1530.128 ns/op
Iteration   3: 1327.534 ns/op
Iteration   4: 1331.936 ns/op
Iteration   5: 1309.018 ns/op


Result "org.example.Main.yieldThenContinue":
1394.713 ±(99.9%) 387.809 ns/op [Average]
(min, avg, max) = (1309.018, 1394.713, 1530.128), stdev = 100.713
CI (99.9%): [1006.904, 1782.521] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 1, stackDepth = 10)

# Run progress: 84.72% complete, ETA 00:01:53

# Fork: 1 of 1

# Warmup Iteration   1: 1443.132 ns/op

# Warmup Iteration   2: 1492.628 ns/op

# Warmup Iteration   3: 1300.799 ns/op

# Warmup Iteration   4: 1288.026 ns/op

# Warmup Iteration   5: 1380.979 ns/op

Iteration   1: 1367.681 ns/op
Iteration   2: 1402.386 ns/op
Iteration   3: 1310.434 ns/op
Iteration   4: 1304.593 ns/op
Iteration   5: 1444.011 ns/op


Result "org.example.Main.yieldThenContinue":
1365.821 ±(99.9%) 230.000 ns/op [Average]
(min, avg, max) = (1304.593, 1365.821, 1444.011), stdev = 59.730
CI (99.9%): [1135.821, 1595.821] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 1, stackDepth = 20)

# Run progress: 86.11% complete, ETA 00:01:43

# Fork: 1 of 1

# Warmup Iteration   1: 1489.226 ns/op

# Warmup Iteration   2: 1325.365 ns/op

# Warmup Iteration   3: 1392.921 ns/op

# Warmup Iteration   4: 1338.914 ns/op

# Warmup Iteration   5: 1349.603 ns/op

Iteration   1: 1453.072 ns/op
Iteration   2: 1310.785 ns/op
Iteration   3: 1433.976 ns/op
Iteration   4: 1335.366 ns/op
Iteration   5: 1477.861 ns/op


Result "org.example.Main.yieldThenContinue":
1402.212 ±(99.9%) 286.515 ns/op [Average]
(min, avg, max) = (1310.785, 1402.212, 1477.861), stdev = 74.407
CI (99.9%): [1115.697, 1688.727] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 1, stackDepth = 100)

# Run progress: 87.50% complete, ETA 00:01:33

# Fork: 1 of 1

# Warmup Iteration   1: 2167.021 ns/op

# Warmup Iteration   2: 2120.393 ns/op

# Warmup Iteration   3: 1914.017 ns/op

# Warmup Iteration   4: 1844.809 ns/op

# Warmup Iteration   5: 1951.357 ns/op

Iteration   1: 1896.198 ns/op
Iteration   2: 1884.207 ns/op
Iteration   3: 2032.441 ns/op
Iteration   4: 1862.472 ns/op
Iteration   5: 1865.747 ns/op


Result "org.example.Main.yieldThenContinue":
1908.213 ±(99.9%) 272.614 ns/op [Average]
(min, avg, max) = (1862.472, 1908.213, 2032.441), stdev = 70.797
CI (99.9%): [1635.599, 2180.827] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 2, stackDepth = 5)

# Run progress: 88.89% complete, ETA 00:01:22

# Fork: 1 of 1

# Warmup Iteration   1: 1398.037 ns/op

# Warmup Iteration   2: 1347.596 ns/op

# Warmup Iteration   3: 1275.226 ns/op

# Warmup Iteration   4: 1306.604 ns/op

# Warmup Iteration   5: 1354.147 ns/op

Iteration   1: 1300.160 ns/op
Iteration   2: 1414.647 ns/op
Iteration   3: 1424.090 ns/op
Iteration   4: 1433.346 ns/op
Iteration   5: 1289.342 ns/op


Result "org.example.Main.yieldThenContinue":
1372.317 ±(99.9%) 274.237 ns/op [Average]
(min, avg, max) = (1289.342, 1372.317, 1433.346), stdev = 71.218
CI (99.9%): [1098.080, 1646.554] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 2, stackDepth = 10)

# Run progress: 90.28% complete, ETA 00:01:12

# Fork: 1 of 1

# Warmup Iteration   1: 1498.128 ns/op

# Warmup Iteration   2: 1451.235 ns/op

# Warmup Iteration   3: 1333.662 ns/op

# Warmup Iteration   4: 1466.874 ns/op

# Warmup Iteration   5: 1408.487 ns/op

Iteration   1: 1379.813 ns/op
Iteration   2: 1352.922 ns/op
Iteration   3: 1323.277 ns/op
Iteration   4: 1367.527 ns/op
Iteration   5: 1334.376 ns/op


Result "org.example.Main.yieldThenContinue":
1351.583 ±(99.9%) 89.278 ns/op [Average]
(min, avg, max) = (1323.277, 1351.583, 1379.813), stdev = 23.185
CI (99.9%): [1262.305, 1440.861] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 2, stackDepth = 20)

# Run progress: 91.67% complete, ETA 00:01:02

# Fork: 1 of 1

# Warmup Iteration   1: 1500.453 ns/op

# Warmup Iteration   2: 1431.128 ns/op

# Warmup Iteration   3: 1524.672 ns/op

# Warmup Iteration   4: 1390.647 ns/op

# Warmup Iteration   5: 1399.389 ns/op

Iteration   1: 1408.415 ns/op
Iteration   2: 1390.271 ns/op
Iteration   3: 1403.752 ns/op
Iteration   4: 1537.222 ns/op
Iteration   5: 1412.005 ns/op


Result "org.example.Main.yieldThenContinue":
1430.333 ±(99.9%) 232.264 ns/op [Average]
(min, avg, max) = (1390.271, 1430.333, 1537.222), stdev = 60.318
CI (99.9%): [1198.069, 1662.596] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 2, stackDepth = 100)

# Run progress: 93.06% complete, ETA 00:00:51

# Fork: 1 of 1

# Warmup Iteration   1: 2660.584 ns/op

# Warmup Iteration   2: 2419.076 ns/op

# Warmup Iteration   3: 2441.560 ns/op

# Warmup Iteration   4: 2449.739 ns/op

# Warmup Iteration   5: 2357.047 ns/op

Iteration   1: 2340.887 ns/op
Iteration   2: 2434.416 ns/op
Iteration   3: 2361.346 ns/op
Iteration   4: 2541.501 ns/op
Iteration   5: 2342.878 ns/op


Result "org.example.Main.yieldThenContinue":
2404.206 ±(99.9%) 329.954 ns/op [Average]
(min, avg, max) = (2340.887, 2404.206, 2541.501), stdev = 85.688
CI (99.9%): [2074.252, 2734.160] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 3, stackDepth = 5)

# Run progress: 94.44% complete, ETA 00:00:41

# Fork: 1 of 1

# Warmup Iteration   1: 1456.509 ns/op

# Warmup Iteration   2: 1325.766 ns/op

# Warmup Iteration   3: 1336.036 ns/op

# Warmup Iteration   4: 1348.603 ns/op

# Warmup Iteration   5: 1437.335 ns/op

Iteration   1: 1308.021 ns/op
Iteration   2: 1390.104 ns/op
Iteration   3: 1308.289 ns/op
Iteration   4: 1311.362 ns/op
Iteration   5: 1448.825 ns/op


Result "org.example.Main.yieldThenContinue":
1353.320 ±(99.9%) 245.918 ns/op [Average]
(min, avg, max) = (1308.021, 1353.320, 1448.825), stdev = 63.864
CI (99.9%): [1107.402, 1599.238] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 3, stackDepth = 10)

# Run progress: 95.83% complete, ETA 00:00:31

# Fork: 1 of 1

# Warmup Iteration   1: 1515.133 ns/op

# Warmup Iteration   2: 1343.330 ns/op

# Warmup Iteration   3: 1418.894 ns/op

# Warmup Iteration   4: 1421.053 ns/op

# Warmup Iteration   5: 1538.352 ns/op

Iteration   1: 1541.311 ns/op
Iteration   2: 1386.141 ns/op
Iteration   3: 1423.160 ns/op
Iteration   4: 1457.815 ns/op
Iteration   5: 1356.451 ns/op


Result "org.example.Main.yieldThenContinue":
1432.976 ±(99.9%) 275.660 ns/op [Average]
(min, avg, max) = (1356.451, 1432.976, 1541.311), stdev = 71.588
CI (99.9%): [1157.316, 1708.635] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 3, stackDepth = 20)

# Run progress: 97.22% complete, ETA 00:00:20

# Fork: 1 of 1

# Warmup Iteration   1: 1639.607 ns/op

# Warmup Iteration   2: 1541.867 ns/op

# Warmup Iteration   3: 1809.379 ns/op

# Warmup Iteration   4: 1505.776 ns/op

# Warmup Iteration   5: 1585.128 ns/op

Iteration   1: 1476.148 ns/op
Iteration   2: 1624.831 ns/op
Iteration   3: 1574.350 ns/op
Iteration   4: 1568.790 ns/op
Iteration   5: 1600.732 ns/op


Result "org.example.Main.yieldThenContinue":
1568.970 ±(99.9%) 217.606 ns/op [Average]
(min, avg, max) = (1476.148, 1568.970, 1624.831), stdev = 56.512
CI (99.9%): [1351.364, 1786.576] (assumes normal distribution)


# JMH version: 1.37

# VM version: JDK 19.0.2, OpenJDK 64-Bit Server VM, 19.0.2+7-44

# VM invoker: C:\TencentKona\jdk-19.0.2\bin\java.exe

# VM options: --enable-preview --add-opens=java.base/jdk.internal.vm=ALL-UNNAMED

# Blackhole mode: compiler (auto-detected, use -Djmh.blackhole.autoDetect=false to disable)

# Warmup: 5 iterations, 1 s each

# Measurement: 5 iterations, 1 s each

# Timeout: 10 min per iteration

# Threads: 1 thread, will synchronize iterations

# Benchmark mode: Average time, time/op

# Benchmark: org.example.Main.yieldThenContinue

# Parameters: (paramCount = 3, stackDepth = 100)

# Run progress: 98.61% complete, ETA 00:00:10

# Fork: 1 of 1

# Warmup Iteration   1: 3439.444 ns/op

# Warmup Iteration   2: 3161.039 ns/op

# Warmup Iteration   3: 3597.905 ns/op

# Warmup Iteration   4: 3334.505 ns/op

# Warmup Iteration   5: 3486.381 ns/op

Iteration   1: 3453.729 ns/op
Iteration   2: 3538.097 ns/op
Iteration   3: 3485.489 ns/op
Iteration   4: 3546.461 ns/op
Iteration   5: 3263.787 ns/op


Result "org.example.Main.yieldThenContinue":
3457.513 ±(99.9%) 442.122 ns/op [Average]
(min, avg, max) = (3263.787, 3457.513, 3546.461), stdev = 114.818
CI (99.9%): [3015.391, 3899.635] (assumes normal distribution)


# Run complete. Total time: 00:12:24

REMEMBER: The numbers below are just data. To gain reusable insights, you need to follow up on
why the numbers are the way they are. Use profilers (see -prof, -lprof), design factorial
experiments, perform baseline and negative tests that provide experimental control, make sure
the benchmarking environment is safe on JVM/OS/HW level, ask for reviews from the domain experts.
Do not assume the numbers tell you what you want them to tell.

NOTE: Current JVM experimentally supports Compiler Blackholes, and they are in use. Please exercise
extra caution when trusting the results, look into the generated code to check the benchmark still
works, and factor in a small probability of new VM bugs. Additionally, while comparisons between
different JVMs are already problematic, the performance difference caused by different Blackhole
modes can be very significant. Please make sure you use the consistent Blackhole mode for comparisons.
```
