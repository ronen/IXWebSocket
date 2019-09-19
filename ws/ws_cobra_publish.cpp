/*
 *  ws_cobra_publish.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2019 Machine Zone, Inc. All rights reserved.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <jsoncpp/json/json.h>
#include <ixcobra/IXCobraMetricsPublisher.h>
#include <spdlog/spdlog.h>

namespace ix
{
    int ws_cobra_publish_main(const std::string& appkey,
                              const std::string& endpoint,
                              const std::string& rolename,
                              const std::string& rolesecret,
                              const std::string& channel,
                              const std::string& path)
    {
        std::ifstream f(path);
        std::string str((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

        Json::Value data;
        Json::Reader reader;
        if (!reader.parse(str, data))
        {
            spdlog::info("Input file is not a JSON file");
            return 1;
        }

        ix::CobraConnection conn;
        conn.configure(appkey, endpoint,
                       rolename, rolesecret,
                       ix::WebSocketPerMessageDeflateOptions(true));
        conn.connect();

        // Display incoming messages
        std::atomic<bool> authenticated(false);
        std::atomic<bool> messageAcked(false);
        std::condition_variable condition;

        conn.setEventCallback(
            [&conn, &channel, &data, &authenticated, &messageAcked, &condition]
            (ix::CobraConnectionEventType eventType,
             const std::string& errMsg,
             const ix::WebSocketHttpHeaders& headers,
             const std::string& subscriptionId,
             CobraConnection::MsgId msgId)
            {
                if (eventType == ix::CobraConnection_EventType_Open)
                {
                    spdlog::info("Publisher connected");

                    for (auto it : headers)
                    {
                        spdlog::info("{}: {}", it.first, it.second);
                    }
                }
                else if (eventType == ix::CobraConnection_EventType_Authenticated)
                {
                    spdlog::info("Publisher authenticated");
                    authenticated = true;

                    spdlog::info("Publishing data");

                    Json::Value channels;
                    channels[0] = channel;
                    conn.publish(channels, data);
                }
                else if (eventType == ix::CobraConnection_EventType_Subscribed)
                {
                    spdlog::info("Publisher: subscribed to channel {}", subscriptionId);
                }
                else if (eventType == ix::CobraConnection_EventType_UnSubscribed)
                {
                    spdlog::info("Publisher: unsubscribed from channel {}", subscriptionId);
                }
                else if (eventType == ix::CobraConnection_EventType_Error)
                {
                    spdlog::error("Publisher: error {}", errMsg);
                    condition.notify_one();
                }
                else if (eventType == ix::CobraConnection_EventType_Published)
                {
                    spdlog::info("Published message acked: {}", msgId);
                    messageAcked = true;
                    condition.notify_one();
                }
            }
        );

        while (!authenticated) ;
        while (!messageAcked) ;

        return 0;
    }
}