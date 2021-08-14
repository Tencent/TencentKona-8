import io.lettuce.core.RedisClient;
import io.lettuce.core.RedisFuture;
import io.lettuce.core.RedisURI;
import io.lettuce.core.api.StatefulRedisConnection;
import io.lettuce.core.api.async.RedisAsyncCommands;
import io.lettuce.core.api.sync.RedisCommands;

import java.time.Duration;
import java.time.temporal.ChronoUnit;
import java.util.concurrent.ExecutionException;

public class RedisUtil {
    private static StatefulRedisConnection<String, String> CONNECTION;
    private static RedisClient CLIENT;

    private static RedisAsyncCommands<String, String> ASYNC_COMMAND;
    private static RedisCommands<String, String> SYNC_COMMAND;

    RedisUtil() {
        RedisURI redisUri = RedisURI.builder()
                .withHost("localhost")
                .withPort(6379)
                .withTimeout(Duration.of(10, ChronoUnit.SECONDS))
                .build();
        CLIENT = RedisClient.create(redisUri);
        CONNECTION = CLIENT.connect();
        ASYNC_COMMAND = CONNECTION.async();
        SYNC_COMMAND = CONNECTION.sync();
    }

    public String AsyncCall(String key) throws ExecutionException, InterruptedException {
        RedisFuture<String> future = ASYNC_COMMAND.get(key);
        return future.get();
    }

    public String SyncCall(String key) {
        return SYNC_COMMAND.get(key);
    }

    public void close() {
        CONNECTION.close();
        CLIENT.shutdown();
    }
}
