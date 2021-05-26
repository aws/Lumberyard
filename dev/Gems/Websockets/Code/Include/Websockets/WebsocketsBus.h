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

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/string/string.h>

namespace Websockets
{
    //!OnMessage function definition for customers to provide.
    using OnMessage = AZStd::function<void(AZStd::string_view)>; 

    //!Interface to a websocket client.
    class IWebsocketClient
    {
    public:
        virtual ~IWebsocketClient() {};

        //!Used to verify connection status.
        virtual bool IsConnectionOpen() const = 0;

        //!Send websocket string messages.
        virtual void SendWebsocketMessage(const AZStd::string& msgBody) = 0;
        //!For systems that require binary messages, or want to deliver other forms of data more efficiently.
        virtual void SendWebsocketMessageBinary(const void* payload, const size_t len) = 0;

        //!Close your websocket when it is no longer needed, before it is destroyed.
        //!Preferably several frames before destruction, if possible
        //!allows everything to exit cleanly without issue.
        virtual void CloseWebsocket() = 0;
    };  

    //!Interface to request the system to provide a websocket.
    class IWebsocketRequests
    {
    public:
        enum WebsocketType
        {
            Secure,
            InSecure
        };

        //!Create websocket connections.
        //!When creating a websocket client, the default should be to create a Secure connection.  Insecure should generally be
        //!avoided in production.
        virtual AZStd::unique_ptr<IWebsocketClient> CreateClient(const AZStd::string& websocket,
            const OnMessage& messageFunc, WebsocketType type) = 0;

        virtual AZStd::unique_ptr<IWebsocketClient> CreateClientWithAuth(const AZStd::string& websocket,
            const OnMessage& messageFunc, const AZStd::string& authorization) = 0;
    };


    //!Requests a websocket via bus
    class WebsocketsRequests
        : public IWebsocketRequests
        , public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using WebsocketsRequestBus = AZ::EBus<WebsocketsRequests>;

} // namespace Websockets
