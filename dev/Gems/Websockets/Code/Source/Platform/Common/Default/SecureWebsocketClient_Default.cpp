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

#include "SecureWebsocketClient_Default.h"

namespace Websockets
{
    void SecureWebsocketClient_Default::Destroy()
    {
        if (m_connection && m_connection->get_state() != websocketpp::session::state::closed)
        {
            m_client.stop();  //hard stop client because it hasn't been properly closed previously
        }
        if (m_thread && m_thread->joinable())
        {
            m_thread->join();
            m_thread.reset();
        }
    }

    bool SecureWebsocketClient_Default::ConnectWebsocket(const AZStd::string & websocket, const OnMessage & messageFunc, const AZStd::string& authorization)
    {
        m_messageFunction = messageFunc;

        //websocket setup
        m_client.set_access_channels(websocketpp::log::alevel::all);
        m_client.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize ASIO
        m_client.init_asio();

        //use lambda to set handlers
        //TLS handler (Transport Layer Security, also often referred to as SSL secure socket handler)
        m_client.set_tls_init_handler([](websocketpp::connection_hdl hdl) -> websocketpp::lib::shared_ptr<asio::ssl::context>
        {
            websocketpp::lib::shared_ptr<asio::ssl::context> contextPtr(new asio::ssl::context(asio::ssl::context::sslv23));

            websocketpp::lib::error_code errorCode;
            contextPtr->set_options(asio::ssl::context::default_workarounds
                | asio::ssl::context::no_sslv2
                | asio::ssl::context::single_dh_use
                , errorCode);

            AZ_Error("Websocket Gem", !(errorCode.value()), "Secure websocket setup options failed", errorCode.message().c_str());

            return contextPtr;
        }
        );

        //message handler
        m_client.set_message_handler([this](websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr pMessage)
        {
            const std::string& messageString = pMessage->get_payload();
            m_messageFunction(AZStd::string_view(messageString.c_str(), messageString.size()));
        }
        );

        websocketpp::lib::error_code errorCode;
        m_connection = m_client.get_connection(std::string(websocket.c_str(), websocket.size()), errorCode);

        AZ_Error("Websocket Gem", !(errorCode.value()), "Could not create connection because: %s", errorCode.message().c_str());
        AZ_Error("Websocket Gem", m_connection, "Connection is null");

        if (!m_connection)
        {
            return false;
        }

        if (!authorization.empty())
        {
            m_connection->append_header("Authorization", authorization.c_str());
        }

        // Connect only requests a connection. No network messages are
        // exchanged until run().
        m_client.connect(m_connection);

        //run() is blocking, so we start it up on another thread  
        m_thread = AZStd::make_unique<AZStd::thread>([this]() { m_client.run(); });

        return true;
    }

    bool SecureWebsocketClient_Default::IsConnectionOpen() const
    {
        return m_connection->get_state() == websocketpp::session::state::open;
    }

    void SecureWebsocketClient_Default::SendWebsocketMessage(const AZStd::string & msgBody)
    {
        if (!m_connection)
        {
            AZ_Warning("Websocket Gem", m_connection, "Cannot send message because we are not connected to a websocket.");
            return;
        }

        websocketpp::lib::error_code errorCode;

        AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "Send Websocket Message");

        m_client.send(m_connection->get_handle(), msgBody.c_str(), websocketpp::frame::opcode::text, errorCode);

        AZ_Warning("Websocket Gem", !(errorCode.value()), "Message failed to send because: %s", errorCode.message().c_str());
    }

    void SecureWebsocketClient_Default::SendWebsocketMessageBinary(const void* payload, const size_t len)
    {
        if (!m_connection)
        {
            AZ_Warning("Websocket Gem", m_connection, "Cannot send binary message because we are not connected to a websocket.");
            return;
        }

        websocketpp::lib::error_code errorCode;

        AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "Send Websocket Message");

        m_client.send(m_connection->get_handle(), payload, len, websocketpp::frame::opcode::binary, errorCode);

        AZ_Warning("Websocket Gem", !(errorCode.value()), "Binary message failed to send because: %s", errorCode.message().c_str());
    }

    void SecureWebsocketClient_Default::CloseWebsocket()
    {
        websocketpp::lib::error_code ec;
        m_client.close(m_connection->get_handle(), websocketpp::close::status::going_away, "Websocket client closing", ec);
    }
}

