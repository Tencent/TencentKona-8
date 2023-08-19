## Test Instructions (Kona Fiber)

1. export `JAVA_HOME`=kona_fiber_jdk

2. mvn clean package

3. java -jar ./target/benchmark.jar

## Test Results

*Tested 20230809*

**Kona**

| Benchmark             | (maxDepth) | (minDepth) | (repeat) | Mode | Cnt  | Score       | Error    | Units |
| --------------------- | ---------- | ---------- | -------- | ---- | ---- | ----------- | -------- | ----- |
| Oscillation.oscillate | 5          | 2          | 10       | avgt | 5    | 700.553 ±   | 9.861    | ns/op |
| Oscillation.oscillate | 5          | 2          | 100      | avgt | 5    | 5324.248 ±  | 75.229   | ns/op |
| Oscillation.oscillate | 5          | 2          | 1000     | avgt | 5    | 51905.074 ± | 465.093  | ns/op |
| Oscillation.oscillate | 5          | 3          | 10       | avgt | 5    | 669.799 ±   | 4.034    | ns/op |
| Oscillation.oscillate | 5          | 3          | 100      | avgt | 5    | 5333.694 ±  | 78.920   | ns/op |
| Oscillation.oscillate | 5          | 3          | 1000     | avgt | 5    | 52799.600 ± | 884.444  | ns/op |
| Oscillation.oscillate | 5          | 4          | 10       | avgt | 5    | 580.853 ±   | 3.119    | ns/op |
| Oscillation.oscillate | 5          | 4          | 100      | avgt | 5    | 3856.314 ±  | 57.457   | ns/op |
| Oscillation.oscillate | 5          | 4          | 1000     | avgt | 5    | 35810.131 ± | 201.640  | ns/op |
| Oscillation.oscillate | 6          | 2          | 10       | avgt | 5    | 729.280 ±   | 10.916   | ns/op |
| Oscillation.oscillate | 6          | 2          | 100      | avgt | 5    | 5538.679 ±  | 94.033   | ns/op |
| Oscillation.oscillate | 6          | 2          | 1000     | avgt | 5    | 54200.110 ± | 1581.060 | ns/op |
| Oscillation.oscillate | 6          | 3          | 10       | avgt | 5    | 711.348 ±   | 11.226   | ns/op |
| Oscillation.oscillate | 6          | 3          | 100      | avgt | 5    | 5445.645 ±  | 103.571  | ns/op |
| Oscillation.oscillate | 6          | 3          | 1000     | avgt | 5    | 51441.225 ± | 479.376  | ns/op |
| Oscillation.oscillate | 6          | 4          | 10       | avgt | 5    | 684.155 ±   | 14.006   | ns/op |
| Oscillation.oscillate | 6          | 4          | 100      | avgt | 5    | 5222.961 ±  | 102.739  | ns/op |
| Oscillation.oscillate | 6          | 4          | 1000     | avgt | 5    | 50463.414 ± | 1942.261 | ns/op |
| Oscillation.oscillate | 7          | 2          | 10       | avgt | 5    | 714.366 ±   | 17.531   | ns/op |
| Oscillation.oscillate | 7          | 2          | 100      | avgt | 5    | 5590.449 ±  | 96.604   | ns/op |
| Oscillation.oscillate | 7          | 2          | 1000     | avgt | 5    | 53061.732 ± | 1251.199 | ns/op |
| Oscillation.oscillate | 7          | 3          | 10       | avgt | 5    | 776.839 ±   | 55.117   | ns/op |
| Oscillation.oscillate | 7          | 3          | 100      | avgt | 5    | 5586.308 ±  | 128.448  | ns/op |
| Oscillation.oscillate | 7          | 3          | 1000     | avgt | 5    | 51805.725 ± | 346.772  | ns/op |
| Oscillation.oscillate | 7          | 4          | 10       | avgt | 5    | 719.212 ±   | 9.589    | ns/op |
| Oscillation.oscillate | 7          | 4          | 100      | avgt | 5    | 5357.271 ±  | 388.972  | ns/op |
| Oscillation.oscillate | 7          | 4          | 1000     | avgt | 5    | 52335.998 ± | 1177.277 | ns/op |
| Oscillation.oscillate | 8          | 2          | 10       | avgt | 5    | 759.944 ±   | 4.193    | ns/op |
| Oscillation.oscillate | 8          | 2          | 100      | avgt | 5    | 5843.283 ±  | 132.809  | ns/op |
| Oscillation.oscillate | 8          | 2          | 1000     | avgt | 5    | 56074.099 ± | 1816.338 | ns/op |
| Oscillation.oscillate | 8          | 3          | 10       | avgt | 5    | 719.826 ±   | 6.277    | ns/op |
| Oscillation.oscillate | 8          | 3          | 100      | avgt | 5    | 5517.693 ±  | 74.484   | ns/op |
| Oscillation.oscillate | 8          | 3          | 1000     | avgt | 5    | 54275.007 ± | 1752.451 | ns/op |
| Oscillation.oscillate | 8          | 4          | 10       | avgt | 5    | 726.369 ±   | 9.225    | ns/op |
| Oscillation.oscillate | 8          | 4          | 100      | avgt | 5    | 5660.347 ±  | 51.736   | ns/op |
| Oscillation.oscillate | 8          | 4          | 1000     | avgt | 5    | 53796.846 ± | 1146.315 | ns/op |

