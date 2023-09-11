## How to Run the Test
### prerequisites
Java 1.8.0_362_fiber is needed for this project, so make sure you have it installed.

### build the project
Go to the directory where the project is located.
Run the command `mvn clean package` to build and package the project.

### run the test
Run the command `java -jar ./target/oneshot_test.jar` to run the JMH test.


## Test Results
### KonaFiber
```
Benchmark                            (paramCount)  (stackDepth)  Mode  Cnt     Score      Error  Units
OneShot.noYield                                 1             5  avgt    5   193.347 ±   23.752  ns/op
OneShot.noYield                                 1            10  avgt    5   190.794 ±   15.101  ns/op
OneShot.noYield                                 1            20  avgt    5   201.511 ±   12.077  ns/op
OneShot.noYield                                 1           100  avgt    5   511.413 ±   44.641  ns/op
OneShot.noYield                                 2             5  avgt    5   226.515 ±   15.460  ns/op
OneShot.noYield                                 2            10  avgt    5   271.085 ±    7.055  ns/op
OneShot.noYield                                 2            20  avgt    5   368.990 ±   44.183  ns/op
OneShot.noYield                                 2           100  avgt    5  1015.091 ±   45.551  ns/op
OneShot.noYield                                 3             5  avgt    5   249.178 ±   25.278  ns/op
OneShot.noYield                                 3            10  avgt    5   329.544 ±   36.000  ns/op
OneShot.noYield                                 3            20  avgt    5   469.367 ±   19.522  ns/op
OneShot.noYield                                 3           100  avgt    5  1530.162 ±   54.525  ns/op
OneShot.yieldAfterEachCall                      1             5  avgt    5   403.407 ±   43.957  ns/op
OneShot.yieldAfterEachCall                      1            10  avgt    5   610.780 ±   61.477  ns/op
OneShot.yieldAfterEachCall                      1            20  avgt    5  1070.246 ±  131.096  ns/op
OneShot.yieldAfterEachCall                      1           100  avgt    5  4691.595 ±  804.697  ns/op
OneShot.yieldAfterEachCall                      2             5  avgt    5   412.573 ±   36.146  ns/op
OneShot.yieldAfterEachCall                      2            10  avgt    5   612.922 ±   80.739  ns/op
OneShot.yieldAfterEachCall                      2            20  avgt    5  1064.759 ±   82.276  ns/op
OneShot.yieldAfterEachCall                      2           100  avgt    5  4681.689 ±  349.937  ns/op
OneShot.yieldAfterEachCall                      3             5  avgt    5   418.512 ±   37.332  ns/op
OneShot.yieldAfterEachCall                      3            10  avgt    5   655.347 ±   43.943  ns/op
OneShot.yieldAfterEachCall                      3            20  avgt    5  1107.613 ±  158.758  ns/op
OneShot.yieldAfterEachCall                      3           100  avgt    5  4883.132 ± 1361.582  ns/op
OneShot.yieldBeforeAndAfterEachCall             1             5  avgt    5   591.677 ±   56.482  ns/op
OneShot.yieldBeforeAndAfterEachCall             1            10  avgt    5  1022.626 ±   81.977  ns/op
OneShot.yieldBeforeAndAfterEachCall             1            20  avgt    5  1830.952 ±   97.383  ns/op
OneShot.yieldBeforeAndAfterEachCall             1           100  avgt    5  8505.499 ± 1018.192  ns/op
OneShot.yieldBeforeAndAfterEachCall             2             5  avgt    5   670.567 ±   42.148  ns/op
OneShot.yieldBeforeAndAfterEachCall             2            10  avgt    5   983.444 ±   95.015  ns/op
OneShot.yieldBeforeAndAfterEachCall             2            20  avgt    5  1879.760 ±  134.950  ns/op
OneShot.yieldBeforeAndAfterEachCall             2           100  avgt    5  8709.325 ±  220.228  ns/op
OneShot.yieldBeforeAndAfterEachCall             3             5  avgt    5   635.915 ±   48.346  ns/op
OneShot.yieldBeforeAndAfterEachCall             3            10  avgt    5   998.674 ±   72.236  ns/op
OneShot.yieldBeforeAndAfterEachCall             3            20  avgt    5  1891.288 ±  439.041  ns/op
OneShot.yieldBeforeAndAfterEachCall             3           100  avgt    5  8893.354 ±  293.804  ns/op
OneShot.yieldBeforeEachCall                     1             5  avgt    5   430.792 ±   19.579  ns/op
OneShot.yieldBeforeEachCall                     1            10  avgt    5   658.067 ±   37.914  ns/op
OneShot.yieldBeforeEachCall                     1            20  avgt    5  1196.539 ±   66.785  ns/op
OneShot.yieldBeforeEachCall                     1           100  avgt    5  4695.458 ±  306.384  ns/op
OneShot.yieldBeforeEachCall                     2             5  avgt    5   430.069 ±   24.247  ns/op
OneShot.yieldBeforeEachCall                     2            10  avgt    5   661.808 ±   60.241  ns/op
OneShot.yieldBeforeEachCall                     2            20  avgt    5  1098.785 ±   91.818  ns/op
OneShot.yieldBeforeEachCall                     2           100  avgt    5  4692.654 ±  642.248  ns/op
OneShot.yieldBeforeEachCall                     3             5  avgt    5   419.345 ±   44.892  ns/op
OneShot.yieldBeforeEachCall                     3            10  avgt    5   630.814 ±   22.209  ns/op
OneShot.yieldBeforeEachCall                     3            20  avgt    5  1097.909 ±  174.964  ns/op
OneShot.yieldBeforeEachCall                     3           100  avgt    5  4813.076 ±  676.743  ns/op
OneShot.yieldThenContinue                       1             5  avgt    5   231.585 ±   25.771  ns/op
OneShot.yieldThenContinue                       1            10  avgt    5   248.022 ±   18.576  ns/op
OneShot.yieldThenContinue                       1            20  avgt    5   257.119 ±   33.057  ns/op
OneShot.yieldThenContinue                       1           100  avgt    5   585.823 ±   63.343  ns/op
OneShot.yieldThenContinue                       2             5  avgt    5   272.621 ±   29.215  ns/op
OneShot.yieldThenContinue                       2            10  avgt    5   308.889 ±   22.920  ns/op
OneShot.yieldThenContinue                       2            20  avgt    5   383.663 ±   16.034  ns/op
OneShot.yieldThenContinue                       2           100  avgt    5   890.611 ±   64.512  ns/op
OneShot.yieldThenContinue                       3             5  avgt    5   295.548 ±   40.249  ns/op
OneShot.yieldThenContinue                       3            10  avgt    5   352.769 ±   16.000  ns/op
OneShot.yieldThenContinue                       3            20  avgt    5   481.279 ±   33.512  ns/op
OneShot.yieldThenContinue                       3           100  avgt    5  1443.510 ±   21.570  ns/op
```

