﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#ifdef ENABLE_MP4
#include <signal.h>
#include <atomic>
#include <iostream>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/CMD.h"
#include "Common/config.h"
#include "Common/Parser.h"
#include "Rtsp/Rtsp.h"
#include "Thread/WorkThreadPool.h"
#include "Pusher/MediaPusher.h"
#include "Player/PlayerProxy.h"
#include "Record/MP4Reader.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

class CMD_main : public CMD {
public:
    CMD_main() {
        _parser.reset(new OptionParser(nullptr));

        (*_parser) << Option('l',/*该选项简称，如果是\x00则说明无简称*/
                             "level",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(LTrace).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "日志等级,LTrace~LError(0~4)",/*该选项说明文字*/
                             nullptr);


        (*_parser) << Option('t',/*该选项简称，如果是\x00则说明无简称*/
                             "threads",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(thread::hardware_concurrency()).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动事件触发线程数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('i',/*该选项简称，如果是\x00则说明无简称*/
                             "in",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             nullptr,/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "拉流url,支持rtsp/rtmp/hls/mp4文件",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('o',/*该选项简称，如果是\x00则说明无简称*/
                             "out",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             nullptr,/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "推流url,支持rtsp/rtmp",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('c',/*该选项简称，如果是\x00则说明无简称*/
                             "count",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "1000",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "推流客户端个数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('d',/*该选项简称，如果是\x00则说明无简称*/
                             "delay",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "50",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动推流客户端间隔,单位毫秒",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('m',/*该选项简称，如果是\x00则说明无简称*/
                             "merge",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "300",/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "推流合并写毫秒,合并写能提高性能",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('T',/*该选项简称，如果是\x00则说明无简称*/
                             "rtp",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string((int) (Rtsp::RTP_TCP)).data(),/*该选项默认值*/
                             true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                            "rtsp拉流和推流方式,支持tcp/udp:0/1", /*该选项说明文字*/
                            nullptr);
    }

    ~CMD_main() override {}

    const char *description() const override { return "主程序命令参数"; }
};

// 此程序用于推流性能测试  [AUTO-TRANSLATED:45b48457]
// This program is used for streaming performance testing
int main(int argc, char *argv[]) {
    CMD_main cmd_main;
    try {
        cmd_main.operator()(argc, argv);
    } catch (ExitException &) {
        return 0;
    } catch (std::exception &ex) {
        cout << ex.what() << endl;
        return -1;
    }

    int threads = cmd_main["threads"];
    LogLevel logLevel = (LogLevel)cmd_main["level"].as<int>();
    logLevel = MIN(MAX(logLevel, LTrace), LError);
    auto in_url = cmd_main["in"];
    auto out_url = cmd_main["out"];
    auto rtp_type = cmd_main["rtp"].as<int>();
    auto delay_ms = cmd_main["delay"].as<int>();
    auto pusher_count = cmd_main["count"].as<int>();
    auto merge_ms = cmd_main["merge"].as<int>();
    auto schema = findSubString(out_url.data(), nullptr, "://");
    if (schema != RTSP_SCHEMA && schema != RTMP_SCHEMA) {
        cout << "推流协议只支持rtsp或rtmp！" << endl;
        return -1;
    }
    const std::string app = "app";
    const std::string stream = "test";

    // 设置日志  [AUTO-TRANSLATED:50372045]
    // Set log
    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", logLevel));
    // 启动异步日志线程  [AUTO-TRANSLATED:c93cc6f4]
    // Start asynchronous log thread
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    // 设置线程数  [AUTO-TRANSLATED:22ec5cc9]
    // Set the number of threads
    EventPollerPool::setPoolSize(threads);
    WorkThreadPool::setPoolSize(threads);

    // 设置合并写  [AUTO-TRANSLATED:7bf3456d]
    // Set merge write
    mINI::Instance()[General::kMergeWriteMS] = merge_ms;

    ProtocolOption option;
    option.enable_hls = false;
    option.enable_mp4 = false;
    MediaSource::Ptr src = nullptr;
    PlayerProxy::Ptr proxy = nullptr;;

    auto tuple = MediaTuple { DEFAULT_VHOST, app, stream, "" };
    if (end_with(in_url, ".mp4")) {
        // create MediaSource from mp4file
        auto reader = std::make_shared<MP4Reader>(tuple, in_url);
        //mp4 repeat
        reader->startReadMP4(0, true, true);
        src = MediaSource::find(schema, DEFAULT_VHOST, app, stream, false);
        if (!src) {
            // mp4文件不存在  [AUTO-TRANSLATED:80188fb8]
            // mp4 file does not exist
            WarnL << "no such file or directory: " << in_url;
            return -1;
        }
    } else {
        // 添加拉流代理  [AUTO-TRANSLATED:aa516f44]
        // Add pull stream proxy
        proxy = std::make_shared<PlayerProxy>(tuple, option);
        // rtsp拉流代理方式  [AUTO-TRANSLATED:065d328d]
        // rtsp pull stream proxy method
        (*proxy)[Client::kRtpType] = rtp_type;
        // 开始拉流代理  [AUTO-TRANSLATED:6937338d]
        // Start pull stream proxy
        proxy->play(in_url);

    }

    auto get_src = [schema,app,stream]() {
        return MediaSource::find(schema, DEFAULT_VHOST, app, stream, false);
    };

    // 推流器map  [AUTO-TRANSLATED:279fcfb0]
    // Streamer map
    recursive_mutex mtx;
    unordered_map<void *, MediaPusher::Ptr> pusher_map;


    auto add_pusher = [&](const MediaSource::Ptr &src, const string &rand_str, size_t index) {
        auto pusher = std::make_shared<MediaPusher>(src);
        auto tag = pusher.get();
        pusher->setOnCreateSocket([](const EventPoller::Ptr &poller) {
            // socket关闭互斥锁，提高性能  [AUTO-TRANSLATED:471fc644]
            // Socket close mutex, improve performance
            return Socket::createSocket(poller, false);
        });
        // 设置推流失败监听  [AUTO-TRANSLATED:4ad49de9]
        // Set push stream failure listener
        pusher->setOnPublished([&mtx, &pusher_map, tag](const SockException &ex) {
            if (ex) {
                // 推流失败，移除之  [AUTO-TRANSLATED:97a29246]
                // Push stream failed, remove it
                lock_guard<recursive_mutex> lck(mtx);
                pusher_map.erase(tag);
            }
        });
        // 设置推流中途断开监听  [AUTO-TRANSLATED:228076ef]
        // Set push stream disconnection listener
        pusher->setOnShutdown([&mtx, &pusher_map, tag](const SockException &ex) {
            // 推流中途失败，移除之  [AUTO-TRANSLATED:00e0928a]
            // Push stream failed halfway, remove it
            lock_guard<recursive_mutex> lck(mtx);
            pusher_map.erase(tag);
        });
        // 设置rtsp推流方式(在rtsp推流时有效)  [AUTO-TRANSLATED:2dc733df]
        // Set rtsp push stream method (effective when rtsp push stream)
        (*pusher)[Client::kRtpType] = rtp_type;
        // 发起推流请求,每个推流端的stream_id都不一样  [AUTO-TRANSLATED:8b356fcb]
        // Initiate push stream request, each push stream end has a different stream_id
        string url = StrPrinter << out_url << "_" << rand_str << "_" << index;
        pusher->publish(url);

        // 保持对象不销毁  [AUTO-TRANSLATED:650977d0]
        // Keep the object from being destroyed
        lock_guard<recursive_mutex> lck(mtx);
        pusher_map.emplace(tag, std::move(pusher));

        // 休眠后再启动下一个推流，防止短时间海量链接  [AUTO-TRANSLATED:df224fc9]
        // Sleep and then start the next push stream to prevent massive connections in a short time
        if (delay_ms > 0) {
            usleep(1000 * delay_ms);
        }
    };

    // 设置退出信号  [AUTO-TRANSLATED:02c7fa30]
    // Set exit signal
    static bool exit_flag = false;
    signal(SIGINT, [](int) { exit_flag = true; });
    while (!exit_flag) {
        // 休眠一秒打印  [AUTO-TRANSLATED:239dc996]
        // Sleep for one second and print
        sleep(1);

        size_t alive_pusher = 0;
        {
            lock_guard<recursive_mutex> lck(mtx);
            alive_pusher = pusher_map.size();
        }
        InfoL << "在线推流器个数:" << alive_pusher;
        auto src = get_src();
        for(size_t i = 0; i < pusher_count - alive_pusher && src && !exit_flag; ++i){
            // 有些推流器失败了，那么我们重试添加  [AUTO-TRANSLATED:d01fb300]
            // Some push streamers failed, so we retry adding
            add_pusher(get_src(), makeRandStr(8), i);
        }
    }

    return 0;
}

#endif
