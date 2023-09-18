How to Run the Test
Make sure Java 1.8.0_362_fiber is installed.
Run the command mvn clean packageto build the project.
Run the main method in the class org.example.Ring to start the test.
See the result in the file res.json
changes:
Add jmh dependencies in pom.xml

```
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
运行介绍

测试目标：

测试的主要目的是衡量 Ring 类中 trip 方法的性能。该方法发送一个消息并接收一个响应，评估在一个由多个Worker线程组成的循环链路中。消息是通过一个循环链路（Ring）传递的，由多个Worker线程组成。每个Worker线程从它的source通道接收消息，并将该消息发送到它的sink通道。这就形成了一个消息传递链，从head通道开始，经过所有的Worker线程，最后到达tail通道。

测试参数：
threads: 测试中使用的线程数。
queue: 使用的队列类型，只有 "sq"（SynchronousQueue）。
stackDepth: 栈深度，取值有 "1", "4", "16", "64", "256"。
stackFrame: 栈帧，取值有 "1", "4", "8"。
allocalot: 是否为每个消息分配固定的内存（根据代码，4242表示固定的内存量，0表示无）。
singleshot: 是否是单次发送/接收。

测试结果分析：
总体性能: 第一个给出的结果是平均时间为 9832731.679 ns/op，其中 ns/op 表示每个操作的纳秒数。该结果具有 99.9% 的置信区间，误差范围在 ±432011.747 ns/op。
不同参数下的性能: 结果表中列出了不同参数组合下的性能数据。从数据可以看出，随着 stackDepth 和 stackFrame 的增加，操作的纳秒数通常会增加，这意味着性能会下降。
allocalot 参数: 当 allocalot 为 true 时，性能通常较差。这可能是因为为每个消息分配固定的内存会增加额外的开销。
singleshot 参数: 当 singleshot 为 true 时，性能似乎较好，可能是因为只发送和接收一次消息，而不是在整个测试期间持续发送和接收。

代码分析：
Worker 类: 代表一个任务，它从一个源通道接收消息，然后将消息发送到一个目标通道。它还接收一个 finisher 函数，该函数决定何时停止工作。
setup(): 在基准测试开始前设置测试环境。
tearDown(): 在基准测试结束后清理资源。
trip(): 这是实际进行基准测试的方法。它启动所有的工作线程，发送一个消息，并接收一个响应。
getChannel(): 根据指定的 stackFrame 参数创建并返回一个通道。

结论：
该测试主要关注于 Ring 类的 trip 方法的性能。
随着 stackDepth 和 stackFrame 的增加，性能通常会降低。
allocalot 参数为 true 时，性能可能会受到影响，因为为每个消息分配固定的内存增加了额外的开销。
singleshot 参数为 true 时，性能似乎较好，这可能是因为测试只发送和接收一次消息。
总的来说，为了得到最佳性能，应考虑使用较低的 stackDepth 和 stackFrame 值，设置 allocalot 为 false，并考虑使用 singleshot。




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
git diff /Users/lth/Downloads/loom-fibers/test/micro/org/openjdk/bench/loom/ring/Ring.java /Users/lth/Desktop/OpenSourceRecord/kona_copy/TencentKona-8/demo/fiber/Ring_test/src/main/java/org/example/Ring.java
diff --git a/Users/lth/Downloads/loom-fibers/test/micro/org/openjdk/bench/loom/ring/Ring.java b/Users/lth/Desktop/OpenSourceRecord/kona_copy/TencentKona-8/demo/fiber/Ring_test/src/main/java/org/example/Ring.java
old mode 100755
new mode 100644
index 58f74586..98d50663
--- a/Users/lth/Downloads/loom-fibers/test/micro/org/openjdk/bench/loom/ring/Ring.java
+++ b/Users/lth/Desktop/OpenSourceRecord/kona_copy/TencentKona-8/demo/fiber/Ring_test/src/main/java/org/example/Ring.java
@@ -20,8 +20,10 @@

- or visit [www.oracle.com](http://www.oracle.com/) if you need additional information or have any
- questions.
  */
  -package org.openjdk.bench.loom.ring;
  +package org.example;

+import org.example.Channel;
+import org.example.Channels;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
@@ -92,7 +94,6 @@ public class Ring {
[@param](https://github.com/param)({"true", "false"})
public boolean singleshot;

- [@setup](https://github.com/setup)
  [@SuppressWarnings](https://github.com/SuppressWarnings)("unchecked")
  public void setup() {
  @@ -154,14 +155,18 @@ public class Ring {
  }

  Channel getChannel() {

     return switch(stackFrame) {

         case 1 -> new Channels.ChannelFixedStackR1<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
      
         case 2 -> new Channels.ChannelFixedStackR2<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
      
         case 4 -> new Channels.ChannelFixedStackR4<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
      
         case 8 -> new Channels.ChannelFixedStackR8<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
      
         default -> throw new RuntimeException("Illegal stack parameter value: "+ stackFrame +" (allowed: 1,2,4,8)");

     };

     switch (stackFrame) {
         case 1:
             return new Channels.ChannelFixedStackR1<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);
  ```
         case 2:
             return new Channels.ChannelFixedStackR2<>(getQueue(queue), stackDepth, allocalot ? 4242 : 0);



