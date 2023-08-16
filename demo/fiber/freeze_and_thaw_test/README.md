## Test Environment

- JMH version: 1.37

- Java version: 

  openjdk version "1.8.0_362_fiber"
  OpenJDK Runtime Environment (Tencent Kona 8.0.13) (build 1.8.0_362_fiber-b1)
  OpenJDK 64-Bit Server VM (Tencent Kona 8.0.13) (build 25.362-b1, mixed mode)

- Device: 11th Gen Intel(R) Core(TM) i5-11300H @ 3.10GHz   3.11 GHz

## How to Run the Test

1. Make sure Java 1.8.0_362_fiber is installed.
1. Run the command `mvn clean package`to build the project.
1. Run the command `java -jar ./target/freeze_and_thaw_test.jar` to run the JMH test.
1. See the result in the file `result.text` 

## Notes

- Test results may be affected by system resources, hardware configurations, and other running processes.
- Depending on the specific requirements, you can adjust the test configuration and environment based on the test results.
- For more accurate results, it is recommended to run the test multiple times in a stable testing environment and take the average of multiple runs.