**Loom**


| Benchmark             | (maxDepth) | (minDepth) | (repeat) | Mode | Cnt  | Score        | Error     | Units |
| --------------------- | ---------- | ---------- | -------- | ---- | ---- | ------------ | --------- | ----- |
| Oscillation.oscillate | 5          | 2          | 10       | avgt | 5    | 2233.259 ±   | 45.816    | ns/op |
| Oscillation.oscillate | 5          | 2          | 100      | avgt | 5    | 14295.556 ±  | 587.699   | ns/op |
| Oscillation.oscillate | 5          | 2          | 1000     | avgt | 5    | 138033.235 ± | 1868.857  | ns/op |
| Oscillation.oscillate | 5          | 3          | 10       | avgt | 5    | 2253.303 ±   | 34.971    | ns/op |
| Oscillation.oscillate | 5          | 3          | 100      | avgt | 5    | 14492.981 ±  | 707.160   | ns/op |
| Oscillation.oscillate | 5          | 3          | 1000     | avgt | 5    | 129173.176 ± | 1790.267  | ns/op |
| Oscillation.oscillate | 5          | 4          | 10       | avgt | 5    | 2134.928 ±   | 24.086    | ns/op |
| Oscillation.oscillate | 5          | 4          | 100      | avgt | 5    | 13643.234 ±  | 564.901   | ns/op |
| Oscillation.oscillate | 5          | 4          | 1000     | avgt | 5    | 125395.860 ± | 2265.762  | ns/op |
| Oscillation.oscillate | 6          | 2          | 10       | avgt | 5    | 2371.510 ±   | 37.739    | ns/op |
| Oscillation.oscillate | 6          | 2          | 100      | avgt | 5    | 16676.964 ±  | 3215.449  | ns/op |
| Oscillation.oscillate | 6          | 2          | 1000     | avgt | 5    | 156557.836 ± | 15430.178 | ns/op |
| Oscillation.oscillate | 6          | 3          | 10       | avgt | 5    | 2321.321 ±   | 124.723   | ns/op |
| Oscillation.oscillate | 6          | 3          | 100      | avgt | 5    | 15012.203 ±  | 346.957   | ns/op |
| Oscillation.oscillate | 6          | 3          | 1000     | avgt | 5    | 137740.682 ± | 3971.768  | ns/op |
| Oscillation.oscillate | 6          | 4          | 10       | avgt | 5    | 2217.479 ±   | 49.514    | ns/op |
| Oscillation.oscillate | 6          | 4          | 100      | avgt | 5    | 14261.390 ±  | 311.067   | ns/op |
| Oscillation.oscillate | 6          | 4          | 1000     | avgt | 5    | 131453.373 ± | 6827.855  | ns/op |
| Oscillation.oscillate | 7          | 2          | 10       | avgt | 5    | 2356.035 ±   | 45.101    | ns/op |
| Oscillation.oscillate | 7          | 2          | 100      | avgt | 5    | 16223.782 ±  | 569.419   | ns/op |
| Oscillation.oscillate | 7          | 2          | 1000     | avgt | 5    | 151363.488 ± | 604.032   | ns/op |
| Oscillation.oscillate | 7          | 3          | 10       | avgt | 5    | 2389.164 ±   | 44.223    | ns/op |
| Oscillation.oscillate | 7          | 3          | 100      | avgt | 5    | 15809.727 ±  | 264.528   | ns/op |
| Oscillation.oscillate | 7          | 3          | 1000     | avgt | 5    | 146339.161 ± | 1310.419  | ns/op |
| Oscillation.oscillate | 7          | 4          | 10       | avgt | 5    | 2250.849 ±   | 170.400   | ns/op |
| Oscillation.oscillate | 7          | 4          | 100      | avgt | 5    | 14430.708 ±  | 1567.084  | ns/op |
| Oscillation.oscillate | 7          | 4          | 1000     | avgt | 5    | 144204.738 ± | 27412.971 | ns/op |
| Oscillation.oscillate | 8          | 2          | 10       | avgt | 5    | 2429.273 ±   | 264.055   | ns/op |
| Oscillation.oscillate | 8          | 2          | 100      | avgt | 5    | 17211.370 ±  | 3009.752  | ns/op |
| Oscillation.oscillate | 8          | 2          | 1000     | avgt | 5    | 151507.099 ± | 2534.951  | ns/op |
| Oscillation.oscillate | 8          | 3          | 10       | avgt | 5    | 2393.810 ±   | 183.850   | ns/op |
| Oscillation.oscillate | 8          | 3          | 100      | avgt | 5    | 19158.611 ±  | 7092.478  | ns/op |
| Oscillation.oscillate | 8          | 3          | 1000     | avgt | 5    | 160000.014 ± | 14496.590 | ns/op |
| Oscillation.oscillate | 8          | 4          | 10       | avgt | 5    | 2340.369 ±   | 29.636    | ns/op |
| Oscillation.oscillate | 8          | 4          | 100      | avgt | 5    | 15964.669 ±  | 441.926   | ns/op |
| Oscillation.oscillate | 8          | 4          | 1000     | avgt | 5    | 156744.706 ± | 9511.889  | ns/op |

## Test Device

4.2 GHz 4-core processor Core i7-6700k

16 GB 2666 MHz DDR4 