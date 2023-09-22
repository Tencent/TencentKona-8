## 如何运行测试

- 确保已安装 `Java 1.8.0_362_fiber`。
- 运行命令 `mvn clean package` 构建项目。
- 在 `org.example.Ring` 类中运行 `jmh` 方法启动测试。

### 配置信息

- 处理器	13th Gen Intel(R) Core(TM) i5-13600K   3.50 GHz
- 机带 RAM	32.0 GB 
  

### 更改

- 在 `pom.xml` 中添加 `jmh` 依赖：

```xml
<dependencies>
    <dependency>
        <groupId>org.openjdk.jmh</groupId>
        <artifactId>jmh-core</artifactId>
        <version>1.37</version>
    </dependency>
    <dependency>
        <groupId>org.openjdk.jmh</groupId>
        <artifactId>jmh-generator-annprocess</artifactId>
        <version>1.37</version>
    </dependency>
</dependencies>
```

### 运行介绍

#### 测试目标：

测试的主要目的是衡量 `Ring` 类中 `trip` 方法的性能。

该方法发送一个消息并接收一个响应，评估在一个由多个 `Worker` 线程组成的循环链路中。消息通过一个由多个 `Worker` 线程组成的循环链路（`Ring`）传递。每个 `Worker` 线程从其 `source` 通道接收消息，并将该消息发送到其 `sink` 通道，从而形成一个从 `head` 通道开始、经过所有的 `Worker` 线程、最后到达 `tail` 通道的消息传递链。

#### 测试参数：

- `threads`: 测试中使用的线程数。

- `queue`: 使用的队列类型，只有 "sq"（SynchronousQueue）。

- `stackDepth`: 运行时期的栈深度，取值有 "1", "4", "16", "64", "256"。

  ```java
    private void recursiveSend(int depth, T msg, Ref x1) {
              if (depth == 0) {
                  sendImpl(msg);
              } else {
                  recursiveSend(depth - 1, msg, x1.inc());
              }
          }
  
          private T recursiveReceive(int depth, Ref x1) {
              if (depth == 0) {
                  return receiveImpl();
              } else {
                  return recursiveReceive(depth - 1, x1.inc());
              }
          }
  ```

- `stackFrame`: 栈帧，取值有 "1", "4", "8"。

  - 以`ChannelFixedStackR4` 类为例子，模拟了一个具有4个堆栈帧深度的函数调用。在每次递归调用中，这些内部的变量都会增加，模拟了堆栈的增长。

    ```java
      public static class ChannelFixedStackR4<T> extends AbstractStackChannel<T> {
            private final int depth;
            private int x1;
            private int x2;
            private int x3;
            private int x4;
    
            public ChannelFixedStackR4(BlockingQueue<T> q, int depth, int start) {
                super(q);
                this.depth = depth;
                x1 = start + 1;
                x2 = start + 2;
                x3 = start + 3;
                x4 = start + 4;
        }
    ···
    ```

    

- `allocalot`: 是否为每个消息分配固定的内存。

- `singleshot`: 是否是单次发送/接收。

#### 测试结果分析：

- **总体性能**：结果显示平均时间为 9832731.679 ns/op。
- **不同参数下的性能**：结果显示随着 `stackDepth` 和 `stackFrame` 的增加，性能通常会下降。
- **allocalot 参数**：不为每个消息分配固定内存时，性能会受到极大的不好的影响。
- **singleshot 参数**：当为 false 时，性能较好。

#### 代码分析：

- `Worker` 类：代表一个任务，-是为了模拟一个工作线程，从一个源通道接收消息，然后发送到另一个目标通道，直到Predicate的条件被满足后结束。

  - **属性**：

    - `source`：源通道，从这里接收消息。
    - `sink`：目标通道，向这里发送消息。
    - `finisher`：一个Predicate，用于决定何时停止工作。

  - 运行的run方法：

    - 方法开始时，`endOfWork` 标志被设置为 `false`，意味着工作尚未完成。在一个 `do-while`循环中：从 `source` 通道接收一个消息。使用 `finisher` 断言测试这个消息。如果测试返回 `true`，那么 `endOfWork` 被设置为 `true`。然后，将该消息发送到 `sink` 通道。

    - 循环继续进行，直到 `endOfWork` 变为 `false`。

