Dependency:

Oracle Oracle 11g

1. Start tns monitoring

```shell
#Linux 
>root@itwai# lsnrctl status
>root@itwai# lsnrctl start

```

2. Log in to oracle

```shell
#Linux 
>root@itwai# sqlplus / as sysdba
```

---

Test steps:
1. create DATABASE zm;(execute "create DATABASE zm;")
2. create TABLE hello;(execute "CREATE TABLE hello ( id number(8,0), hello VARCHAR(255), response VARCHAR(255), PRIMARY KEY(id));");
3. insert data into hello;(execute "INSERT INTO hello (id, hello, response) VALUES ("800", "hello", "zm");");
4. execute "java -jar target/mysql-demo-1.0-SNAPSHOT-jar-with-dependencies.jar 1000 100000 0"(1000 means thread num, 100000 means request count, 0 means use fiber)

---

Test result:

| thread count  | use thread directly | use thread pool  |  KonaFiber | async |
| ------------ | ------------ | ------------ | ------------ | ------------ |
| 1000    |30116.89 |29818.87  | 39221.96 | 49166.09 |
| 3000    |1961.37    | 14228.67 |33982.77  | 48724.43 |

---

Device:

Device Name:	HP
processor:	Intel(R) Core(TM) i5-8265U CPU @ 1.60GHz   1.80 GHz
RAM:	16.0 GB  2666 MHz
