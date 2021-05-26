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

#include <WebsocketsSystemComponent.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>

namespace Websockets
{
    void WebsocketsSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        WebsocketClient::Reflect(context);
        SecureWebsocketClient::Reflect(context);

        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<WebsocketsSystemComponent, AZ::Component>()
                ->Version(1)
                ;

            if (AZ::EditContext* editContext = serialize->GetEditContext())
            {
                editContext->Class<WebsocketsSystemComponent>("Websockets", "Websocket Manager for all active websockets")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }
    }

    void WebsocketsSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("WebsocketsService"));
    }

    void WebsocketsSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("WebsocketsService"));
    }

    void WebsocketsSystemComponent::Activate()
    {
        WebsocketsRequestBus::Handler::BusConnect();
    }

    void WebsocketsSystemComponent::Deactivate()
    {
        WebsocketsRequestBus::Handler::BusDisconnect();
    }

    AZStd::unique_ptr<IWebsocketClient> WebsocketsSystemComponent::CreateClientWithAuth(const AZStd::string& websocket, const OnMessage& messageFunc, const AZStd::string& authorization)
    {
        AZStd::unique_ptr<SecureWebsocketClient> socket = AZStd::make_unique<SecureWebsocketClient>();

        if (!socket)
        {
            AZ_Error("Websocket Gem", socket, "Socket not created!");
            return nullptr;
        }

        if (!socket->ConnectWebsocket(websocket, messageFunc, authorization))
        {
            AZ_Error("Websocket Gem", false, "Socket connection unsuccessful");
            return nullptr;
        }

        return socket;
    }
    
    AZStd::unique_ptr<IWebsocketClient> WebsocketsSystemComponent::CreateClient(const AZStd::string& websocket, const OnMessage& messageFunc, WebsocketType type)
    {
        switch (type)
        {
            case WebsocketType::Secure:
            {
                AZStd::unique_ptr<SecureWebsocketClient> socket = AZStd::make_unique<SecureWebsocketClient>();

                if (!socket)
                {
                    AZ_Error("Websocket Gem", socket, "Socket not created!");
                    return nullptr;
                }

                if (!socket->ConnectWebsocket(websocket, messageFunc))
                {
                    AZ_Error("Websocket Gem", false, "Socket connection unsuccessful");
                    return nullptr;
                }

                return socket;
            }
            break;

            case WebsocketType::InSecure:
            {
                AZStd::unique_ptr<WebsocketClient> socket = AZStd::make_unique<WebsocketClient>();

                if (!socket)
                {
                    AZ_Error("Websocket Gem", socket, "Socket not created!");
                    return nullptr;
                }

                if (!socket->ConnectWebsocket(websocket, messageFunc))
                {
                    AZ_Error("Websocket Gem", false, "Socket connection unsuccessful");
                    return nullptr;
                }

                return socket;
            }
            break;

            default:
            break;
        }

        return nullptr;
    }
} // namespace Websockets
