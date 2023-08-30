## Test Environment

- TestTime：2023-08-28

- JMH version: 1.37

- Java version: 

  openjdk version "1.8.0_362_fiber"
  OpenJDK Runtime Environment (Tencent Kona 8.0.13) (build 1.8.0_362_fiber-b1)
  OpenJDK 64-Bit Server VM (Tencent Kona 8.0.13) (build 25.362-b1, mixed mode)

- Device: 11th Gen Intel(R) Core(TM) i5-11300H @ 3.10GHz   3.11 GHz

## How to Run the Test

1. Make sure Java 1.8.0_362_fiber is installed.
1. Run the command `mvn clean package`to build the project.
1. Run the command `java -jar ./target/freeze_and_thaw.jar` to run the JMH test.

## Comparison of results

**kona_fiber**

| Benchmark                      | paramCount | stackDepth | Mode | Cnt  | Score    | Error     | Units |
| ------------------------------ | ---------- | ---------- | ---- | ---- | -------- | --------- | ----- |
| FreezeAndThaw.baseline         | 1          | 5          | avgt | 5    | 220.960  | ± 19.843  | ns/op |
| FreezeAndThaw.baseline         | 1          | 10         | avgt | 5    | 249.152  | ± 37.142  | ns/op |
| FreezeAndThaw.baseline         | 1          | 20         | avgt | 5    | 266.363  | ± 58.291  | ns/op |
| FreezeAndThaw.baseline         | 1          | 100        | avgt | 5    | 561.013  | ± 445.641 | ns/op |
| FreezeAndThaw.baseline         | 2          | 5          | avgt | 5    | 329.246  | ± 272.433 | ns/op |
| FreezeAndThaw.baseline         | 2          | 10         | avgt | 5    | 341.524  | ± 27.617  | ns/op |
| FreezeAndThaw.baseline         | 2          | 20         | avgt | 5    | 445.994  | ± 24.976  | ns/op |
| FreezeAndThaw.baseline         | 2          | 100        | avgt | 5    | 1419.743 | ± 446.189 | ns/op |
| FreezeAndThaw.baseline         | 3          | 5          | avgt | 5    | 299.963  | ± 13.333  | ns/op |
| FreezeAndThaw.baseline         | 3          | 10         | avgt | 5    | 406.313  | ± 39.377  | ns/op |
| FreezeAndThaw.baseline         | 3          | 20         | avgt | 5    | 593.843  | ± 48.480  | ns/op |
| FreezeAndThaw.baseline         | 3          | 100        | avgt | 5    | 2177.008 | ± 183.408 | ns/op |
| FreezeAndThaw.yieldAndContinue | 1          | 5          | avgt | 5    | 306.996  | ± 23.532  | ns/op |
| FreezeAndThaw.yieldAndContinue | 1          | 10         | avgt | 5    | 345.014  | ± 21.258  | ns/op |
| FreezeAndThaw.yieldAndContinue | 1          | 20         | avgt | 5    | 372.042  | ± 62.178  | ns/op |
| FreezeAndThaw.yieldAndContinue | 1          | 100        | avgt | 5    | 534.667  | ± 20.336  | ns/op |
| FreezeAndThaw.yieldAndContinue | 2          | 5          | avgt | 5    | 328.655  | ± 15.568  | ns/op |
| FreezeAndThaw.yieldAndContinue | 2          | 10         | avgt | 5    | 402.053  | ± 20.601  | ns/op |
| FreezeAndThaw.yieldAndContinue | 2          | 20         | avgt | 5    | 466.259  | ± 27.028  | ns/op |
| FreezeAndThaw.yieldAndContinue | 2          | 100        | avgt | 5    | 1156.349 | ± 35.314  | ns/op |
| FreezeAndThaw.yieldAndContinue | 3          | 5          | avgt | 5    | 370.111  | ± 88.692  | ns/op |
| FreezeAndThaw.yieldAndContinue | 3          | 10         | avgt | 5    | 464.472  | ± 47.940  | ns/op |
| FreezeAndThaw.yieldAndContinue | 3          | 20         | avgt | 5    | 615.868  | ± 18.266  | ns/op |
| FreezeAndThaw.yieldAndContinue | 3          | 100        | avgt | 5    | 1920.457 | ± 152.374 | ns/op |