### Loom
```
Benchmark                         (paramCount)  (stackDepth)  Mode  Cnt      Score      Error  Units
Main.noYield                                 1             5  avgt    5   1062.298 ±   81.136  ns/op
Main.noYield                                 1            10  avgt    5   1161.972 ±  419.487  ns/op
Main.noYield                                 1            20  avgt    5   1075.576 ±  180.958  ns/op
Main.noYield                                 1           100  avgt    5   1466.854 ±  261.045  ns/op
Main.noYield                                 2             5  avgt    5   1089.893 ±  190.795  ns/op
Main.noYield                                 2            10  avgt    5   1146.008 ±  245.568  ns/op
Main.noYield                                 2            20  avgt    5   1160.599 ±  185.247  ns/op
Main.noYield                                 2           100  avgt    5   1742.116 ±  523.224  ns/op
Main.noYield                                 3             5  avgt    5   1221.536 ±  420.466  ns/op
Main.noYield                                 3            10  avgt    5   1220.368 ±  298.187  ns/op
Main.noYield                                 3            20  avgt    5   1311.096 ±  339.824  ns/op
Main.noYield                                 3           100  avgt    5   2156.029 ±  258.836  ns/op
Main.yield                                   1             5  avgt    5   1223.398 ±  164.570  ns/op
Main.yield                                   1            10  avgt    5   1306.370 ±  351.775  ns/op
Main.yield                                   1            20  avgt    5   1531.317 ±  532.242  ns/op
Main.yield                                   1           100  avgt    5   1814.221 ±  143.709  ns/op
Main.yield                                   2             5  avgt    5   1359.454 ±  408.243  ns/op
Main.yield                                   2            10  avgt    5   1241.918 ±  177.456  ns/op
Main.yield                                   2            20  avgt    5   1368.517 ±  468.354  ns/op
Main.yield                                   2           100  avgt    5   1915.263 ±  216.665  ns/op
Main.yield                                   3             5  avgt    5   1215.129 ±  128.022  ns/op
Main.yield                                   3            10  avgt    5   1259.716 ±  211.411  ns/op
Main.yield                                   3            20  avgt    5   1271.351 ±  103.347  ns/op
Main.yield                                   3           100  avgt    5   1658.042 ±   31.964  ns/op
Main.yieldAfterEachCall                      1             5  avgt    5   1863.082 ±  281.169  ns/op
Main.yieldAfterEachCall                      1            10  avgt    5   2907.120 ± 1182.594  ns/op
Main.yieldAfterEachCall                      1            20  avgt    5   3873.716 ±  758.018  ns/op
Main.yieldAfterEachCall                      1           100  avgt    5  16103.060 ± 2404.533  ns/op
Main.yieldAfterEachCall                      2             5  avgt    5   1821.752 ±  218.814  ns/op
Main.yieldAfterEachCall                      2            10  avgt    5   2464.019 ±  294.927  ns/op
Main.yieldAfterEachCall                      2            20  avgt    5   3998.023 ±  961.828  ns/op
Main.yieldAfterEachCall                      2           100  avgt    5  16919.310 ± 4073.997  ns/op
Main.yieldAfterEachCall                      3             5  avgt    5   1816.323 ±  128.392  ns/op
Main.yieldAfterEachCall                      3            10  avgt    5   2681.004 ±  486.156  ns/op
Main.yieldAfterEachCall                      3            20  avgt    5   3891.581 ±  520.480  ns/op
Main.yieldAfterEachCall                      3           100  avgt    5  17418.811 ± 5941.562  ns/op
Main.yieldBeforeAndAfterEachCall             1             5  avgt    5   2575.958 ±  316.263  ns/op
Main.yieldBeforeAndAfterEachCall             1            10  avgt    5   3832.544 ±  273.604  ns/op
Main.yieldBeforeAndAfterEachCall             1            20  avgt    5   6835.066 ±  893.587  ns/op
Main.yieldBeforeAndAfterEachCall             1           100  avgt    5  37036.262 ± 2857.272  ns/op
Main.yieldBeforeAndAfterEachCall             2             5  avgt    5   2568.553 ±  352.845  ns/op
Main.yieldBeforeAndAfterEachCall             2            10  avgt    5   4266.778 ± 1347.114  ns/op
Main.yieldBeforeAndAfterEachCall             2            20  avgt    5   8404.707 ± 1177.629  ns/op
Main.yieldBeforeAndAfterEachCall             2           100  avgt    5  39821.564 ± 4044.214  ns/op
Main.yieldBeforeAndAfterEachCall             3             5  avgt    5   2484.646 ±  186.439  ns/op
Main.yieldBeforeAndAfterEachCall             3            10  avgt    5   4014.772 ±  451.216  ns/op
Main.yieldBeforeAndAfterEachCall             3            20  avgt    5   7222.490 ± 1095.091  ns/op
Main.yieldBeforeAndAfterEachCall             3           100  avgt    5  40549.236 ± 7232.080  ns/op
Main.yieldBeforeEachCall                     1             5  avgt    5   1961.264 ±  261.818  ns/op
Main.yieldBeforeEachCall                     1            10  avgt    5   2662.193 ±  196.821  ns/op
Main.yieldBeforeEachCall                     1            20  avgt    5   4277.673 ±  913.365  ns/op
Main.yieldBeforeEachCall                     1           100  avgt    5  24238.366 ± 8373.447  ns/op
Main.yieldBeforeEachCall                     2             5  avgt    5   1870.802 ±  128.031  ns/op
Main.yieldBeforeEachCall                     2            10  avgt    5   2645.781 ±  314.471  ns/op
Main.yieldBeforeEachCall                     2            20  avgt    5   4249.448 ±  584.448  ns/op
Main.yieldBeforeEachCall                     2           100  avgt    5  24919.794 ± 5597.228  ns/op
Main.yieldBeforeEachCall                     3             5  avgt    5   1917.586 ±  297.440  ns/op
Main.yieldBeforeEachCall                     3            10  avgt    5   2660.533 ±  314.946  ns/op
Main.yieldBeforeEachCall                     3            20  avgt    5   4341.655 ±  728.594  ns/op
Main.yieldBeforeEachCall                     3           100  avgt    5  24617.372 ± 1969.133  ns/op
Main.yieldThenContinue                       1             5  avgt    5   1394.713 ±  387.809  ns/op
Main.yieldThenContinue                       1            10  avgt    5   1365.821 ±  230.000  ns/op
Main.yieldThenContinue                       1            20  avgt    5   1402.212 ±  286.515  ns/op
Main.yieldThenContinue                       1           100  avgt    5   1908.213 ±  272.614  ns/op
Main.yieldThenContinue                       2             5  avgt    5   1372.317 ±  274.237  ns/op
Main.yieldThenContinue                       2            10  avgt    5   1351.583 ±   89.278  ns/op
Main.yieldThenContinue                       2            20  avgt    5   1430.333 ±  232.264  ns/op
Main.yieldThenContinue                       2           100  avgt    5   2404.206 ±  329.954  ns/op
Main.yieldThenContinue                       3             5  avgt    5   1353.320 ±  245.918  ns/op
Main.yieldThenContinue                       3            10  avgt    5   1432.976 ±  275.660  ns/op
Main.yieldThenContinue                       3            20  avgt    5   1568.970 ±  217.606  ns/op
Main.yieldThenContinue                       3           100  avgt    5   3457.513 ±  442.122  ns/op
```
