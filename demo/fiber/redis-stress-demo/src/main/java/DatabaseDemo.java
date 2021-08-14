import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;

public class DatabaseDemo {
    private static ExecutorService e;

    private static int threadCount;
    private static int requestCount;
    private static int statsInterval;
    private static boolean useFiber;
    private static boolean useAsync;

    public static void initExecutor() {
        ThreadFactory factory;
        if (useFiber) {
            factory = Thread.ofVirtual().name("Test-", 0).factory();
        } else {
            factory = Thread.ofPlatform().factory();
        }
        e = Executors.newFixedThreadPool(threadCount, factory);
    }

    static void testCall() throws Exception {

        CountDownLatch startSignal = new CountDownLatch(1);
        CountDownLatch doneSignal = new CountDownLatch(requestCount);
        AtomicLong count = new AtomicLong();
        AtomicLong statsTimes = new AtomicLong();

        RedisUtil redis = new RedisUtil();

        Runnable r = new Runnable() {
            @Override
            public void run() {
                try {
                    startSignal.await();
                    String result;
                    if (useAsync) {
                        result = redis.AsyncCall("hello");
                    } else {
                        result = redis.SyncCall("hello");
                    }

                    long val = count.addAndGet(1);
                    if ((val % statsInterval) == 0) {
                        long time = System.currentTimeMillis();
                        long prev = statsTimes.getAndSet(time);
                        System.out.println("interval " + val + " throughput " + statsInterval / ((time - prev) / 1000.0));
                    }
                    //System.out.println("[redis]get value: " + result);
                    doneSignal.countDown();
                } catch (Exception e) {

                }
            }
        };

        for (int i = 0; i < requestCount; i++) {
            e.execute(r);
        }

        long before = System.currentTimeMillis();
        statsTimes.set(before);
        startSignal.countDown();
        doneSignal.await();

        long after = System.currentTimeMillis();
        long duration = (after - before);
        System.out.println("finish " + count.get() + " time " + duration + "ms throughput " + (count.get() / (duration / 1000.0)));

        e.shutdown();
        e.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
        redis.close();
    }

    public static void main(String[] args) throws Exception {

        threadCount = Integer.parseInt(args[0]);
        requestCount = Integer.parseInt(args[1]);
        useFiber = Boolean.parseBoolean(args[2]);
        useAsync = Boolean.parseBoolean(args[3]);
        statsInterval = requestCount / 10;

        System.out.println("options: thread " + threadCount + " requestCount " + requestCount + " use fiber " + useFiber + " use async " + useAsync);

        initExecutor();

        testCall();

        System.exit(0);

    }
}