- 发送send & 接受receive

  1. **send 方法**:

     - `q.put(e)`是一个阻塞调用，如果队列`q`已满，它将等待直到有空间。
     - 如果在尝试发送消息时线程被中断，该方法将继续尝试发送消息，并在完成后重新设置线程的中断状态。

  2. **receive 方法**:

     - `q.take()`是一个阻塞调用，如果队列`q`为空，它将等待直到有消息可用。
     - 如果在尝试接收消息时线程被中断，该方法将继续尝试接收消息，并在完成后重新设置线程的中断状态。

     ```java
     @Override
     public void send(T e) {
         boolean interrupted = false;
         while (true) {
             try {
                 q.put(e);
                 break;
             } catch (InterruptedException x) {
                 interrupted = true;
             }
         }
         if (interrupted)
             Thread.currentThread().interrupt();
     }
     
     @Override
     public T receive() {
         boolean interrupted = false;
         T e;
         while (true) {
             try {
                 e = q.take();
                 break;
             } catch (InterruptedException x) {
                 interrupted = true;
             }
         }
         if (interrupted)
             Thread.currentThread().interrupt();
         return e;
     }
     }
     ```

     

- `setup()`：设置消息、初始化通道、创建工作线程并根据需要启动它们。

  - `head = chans[0];` 和 `tail = chans[chans.length - 1];`: 设置 `head` 和 `tail` 变量分别指向通道数组的第一个和最后一个通道。

- **`singleshot`的设置：它决定了消息是一次性在环中传递还是在环中持续传递，还有信息传递的模式。**

  - 一次性&持续
    - 如果 `singleshot` 为 `true`：每次基准测试的迭代都会启动所有工作线程，每次迭代都是一个新的开始，所有线程都是新启动的。
    - 如果 `singleshot` 为 `false`：所有工作线程都会在 `setup` 方法中被启动，并且在所有基准测试迭代中继续运行。我觉得，这样可以减少启动线程的开销，但可能导致线程之间的状态干扰。

  - **发送信息机制**
    - 当 `singleshot` 设置为 `true` 时，`setup` 方法中定义的 `finalCondition` 谓词会始终返回 `true`，无论 `Worker` 从 `source` 通道接收到什么消息，`finisher.test(msg)` 都会返回 `true`。在 `Worker` 的 `run` 方法中，一旦 `finisher.test(msg)` 返回 `true`，`endOfWork` 就会被设置为 `true`，从而结束 `do-while` 循环。因此，`Worker` 只会处理一个消息，然后停止。
    - 当 `singleshot` 设置为 `false` 时，`setup` 方法中定义的 `finalCondition` 谓词会检查消息是否小于 0，只有当 `Worker` 从 `source` 通道接收到一个小于 0 的消息时，`finisher.test(msg)` 才会返回 `true`。这意味着，只有在接收到一个小于 0 的消息时，`endOfWork` 才会被设置为 `true`，从而结束 `do-while` 循环，也就是当基准测试结束时，`tearDown` 方法会被调用，它会发送一个 `-1` 的消息



  1. **当`singleshot`为`true`时**:

     **在`setup`方法中**:
     ```java
     msg = 42;
     Channel<Integer>[] chans = new Channel[threads + 1];
     for (int i = 0; i < chans.length; i++) {
         chans[i] = getChannel();
     }
     head = chans[0];
     tail = chans[chans.length - 1];
     Predicate<Integer> finalCondition = (x -> true);
     workers = new Worker[chans.length - 1];
     for (int i = 0; i < chans.length - 1; i++) {
         workers[i] = new Worker<>(chans[i], chans[i+1], finalCondition);
     }
     ```

     **在`trip`基准测试方法中**:
     ```java
     startAll();  // 启动所有工作线程
     head.send(msg);  // 发送消息到环的开始
     return tail.receive();  // 从环的末尾接收消息
     ```

  2. **当`singleshot`为`false`时**:

     **在`setup`方法中**:
     ```java
     msg = 42;
     Channel<Integer>[] chans = new Channel[threads + 1];
     for (int i = 0; i < chans.length; i++) {
         chans[i] = getChannel();
     }
     head = chans[0];
     tail = chans[chans.length - 1];
     Predicate<Integer> finalCondition = (x -> (x < 0));  // 当消息小于0时结束
     workers = new Worker[chans.length - 1];
     for (int i = 0; i < chans.length - 1; i++) {
         workers[i] = new Worker<>(chans[i], chans[i+1], finalCondition);
     }
     startAll();  // 启动所有工作线程
     ```

     **在`trip`基准测试方法中**:
     ```java
     head.send(msg);  // 发送消息到环的开始
     return tail.receive();  // 从环的末尾接收消息
     ```

     **在`tearDown`方法中**:
     ```java
     head.send(-1);  // 发送结束消息到环的开始
     tail.receive();  // 从环的末尾接收结束消息
     ```

  

  - `finalCondition`：根据 `singleshot` 参数的值，初始化一个断言 `finalCondition`。
    - 如果 `singleshot` 为 `true`，那么 `finalCondition` 为一个总是返回 `true` 的谓词，意味着 `Worker` 只会处理一个消息就结束。

    - 如果 `singleshot` 为 `false`，那么 `finalCondition` 为一个检查消息是否小于0的谓词。这意味着只有当接收到的消息小于0时，`Worker` 才会停止。

  - `tearDown()`：在基准测试结束后清理资源。

