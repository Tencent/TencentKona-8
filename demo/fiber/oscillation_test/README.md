## Test Instructions

1. export `JAVA_HOME`=kona_fiber_jdk

2. mvn clean package

3. java -jar ./target/benchmark.jar

## Test Results

*Tested 20230809*

**Kona**

| Benchmark             | (maxDepth) | (minDepth) | (repeat) | Mode | Cnt | Score       | Error     | Units |
|-----------------------|------------|------------|----------|------|-----|-------------|-----------|-------|
| Oscillation.oscillate | 5          | 2          | 10       | avgt | 5   | 7742.673 ±  | 9982.020  | ns/op |
| Oscillation.oscillate | 5          | 2          | 100      | avgt | 5   | 12314.416 ± | 7848.672  | ns/op |
| Oscillation.oscillate | 5          | 2          | 1000     | avgt | 5   | 53745.305 ± | 669.536   | ns/op |
| Oscillation.oscillate | 5          | 3          | 10       | avgt | 5   | 8442.800 ±  | 18295.416 | ns/op |
| Oscillation.oscillate | 5          | 3          | 100      | avgt | 5   | 10848.567 ± | 94.919    | ns/op |
| Oscillation.oscillate | 5          | 3          | 1000     | avgt | 5   | 56095.675 ± | 7546.005  | ns/op |
| Oscillation.oscillate | 5          | 4          | 10       | avgt | 5   | 9852.718 ±  | 17316.212 | ns/op |
| Oscillation.oscillate | 5          | 4          | 100      | avgt | 5   | 9560.146 ±  | 77.981    | ns/op |
| Oscillation.oscillate | 5          | 4          | 1000     | avgt | 5   | 41800.793 ± | 9956.250  | ns/op |
| Oscillation.oscillate | 6          | 2          | 10       | avgt | 5   | 9589.576 ±  | 12800.158 | ns/op |
| Oscillation.oscillate | 6          | 2          | 100      | avgt | 5   | 11019.847 ± | 111.813   | ns/op |
| Oscillation.oscillate | 6          | 2          | 1000     | avgt | 5   | 56674.087 ± | 719.747   | ns/op |
| Oscillation.oscillate | 6          | 3          | 10       | avgt | 5   | 9742.761 ±  | 14568.871 | ns/op |
| Oscillation.oscillate | 6          | 3          | 100      | avgt | 5   | 12723.615 ± | 8519.448  | ns/op |
| Oscillation.oscillate | 6          | 3          | 1000     | avgt | 5   | 57428.232 ± | 8505.805  | ns/op |
| Oscillation.oscillate | 6          | 4          | 10       | avgt | 5   | 8281.591 ±  | 16821.245 | ns/op |
| Oscillation.oscillate | 6          | 4          | 100      | avgt | 5   | 11495.237 ± | 6226.590  | ns/op |
| Oscillation.oscillate | 6          | 4          | 1000     | avgt | 5   | 55232.178 ± | 8647.081  | ns/op |
| Oscillation.oscillate | 7          | 2          | 10       | avgt | 5   | 10578.234 ± | 16913.825 | ns/op |
| Oscillation.oscillate | 7          | 2          | 100      | avgt | 5   | 11141.784 ± | 112.739   | ns/op |
| Oscillation.oscillate | 7          | 2          | 1000     | avgt | 5   | 59327.553 ± | 8717.792  | ns/op |
| Oscillation.oscillate | 7          | 3          | 10       | avgt | 5   | 8138.961 ±  | 15459.510 | ns/op |
| Oscillation.oscillate | 7          | 3          | 100      | avgt | 5   | 12952.968 ± | 9512.180  | ns/op |
| Oscillation.oscillate | 7          | 3          | 1000     | avgt | 5   | 59160.892 ± | 8594.802  | ns/op |
| Oscillation.oscillate | 7          | 4          | 10       | avgt | 5   | 9737.623 ±  | 14318.696 | ns/op |
| Oscillation.oscillate | 7          | 4          | 100      | avgt | 5   | 10951.468 ± | 282.979   | ns/op |
| Oscillation.oscillate | 7          | 4          | 1000     | avgt | 5   | 56609.844 ± | 9915.892  | ns/op |
| Oscillation.oscillate | 8          | 2          | 10       | avgt | 5   | 10955.002 ± | 18969.448 | ns/op |
| Oscillation.oscillate | 8          | 2          | 100      | avgt | 5   | 14742.536 ± | 14038.434 | ns/op |
| Oscillation.oscillate | 8          | 2          | 1000     | avgt | 5   | 62444.842 ± | 9888.207  | ns/op |
| Oscillation.oscillate | 8          | 3          | 10       | avgt | 5   | 9020.391 ±  | 22833.925 | ns/op |
| Oscillation.oscillate | 8          | 3          | 100      | avgt | 5   | 11360.110 ± | 190.526   | ns/op |
| Oscillation.oscillate | 8          | 3          | 1000     | avgt | 5   | 63492.279 ± | 11230.414 | ns/op |
| Oscillation.oscillate | 8          | 4          | 10       | avgt | 5   | 8575.598 ±  | 18920.477 | ns/op |
| Oscillation.oscillate | 8          | 4          | 100      | avgt | 5   | 13005.530 ± | 9989.564  | ns/op |
| Oscillation.oscillate | 8          | 4          | 1000     | avgt | 5   | 59412.130 ± | 10624.385 | ns/op |

