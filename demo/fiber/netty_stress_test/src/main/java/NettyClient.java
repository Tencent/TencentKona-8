/*
 *
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

import io.netty.bootstrap.Bootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioSocketChannel;

import io.netty.channel.Channel;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import io.netty.handler.codec.string.StringDecoder;  
import io.netty.handler.codec.string.StringEncoder;
import io.netty.handler.codec.DelimiterBasedFrameDecoder;
import io.netty.channel.ChannelOption;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;

public class NettyClient {
    private Channel channel;
    private static final AtomicLong INVOKE_ID = new AtomicLong(0);
    private Bootstrap b;

    public NettyClient() {
        EventLoopGroup group = new NioEventLoopGroup();
        NettyClientHandler clientHandler = new NettyClientHandler();
        try {
            b = new Bootstrap();
            b.group(group).channel(NioSocketChannel.class).option(ChannelOption.TCP_NODELAY, true)
                    .handler(new ChannelInitializer<SocketChannel>() {
                        @Override
                        public void initChannel(SocketChannel ch) throws Exception {
                            ByteBuf delimiter = Unpooled.copiedBuffer("|".getBytes());
                            ch.pipeline().addLast(new DelimiterBasedFrameDecoder(1000, delimiter))
                                         .addLast(new StringDecoder())
                                         .addLast(new StringEncoder())
                                         .addLast(new NettyClientHandler());
                        }
                    });
            ChannelFuture f = b.connect("127.0.0.1", 6668).sync();
            if (f.isDone() && f.isSuccess()) {
                this.channel = f.channel();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void sendMsg(String msg) {
        channel.writeAndFlush(msg);
    }

    public void close() {
        if (b != null) {
            b.group().shutdownGracefully();
        }
        if (channel != null) {
            channel.close();
        }
    }

    public String SyncCall(String msg) throws InterruptedException, ExecutionException {
        CompletableFuture<String> future = new CompletableFuture<>();

        String reqId = INVOKE_ID.getAndIncrement() + "";
        msg = FutureMapUtil.generatorFrame(msg, reqId);

        FutureMapUtil.put(reqId, future);
        sendMsg(msg);

        return future.get();
    }
}
