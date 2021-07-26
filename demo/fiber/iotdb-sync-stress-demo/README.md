dependency:
1. iotdb [v0.12.1](https://www.apache.org/dyn/closer.cgi/iotdb/0.12.1/apache-iotdb-0.12.1-server-bin.zip)

2. start iotdb server
```
# Unix/OS X
> nohup sbin/start-server.sh >/dev/null 2>&1 &
or
> nohup sbin/start-server.sh -c <conf_path> -rpc_port <rpc_port> >/dev/null 2>&1 &

# Windows
> sbin\start-server.bat -c <conf_path> -rpc_port <rpc_port>
```
3. start iotdb cli

```
# Unix/OS X
> sbin/start-cli.sh -h 127.0.0.1 -p 6667 -u root -pw root

# Windows
> sbin\start-cli.bat -h 127.0.0.1 -p 6667 -u root -pw root
```

test steps:
1. create DATABASE zm;(execute "SET STORAGE GROUP TO root.zm")
2. insert data into root.zm;(execute "INSERT INTO root.zm.wf01.wt01(timestamp,status,temperature) values(200,false,20.71)");
3. execute "java -jar target/iotdb-sync-stress-demo-1.0-SNAPSHOT-jar-with-dependencies.jar 1000 100000 0"(1000 means thread num, 100000 means request count, 0 means use fiber)


test result:
| thread count  | use thread directly | use thread pool  |  KonaFiber | async |
| ------------ | ------------ | ------------ | ------------ | ------------ |
| 1000    |30238.88 |38624.95  | 44326.24  | 51440.32 |
| 3000    |2151.46    | 16680.56 |42354.93  | 51786.63 |