- `trip()`：进行基准测试的方法。

- `getChannel()`：基于指定的`stackFrame`参数来创建并返回一个相应类型，不同调用`stackFrame`的`SychronousQueue `的通道（Channel）。

​	

### 测试目的

1. **短暂的任务 vs. 持续的任务**:
   - 当 `singleshot` 为 `true` 时，模拟了一个需要在短暂时间内处理大量内容的任务，即每个工作线程只处理一次消息，但是所有任务的启动和结束开销都将被同时记录。
   - 当 `singleshot` 为 `false` 时，它模拟了一个持续的任务，即工作线程持续处理消息，直到接收到一个结束信号，应对的场景是系统可能需要持续运行和处理数据的时候。
2. **启动和关闭的开销**:
   - 对于 `singleshot` 为 `true` 的情况，由于每次只处理一个消息，每次基准测试都涉及到线程的启动和关闭。这可以帮助我们了解启动和关闭线程的开销。
   - 对于 `singleshot` 为 `false` 的情况，线程在整个基准测试期间一直运行，因此与启动和关闭相关的开销对于整体性能的影响较小。

### 测试结果

#### Kona Fiber

```bash

Result "org.example.Ring.trip":
  9832731.679 ±(99.9%) 432011.747 ns/op [Average]
  (min, avg, max) = (8829951.724, 9832731.679, 15627126.471), stdev = 966256.250
  CI (99.9%): [9400719.932, 10264743.426] (assumes normal distribution)


# Run complete. Total time: 01:56:24

REMEMBER: The numbers below are just data. To gain reusable insights, you need to follow up on
why the numbers are the way they are. Use profilers (see -prof, -lprof), design factorial
experiments, perform baseline and negative tests that provide experimental control, make sure
the benchmarking environment is safe on JVM/OS/HW level, ask for reviews from the domain experts.
Do not assume the numbers tell you what you want them to tell.

Benchmark  (allocalot)  (queue)  (singleshot)  (stackDepth)  (stackFrame)  (threads)  Mode  Cnt         Score         Error  Units
Ring.trip         true       sq          true             1             1       1000  avgt   60   1267257.899 ±    6584.308  ns/op
Ring.trip         true       sq          true             1             4       1000  avgt   60   1348996.336 ±   45103.779  ns/op
Ring.trip         true       sq          true             1             8       1000  avgt   60   1393520.658 ±   24905.538  ns/op
Ring.trip         true       sq          true             4             1       1000  avgt   60   1383127.155 ±   39583.114  ns/op
Ring.trip         true       sq          true             4             8       1000  avgt   60   1598757.617 ±   16586.815  ns/op
Ring.trip         true       sq          true            16             4       1000  avgt   60   1900746.559 ±   25163.328  ns/op
Ring.trip         true       sq          true            16             8       1000  avgt   60   2331098.821 ±   23198.599  ns/op
Ring.trip         true       sq          true            64             4       1000  avgt   60   3250828.730 ±   55569.332  ns/op
Ring.trip         true       sq          true            64             8       1000  avgt   60   4533055.210 ±  404260.990  ns/op
Ring.trip         true       sq          true           256             1       1000  avgt   60   4039349.684 ±  549508.385  ns/op
Ring.trip         true       sq          true           256             4       1000  avgt   60   6217062.232 ±  692858.885  ns/op
Ring.trip         true       sq          true           256             8       1000  avgt   60   9422527.800 ±  140160.237  ns/op
Ring.trip         true       sq         false             1             1       1000  avgt   60    800374.460 ±    5627.415  ns/op
Ring.trip         true       sq         false             1             4       1000  avgt   60    919680.749 ±   20713.283  ns/op
Ring.trip         true       sq         false             1             8       1000  avgt   60   1015958.118 ±   34486.552  ns/op
Ring.trip         true       sq         false             4             1       1000  avgt   60    936582.220 ±   13916.158  ns/op
Ring.trip         true       sq         false             4             4       1000  avgt   60    977872.307 ±   30986.451  ns/op
Ring.trip         true       sq         false             4             8       1000  avgt   60   1104712.629 ±    7978.531  ns/op
Ring.trip         true       sq         false            16             1       1000  avgt   60   1025042.365 ±   10610.997  ns/op
Ring.trip         true       sq         false            16             4       1000  avgt   60   1199811.108 ±   19626.172  ns/op
Ring.trip         true       sq         false            16             8       1000  avgt   60   1602661.284 ±   27986.445  ns/op
Ring.trip         true       sq         false            64             1       1000  avgt   60   1404859.826 ±   41077.166  ns/op
Ring.trip         true       sq         false            64             4       1000  avgt   60   2331485.591 ±   75855.377  ns/op
Ring.trip         true       sq         false            64             8       1000  avgt   60   4193023.825 ±  179583.329  ns/op
Ring.trip         true       sq         false           256             1       1000  avgt   60   5078119.243 ±  164534.311  ns/op
Ring.trip         true       sq         false           256             4       1000  avgt   60  10693310.600 ±  115627.647  ns/op
Ring.trip         true       sq         false           256             8       1000  avgt   60  15684111.048 ±  195323.958  ns/op
Ring.trip        false       sq          true             1             4       1000  avgt   60   2798679.967 ± 4825753.779  ns/op
Ring.trip        false       sq          true             1             8       1000  avgt   60   1457902.548 ±   29718.027  ns/op
Ring.trip        false       sq          true             4             4       1000  avgt   60   1482432.217 ±   21152.990  ns/op
Ring.trip        false       sq          true             4             8       1000  avgt   60   1564461.898 ±   23844.356  ns/op
Ring.trip        false       sq          true            16             1       1000  avgt   60   1645925.679 ±   17829.150  ns/op
Ring.trip        false       sq          true            16             4       1000  avgt   60   1743853.565 ±   33876.672  ns/op
Ring.trip        false       sq          true            16             8       1000  avgt   60   2038324.917 ±   24139.221  ns/op
Ring.trip        false       sq          true            64             1       1000  avgt   60   2568607.693 ±   27898.813  ns/op
Ring.trip        false       sq          true            64             4       1000  avgt   60   2773687.348 ±   41516.999  ns/op
Ring.trip        false       sq          true            64             8       1000  avgt   60   3414921.632 ±   67082.218  ns/op
Ring.trip        false       sq          true           256             1       1000  avgt   60   5476092.884 ±  103461.557  ns/op
Ring.trip        false       sq          true           256             4       1000  avgt   60   6859041.420 ±  132258.105  ns/op
Ring.trip        false       sq          true           256             8       1000  avgt   60  10987874.231 ±  830243.156  ns/op
Ring.trip        false       sq         false             1             1       1000  avgt   60    903279.779 ±   17676.543  ns/op
Ring.trip        false       sq         false             1             4       1000  avgt   60    935841.841 ±   11823.809  ns/op
Ring.trip        false       sq         false             1             8       1000  avgt   60    947621.351 ±   15751.615  ns/op
Ring.trip        false       sq         false             4             1       1000  avgt   60   1289123.658 ±  347305.920  ns/op
Ring.trip        false       sq         false             4             4       1000  avgt   60   1053212.580 ±    9732.391  ns/op
Ring.trip        false       sq         false             4             8       1000  avgt   60   1015458.167 ±   71451.332  ns/op
Ring.trip        false       sq         false            16             1       1000  avgt   60   1047941.786 ±   10775.830  ns/op
Ring.trip        false       sq         false            16             4       1000  avgt   60   1033898.148 ±   25855.874  ns/op
Ring.trip        false       sq         false            16             8       1000  avgt   60   1028970.008 ±    8556.825  ns/op
Ring.trip        false       sq         false            64             1       1000  avgt   60   1274105.598 ±   99828.474  ns/op
Ring.trip        false       sq         false            64             4       1000  avgt   60   2043190.996 ±  364431.374  ns/op
Ring.trip        false       sq         false            64             8       1000  avgt   60   2131769.282 ±  184472.588  ns/op
Ring.trip        false       sq         false           256             1       1000  avgt   60   4473116.963 ±  200087.558  ns/op
Ring.trip        false       sq         false           256             4       1000  avgt   60   5166688.950 ±  324642.199  ns/op
Ring.trip        false       sq         false           256             8       1000  avgt   60   9832731.679 ±  432011.747  ns/op
```



