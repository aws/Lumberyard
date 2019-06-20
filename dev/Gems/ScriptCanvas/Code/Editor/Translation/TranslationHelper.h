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

#include <QCoreApplication>

#include <AzCore/std/string/conversions.h>

#include <ScriptCanvas/Core/Slot.h>
#include <GraphCanvas/Types/TranslationTypes.h>

namespace ScriptCanvasEditor
{
    enum class TranslationContextGroup : AZ::u32
    {
        EbusSender,
        EbusHandler,
        ClassMethod,
        Invalid
    };

    enum class TranslationItemType : AZ::u32
    {
        Node,
        Wrapper,
        ExecutionInSlot,
        ExecutionOutSlot,
        ParamDataSlot,
        ReturnDataSlot,
        BusIdSlot,
        Invalid
    };

    enum class TranslationKeyId : AZ::u32
    {
        Name,
        Tooltip,
        Category,
        Invalid
    };

    namespace TranslationContextGroupParts
    {
        const char* const ebusSender  = "EBus";
        const char* const ebusHandler = "Handler";
        const char* const classMethod = "Method";
    };

    namespace TranslationKeyParts
    {
        const char* const handler   = "HANDLER_";
        const char* const name      = "NAME";
        const char* const tooltip   = "TOOLTIP";
        const char* const category  = "CATEGORY";
        const char* const in        = "IN";
        const char* const out       = "OUT";
        const char* const param     = "PARAM";
        const char* const output    = "OUTPUT";
        const char* const busid     = "BUSID";
    }

    // The context name and keys generated by TranslationHelper should match the keys
    // being exported by the TSGenerateAction.cpp in the ScriptCanvasDeveloper Gem.
    class TranslationHelper
    {
    public:
        static AZStd::string GetContextName(TranslationContextGroup group, const AZStd::string& keyBase)
        {
            if (group == TranslationContextGroup::Invalid || keyBase.empty())
            {
                // Missing information
                return AZStd::string();
            }

            const char* groupPart;

            switch (group)
            {
            case TranslationContextGroup::EbusSender:
                groupPart = TranslationContextGroupParts::ebusSender;
                break;
            case TranslationContextGroup::EbusHandler:
                groupPart = TranslationContextGroupParts::ebusHandler;
                break;
            case TranslationContextGroup::ClassMethod:
                groupPart = TranslationContextGroupParts::classMethod;
                break;
            default:
                AZ_Warning("TranslationComponent", false, "Invalid translation group ID.");
                groupPart = "";
            }

            AZStd::string fullKey = AZStd::string::format("%s: %s",
                groupPart,
                keyBase.c_str()
            );

            return fullKey;
        }
        
        // UserDefined
        static AZStd::string GetUserDefinedContext(const AZStd::string& contextName)
        {
            return GetContextName(TranslationContextGroup::ClassMethod, contextName);
        }

        static AZStd::string GetUserDefinedKey(const AZStd::string& contextName, TranslationKeyId keyId)
        {
            return GetClassKey(TranslationContextGroup::ClassMethod, contextName, keyId);
        }

        static AZStd::string GetUserDefinedNodeKey(const AZStd::string& contextName, const AZStd::string& nodeName, TranslationKeyId keyId)
        {
            return GetKey(TranslationContextGroup::ClassMethod, contextName, nodeName, TranslationItemType::Node, keyId);
        }

        static AZStd::string GetUserDefinedNodeSlotKey(const AZStd::string& contextName, const AZStd::string& nodeName, TranslationItemType itemType, TranslationKeyId keyId, int slotIndex)
        {
            return GetKey(TranslationContextGroup::ClassMethod, contextName, nodeName, itemType, keyId, slotIndex);
        }
        ////

        // EBusEvent
        static AZStd::string GetEbusHandlerContext(const AZStd::string& busName)
        {
            return GetContextName(TranslationContextGroup::EbusHandler, busName);
        }

        static AZStd::string GetEbusHandlerKey(const AZStd::string& busName, TranslationKeyId keyId)
        {
            return GetClassKey(TranslationContextGroup::EbusHandler, busName, keyId);
        }

        static AZStd::string GetEbusHandlerEventKey(const AZStd::string& busName, const AZStd::string& eventName, TranslationKeyId keyId)
        {
            return GetKey(TranslationContextGroup::EbusHandler, busName, eventName, TranslationItemType::Node, keyId);
        }

