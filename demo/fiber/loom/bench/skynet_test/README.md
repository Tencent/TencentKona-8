# Test Instructions

The provided code is a Skynet benchmark program written in Java. The Skynet benchmark is a common benchmark used to measure the performance of computer systems. It simulates the execution of a computationally intensive application by recursively dividing tasks into subtasks.

The `Skynet` class contains a nested `Channel` class, which is a simple implementation of a communication channel using a blocking queue. The `skynet` method is a recursive implementation of the Skynet algorithm, which divides the task into smaller subtasks and uses the `Channel` to communicate and aggregate the results.

The `@Benchmark` annotation is applied to the `skynet` method, indicating that this method will be benchmarked. It creates a `Channel` instance, starts a virtual thread to execute the `skynet` method recursively, and returns the result received from the `Channel`.

## Test Environment

- JMH version: 1.36
- Java version: JDK 1.8.0_362_fiber, OpenJDK 64-Bit Server VM, 25.362-b1
- Device:Intel(R) Core(TM) i5-8400 CPU @ 2.80GHz   2.81 GHz 16GB

## How to Run the Test

1. Make sure Java 1.8.0_362_fiber is installed.
1. Download and build the project.
1. Run the command `mvn clean package`to build the project.
1. Run the command `java -jar ./target/Skynet_test.jar` to run the JMH test.

## Test Setup

- `@Fork(3)`: This is a JMH (Java Microbenchmark Harness) annotation that indicates the benchmark will be run with three independent process copies (forks). This helps reduce result deviations caused by external factors.
- `@Warmup(iterations = 5, time = 10)`: This is a JMH annotation used to define the warm-up phase settings. It specifies that there will be five iterations of warm-up runs, each running for 10 seconds. This allows JVM to warm up before the actual benchmark measurement to obtain more accurate results.
- `@Measurement(iterations = 10, time = 10)`: This is another JMH annotation that defines the measurement phase settings. It indicates that the benchmark will be executed for ten iterations, where each iteration runs for 10 seconds. The benchmark results will be collected during this phase.
- `@BenchmarkMode(Mode.AverageTime)`: This JMH annotation specifies the benchmark mode as AverageTime, indicating that the benchmark results will be reported as the average time taken for each method.
- `@OutputTimeUnit(TimeUnit.MILLISECONDS)`: This annotation specifies that the benchmark results will be reported in milliseconds.
- `@State(Scope.Thread)`: This annotation indicates that the benchmark state will be scoped at the thread level, meaning each thread executing the benchmark will have its own instance of the benchmark class.

## Test Results

kona fiber result：

| Benchmark     | (num)  | Mode | Cnt  | Score   | Error   | Units |
| ------------- | ------ | ---- | ---- | ------- | ------- | ----- |
| Skynet.skynet | 1000   | avgt | 30   | 0.544   | ±0.020  | ms/op |
| Skynet.skynet | 10000  | avgt | 30   | 7.220   | ±0.304  | ms/op |
| Skynet.skynet | 100000 | avgt | 30   | 555.224 | ±23.542 | ms/op |

loom result：

| Benchmark     | (num)  | Mode | Cnt  | Score  | Error   | Units |
| ------------- | ------ | ---- | ---- | ------ | ------- | ----- |
| Skynet.skynet | 1000   | avgt | 30   | 0.506  | ± 0.007 | ms/op |
| Skynet.skynet | 10000  | avgt | 30   | 4.816  | ± 0.128 | ms/op |
| Skynet.skynet | 100000 | avgt | 30   | 50.348 | ± 0.952 | ms/op |

- Benchmark: Name of the tested method
- (num): Name and value of the parameter
- Mode: Measurement mode (average time, average throughput, etc.)
- Cnt: Number of iterations
- Score: Average score of the test results
- Error: Error range of the test results
- Units: Units of the result

## Conclusion

- Based on the provided benchmark test results, the Skynet.skynet method has an average execution time of 555.224 milliseconds and a standard deviation of ±23.542 milliseconds when processing 100,000 elements. This indicates that the method demonstrates relatively stable and fast performance when handling a problem of size 100,000.
- However, exceeding the parameter (num) beyond 100,000 may result in an insufficient memory error. Solutions to address memory limitations include optimizing algorithms, processing data in batches, increasing memory limits, using more efficient data structures, and considering distributed computing. These solutions can help improve memory consumption and enhance the ability to handle larger-scale problems.

## Notes

- Test results may be affected by system resources, hardware configurations, and other running processes.
- Depending on the specific requirements, you can adjust the test configuration and environment based on the test results.
- For more accurate results, it is recommended to run the test multiple times in a stable testing environment and take the average of multiple runs.
