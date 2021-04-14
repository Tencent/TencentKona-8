export JAVA_HOME=kona_fiber_jdk

mvn clean install

java -jar ./target/microbenchmarks.jar org.sample.ContinuationCreateJMH.testCreate


Tested 20200720
Loom
Result : 3887153.050 ±(99.9%) 31355.987 ops/s
  Statistics: (min, avg, max) = (3874081.133, 3887153.050, 3894513.333), stdev = 8143.050
  Confidence interval (99.9%): [3855797.063, 3918509.037]


Benchmark                              (iter)   Mode   Samples         Mean   Mean error    Units
o.s.ContinuationCreateJMH.testCreate        1  thrpt         5  3894714.700    14079.429    ops/s
o.s.ContinuationCreateJMH.testCreate        2  thrpt         5  3925551.373    34600.402    ops/s
o.s.ContinuationCreateJMH.testCreate        3  thrpt         5  3856985.327    23472.640    ops/s
o.s.ContinuationCreateJMH.testCreate        4  thrpt         5  3796418.807    41964.725    ops/s
o.s.ContinuationCreateJMH.testCreate        5  thrpt         5  3887153.050    31355.987    ops/s


Kona
Result : 3226988.100 ±(99.9%) 63394.290 ops/s
  Statistics: (min, avg, max) = (3201812.367, 3226988.100, 3242458.533), stdev = 16463.296
  Confidence interval (99.9%): [3163593.810, 3290382.390]
  

Benchmark                              (iter)   Mode   Samples         Mean   Mean error    Units
o.s.ContinuationCreateJMH.testCreate        1  thrpt         5  3228859.490    30650.976    ops/s
o.s.ContinuationCreateJMH.testCreate        2  thrpt         5  3210216.173    83007.600    ops/s
o.s.ContinuationCreateJMH.testCreate        3  thrpt         5  3199789.210    43973.053    ops/s
o.s.ContinuationCreateJMH.testCreate        4  thrpt         5  3266121.330   164820.737    ops/s
o.s.ContinuationCreateJMH.testCreate        5  thrpt         5  3226988.100    63394.290    ops/s
