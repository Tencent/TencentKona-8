dependency:

redis v6.2.5

test steps:

1. $ set "hello" "world"
2. mvn clean install
3. java -jar target/redis-stress-demo-1.0-SNAPSHOT-jar-with-dependencies.jar  $thread_num $request_cnt $whether_use_fiber $whether_use_async

test result:

| thread count  | thread async |  KonaFiber async| thread sync |  KonaFiber sync|
| ------------ | ------------ | ------------ |------------ | ------------ |
| 1000    |14471.78   | 22675.73  | 13227.51   | 17574.69 |
| 3000    |7689.22    | 17921.14  | 7633.58   | 16806.72  |

test device: 

MacBook Pro (16-inch, 2019) 

2.6 GHz 6-core processor Core i7 

16 GB 2667 MHz DDR4