**Loom**

| Benchmark             | (maxDepth) | (minDepth) | (repeat) | Mode | Cnt | Score        | Error     | Units |
|-----------------------|------------|------------|----------|------|-----|--------------|-----------|-------|
| Oscillation.oscillate | 5          | 2          | 10       | avgt | 5   | 2183.550 ±   | 77.506    | ns/op |
| Oscillation.oscillate | 5          | 2          | 100      | avgt | 5   | 15783.309 ±  | 1972.188  | ns/op |
| Oscillation.oscillate | 5          | 2          | 1000     | avgt | 5   | 136897.221 ± | 1801.156  | ns/op |
| Oscillation.oscillate | 5          | 3          | 10       | avgt | 5   | 2168.950 ±   | 25.949    | ns/op |
| Oscillation.oscillate | 5          | 3          | 100      | avgt | 5   | 13924.862 ±  | 185.871   | ns/op |
| Oscillation.oscillate | 5          | 3          | 1000     | avgt | 5   | 124003.554 ± | 606.569   | ns/op |
| Oscillation.oscillate | 5          | 4          | 10       | avgt | 5   | 2079.124 ±   | 27.772    | ns/op |
| Oscillation.oscillate | 5          | 4          | 100      | avgt | 5   | 13100.486 ±  | 528.550   | ns/op |
| Oscillation.oscillate | 5          | 4          | 1000     | avgt | 5   | 121361.532 ± | 1588.686  | ns/op |
| Oscillation.oscillate | 6          | 2          | 10       | avgt | 5   | 2327.471 ±   | 51.302    | ns/op |
| Oscillation.oscillate | 6          | 2          | 100      | avgt | 5   | 15520.832 ±  | 213.230   | ns/op |
| Oscillation.oscillate | 6          | 2          | 1000     | avgt | 5   | 144563.304 ± | 5455.549  | ns/op |
| Oscillation.oscillate | 6          | 3          | 10       | avgt | 5   | 2251.221 ±   | 24.500    | ns/op |
| Oscillation.oscillate | 6          | 3          | 100      | avgt | 5   | 14732.015 ±  | 372.570   | ns/op |
| Oscillation.oscillate | 6          | 3          | 1000     | avgt | 5   | 135699.900 ± | 319.209   | ns/op |
| Oscillation.oscillate | 6          | 4          | 10       | avgt | 5   | 2178.801 ±   | 10.677    | ns/op |
| Oscillation.oscillate | 6          | 4          | 100      | avgt | 5   | 14062.333 ±  | 365.991   | ns/op |
| Oscillation.oscillate | 6          | 4          | 1000     | avgt | 5   | 128496.283 ± | 1176.401  | ns/op |
| Oscillation.oscillate | 7          | 2          | 10       | avgt | 5   | 2338.967 ±   | 47.075    | ns/op |
| Oscillation.oscillate | 7          | 2          | 100      | avgt | 5   | 15749.554 ±  | 284.729   | ns/op |
| Oscillation.oscillate | 7          | 2          | 1000     | avgt | 5   | 146511.481 ± | 2652.611  | ns/op |
| Oscillation.oscillate | 7          | 3          | 10       | avgt | 5   | 2340.597 ±   | 53.675    | ns/op |
| Oscillation.oscillate | 7          | 3          | 100      | avgt | 5   | 15504.935 ±  | 249.728   | ns/op |
| Oscillation.oscillate | 7          | 3          | 1000     | avgt | 5   | 144161.079 ± | 577.302   | ns/op |
| Oscillation.oscillate | 7          | 4          | 10       | avgt | 5   | 2185.900 ±   | 36.256    | ns/op |
| Oscillation.oscillate | 7          | 4          | 100      | avgt | 5   | 14170.031 ±  | 385.011   | ns/op |
| Oscillation.oscillate | 7          | 4          | 1000     | avgt | 5   | 138724.193 ± | 1822.079  | ns/op |
| Oscillation.oscillate | 8          | 2          | 10       | avgt | 5   | 2343.584 ±   | 23.644    | ns/op |
| Oscillation.oscillate | 8          | 2          | 100      | avgt | 5   | 15686.072 ±  | 323.471   | ns/op |
| Oscillation.oscillate | 8          | 2          | 1000     | avgt | 5   | 148753.644 ± | 546.249   | ns/op |
| Oscillation.oscillate | 8          | 3          | 10       | avgt | 5   | 2358.092 ±   | 33.726    | ns/op |
| Oscillation.oscillate | 8          | 3          | 100      | avgt | 5   | 15952.329 ±  | 323.395   | ns/op |
| Oscillation.oscillate | 8          | 3          | 1000     | avgt | 5   | 154378.953 ± | 11315.280 | ns/op |
| Oscillation.oscillate | 8          | 4          | 10       | avgt | 5   | 2312.716 ±   | 109.322   | ns/op |
| Oscillation.oscillate | 8          | 4          | 100      | avgt | 5   | 15559.840 ±  | 362.129   | ns/op |
| Oscillation.oscillate | 8          | 4          | 1000     | avgt | 5   | 144895.694 ± | 3900.120  | ns/op |

## Test Device

4.2 GHz 4-core processor Core i7-6700k

16 GB 2666 MHz DDR4 