#### Loom

```bash
Result "org.example.Ring.trip":
  4348134.688 ±(99.9%) 217259.176 ns/op [Average]
  (min, avg, max) = (3757978.889, 4348134.688, 5633613.333), stdev = 485931.315
  CI (99.9%): [4130875.512, 4565393.863] (assumes normal distribution)


# Run complete. Total time: 02:02:21

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

Benchmark  (allocalot)  (queue)  (singleshot)  (stackDepth)  (stackFrame)  (threads)  Mode  Cnt         Score         Error  Units
Ring.trip         true       sq          true             1             1       1000  avgt   60   1439322.398 ±   31521.867  ns/op
Ring.trip         true       sq          true             1             4       1000  avgt   60   1488476.088 ±   42898.918  ns/op
Ring.trip         true       sq          true             1             8       1000  avgt   60   1523387.176 ±   30900.102  ns/op
Ring.trip         true       sq          true             4             1       1000  avgt   60   1569493.319 ±   61293.449  ns/op
Ring.trip         true       sq          true             4             4       1000  avgt   60   1464370.088 ±   26147.384  ns/op
Ring.trip         true       sq          true             4             8       1000  avgt   60   1654358.699 ±   38920.023  ns/op
Ring.trip         true       sq          true            16             1       1000  avgt   60   1638074.561 ±   27168.943  ns/op
Ring.trip         true       sq          true            16             4       1000  avgt   60   1728886.518 ±   96001.228  ns/op
Ring.trip         true       sq          true            16             8       1000  avgt   60   1764394.237 ±   38599.348  ns/op
Ring.trip         true       sq          true            64             1       1000  avgt   60   1962124.037 ±   72834.126  ns/op
Ring.trip         true       sq          true            64             4       1000  avgt   60   2809407.383 ±   53315.252  ns/op
Ring.trip         true       sq          true            64             8       1000  avgt   60   3615263.244 ±  463531.547  ns/op
Ring.trip         true       sq          true           256             1       1000  avgt   60   3847974.646 ±  145739.904  ns/op
Ring.trip         true       sq          true           256             4       1000  avgt   60   6577977.054 ±  678516.624  ns/op
Ring.trip         true       sq          true           256             8       1000  avgt   60  14315743.617 ± 1385207.753  ns/op
Ring.trip         true       sq         false             1             1       1000  avgt   60    962736.406 ±   41147.364  ns/op
Ring.trip         true       sq         false             1             4       1000  avgt   60    935106.720 ±   35893.957  ns/op
Ring.trip         true       sq         false             1             8       1000  avgt   60    951998.181 ±   56104.200  ns/op
Ring.trip         true       sq         false             4             1       1000  avgt   60   1003178.361 ±   18260.973  ns/op
Ring.trip         true       sq         false             4             4       1000  avgt   60   1016453.336 ±   28260.506  ns/op
Ring.trip         true       sq         false             4             8       1000  avgt   60   1031031.027 ±   36307.047  ns/op
Ring.trip         true       sq         false            16             1       1000  avgt   60   1044743.979 ±   28799.230  ns/op
Ring.trip         true       sq         false            16             4       1000  avgt   60   1053632.691 ±   17105.606  ns/op
Ring.trip         true       sq         false            16             8       1000  avgt   60   1146293.096 ±   33930.547  ns/op
Ring.trip         true       sq         false            64             1       1000  avgt   60   1104410.652 ±   24661.521  ns/op
Ring.trip         true       sq         false            64             4       1000  avgt   60   1489766.361 ±   71025.337  ns/op
Ring.trip         true       sq         false            64             8       1000  avgt   60   2595503.884 ±  114833.898  ns/op
Ring.trip         true       sq         false           256             1       1000  avgt   60   3441005.172 ±  299450.877  ns/op
Ring.trip         true       sq         false           256             4       1000  avgt   60   6548207.747 ±  319881.573  ns/op
Ring.trip         true       sq         false           256             8       1000  avgt   60  10962191.394 ±  659530.545  ns/op
Ring.trip        false       sq          true             1             1       1000  avgt   60   1709609.467 ±   41541.488  ns/op
Ring.trip        false       sq          true             1             4       1000  avgt   60   1643729.629 ±   29475.373  ns/op
Ring.trip        false       sq          true             1             8       1000  avgt   60   1510606.226 ±   21153.605  ns/op
Ring.trip        false       sq          true             4             1       1000  avgt   60   1628137.903 ±   31794.481  ns/op
Ring.trip        false       sq          true             4             4       1000  avgt   60   1647405.923 ±   40967.952  ns/op
Ring.trip        false       sq          true             4             8       1000  avgt   60   1553336.905 ±   34792.124  ns/op
Ring.trip        false       sq          true            16             1       1000  avgt   60   1600414.239 ±   43951.884  ns/op
Ring.trip        false       sq          true            16             4       1000  avgt   60   1631887.367 ±   35365.472  ns/op
Ring.trip        false       sq          true            16             8       1000  avgt   60   1841535.083 ±   64736.370  ns/op
Ring.trip        false       sq          true            64             1       1000  avgt   60   1954800.154 ±   76568.626  ns/op
Ring.trip        false       sq          true            64             4       1000  avgt   60   2090295.742 ±   60487.853  ns/op
Ring.trip        false       sq          true            64             8       1000  avgt   60   2121541.183 ±   94528.475  ns/op
Ring.trip        false       sq          true           256             1       1000  avgt   60   4631673.218 ±  636837.528  ns/op
Ring.trip        false       sq          true           256             4       1000  avgt   60   7129298.471 ± 1244346.384  ns/op
Ring.trip        false       sq          true           256             8       1000  avgt   60   6515093.471 ±  757045.921  ns/op
Ring.trip        false       sq         false             1             1       1000  avgt   60    846960.056 ±   20592.385  ns/op
Ring.trip        false       sq         false             1             4       1000  avgt   60   1005968.694 ±   32207.747  ns/op
Ring.trip        false       sq         false             1             8       1000  avgt   60    951319.963 ±   53748.513  ns/op
Ring.trip        false       sq         false             4             1       1000  avgt   60    897392.975 ±   37556.273  ns/op
Ring.trip        false       sq         false             4             4       1000  avgt   60    818960.689 ±   31949.389  ns/op
Ring.trip        false       sq         false             4             8       1000  avgt   60    829095.477 ±   44409.865  ns/op
Ring.trip        false       sq         false            16             1       1000  avgt   60    798250.551 ±   25992.656  ns/op
Ring.trip        false       sq         false            16             4       1000  avgt   60    824147.572 ±   38904.524  ns/op
Ring.trip        false       sq         false            16             8       1000  avgt   60    807108.660 ±   19644.688  ns/op
Ring.trip        false       sq         false            64             1       1000  avgt   60    824252.087 ±   25564.116  ns/op
Ring.trip        false       sq         false            64             4       1000  avgt   60    890789.686 ±   24291.418  ns/op
Ring.trip        false       sq         false            64             8       1000  avgt   60   1022924.341 ±   17073.801  ns/op
Ring.trip        false       sq         false           256             1       1000  avgt   60   2644856.304 ±   81473.167  ns/op
Ring.trip        false       sq         false           256             4       1000  avgt   60   4059619.400 ±   89837.465  ns/op
Ring.trip        false       sq         false           256             8       1000  avgt   60   4348134.688 ±  217259.176  ns/op

Process finished with exit code 0
```





