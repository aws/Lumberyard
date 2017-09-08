/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#ifndef LOGGINGCOMPONENT_H
#define LOGGINGCOMPONENT_H

#include <AzCore/Component/Component.h>
#include <AzCore/Debug/TraceMessageBus.h>

#include "LogFile.h"

namespace AzFramework
{
    //! LogComponent
    //! LogComponent listens to AZ trace messages and forwards them to a log file
    class LogComponent
        : public AZ::Component
        , private AZ::Debug::TraceMessageBus::Handler
    {
    public:
        AZ_COMPONENT(LogComponent, "{04AEB2E7-7F51-4426-9423-29D66C8DE1C1}")
        static void Reflect(AZ::ReflectContext* context);

        LogComponent();
        virtual ~LogComponent();

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        virtual void Init();
        virtual void Activate();
        virtual void Deactivate();
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AZ::Debug::TraceMessagesBus
        virtual AZ::Debug::Result OnPrintf(const AZ::Debug::TraceMessageParameters& parameters) override;
        virtual AZ::Debug::Result OnAssert(const AZ::Debug::TraceMessageParameters& parameters) override;
        virtual AZ::Debug::Result OnException(const AZ::Debug::TraceMessageParameters& parameters) override;
        virtual AZ::Debug::Result OnError(const AZ::Debug::TraceMessageParameters& parameters) override;
        virtual AZ::Debug::Result OnWarning(const AZ::Debug::TraceMessageParameters& parameters) override;
        //////////////////////////////////////////////////////////////////////////

        virtual void OutputMessage(LogFile::SeverityLevel severity, const char* window, const char* message);

        void SetLogFileBaseName(const char* baseName);
        void SetRollOverLength(AZ::u64 rolloverLength);
        void SetMachineReadable(bool newValue);

        const char* GetLogFileBaseName();
        AZ::u64 GetRollOverLength();

    private:
        /// \ref AZ::ComponentDescriptor::GetProvidedServices
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        /// \ref AZ::ComponentDescriptor::GetIncompatibleServices
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void ActivateLogFile();
        void DeactivateLogFile();

        bool m_machineReadable = true;
        AZStd::string m_logFileBaseName;
        AZ::u64 m_rolloverLength;
        LogFile* m_logFile;
    };
}

#endif
