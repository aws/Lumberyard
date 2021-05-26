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
#include <AzCore/Component/Component.h>

#include <SecureWebsocketClient_Platform.h>

namespace Websockets
{
    class SecureWebsocketClient
        : public IWebsocketClient
        , public SecureWebsocketClient_Platform
    {

    private:
        using Platform = SecureWebsocketClient_Platform;

    public:
        AZ_CLASS_ALLOCATOR(SecureWebsocketClient, AZ::SystemAllocator, 0);
        AZ_RTTI(SecureWebsocketClient, "{EE02CE61-52C5-4D96-BF45-66190959BE21}");

        SecureWebsocketClient() { };
        SecureWebsocketClient(const SecureWebsocketClient& socket) { };  //avoids copying because websocketpp deletes these functions and values will be constructed anyway

        ~SecureWebsocketClient() override;

        SecureWebsocketClient& operator=(const SecureWebsocketClient& socket) { return *this; };  //again, operator= is deleted for clients, etc. in websocketpp

        static void Reflect(AZ::ReflectContext* context);

        bool ConnectWebsocket(const AZStd::string& websocket, const OnMessage& messageFunc, const AZStd::string& authorization = "");

        //interface functions
        bool IsConnectionOpen() const override;
        void SendWebsocketMessage(const AZStd::string& msgBody) override;
        void SendWebsocketMessageBinary(const void* payload, const size_t len) override;
        void CloseWebsocket() override;
    };
}