        static AZStd::string GetEBusHandlerSlotKey(const AZStd::string& busName, const AZStd::string& eventName, TranslationItemType type, TranslationKeyId keyId, int paramIndex)
        {
            return GetKey(TranslationContextGroup::EbusHandler, busName, eventName, type, keyId, paramIndex);
        }
        ////

        static AZStd::string GetKey(TranslationContextGroup group, const AZStd::string& keyBase, const AZStd::string& keyName, TranslationItemType type, TranslationKeyId keyId, int paramIndex = 0)
        {
            if (group == TranslationContextGroup::Invalid || keyBase.empty()
                || type == TranslationItemType::Invalid || keyId == TranslationKeyId::Invalid)
            {
                // Missing information
                return AZStd::string();
            }

            if (type != TranslationItemType::Wrapper && keyName.empty())
            {
                // Missing information
                return AZStd::string();
            }

            AZStd::string fullKey;

            const char* prefix = "";
            if (group == TranslationContextGroup::EbusHandler)
            {
                prefix = TranslationKeyParts::handler;
            }

            const char* keyPart = GetKeyPart(keyId);

            switch (type)
            {
            case TranslationItemType::Node:
                fullKey = AZStd::string::format("%s%s_%s_%s",
                    prefix,
                    keyBase.c_str(),
                    keyName.c_str(),
                    keyPart
                );
                break;
            case TranslationItemType::Wrapper:
                fullKey = GetClassKey(group, keyBase, keyId);
                break;
            case TranslationItemType::ExecutionInSlot:
                fullKey = AZStd::string::format("%s%s_%s_%s_%s",
                    prefix,
                    keyBase.c_str(),
                    keyName.c_str(),
                    TranslationKeyParts::in,
                    keyPart
                );
                break;
            case TranslationItemType::ExecutionOutSlot:
                fullKey = AZStd::string::format("%s%s_%s_%s_%s",
                    prefix,
                    keyBase.c_str(),
                    keyName.c_str(),
                    TranslationKeyParts::out,
                    keyPart
                );
                break;
            case TranslationItemType::ParamDataSlot:
                fullKey = AZStd::string::format("%s%s_%s_%s%d_%s",
                    prefix,
                    keyBase.c_str(),
                    keyName.c_str(),
                    TranslationKeyParts::param,
                    paramIndex,
                    keyPart
                );
                break;
            case TranslationItemType::ReturnDataSlot:
                fullKey = AZStd::string::format("%s%s_%s_%s%d_%s",
                    prefix,
                    keyBase.c_str(),
                    keyName.c_str(),
                    TranslationKeyParts::output,
                    paramIndex,
                    keyPart
                );
                break;
            case TranslationItemType::BusIdSlot:
                fullKey = AZStd::string::format("%s%s_%s_%s_%s",
                    prefix,
                    keyBase.c_str(),
                    keyName.c_str(),
                    TranslationKeyParts::busid,
                    keyPart
                );
                break;
            default:
                AZ_Warning("TranslationComponent", false, "Invalid translation item type.");
            }

            AZStd::to_upper(fullKey.begin(), fullKey.end());

            return fullKey;
        }

        static AZStd::string GetClassKey(TranslationContextGroup group, const AZStd::string& keyBase, TranslationKeyId keyId)
        {
            const char* prefix = "";
            if (group == TranslationContextGroup::EbusHandler)
            {
                prefix = TranslationKeyParts::handler;
            }

            const char* keyPart = GetKeyPart(keyId);

            AZStd::string fullKey = AZStd::string::format("%s%s_%s",
                prefix,
                keyBase.c_str(),
                keyPart
            );

            AZStd::to_upper(fullKey.begin(), fullKey.end());

            return fullKey;
        }

        static const char* GetKeyPart(TranslationKeyId keyId)
        {
            const char* keyPart = "";

            switch (keyId)
            {
            case TranslationKeyId::Name:
                keyPart = TranslationKeyParts::name;
                break;
            case TranslationKeyId::Tooltip:
                keyPart = TranslationKeyParts::tooltip;
                break;
            case TranslationKeyId::Category:
                keyPart = TranslationKeyParts::category;
                break;


            default:
                AZ_Warning("TranslationComponent", false, "Invalid translation key ID.");
            }

            return keyPart;
        }