**loom**

| Benchmark                      | (paramCount) | (stackDepth) | Mode | Cnt  | Score    | Error      | Units |
| ------------------------------ | ------------ | ------------ | ---- | ---- | -------- | ---------- | ----- |
| FreezeAndThaw.baseline         | 1            | 5            | avgt | 5    | 2216.282 | ± 149.945  | ns/op |
| FreezeAndThaw.baseline         | 1            | 10           | avgt | 5    | 2314.205 | ± 313.576  | ns/op |
| FreezeAndThaw.baseline         | 1            | 20           | avgt | 5    | 2295.178 | ± 432.971  | ns/op |
| FreezeAndThaw.baseline         | 1            | 100          | avgt | 5    | 2761.366 | ± 766.183  | ns/op |
| FreezeAndThaw.baseline         | 2            | 5            | avgt | 5    | 2229.762 | ± 101.509  | ns/op |
| FreezeAndThaw.baseline         | 2            | 10           | avgt | 5    | 2485.628 | ± 375.584  | ns/op |
| FreezeAndThaw.baseline         | 2            | 20           | avgt | 5    | 2618.913 | ± 221.794  | ns/op |
| FreezeAndThaw.baseline         | 2            | 100          | avgt | 5    | 3313.095 | ± 304.245  | ns/op |
| FreezeAndThaw.baseline         | 3            | 5            | avgt | 5    | 2507.394 | ± 595.422  | ns/op |
| FreezeAndThaw.baseline         | 3            | 10           | avgt | 5    | 2507.569 | ± 48.938   | ns/op |
| FreezeAndThaw.baseline         | 3            | 20           | avgt | 5    | 2759.768 | ± 653.328  | ns/op |
| FreezeAndThaw.baseline         | 3            | 100          | avgt | 5    | 4334.301 | ± 1138.732 | ns/op |
| FreezeAndThaw.yieldAndContinue | 1            | 5            | avgt | 5    | 2916.257 | ± 936.015  | ns/op |
| FreezeAndThaw.yieldAndContinue | 1            | 10           | avgt | 5    | 2777.520 | ± 261.052  | ns/op |
| FreezeAndThaw.yieldAndContinue | 1            | 20           | avgt | 5    | 3009.245 | ± 708.279  | ns/op |
| FreezeAndThaw.yieldAndContinue | 1            | 100          | avgt | 5    | 3431.348 | ± 146.899  | ns/op |
| FreezeAndThaw.yieldAndContinue | 2            | 5            | avgt | 5    | 2686.477 | ± 137.593  | ns/op |
| FreezeAndThaw.yieldAndContinue | 2            | 10           | avgt | 5    | 2739.388 | ± 178.131  | ns/op |
| FreezeAndThaw.yieldAndContinue | 2            | 20           | avgt | 5    | 3997.687 | ± 2564.217 | ns/op |
| FreezeAndThaw.yieldAndContinue | 2            | 100          | avgt | 5    | 4331.048 | ± 137.065  | ns/op |
| FreezeAndThaw.yieldAndContinue | 3            | 5            | avgt | 5    | 2663.579 | ± 76.803   | ns/op |
| FreezeAndThaw.yieldAndContinue | 3            | 10           | avgt | 5    | 2826.586 | ± 322.418  | ns/op |
| FreezeAndThaw.yieldAndContinue | 3            | 20           | avgt | 5    | 3042.993 | ± 360.745  | ns/op |
| FreezeAndThaw.yieldAndContinue | 3            | 100          | avgt | 5    | 5683.074 | ± 214.237  | ns/op |

## Notes

- Test results may be affected by system resources, hardware configurations, and other running processes.
- Depending on the specific requirements, you can adjust the test configuration and environment based on the test results.
- For more accurate results, it is recommended to run the test multiple times in a stable testing environment and take the average of multiple runs.
