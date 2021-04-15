dependency:
1. mysql

test steps:
1. create DATABASE zm;(execute "create DATABASE zm;")
2. create TABLE hello;(execute "CREATE TABLE hello ( id BIGINT NOT NULL AUTO_INCREMENT, hello VARCHAR(255), response VARCHAR(255), PRIMARY KEY(id));");
3. insert data into hello;(execute "INSERT INTO hello (id, hello, response) VALUES ("801", "hello", "zm");");
4. execute "java -jar target/mysql-demo-1.0-SNAPSHOT-jar-with-dependencies.jar 1000 100000 0"(1000 means thread num, 100000 means request count, 0 means use fiber)


test result:
| thread count  | use thread directly | use thread pool  |  KonaFiber | async |
| ------------ | ------------ | ------------ | ------------ | ------------ |
| 10000    |42582.18 |51546.39  | 55997.31  | 62394.70 |
| 30000    |8761.37    | 46989.53 |56372.96  | 65963.06 |
