/*
 * All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
 * its licensors.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this
 * distribution (the "License"). All use of this software is governed by the License,
 * or, if provided, by the license below or the license accompanying this file. Do not
 * remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 */

#pragma once
#include <Websockets/WebsocketsBus.h>
#include <Platform/Common/Default/WebsocketsLibraryIncludes.h>

#include <AzCore/std/containers/queue.h>

namespace Websockets
{
    typedef websocketpp::client<websocketpp::config::asio_tls_client> SecureWebsocketClientType;

    class SecureWebsocketClient_Default
    {
    public:

        void Destroy();

        bool ConnectWebsocket(const AZStd::string& websocket, const OnMessage& messageFunc, const AZStd::string& authorization);

        bool IsConnectionOpen() const;
        void SendWebsocketMessage(const AZStd::string& msgBody);
        void SendWebsocketMessageBinary(const void* payload, const size_t len);
        void CloseWebsocket();


    protected:
        //websocket members
        OnMessage m_messageFunction;

        SecureWebsocketClientType m_client;
        SecureWebsocketClientType::connection_ptr m_connection;  //the actual websocket URI and its settings - connection_ptr is a ptr to the templated connection type
        AZStd::unique_ptr<AZStd::thread> m_thread;  //each client needs a thread so it can receive messages without blocking
    };

    using SecureWebsocketClient_Platform = SecureWebsocketClient_Default;
}