        static TranslationItemType GetItemType(ScriptCanvas::SlotType slotType)
        {
            switch (slotType)
            {
            case ScriptCanvas::SlotType::ExecutionIn:
                return TranslationItemType::ExecutionInSlot;
            case ScriptCanvas::SlotType::ExecutionOut:
                return TranslationItemType::ExecutionOutSlot;
            case ScriptCanvas::SlotType::DataIn:
                return TranslationItemType::ParamDataSlot;
            case ScriptCanvas::SlotType::DataOut:
                return TranslationItemType::ReturnDataSlot;
            default:
                return TranslationItemType::Invalid;
            }
        }

        static AZStd::string GetSafeTypeName(ScriptCanvas::Data::Type dataType)
        {
            if (!dataType.IsValid())
            {
                return "";
            }

            return ScriptCanvas::Data::GetName(dataType);
        }

        static AZStd::string GetKeyTranslation(TranslationContextGroup group, const AZStd::string& keyBase, const AZStd::string& keyName, TranslationItemType type, TranslationKeyId keyId, int paramIndex = 0)
        {
            AZStd::string translationContext = TranslationHelper::GetContextName(group, keyBase);
            AZStd::string translationKey = TranslationHelper::GetKey(group, keyBase, keyName, type, keyId, paramIndex);
            AZStd::string translated = QCoreApplication::translate(translationContext.c_str(), translationKey.c_str()).toUtf8().data();

            if (translated == translationKey)
            {
                return AZStd::string();
            }

            return translated;
        }

        static AZStd::string GetClassKeyTranslation(TranslationContextGroup group, const AZStd::string& keyBase, TranslationKeyId keyId)
        {
            AZStd::string translationContext = TranslationHelper::GetContextName(group, keyBase);
            AZStd::string translationKey = TranslationHelper::GetClassKey(group, keyBase, keyId);
            AZStd::string translated = QCoreApplication::translate(translationContext.c_str(), translationKey.c_str()).toUtf8().data();

            if (translated == translationKey)
            {
                return AZStd::string();
            }

            return translated;
        }

        static GraphCanvas::TranslationKeyedString GetEBusHandlerBusIdNameKey()
        {
            GraphCanvas::TranslationKeyedString keyedString;
            keyedString.m_context = "Globals";
            keyedString.m_key = "DEFAULTS_EBUSHANDLER_BUSID_NAME";
            keyedString.SetFallback("BusId");

            return keyedString;
        }

        static GraphCanvas::TranslationKeyedString GetEBusHandlerBusIdTooltipKey()
        {
            GraphCanvas::TranslationKeyedString keyedString;
            keyedString.m_context = "Globals";
            keyedString.m_key = "DEFAULTS_EBUSHANDLER_BUSID_TOOLTIP";
            keyedString.SetFallback("BusId");

            return keyedString;
        }

        static GraphCanvas::TranslationKeyedString GetEBusHandlerOnEventTriggeredNameKey()
        {
            GraphCanvas::TranslationKeyedString keyedString;
            keyedString.m_context = "Globals";
            keyedString.m_key = "DEFAULTS_EBUSHANDLER_ONTRIGGERED_NAME";
            keyedString.SetFallback("Out");

            return keyedString;
        }

        static GraphCanvas::TranslationKeyedString GetEBusHandlerOnEventTriggeredTooltipKey()
        {
            GraphCanvas::TranslationKeyedString keyedString;
            keyedString.m_context = "Globals";
            keyedString.m_key = "DEFAULTS_EBUSHANDLER_ONTRIGGERED_TOOLTIP";
            keyedString.SetFallback("Out");

            return keyedString;
        }

        static GraphCanvas::TranslationKeyedString GetEBusSenderBusIdNameKey()
        {
            GraphCanvas::TranslationKeyedString keyedString;
            keyedString.m_context = "Globals";
            keyedString.m_key = "DEFAULTS_EBUSSENDER_BUSID_NAME";
            keyedString.SetFallback("BusId");

            return keyedString;
        }

        static GraphCanvas::TranslationKeyedString GetEBusSenderBusIdTooltipKey()
        {
            GraphCanvas::TranslationKeyedString keyedString;
            keyedString.m_context = "Globals";
            keyedString.m_key = "DEFAULTS_EBUSSENDER_BUSID_TOOLTIP";
            keyedString.SetFallback("BusId");

            return keyedString;
        }
    };
}