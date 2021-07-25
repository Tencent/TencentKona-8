dependency:
1. iotdb

test steps:
1. create DATABASE zm;(execute "SET STORAGE GROUP TO root.zm")
2. insert data into root.zm;(execute "INSERT INTO root.zm.wf01.wt01(timestamp,status,temperature) values(200,false,20.71)");
3. execute "java -jar target/iotdb-sync-stress-demo-1.0-SNAPSHOT-jar-with-dependencies.jar 1000 100000 0"(1000 means thread num, 100000 means request count, 0 means use fiber)


test result:
| thread count  | use thread directly | use thread pool  |  KonaFiber | async |
| ------------ | ------------ | ------------ | ------------ | ------------ |
| 1000    |30238.88 |38624.95  | 44326.24  | 51440.32 |
| 3000    |2151.46    | 16680.56 |42354.93  | 51786.63 |