#### 结论：

为了得到最佳性能，应考虑使用较低的 `stackDepth` 和 `stackFrame` 值，设置 `allocalot` 为 `true`，并考虑设置 `singleshot`为false。

#### **与Loom的整体性能对比**：

- Kona Fiber的`Ring.trip`平均性能为9,832,731.679±432,011.7479,832,731.679±432,011.747 ns/op。
- Loom的`Ring.trip`平均性能为4,348,134.688±217,259.1764,348,134.688±217,259.176 ns/op。

显然，Loom的性能更好，其平均值不到Kona Fiber的一半。

---

### Worker 启动方法的更改：

在 Loom 版本中，`getChannel()` 方法使用了增强的 switch 表达式语法，而在 Kona 版本中，我将其改回了传统的 switch 语句。

原始 Loom 版本：

```java
return switch(stackFrame) {
    ...
    default -> throw new RuntimeException(...);
};
```

更改后的 Kona 版本：

```java
switch (stackFrame) {
    ...
    default:
        throw new RuntimeException(...);
}
```

---

以下是原始版本与我适应 Kona 进行的版本之间的 git diff：

```diff
...
```

（注意：这里只提供了部分差异化内容，具体差异化内容可能需要更详细地列出）

---




Altered the Method for Starting Workers:

In the Loom version, the getChannel() method used the enhanced switch expression syntax:
return switch(stackFrame) {

```java
default -> throw new RuntimeException("Illegal stack parameter value: "+ stackFrame +" (allowed: 1,2,4,8)");
};
In the Kona version, I changed it back to the conventional switch statement since the arrow syntax used in “switch” only can be complied in jdk 14 or other later version.
switch (stackFrame) {
...
default:
throw new RuntimeException("Illegal stack parameter value: " + stackFrame + " (allowed: 1,2,4,8)");
}
```



Here is the git diff between the original one and the version I made it adapted to Kona.

```bash
git diff /Users/lth/Downloads/loom-fibers/test/micro/org/openjdk/bench/loom/ring/Ring.java /Users/lth/Desktop/OpenSourceRecord/kona_copy/TencentKona-8/demo/fiber/Ring_test/src/main/java/org/example/Ring.java
diff --git a/Users/lth/Downloads/loom-fibers/test/micro/org/openjdk/bench/loom/ring/Ring.java b/Users/lth/Desktop/OpenSourceRecord/kona_copy/TencentKona-8/demo/fiber/Ring_test/src/main/java/org/example/Ring.java
old mode 100755
new mode 100644
index 58f7458..1271b77
--- a/Users/lth/Downloads/loom-fibers/test/micro/org/openjdk/bench/loom/ring/Ring.java
+++ b/Users/lth/Desktop/OpenSourceRecord/kona_copy/TencentKona-8/demo/fiber/Ring_test/src/main/java/org/example/Ring.java
@@ -20,8 +20,10 @@
  * or visit www.oracle.com if you need additional information or have any
  * questions.
  */
-package org.openjdk.bench.loom.ring;
+package org.example;

+import org.example.Channel;
+import org.example.Channels;
 import org.openjdk.jmh.annotations.Benchmark;
 import org.openjdk.jmh.annotations.BenchmarkMode;
 import org.openjdk.jmh.annotations.Fork;
@@ -154,14 +156,18 @@ public class Ring {
     }

     Channel<Integer> getChannel() {
-        return switch(stackFrame) {
-            case 1 -> new Channels.ChannelFixedStackR1<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
-            case 2 -> new Channels.ChannelFixedStackR2<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
-            case 4 -> new Channels.ChannelFixedStackR4<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
-            case 8 -> new Channels.ChannelFixedStackR8<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
-            default -> throw new RuntimeException("Illegal stack parameter value: "+ stackFrame +" (allowed: 1,2,4,8)");
-        };
-
+        switch (stackFrame) {
+            case 1:
+                return new Channels.ChannelFixedStackR1<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
+            case 2:
+                return new Channels.ChannelFixedStackR2<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
+            case 4:
+                return new Channels.ChannelFixedStackR4<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
+            case 8:
+                return new Channels.ChannelFixedStackR8<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
+            default:
+                throw new RuntimeException("Illegal stack parameter value: " + stackFrame + " (allowed: 1,2,4,8)");
+        }
     }

 }
(END)
```

