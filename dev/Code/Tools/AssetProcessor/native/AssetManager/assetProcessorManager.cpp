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
#include <QStringList>
#include <QFile>
#include <QRunnable>
#include <QCoreApplication>
#include <QFileInfo>
#include <QPair>
#include <QDateTime>
#include <QMutexLocker>
#include <QDateTime>
#include <QElapsedTimer>

#include <AzCore/Serialization/Utils.h>
#include <AzCore/Casting/lossy_cast.h>
#include <AzCore/Casting/numeric_cast.h>

#include <AzCore/std/algorithm.h>

#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzFramework/Asset/AssetRegistry.h>

#include <AzToolsFramework/Debug/TraceContext.h>

#include <AssetBuilderSDK/AssetBuilderBusses.h>

#include "native/AssetManager/assetProcessorManager.h"
#include "native/utilities/PlatformConfiguration.h"
#include "native/utilities/assetUtils.h"
#include "native/utilities/AssetUtilEBusHelper.h"
#include "native/AssetDatabase/AssetDatabase.h"
#include "native/AssetManager/assetScanFolderInfo.h"
#include <AzCore/std/utils.h>
#include "native/utilities/ByteArrayStream.h"
#include "native/resourcecompiler/RCBuilder.h"
#include <AzToolsFramework/Asset/AssetProcessorMessages.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzCore/std/sort.h>
#include <AzToolsFramework/API/AssetDatabaseBus.h>

#include <AzCore/IO/FileIO.h>

namespace AssetProcessor
{
    const AZ::u32 FAILED_FINGERPRINT = 1;
    const int MILLISECONDS_BETWEEN_CREATE_JOBS_STATUS_UPDATE = 1000;
    const int MILLISECONDS_BETWEEN_PROCESS_JOBS_STATUS_UPDATE = 100;

    using namespace AzToolsFramework::AssetSystem;
    using namespace AzFramework::AssetSystem;

    AssetProcessorManager::AssetProcessorManager(AssetProcessor::PlatformConfiguration* config, QObject* parent)
        : QObject(parent)
        , m_platformConfig(config)
    {
        m_stateData = AZStd::shared_ptr<AssetDatabaseConnection>(aznew AssetDatabaseConnection());
        // note that this is not the first time we're opening the database - the main thread also opens it before this happens,
        // which allows it to upgrade it and check it for errors.  If we get here, it means the database is already good to go.

        // note that this is not the first time we're opening the database - the main thread also opens it before this happens,
        // which allows it to upgrade it and check it for errors.  If we get here, it means the database is already good to go.
        m_stateData->OpenDatabase(); 

        MigrateScanFolders();

        m_highestJobRunKeySoFar = m_stateData->GetHighestJobRunKey() + 1;


        // cache this up front.  Note that it can fail here, and will retry later.
        InitializeCacheRoot();

        m_absoluteDevFolderPath[0] = 0;
        m_absoluteDevGameFolderPath[0] = 0;

        QDir assetRoot;
        if (AssetUtilities::ComputeAssetRoot(assetRoot))
        {
            azstrcpy(m_absoluteDevFolderPath, AZ_MAX_PATH_LEN, assetRoot.absolutePath().toUtf8().constData());
            QString absoluteDevGameFolderPath = assetRoot.absoluteFilePath(AssetUtilities::ComputeGameName());
            azstrcpy(m_absoluteDevGameFolderPath, AZ_MAX_PATH_LEN, absoluteDevGameFolderPath.toUtf8().constData());
        }

        AssetProcessor::ProcessingJobInfoBus::Handler::BusConnect();
    }

    AssetProcessorManager::~AssetProcessorManager()
    {
        AssetProcessor::ProcessingJobInfoBus::Handler::BusDisconnect();
    }

    template <class R>
    inline bool AssetProcessorManager::Recv(unsigned int connId, QByteArray payload, R& request)
    {
        bool readFromStream = AZ::Utils::LoadObjectFromBufferInPlace(payload.data(), payload.size(), request);
        AZ_Assert(readFromStream, "AssetProcessorManager::Recv: Could not deserialize from stream (type=%u)", request.GetMessageType());
        return readFromStream;
    }

    bool AssetProcessorManager::InitializeCacheRoot()
    {
        if (AssetUtilities::ComputeProjectCacheRoot(m_cacheRootDir))
        {
            m_normalizedCacheRootPath = AssetUtilities::NormalizeDirectoryPath(m_cacheRootDir.absolutePath());
            return !m_normalizedCacheRootPath.isEmpty();
        }

        return false;
    }

    void AssetProcessorManager::OnAssetScannerStatusChange(AssetProcessor::AssetScanningStatus status)
    {
        if (status == AssetProcessor::AssetScanningStatus::Started)
        {
            // Ensure that the source file list is populated before a scan begins
            m_sourceFilesInDatabase.clear();
            auto sourcesFunction = [this](AzToolsFramework::AssetDatabase::SourceDatabaseEntry& entry)
                {
                    AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry scanFolder;
                    if (m_stateData->GetScanFolderByScanFolderID(entry.m_scanFolderPK, scanFolder))
                    {
                        QString databaseSourceName = QString::fromUtf8(entry.m_sourceName.c_str());
                        QString scanFolderPath = QString::fromUtf8(scanFolder.m_scanFolder.c_str());
                        QString relativeToScanFolderPath = databaseSourceName;
                        if (!scanFolder.m_outputPrefix.empty())
                        {
                            relativeToScanFolderPath = relativeToScanFolderPath.remove(0, static_cast<int>(scanFolder.m_outputPrefix.size()) + 1);
                        }

                        QString finalAbsolute = (QString("%1/%2").arg(scanFolderPath).arg(relativeToScanFolderPath));
                        m_sourceFilesInDatabase[finalAbsolute] = { scanFolderPath, relativeToScanFolderPath, databaseSourceName };
                    }

                    return true;
                };
            m_stateData->QuerySourcesTable(sourcesFunction);

            m_isCurrentlyScanning = true;
        }
        else if ((status == AssetProcessor::AssetScanningStatus::Completed) ||
                 (status == AssetProcessor::AssetScanningStatus::Stopped))
        {
            m_isCurrentlyScanning = false;
            // we cannot invoke this immediately - the scanner might be done, but we aren't actually ready until we've processed all remaining messages:
            QMetaObject::invokeMethod(this, "CheckMissingFiles", Qt::QueuedConnection);
        }
    }


    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // JOB STATUS REQUEST HANDLING
    void AssetProcessorManager::OnJobStatusChanged(JobEntry jobEntry, JobStatus status)
    {
        //this function just adds an removes to a maps to speed up job status, we don't actually write
        //to the database until it either succeeds or fails
        AZ::Uuid sourceUUID = AssetUtilities::CreateSafeSourceUUIDFromName(jobEntry.m_databaseSourceName.toUtf8().data());
        AZ::Uuid legacySourceUUID = AssetUtilities::CreateSafeSourceUUIDFromName(jobEntry.m_databaseSourceName.toUtf8().data(), false); // legacy source uuid

        if (status == JobStatus::Queued)
        {
            // freshly queued files start out queued.
            JobInfo& jobInfo = m_jobRunKeyToJobInfoMap.insert_key(jobEntry.m_jobRunKey).first->second;
            jobInfo.m_platform = jobEntry.m_platformInfo.m_identifier;
            jobInfo.m_builderGuid = jobEntry.m_builderGuid;
            jobInfo.m_sourceFile = jobEntry.m_pathRelativeToWatchFolder.toUtf8().constData();
            jobInfo.m_watchFolder = jobEntry.m_watchFolderPath.toUtf8().constData();
            jobInfo.m_jobKey = jobEntry.m_jobKey.toUtf8().constData();
            jobInfo.m_jobRunKey = jobEntry.m_jobRunKey;
            jobInfo.m_status = status;

            m_jobKeyToJobRunKeyMap.insert(AZStd::make_pair(jobEntry.m_jobKey.toUtf8().data(), jobEntry.m_jobRunKey));
            SourceInfo sourceInfo = { jobEntry.m_watchFolderPath, jobEntry.m_databaseSourceName, jobEntry.m_pathRelativeToWatchFolder };
            Q_EMIT SourceQueued(sourceUUID, legacySourceUUID, sourceInfo.m_watchFolder, jobEntry.m_pathRelativeToWatchFolder);
        }
        else
        {
            if (status == JobStatus::InProgress)
            {
                //update to in progress status
                m_jobRunKeyToJobInfoMap[jobEntry.m_jobRunKey].m_status = JobStatus::InProgress;
            }
            else //if failed or succeeded remove from the map
            {
                m_jobRunKeyToJobInfoMap.erase(jobEntry.m_jobRunKey);
                Q_EMIT SourceFinished(sourceUUID, legacySourceUUID);

                auto found = m_jobKeyToJobRunKeyMap.equal_range(jobEntry.m_jobKey.toUtf8().data());

                for (auto iter = found.first; iter != found.second; ++iter)
                {
                    if (iter->second == jobEntry.m_jobRunKey)
                    {
                        m_jobKeyToJobRunKeyMap.erase(iter);
                        break;
                    }
                }
            }
        }
    }


    //! A network request came in, Given a Job Run Key (from the above Job Request), asking for the actual log for that job.
    void AssetProcessorManager::ProcessGetAssetJobLogRequest(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed)
    {
        AssetJobLogRequest* request = azrtti_cast<AssetJobLogRequest*>(message);
        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetAssetJobLogRequest: Message is not of type %d.Incoming message type is %d.\n", AssetJobLogRequest::MessageType(), message->GetMessageType());
            return;
        }

        AssetJobLogResponse response;
        ProcessGetAssetJobLogRequest(*request, response);
        EBUS_EVENT_ID(requestId.first, AssetProcessor::ConnectionBus, SendResponse, requestId.second, response);
    }

    void AssetProcessorManager::ProcessGetAssetJobLogRequest(const AssetJobLogRequest& request, AssetJobLogResponse& response)
    {
        JobInfo jobInfo;

        bool hasSpace = false;
        AssetProcessor::DiskSpaceInfoBus::BroadcastResult(hasSpace, &AssetProcessor::DiskSpaceInfoBusTraits::CheckSufficientDiskSpace, m_cacheRootDir.absolutePath().toUtf8().data(), 0, false);

        if (!hasSpace)
        {
            AZ_TracePrintf("AssetProcessorManager", "Warn: AssetProcessorManager: Low disk space detected\n");
            response.m_jobLog = "Warn: Low disk space detected.  Log file may be missing or truncated.  Asset processing is likely to fail.\n";
        }

        //look for the job in flight first
        bool found = false;
        auto foundElement = m_jobRunKeyToJobInfoMap.find(request.m_jobRunKey);
        if (foundElement != m_jobRunKeyToJobInfoMap.end())
        {
            found = true;
            jobInfo = foundElement->second;
        }
        else
        {
            // get the job infos by that job run key.
            JobInfoContainer jobInfos;
            if (!m_stateData->GetJobInfoByJobRunKey(request.m_jobRunKey, jobInfos))
            {
                AZ_TracePrintf("AssetProcessorManager", "Error: AssetProcessorManager: Failed to find the job for a request.\n");
                response.m_jobLog.append("Error: AssetProcessorManager: Failed to find the job for a request.");
                response.m_isSuccess = false;

                return;
            }

            AZ_Assert(jobInfos.size() == 1, "Should only have found one jobInfo!!!");
            jobInfo = AZStd::move(jobInfos[0]);
            found = true;
        }

        if (jobInfo.m_status == JobStatus::Failed_InvalidSourceNameExceedsMaxLimit)
        {
            response.m_jobLog.append(AZStd::string::format("Warn: Source file name exceeds the maximum length allowed (%d).", AP_MAX_PATH_LEN).c_str());
            response.m_isSuccess = true;
            return;
        }

        AssetUtilities::ReadJobLog(jobInfo, response);
    }

    //! A network request came in, Given a Job Run Key (from the above Job Request), asking for the actual log for that job.
    void AssetProcessorManager::ProcessGetAbsoluteAssetDatabaseLocationRequest(NetworkRequestID requestId, BaseAssetProcessorMessage* message)
    {
        GetAbsoluteAssetDatabaseLocationRequest* request = azrtti_cast<GetAbsoluteAssetDatabaseLocationRequest*>(message);
        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetAbsoluteAssetDatabaseLocationRequest: Message is not of type %d.Incoming message type is %d.\n", GetAbsoluteAssetDatabaseLocationRequest::MessageType(), message->GetMessageType());
            return;
        }

        GetAbsoluteAssetDatabaseLocationResponse response;

        AzToolsFramework::AssetDatabase::AssetDatabaseRequestsBus::Broadcast(&AzToolsFramework::AssetDatabase::AssetDatabaseRequests::GetAssetDatabaseLocation, response.m_absoluteAssetDatabaseLocation);

        if (response.m_absoluteAssetDatabaseLocation.size() > 0)
        {
            response.m_isSuccess = true;
        }

        EBUS_EVENT_ID(requestId.first, AssetProcessor::ConnectionBus, SendResponse, requestId.second, response);
    }

    //! A network request came in asking, for a given input asset, what the status is of any jobs related to that request
    void AssetProcessorManager::ProcessGetAssetJobsInfoRequest(NetworkRequestID requestId, BaseAssetProcessorMessage* message, bool fencingFailed)
    {
        AssetJobsInfoRequest* request = azrtti_cast<AssetJobsInfoRequest*>(message);
        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetAssetJobsInfoRequest: Message is not of type %d.Incoming message type is %d.\n", AssetJobsInfoRequest::MessageType(), message->GetMessageType());
            return;
        }

        AssetJobsInfoResponse response;
        ProcessGetAssetJobsInfoRequest(*request, response);
        EBUS_EVENT_ID(requestId.first, AssetProcessor::ConnectionBus, SendResponse, requestId.second, response);
    }

    void AssetProcessorManager::ProcessGetAssetJobsInfoRequest(AssetJobsInfoRequest& request, AssetJobsInfoResponse& response)
    {
        if (request.m_assetId.IsValid())
        {
            //If the assetId is valid than search both the database and the pending queue and update the searchTerm with the source name
            SourceInfo searchResults;
            if (!SearchSourceInfoBySourceUUID(request.m_assetId.m_guid, searchResults))
            {
                // If still not found it means that this source asset is neither in the database nor in the queue for processing
                AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessGetAssetJobsInfoRequest: AssetProcessor unable to find the requested source asset having uuid (%s).\n",
                    request.m_assetId.m_guid.ToString<AZStd::string>().c_str());
                response = AssetJobsInfoResponse(AzToolsFramework::AssetSystem::JobInfoContainer(), false);
                return;
            }
            request.m_searchTerm = searchResults.m_sourceDatabaseName.toUtf8().constData();
        }

        QString normalizedInputAssetPath;

        AzToolsFramework::AssetSystem::JobInfoContainer jobList;
        AssetProcessor::JobIdEscalationList jobIdEscalationList;
        if (!request.m_isSearchTermJobKey)
        {
            normalizedInputAssetPath = AssetUtilities::NormalizeFilePath(request.m_searchTerm.c_str());

            if (QFileInfo(normalizedInputAssetPath).isAbsolute())
            {
                QString scanFolderName;
                QString relativePathToFile;
                if (!m_platformConfig->ConvertToRelativePath(normalizedInputAssetPath, relativePathToFile, scanFolderName))
                {
                    response = AssetJobsInfoResponse(AzToolsFramework::AssetSystem::JobInfoContainer(), false);
                    return;
                }

                normalizedInputAssetPath = relativePathToFile;
            }

            //any queued or in progress jobs will be in the map:
            for (const auto& entry : m_jobRunKeyToJobInfoMap)
            {
                if (AzFramework::StringFunc::Equal(entry.second.m_sourceFile.c_str(), normalizedInputAssetPath.toUtf8().constData()))
                {
                    jobList.push_back(entry.second);
                    if (request.m_escalateJobs)
                    {
                        jobIdEscalationList.append(qMakePair(entry.second.m_jobRunKey, AssetProcessor::JobEscalation::AssetJobRequestEscalation));
                    }
                }
            }
        }
        else
        {
            auto found = m_jobKeyToJobRunKeyMap.equal_range(request.m_searchTerm.c_str());

            for (auto iter = found.first; iter != found.second; ++iter)
            {
                auto foundJobRunKey = m_jobRunKeyToJobInfoMap.find(iter->second);
                if (foundJobRunKey != m_jobRunKeyToJobInfoMap.end())
                {
                    AzToolsFramework::AssetSystem::JobInfo& jobInfo = foundJobRunKey->second;
                    jobList.push_back(jobInfo);
                    if (request.m_escalateJobs)
                    {
                        jobIdEscalationList.append(qMakePair(iter->second, AssetProcessor::JobEscalation::AssetJobRequestEscalation));
                    }
                }
            }
        }

        if (!jobIdEscalationList.empty())
        {
            Q_EMIT EscalateJobs(jobIdEscalationList);
        }

        AzToolsFramework::AssetSystem::JobInfoContainer jobListDataBase;
        if (!request.m_isSearchTermJobKey)
        {
            //any succeeded or failed jobs will be in the table
            m_stateData->GetJobInfoBySourceName(normalizedInputAssetPath.toUtf8().constData(), jobListDataBase);
        }
        else
        {
            //check the database for all jobs with that job key
            m_stateData->GetJobInfoByJobKey(request.m_searchTerm, jobListDataBase);
        }

        for (const AzToolsFramework::AssetSystem::JobInfo& job : jobListDataBase)
        {
            auto result = AZStd::find_if(jobList.begin(), jobList.end(),
                    [&job](AzToolsFramework::AssetSystem::JobInfo& entry) -> bool
                    {
                        return
                        AzFramework::StringFunc::Equal(entry.m_platform.c_str(), job.m_platform.c_str()) &&
                        AzFramework::StringFunc::Equal(entry.m_jobKey.c_str(), job.m_jobKey.c_str()) &&
                        AzFramework::StringFunc::Equal(entry.m_sourceFile.c_str(), job.m_sourceFile.c_str());
                    });
            if (result == jobList.end())
            {
                // A job for this asset has already completed and was registered with the database so report that one as well.
                jobList.push_back(job);
            }
        }

        // resolve any paths here before sending the job info back, in case the AP's %log% is different than whatever is reading
        // the AssetJobsInfoResponse
        for (AzToolsFramework::AssetSystem::JobInfo& job : jobList)
        {
            char resolvedBuffer[AZ_MAX_PATH_LEN] = { 0 };

            AZ::IO::FileIOBase::GetInstance()->ResolvePath(job.m_firstFailLogFile.c_str(), resolvedBuffer, AZ_MAX_PATH_LEN);
            job.m_firstFailLogFile = resolvedBuffer;

            AZ::IO::FileIOBase::GetInstance()->ResolvePath(job.m_lastFailLogFile.c_str(), resolvedBuffer, AZ_MAX_PATH_LEN);
            job.m_lastFailLogFile = resolvedBuffer;
        }

        response = AssetJobsInfoResponse(jobList, true);
    }

    void AssetProcessorManager::CheckMissingFiles()
    {
        if (!m_activeFiles.isEmpty())
        {
            // not ready yet, we have not drained the queue.
            QTimer::singleShot(10, this, SLOT(CheckMissingFiles()));
            return;
        }

        if (m_isCurrentlyScanning)
        {
            return;
        }

        // note that m_SourceFilesInDatabase is a map from (full absolute path) --> (database name for file, which includes outputprefix)
        for (auto iter = m_sourceFilesInDatabase.begin(); iter != m_sourceFilesInDatabase.end(); iter++)
        {
            // CheckDeletedSourceFile actually expects the database name as the second value
            // iter.key is the full path normalized.  iter.value is the database path.
            // we need the relative path too, which involves removing the scan folder outputprefix it present:
            CheckDeletedSourceFile(iter.key(), iter.value().m_sourceRelativeToWatchFolder, iter.value().m_sourceDatabaseName);
        }

        // we want to remove any left over scan folders from the database only after
        // we remove all the products from source files we are no longer interested in,
        // we do it last instead of when we update scan folders because the scan folders table CASCADE DELETE on the sources, jobs,
        // products table and we want to do this last after cleanup of disk.
        for (auto entry : m_scanFoldersInDatabase)
        {
            if (!m_stateData->RemoveScanFolder(entry.second.m_scanFolderID))
            {
                AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckMissingFiles: Unable to remove Scan Folder having id %d from the database.", entry.second.m_scanFolderID);
                return;
            }
        }

        m_scanFoldersInDatabase.clear();
        m_sourceFilesInDatabase.clear();

        QueueIdleCheck();
    }

    void AssetProcessorManager::QueueIdleCheck()
    {
        // avoid putting many check for idle requests in the queue if its already there.
        if (!m_alreadyQueuedCheckForIdle)
        {
            m_alreadyQueuedCheckForIdle = true;
            QMetaObject::invokeMethod(this, "CheckForIdle", Qt::QueuedConnection);
        }
    }

    void AssetProcessorManager::QuitRequested()
    {
        m_quitRequested = true;
        m_filesToExamine.clear();
        Q_EMIT ReadyToQuit(this);
    }

    //! This request comes in and is expected to do whatever heuristic is required in order to determine if an asset actually exists in the database.
    void AssetProcessorManager::OnRequestAssetExists(NetworkRequestID groupID, QString platform, QString searchTerm)
    {
        QString productName = GuessProductOrSourceAssetName(searchTerm, platform, false);

        Q_EMIT SendAssetExistsResponse(groupID, !productName.isEmpty());
    }

    QString AssetProcessorManager::GuessProductOrSourceAssetName(QString searchTerm, QString platform, bool useLikeSearch)
    {
        // Search the product table
        QString productName = AssetUtilities::GuessProductNameInDatabase(searchTerm, platform, m_stateData.get());

        if (!productName.isEmpty())
        {
            return productName;
        }

        // Search the source table
        AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer products;

        if (!useLikeSearch && m_stateData->GetProductsBySourceName(searchTerm, products))
        {
            return searchTerm;
        }
        else if (useLikeSearch && m_stateData->GetProductsLikeSourceName(searchTerm, AzToolsFramework::AssetDatabase::AssetDatabaseConnection::LikeType::StartsWith, products))
        {
            return searchTerm;
        }

        return QString();
    }

    void AssetProcessorManager::RequestReady(NetworkRequestID networkRequestId, BaseAssetProcessorMessage* message, QString platform, bool fencingFailed)
    {
        using namespace AzFramework::AssetSystem;
        using namespace AzToolsFramework::AssetSystem;

        if (message->GetMessageType() == AssetJobsInfoRequest::MessageType())
        {
            ProcessGetAssetJobsInfoRequest(networkRequestId, message, fencingFailed);
        }
        else if (message->GetMessageType() == AssetJobLogRequest::MessageType())
        {
            ProcessGetAssetJobLogRequest(networkRequestId, message, fencingFailed);
        }
        else if (message->GetMessageType() == GetAbsoluteAssetDatabaseLocationRequest::MessageType())
        {
            ProcessGetAbsoluteAssetDatabaseLocationRequest(networkRequestId, message);
        }

        delete message;
    }

    void AssetProcessorManager::AssetCancelled(AssetProcessor::JobEntry jobEntry)
    {
        if (m_quitRequested)
        {
            return;
        }
        // Remove the log file for the cancelled job
        AZStd::string logFile = AssetUtilities::ComputeJobLogFolder() + "/" + AssetUtilities::ComputeJobLogFileName(jobEntry);
        EraseLogFile(logFile.c_str());

        // cancelled jobs are replaced by new jobs to process the same asset, so keep track of that for the analysis tracker too
        // note that this isn't a failure - the job just isn't there anymore.
        UpdateAnalysisTrackerForFile(jobEntry, AnalysisTrackerUpdateType::JobFinished);

        OnJobStatusChanged(jobEntry, JobStatus::Failed);
        // we know that things have changed at this point; ensure that we check for idle
        QueueIdleCheck();
    }

    void AssetProcessorManager::AssetFailed(AssetProcessor::JobEntry jobEntry)
    {
        if (m_quitRequested)
        {
            return;
        }

        m_AssetProcessorIsBusy = true;
        Q_EMIT AssetProcessorManagerIdleState(false);

        // when an asset fails, we must make sure we notify the Analysis Tracker that it has failed, so that it doesn't write into the database
        // that it can be skipped next time:

        UpdateAnalysisTrackerForFile(jobEntry, AnalysisTrackerUpdateType::JobFailed);

        // if its a fake "autofail job" or other reason for it not to exist in the DB, don't do anything here.
        if (!jobEntry.m_addToDatabase)
        {
            return;
        }

        // wipe the times so that it will try again next time.
        // note:  Leave the prior successful products where they are, though.

        // We have to include a fingerprint in the database for this job, otherwise when assets change that
        // affect this failed job, the failed assets won't get rescanned and won't be in the database and
        // therefore won't get reprocessed. Set it to FAILED_FINGERPRINT.
        //create/update the source record for this job
        AzToolsFramework::AssetDatabase::SourceDatabaseEntry source;
        AzToolsFramework::AssetDatabase::SourceDatabaseEntryContainer sources;
        QString absolutePathToFile = jobEntry.GetAbsoluteSourcePath();
        if (m_stateData->GetSourcesBySourceName(jobEntry.m_databaseSourceName, sources))
        {
            AZ_Assert(sources.size() == 1, "Should have only found one source!!!");
            source = AZStd::move(sources[0]);
        }
        else
        {
            //if we didn't find a source, we make a new source
            const ScanFolderInfo* scanFolder = m_platformConfig->GetScanFolderForFile(jobEntry.m_watchFolderPath);
            if (!scanFolder)
            {
                //can't find the scan folder this source came from!?
                AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to find the scan folder for this source!!!");
            }

            //add the new source
            if (!QFile::exists(absolutePathToFile))
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Source file %s no longer exists, it will not be added to database.\n",
                    absolutePathToFile.toUtf8().constData());
                return;
            }
            else
            {
                AddSourceToDatabase(source, scanFolder, jobEntry.m_databaseSourceName);
            }
        }

        //create/update the job
        AzToolsFramework::AssetDatabase::JobDatabaseEntry job;
        AzToolsFramework::AssetDatabase::JobDatabaseEntryContainer jobs;
        if (m_stateData->GetJobsBySourceID(source.m_sourceID, jobs, jobEntry.m_builderGuid, jobEntry.m_jobKey, jobEntry.m_platformInfo.m_identifier.c_str()))
        {
            AZ_Assert(jobs.size() == 1, "Should have only found one job!!!");
            job = AZStd::move(jobs[0]);

            //we only want to keep the first fail and the last fail log
            //if it has failed before, both first and last will be set, only delete last fail file if its not the first fail
            if (job.m_firstFailLogTime && job.m_firstFailLogTime != job.m_lastFailLogTime)
            {
                EraseLogFile(job.m_lastFailLogFile.c_str());
            }

            //we failed so the last fail is the same as the current
            job.m_lastFailLogTime = job.m_lastLogTime = QDateTime::currentMSecsSinceEpoch();
            job.m_lastFailLogFile = job.m_lastLogFile = AssetUtilities::ComputeJobLogFolder() + "/" + AssetUtilities::ComputeJobLogFileName(jobEntry);

            //if we have never failed before also set the first fail to be the last fail
            if (!job.m_firstFailLogTime)
            {
                job.m_firstFailLogTime = job.m_lastFailLogTime;
                job.m_firstFailLogFile = job.m_lastFailLogFile;
            }
        }
        else
        {
            //if we didn't find a job, we make a new one
            job.m_sourcePK = source.m_sourceID;
            job.m_jobKey = jobEntry.m_jobKey.toUtf8().constData();
            job.m_platform = jobEntry.m_platformInfo.m_identifier;
            job.m_builderGuid = jobEntry.m_builderGuid;

            //if this is a new job that failed then first filed ,last failed and current are the same
            job.m_firstFailLogTime = job.m_lastFailLogTime = job.m_lastLogTime = QDateTime::currentMSecsSinceEpoch();
            job.m_firstFailLogFile = job.m_lastFailLogFile = job.m_lastLogFile = AssetUtilities::ComputeJobLogFolder() + "/" + AssetUtilities::ComputeJobLogFileName(jobEntry);
        }

        // invalidate the fingerprint
        job.m_fingerprint = FAILED_FINGERPRINT;

        //set the random key
        job.m_jobRunKey = jobEntry.m_jobRunKey;

        QString fullPath = jobEntry.GetAbsoluteSourcePath();
        //set the new status
        job.m_status = fullPath.length() < AP_MAX_PATH_LEN ? JobStatus::Failed : JobStatus::Failed_InvalidSourceNameExceedsMaxLimit;

        //create/update job
        if (!m_stateData->SetJob(job))
        {
            //somethings wrong...
            AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to update the job in the database!!!");
        }

#if !defined(BATCH_MODE)
        // send a network message when not in batch mode.
        const ScanFolderInfo* scanFolder = m_platformConfig->GetScanFolderForFile(jobEntry.m_watchFolderPath);
        AzToolsFramework::AssetSystem::SourceFileNotificationMessage message(AZ::OSString(source.m_sourceName.c_str()), AZ::OSString(scanFolder->ScanPath().toUtf8().constData()), AzToolsFramework::AssetSystem::SourceFileNotificationMessage::FileFailed, source.m_sourceGuid);
        EBUS_EVENT(AssetProcessor::ConnectionBus, Send, 0, message);
        MessageInfoBus::Broadcast(&MessageInfoBusTraits::OnAssetFailed, source.m_sourceName);
#endif

        OnJobStatusChanged(jobEntry, JobStatus::Failed);
        
        // note that we always print out the failed job status here in both batch and GUI mode.
        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Failed %s, (%s)... \n",
            jobEntry.m_pathRelativeToWatchFolder.toUtf8().constData(),
            jobEntry.m_platformInfo.m_identifier.c_str());
        AZ_TracePrintf(AssetProcessor::DebugChannel, "AssetProcessed [fail] Jobkey \"%s\", Builder UUID \"%s\", Fingerprint %u ) \n",
            jobEntry.m_jobKey.toUtf8().constData(),
            jobEntry.m_builderGuid.ToString<AZStd::string>().c_str(),
            jobEntry.m_computedFingerprint);

        // we know that things have changed at this point; ensure that we check for idle after we've finished processing all of our assets
        // and don't rely on the file watcher to check again.
        // If we rely on the file watcher only, it might fire before the AssetMessage signal has been responded to and the
        // Asset Catalog may not realize that things are dirty by that point.
        QueueIdleCheck();
    }

    void AssetProcessorManager::AssetProcessed_Impl()
    {
        m_processedQueued = false;
        if (m_quitRequested || m_assetProcessedList.empty())
        {
            return;
        }

        // Note: if we get here, the scanning / createjobs phase has finished
        // because we no longer start any jobs until it has finished.  So there is no reason
        // to delay notification or processing.

        // before we accept this outcome, do one final check to make sure its not about to double-address things by stomping on the same subID across many products.
        // let's also make sure that the same product was not emitted by some other job.  we detect this by finding other jobs
        // with the same product, but with different sources.
        
        for (auto itProcessedAsset = m_assetProcessedList.begin(); itProcessedAsset != m_assetProcessedList.end(); )
        {
            AZStd::unordered_set<AZ::u32> existingSubIDs;
            bool remove = false;
            for (const AssetBuilderSDK::JobProduct& product : itProcessedAsset->m_response.m_outputProducts)
            {
                if (!existingSubIDs.insert(product.m_productSubID).second)
                {
                    // insert pair<iter,bool> will return false if the item was already there, indicating a collision.

                    JobDetails jobdetail;
                    jobdetail.m_jobEntry = JobEntry(
                        itProcessedAsset->m_entry.m_watchFolderPath,
                        itProcessedAsset->m_entry.m_pathRelativeToWatchFolder,
                        itProcessedAsset->m_entry.m_databaseSourceName,
                        itProcessedAsset->m_entry.m_builderGuid,
                        itProcessedAsset->m_entry.m_platformInfo,
                        itProcessedAsset->m_entry.m_jobKey, 0, GenerateNewJobRunKey(),
                        itProcessedAsset->m_entry.m_sourceFileUUID);
                    jobdetail.m_autoFail = true;
                    jobdetail.m_critical = true;
                    jobdetail.m_priority = INT_MAX; // front of the queue.
                                                    // the new lines make it easier to copy and paste the file names.
                    QString sourceName = itProcessedAsset->m_entry.GetAbsoluteSourcePath();

                    jobdetail.m_jobParam[AZ_CRC(AutoFailReasonKey)] = AZStd::string::format(
                        "More than one product was emitted for this source file with the same SubID.\n"
                        "Source file:\n"
                        "%s\n"
                        "Product SubID %u from product:\n"
                        "%s\n"
                        "Please check the builder code, specifically where it decides what subIds to assign to its output products and make sure it assigns a unique one to each.",
                        sourceName.toUtf8().constData(),
                        product.m_productSubID,
                        product.m_productFileName.c_str());

                    UpdateAnalysisTrackerForFile(itProcessedAsset->m_entry, AnalysisTrackerUpdateType::JobFailed);

                    Q_EMIT AssetToProcess(jobdetail);// forwarding this job to rccontroller to fail it
                    remove = true;
                    break;
                }

                // The product file path is always lower cased, we can't check that for existance.
                // Let rebuild a fs sensitive file path by replacing the cache path.
                // We assume any file paths normalized, ie no .. nor (back) slashes.
                const QString productFilePath = m_cacheRootDir.filePath(product.m_productFileName.substr(m_normalizedCacheRootPath.length() +1).c_str());

                // if the claimed product does not exist, remove it so it does not get into the database
                if (!QFile::exists(productFilePath))
                {
                    remove = true;
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Was expecting product file %s... but it already appears to be gone. \n", qUtf8Printable(productFilePath));
                }
                else
                {
                    // database products, if present, will be in the form "platform/game/subfolders/productfile", convert
                    // our new products to the same thing by removing the cache root
                    QString newProductName = productFilePath;
                    newProductName = AssetUtilities::NormalizeFilePath(newProductName);
                    if (!newProductName.startsWith(m_normalizedCacheRootPath, Qt::CaseInsensitive))
                    {
                        AZ_Error(AssetProcessor::ConsoleChannel, "AssetProcessed(\" << %s << \", \" << %s << \" ... ) cache file \"  %s << \" does not appear to be within the cache!.\n",
                            itProcessedAsset->m_entry.m_pathRelativeToWatchFolder.toUtf8().constData(),
                            itProcessedAsset->m_entry.m_platformInfo.m_identifier.c_str(),
                            newProductName.toUtf8().constData());
                    }
                    // note that this is a relative path from the cache root dir itself, and thus does need to be lowered in its entirety.
                    newProductName = m_cacheRootDir.relativeFilePath(newProductName).toLower();

                    // query all sources for this exact new product name
                    // the intention here is to find out conflicts where two different sources produce the same exact product.
                    AzToolsFramework::AssetDatabase::SourceDatabaseEntryContainer sources;
                    if (m_stateData->GetSourcesByProductName(newProductName.toUtf8().constData(), sources))
                    {
                        for (const auto& source : sources)
                        {
                            if (azstricmp(source.m_sourceName.c_str(), itProcessedAsset->m_entry.m_databaseSourceName.toUtf8().constData()) != 0)
                            {
                                remove = true;
                                //this means we have a dupe product name for a different source
                                //usually this is caused by /blah/x.tif and an /blah/x.dds in the source folder
                                //they both become /blah/x.dds in the cache
                                //Not much of an option here, if we find a dupe we already lost access to the
                                //first one in the db because it was overwritten. So do not commit this new one and
                                //set the first for reprocessing. That way we will get the original back.

                                //delete the original sources products
                                AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer products;
                                m_stateData->GetProductsBySourceID(source.m_sourceID, products);
                                DeleteProducts(products);

                                //set the fingerprint to failed
                                AzToolsFramework::AssetDatabase::JobDatabaseEntryContainer jobs;
                                m_stateData->GetJobsBySourceID(source.m_sourceID, jobs);
                                for (auto& job : jobs)
                                {
                                    job.m_fingerprint = FAILED_FINGERPRINT;
                                    m_stateData->SetJob(job);
                                }

                                //delete product files for this new source
                                for (const auto& product : itProcessedAsset->m_response.m_outputProducts)
                                {
                                    // The product file path is always lower cased, we can't check that for existance.
                                    // Let rebuild a fs sensitive file path by replacing the cache path.
                                    // We assume any file paths normalized, ie no .. nor (back) slashes.
                                    const QString productFilePath = m_cacheRootDir.filePath(product.m_productFileName.substr(m_normalizedCacheRootPath.length() +1).c_str());

                                    if (!QFile::exists(productFilePath))
                                    {
                                        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Was expecting to delete product file %s... but it already appears to be gone. \n", qUtf8Printable(productFilePath));
                                    }
                                    else if (!QFile::remove(productFilePath))
                                    {
                                        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Was unable to delete product file %s...\n", qUtf8Printable(productFilePath));
                                    }
                                    else
                                    {
                                        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Deleted product file %s\n", qUtf8Printable(productFilePath));
                                    }
                                }

                                //let people know what happened
                                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "%s has failed because another source %s has already produced the same product %s. Rebuild the original Source.\n",
                                    itProcessedAsset->m_entry.m_pathRelativeToWatchFolder.toUtf8().constData(), source.m_sourceName.c_str(), newProductName.toUtf8().constData());

                                //recycle the original source
                                AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry scanfolder;
                                AZStd::string fullSourcePath = source.m_sourceName;
                                if (m_stateData->GetScanFolderByScanFolderID(source.m_scanFolderPK, scanfolder))
                                {
                                    fullSourcePath = AZStd::string::format("%s/%s", scanfolder.m_scanFolder.c_str(), source.m_sourceName.c_str());
                                    AssessFileInternal(fullSourcePath.c_str(), false);
                                }

                                QString duplicateProduct = m_cacheRootDir.absoluteFilePath(newProductName);

                                JobDetails jobdetail;
                                jobdetail.m_jobEntry = JobEntry(
                                    itProcessedAsset->m_entry.m_watchFolderPath, 
                                    itProcessedAsset->m_entry.m_pathRelativeToWatchFolder,
                                    itProcessedAsset->m_entry.m_databaseSourceName,
                                    itProcessedAsset->m_entry.m_builderGuid, 
                                    itProcessedAsset->m_entry.m_platformInfo, 
                                    itProcessedAsset->m_entry.m_jobKey, 0, GenerateNewJobRunKey(), 
                                    itProcessedAsset->m_entry.m_sourceFileUUID);
                                jobdetail.m_autoFail = true;
                                jobdetail.m_critical = true;
                                jobdetail.m_priority = INT_MAX; // front of the queue.                                
                                jobdetail.m_scanFolder = m_platformConfig->GetScanFolderForFile(itProcessedAsset->m_entry.GetAbsoluteSourcePath());
                                // the new lines make it easier to copy and paste the file names.
                                jobdetail.m_jobParam[AZ_CRC(AutoFailReasonKey)] = AZStd::string::format(
                                        "A different source file\n%s\nis already outputting the product\n%s\n"
                                        "Please check other files in the same folder as source file and make sure no two sources output the product file.\n"
                                        "For example, you can't have a DDS file and a TIF file in the same folder, as they would cause overwriting.\n",
                                        fullSourcePath.c_str(),
                                        duplicateProduct.toUtf8().data());

                                // this is a failure, so make sure that the system that is tracking files
                                // knows that this file must not be skipped next time:
                                UpdateAnalysisTrackerForFile(itProcessedAsset->m_entry, AnalysisTrackerUpdateType::JobFailed);

                                Q_EMIT AssetToProcess(jobdetail);// forwarding this job to rccontroller to fail it
                            }
                        }
                    }
                }
            }

            if (remove)
            {
                //we found a dupe remove this entry from the processed list so it does not get into the db
                itProcessedAsset = m_assetProcessedList.erase(itProcessedAsset);
            }
            else
            {
                ++itProcessedAsset;
            }
        }

        //process the asset list
        for (AssetProcessedEntry& processedAsset : m_assetProcessedList)
        {
            // update products / delete no longer relevant products
            // note that the cache stores products WITH the name of the platform in it so you don't have to do anything
            // to those strings to process them.

            //create/update the source record for this job
            AzToolsFramework::AssetDatabase::SourceDatabaseEntry source;
            AzToolsFramework::AssetDatabase::SourceDatabaseEntryContainer sources;
            auto scanFolder = m_platformConfig->GetScanFolderForFile(processedAsset.m_entry.m_watchFolderPath);
            if (!scanFolder)
            {
                //can't find the scan folder this source came from!?
                AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to find the scan folder for this source!!!");
                continue;
            }

            if (m_stateData->GetSourcesBySourceNameScanFolderId(processedAsset.m_entry.m_databaseSourceName, scanFolder->ScanFolderID(), sources))
            {
                AZ_Assert(sources.size() == 1, "Should have only found one source!!!");
                source = AZStd::move(sources[0]);
            }
            else
            {
                //if we didn't find a source, we make a new source
                //add the new source
                AddSourceToDatabase(source, scanFolder, processedAsset.m_entry.m_databaseSourceName);
            }

            //create/update the job
            AzToolsFramework::AssetDatabase::JobDatabaseEntry job;
            AzToolsFramework::AssetDatabase::JobDatabaseEntryContainer jobs;
            if (m_stateData->GetJobsBySourceID(source.m_sourceID, jobs, processedAsset.m_entry.m_builderGuid, processedAsset.m_entry.m_jobKey, processedAsset.m_entry.m_platformInfo.m_identifier.c_str()))
            {
                AZ_Assert(jobs.size() == 1, "Should have only found one job!!!");
                job = AZStd::move(jobs[0]);
            }
            else
            {
                //if we didn't find a job, we make a new one
                job.m_sourcePK = source.m_sourceID;
            }

            job.m_fingerprint = processedAsset.m_entry.m_computedFingerprint;
            job.m_jobKey = processedAsset.m_entry.m_jobKey.toUtf8().constData();
            job.m_platform = processedAsset.m_entry.m_platformInfo.m_identifier;
            job.m_builderGuid = processedAsset.m_entry.m_builderGuid;
            job.m_jobRunKey = processedAsset.m_entry.m_jobRunKey;


            if (!AZ::IO::FileIOBase::GetInstance()->Exists(job.m_lastLogFile.c_str()))
            {
                // its okay for the log to not exist, if there was no log for it (for example simple jobs that just copy assets and did not encounter any problems will generate no logs)
                job.m_lastLogFile.clear();
            }

            // delete any previous failed job logs:
            bool deletedFirstFailedLog = EraseLogFile(job.m_firstFailLogFile.c_str());
            bool deletedLastFailedLog = EraseLogFile(job.m_lastFailLogFile.c_str());

            // also delete the existing log file since we're about to replace it:
            EraseLogFile(job.m_lastLogFile.c_str());

            // if we deleted them, then make sure the DB no longer tracks them either.
            if (deletedLastFailedLog)
            {
                job.m_lastFailLogTime = 0;
                job.m_lastFailLogFile.clear();
            }

            if (deletedFirstFailedLog)
            {
                job.m_firstFailLogTime = 0;
                job.m_firstFailLogFile.clear();
            }

            //set the new status and update log
            job.m_status = JobStatus::Completed;
            job.m_lastLogTime = QDateTime::currentMSecsSinceEpoch();
            job.m_lastLogFile = AssetUtilities::ComputeJobLogFolder() + "/" + AssetUtilities::ComputeJobLogFileName(processedAsset.m_entry);

            // create/update job:
            if (!m_stateData->SetJob(job))
            {
                AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to update the job in the database!");
            }

            //query prior products for this job id
            AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer priorProducts;
            m_stateData->GetProductsByJobID(job.m_jobID, priorProducts);

            //make new product entries from the job response output products
            AZStd::vector<AZStd::pair<AzToolsFramework::AssetDatabase::ProductDatabaseEntry, const AssetBuilderSDK::JobProduct*> > newProducts;
            AZStd::vector<AZStd::vector<AZ::u32> > newLegacySubIDs;  // each product has a vector of legacy subids;
            for (const AssetBuilderSDK::JobProduct& product : processedAsset.m_response.m_outputProducts)
            {
                // prior products, if present, will be in the form "platform/game/subfolders/productfile", convert
                // our new products to the same thing by removing the cache root
                QString newProductName = product.m_productFileName.c_str();
                newProductName = AssetUtilities::NormalizeFilePath(newProductName);
                if (!newProductName.startsWith(m_normalizedCacheRootPath, Qt::CaseInsensitive))
                {
                    AZ_Error(AssetProcessor::ConsoleChannel, "AssetProcessed(\" << %s << \", \" << %s << \" ... ) cache file \"  %s << \" does not appear to be within the cache!.\n",
                        processedAsset.m_entry.m_pathRelativeToWatchFolder.toUtf8().constData(),
                        processedAsset.m_entry.m_platformInfo.m_identifier.c_str(),
                        newProductName.toUtf8().constData());
                }

                // note that the cache root dir is being used here to generate a relative path (not an absolute path).
                // this means that the entire string can be lowered since it contains only the parts up above the cache root dir
                newProductName = m_cacheRootDir.relativeFilePath(newProductName).toLower();

                //make a new product entry for this file
                AzToolsFramework::AssetDatabase::ProductDatabaseEntry newProduct;
                newProduct.m_jobPK = job.m_jobID;
                newProduct.m_productName = newProductName.toUtf8().constData();
                newProduct.m_assetType = product.m_productAssetType;
                newProduct.m_subID = product.m_productSubID;

                //This is the legacy product guid, its only use is for backward compatibility as before the asset id's guid was created off of the relative product name.
                // Right now when we query for an asset guid we first match on the source guid which is correct and secondarily match on the product guid. Eventually this will go away.
                newProductName = newProductName.right(newProductName.length() - newProductName.indexOf('/') - 1); // remove PLATFORM and an extra slash
                newProductName = newProductName.right(newProductName.length() - newProductName.indexOf('/') - 1); // remove GAMENAME and an extra slash
                newProduct.m_legacyGuid = AZ::Uuid::CreateName(newProductName.toUtf8().constData());

                //push back the new product into the new products list
                newProducts.emplace_back(newProduct, &product);
                newLegacySubIDs.push_back(product.m_legacySubIDs);
            }

            //now we want to remove any lingering product files from the previous build that no longer exist
            //so subtract the new products from the prior products, whatever is left over in prior products no longer exists
            if (!priorProducts.empty())
            {
                for (const auto& pair : newProducts)
                {
                    const AzToolsFramework::AssetDatabase::ProductDatabaseEntry& newProductEntry = pair.first;
                    priorProducts.erase(AZStd::remove(priorProducts.begin(), priorProducts.end(), newProductEntry), priorProducts.end());
                }
            }

            // we need to delete these product files from the disk as they no longer exist and inform everyone we did so
            for (const auto& priorProduct : priorProducts)
            {
                // product name will be in the form "platform/game/relativeProductPath"
                // and will always already be a lowercase string, because its relative to the cache.
                QString productName = priorProduct.m_productName.c_str();

                // the full file path is gotten by adding the product name to the cache root.
                // this is case sensitive since it refers to a real location on disk.
                QString fullProductPath = m_cacheRootDir.absoluteFilePath(productName);

                // relative file path is gotten by removing the platform and game from the product name
                QString relativeProductPath = productName;
                relativeProductPath = relativeProductPath.right(relativeProductPath.length() - relativeProductPath.indexOf('/') - 1); // remove PLATFORM and an extra slash
                relativeProductPath = relativeProductPath.right(relativeProductPath.length() - relativeProductPath.indexOf('/') - 1); // remove GAMENAME and an extra slash

                AZ::Data::AssetId assetId(source.m_sourceGuid, priorProduct.m_subID);

                // also compute the legacy ids that used to refer to this asset

                AZ::Data::AssetId legacyAssetId(priorProduct.m_legacyGuid, 0);
                AZ::Data::AssetId legacySourceAssetId(AssetUtilities::CreateSafeSourceUUIDFromName(source.m_sourceName.c_str(), false), priorProduct.m_subID);

                AssetNotificationMessage message(relativeProductPath.toUtf8().constData(), AssetNotificationMessage::AssetRemoved, priorProduct.m_assetType);
                message.m_assetId = assetId;

                if (legacyAssetId != assetId)
                {
                    message.m_legacyAssetIds.push_back(legacyAssetId);
                }

                if (legacySourceAssetId != assetId)
                {
                    message.m_legacyAssetIds.push_back(legacySourceAssetId);
                }

                bool shouldDeleteFile = true;
                for (const auto& pair : newProducts)
                {
                    const auto& currentProduct = pair.first;

                    if (AzFramework::StringFunc::Equal(currentProduct.m_productName.c_str(), priorProduct.m_productName.c_str()))
                    {
                        // This is a special case - The subID and other fields differ but it outputs the same actual product file on disk
                        // so let's not delete that product file since by the time we get here, it has already replaced it in the cache folder
                        // with the new product.
                        shouldDeleteFile = false;
                        break;
                    }
                }
                //delete the full file path
                if (shouldDeleteFile)
                {
                    if (!QFile::exists(fullProductPath))
                    {
                        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Was expecting to delete %s ... but it already appears to be gone. \n", fullProductPath.toUtf8().constData());

                        // we still need to tell everyone that its gone!

                        Q_EMIT AssetMessage(processedAsset.m_entry.m_platformInfo.m_identifier.c_str(), message); // we notify that we are aware of a missing product either way.
                    }
                    else if (!QFile::remove(fullProductPath))
                    {
                        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Was unable to delete file %s will retry next time...\n", fullProductPath.toUtf8().constData());
                        continue; // do not update database
                    }
                    else
                    {
                        AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Deleting file %s because the recompiled input file no longer emitted that product.\n", fullProductPath.toUtf8().constData());

                        Q_EMIT AssetMessage(QString(processedAsset.m_entry.m_platformInfo.m_identifier.c_str()), message); // we notify that we are aware of a missing product either way.
                    }
                }
                else
                {
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "File %s was replaced with a new, but different file.\n", fullProductPath.toUtf8().constData());
                    // Don't report that the file has been removed as it's still there, but as a different kind of file (different sub id, type, etc.).
                }

                //trace that we are about to remove a lingering prior product from the database
                // because of On Delete Cascade this will also remove any legacy subIds associated with that product automatically.
                if (!m_stateData->RemoveProduct(priorProduct.m_productID))
                {
                    //somethings wrong...
                    AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to remove lingering prior products from the database!!! %s", priorProduct.ToString().c_str());
                }
                else
                {
                    AZ_TracePrintf(AssetProcessor::DebugChannel, "Removed lingering prior product %s\n", priorProduct.ToString().c_str());
                }

                QString parentFolderName = QFileInfo(fullProductPath).absolutePath();
                m_checkFoldersToRemove.insert(parentFolderName);
            }

            //trace that we are about to update the products in the database

            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Processed \"%s\" (\"%s\")... \n",
                processedAsset.m_entry.m_pathRelativeToWatchFolder.toUtf8().constData(),
                processedAsset.m_entry.m_platformInfo.m_identifier.c_str());
            AZ_TracePrintf(AssetProcessor::DebugChannel, "JobKey \"%s\", Builder UUID \"%s\", Fingerprint %u ) \n",
                processedAsset.m_entry.m_jobKey.toUtf8().constData(),
                processedAsset.m_entry.m_builderGuid.ToString<AZStd::string>().c_str(),
                processedAsset.m_entry.m_computedFingerprint);

            AzToolsFramework::AssetDatabase::ProductDependencyDatabaseEntryContainer dependencyContainer;

            //set the new products
            for (size_t productIdx = 0; productIdx < newProducts.size(); ++productIdx)
            {
                auto& pair = newProducts[productIdx];
                AzToolsFramework::AssetDatabase::ProductDatabaseEntry& newProduct = pair.first;
                AZStd::vector<AZ::u32>& subIds = newLegacySubIDs[productIdx];

                if (!m_stateData->SetProduct(newProduct))
                {
                    //somethings wrong...
                    AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to set new product in the the database!!! %s", newProduct.ToString().c_str());
                }
                else
                {
                    m_stateData->RemoveLegacySubIDsByProductID(newProduct.m_productID);
                    for (AZ::u32 subId : subIds)
                    {
                        AzToolsFramework::AssetDatabase::LegacySubIDsEntry entryToCreate(newProduct.m_productID, subId);
                        m_stateData->CreateOrUpdateLegacySubID(entryToCreate);
                    }

                    // Remove all previous dependencies
                    if (!m_stateData->RemoveProductDependencyByProductId(newProduct.m_productID))
                    {
                        AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to remove old product dependencies for product %d", newProduct.m_productID);
                    }

                    const AssetBuilderSDK::JobProduct* jobProduct = pair.second;
                    // Build up the list of new dependencies
                    for (auto& productDependency : jobProduct->m_dependencies)
                    {
                        dependencyContainer.emplace_back(newProduct.m_productID, productDependency.m_dependencyId.m_guid, productDependency.m_dependencyId.m_subId, productDependency.m_flags);
                    }
                }
            }

            // Set the new dependencies
            if (!m_stateData->SetProductDependencies(dependencyContainer))
            {
                AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to set product dependencies");
            }

            // now we need notify everyone about the new products
            for (size_t productIdx = 0; productIdx < newProducts.size(); ++productIdx)
            {
                auto& pair = newProducts[productIdx];
                AzToolsFramework::AssetDatabase::ProductDatabaseEntry& newProduct = pair.first;
                AZStd::vector<AZ::u32>& subIds = newLegacySubIDs[productIdx];

                // product name will be in the form "platform/game/relativeProductPath"
                QString productName = QString::fromUtf8(newProduct.m_productName.c_str());

                // the full file path is gotten by adding the product name to the cache root
                QString fullProductPath = m_cacheRootDir.absoluteFilePath(productName);

                // relative file path is gotten by removing the platform and game from the product name
                QString relativeProductPath = productName;
                relativeProductPath = relativeProductPath.right(relativeProductPath.length() - relativeProductPath.indexOf('/') - 1); // remove PLATFORM and an extra slash
                relativeProductPath = relativeProductPath.right(relativeProductPath.length() - relativeProductPath.indexOf('/') - 1); // remove GAMENAME and an extra slash

                AssetNotificationMessage message(relativeProductPath.toUtf8().constData(), AssetNotificationMessage::AssetChanged, newProduct.m_assetType);
                AZ::Data::AssetId assetId(source.m_sourceGuid, newProduct.m_subID);
                AZ::Data::AssetId legacyAssetId(newProduct.m_legacyGuid, 0);
                AZ::Data::AssetId legacySourceAssetId(AssetUtilities::CreateSafeSourceUUIDFromName(source.m_sourceName.c_str(), false), newProduct.m_subID);

                message.m_data = relativeProductPath.toUtf8().data();
                message.m_sizeBytes = QFileInfo(fullProductPath).size();
                message.m_assetId = assetId;

                const AssetBuilderSDK::JobProduct& jobProduct = *pair.second;
                message.m_dependencies.reserve(jobProduct.m_dependencies.size());

                for (const auto& entry : jobProduct.m_dependencies)
                {
                    message.m_dependencies.emplace_back(entry.m_dependencyId, entry.m_flags);
                }

                if (legacyAssetId != assetId)
                {
                    message.m_legacyAssetIds.push_back(legacyAssetId);
                }

                if (legacySourceAssetId != assetId)
                {
                    message.m_legacyAssetIds.push_back(legacySourceAssetId);
                }

                for (AZ::u32 newLegacySubId : subIds)
                {
                    AZ::Data::AssetId createdSubID(source.m_sourceGuid, newLegacySubId);
                    if ((createdSubID != legacyAssetId) && (createdSubID != legacySourceAssetId) && (createdSubID != assetId))
                    {
                        message.m_legacyAssetIds.push_back(createdSubID);
                    }
                }

                Q_EMIT AssetMessage(QString(processedAsset.m_entry.m_platformInfo.m_identifier.c_str()), message);

                AddKnownFoldersRecursivelyForFile(fullProductPath, m_cacheRootDir.absolutePath());
            }

            QString fullSourcePath = processedAsset.m_entry.GetAbsoluteSourcePath();

            // notify the system about inputs:
            Q_EMIT InputAssetProcessed(fullSourcePath, QString(processedAsset.m_entry.m_platformInfo.m_identifier.c_str()));
            OnJobStatusChanged(processedAsset.m_entry, JobStatus::Completed);

            // notify the analysis tracking system of our success (each processed entry is one job)
            // do this after the various checks above and database updates, so that the finalization step can take it all into account if it needs to.
            UpdateAnalysisTrackerForFile(processedAsset.m_entry, AnalysisTrackerUpdateType::JobFinished);

            if (!QFile::exists(fullSourcePath))
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Source file %s deleted during processing - re-checking...\n",
                    fullSourcePath.toUtf8().constData());
                AssessFileInternal(fullSourcePath, true);
            }
        }

        m_assetProcessedList.clear();
        // we know that things have changed at this point; ensure that we check for idle after we've finished processing all of our assets
        // and don't rely on the file watcher to check again.
        // If we rely on the file watcher only, it might fire before the AssetMessage signal has been responded to and the
        // Asset Catalog may not realize that things are dirty by that point.
        QueueIdleCheck();
    }

    void AssetProcessorManager::AssetProcessed(JobEntry jobEntry, AssetBuilderSDK::ProcessJobResponse response)
    {
        if (m_quitRequested)
        {
            return;
        }

        m_AssetProcessorIsBusy = true;
        Q_EMIT AssetProcessorManagerIdleState(false);

        // if its a fake "autosuccess job" or other reason for it not to exist in the DB, don't do anything here.
        if (!jobEntry.m_addToDatabase)
        {
            return;
        }

        m_assetProcessedList.push_back(AssetProcessedEntry(jobEntry, response));

        if (!m_processedQueued)
        {
            m_processedQueued = true;
            AssetProcessed_Impl();
        }
    }

    void AssetProcessorManager::CheckSource(const FileEntry& source)
    {
        // when this function is triggered, it means that a file appeared because it was modified or added or deleted,
        // and the grace period has elapsed.
        // this is the first point at which we MIGHT be interested in a file.
        // to avoid flooding threads we queue these up for later checking.

        AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckSource: %s %s\n", source.m_fileName.toUtf8().constData(), source.m_isDelete ? "true" : "false");

        QString normalizedFilePath = AssetUtilities::NormalizeFilePath(source.m_fileName);

        if (!source.m_isFromScanner) // the scanner already checks for exclusions.
        {
            if (m_platformConfig->IsFileExcluded(normalizedFilePath))
            {
                return;
            }
        }

        // if metadata file change, pretend the actual file changed
        // the fingerprint will be different anyway since metadata file is folded in

        for (int idx = 0; idx < m_platformConfig->MetaDataFileTypesCount(); idx++)
        {
            QPair<QString, QString> metaInfo = m_platformConfig->GetMetaDataFileTypeAt(idx);
            QString originalName = normalizedFilePath;

            if (normalizedFilePath.endsWith("." + metaInfo.first, Qt::CaseInsensitive))
            {
                //its a meta file.  What was the original?

                normalizedFilePath = normalizedFilePath.left(normalizedFilePath.length() - (metaInfo.first.length() + 1));
                if (!metaInfo.second.isEmpty())
                {
                    // its not empty - replace the meta file with the original extension
                    normalizedFilePath += ".";
                    normalizedFilePath += metaInfo.second;
                }

                // we need the actual casing of the source file
                // but the metafile might have different casing... Qt will fail to get the -actual- casing of the source file, which we need.  It uses string ops internally.
                // so we have to work around this by using the Dir that the file is in:

                QFileInfo newInfo(normalizedFilePath);
                QStringList searchPattern;
                searchPattern << newInfo.fileName();

                QStringList actualCasing = newInfo.absoluteDir().entryList(searchPattern, QDir::Files);

                if (actualCasing.empty())
                {
                    QString warning = QCoreApplication::translate("Warning", "Warning:  Metadata file (%1) missing source file (%2)\n").arg(originalName).arg(normalizedFilePath);
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, warning.toUtf8().constData());
                    return;
                }

                // the casing might be different, too, so retrieve the actual case of the actual source file here:
                normalizedFilePath = newInfo.absoluteDir().absoluteFilePath(actualCasing[0]);
                break;
            }
        }
        // even if the entry already exists,
        // overwrite the entry here, so if you modify, then delete it, its the latest action thats always on the list.

        m_filesToExamine[normalizedFilePath] = FileEntry(normalizedFilePath, source.m_isDelete, source.m_isFromScanner);

        // this block of code adds anything which DEPENDS ON the file that was changed, back into the queue so that files
        // that depend on it also re-analyze in case they need rebuilding.  However, files that are deleted will be added
        // in CheckDeletedSourceFile instead, so there's no reason in that case to do that here.
        if ((!source.m_isFromScanner)&&(!source.m_isDelete))
        {
            // since the scanner walks over EVERY file, there's no reason to process dependencies during scan but it is necessary to process deletes.
            QStringList absoluteSourcePathList = GetSourceFilesWhichDependOnSourceFile(normalizedFilePath);

            for (const QString& absolutePath : absoluteSourcePathList)
            {
                // we need to check if its already in the "active files" (things that we are looking over)
                // or if its in the "currently being examined" list.  The latter is likely to be the smaller list, 
                // so we check it first.  Both of those are absolute paths, so we convert to absolute path before
                // searching those lists:
                if (m_filesToExamine.find(absolutePath) != m_filesToExamine.end())
                {
                    // its already in the file to examine queue.
                    continue;
                }
                if (m_alreadyActiveFiles.find(absolutePath) != m_alreadyActiveFiles.end())
                {
                    // its already been picked up by a file monitoring / scanning step.
                    continue;
                }

                if (absolutePath.startsWith(m_placeHolderFileName))
                {
                    // its a missing file, so don't add it to the queue.
                    continue;
                }

                AssessFileInternal(absolutePath, false);
            }
        }

        m_AssetProcessorIsBusy = true;

        if (!m_queuedExamination)
        {
            m_queuedExamination = true;
            QTimer::singleShot(0, this, SLOT(ProcessFilesToExamineQueue()));
            Q_EMIT NumRemainingJobsChanged(m_activeFiles.size() + m_filesToExamine.size() + m_numOfJobsToAnalyze);
        }
    }

    void AssetProcessorManager::CheckDeletedProductFile(QString fullProductFile)
    {
        // this might be interesting, but only if its a known product!
        // the dictionary in statedata stores only the relative path, not the platform.
        // which means right now we have, for example
        // d:/game/root/Cache/SamplesProject/IOS/SamplesProject/textures/favorite.tga
        // ^^^^^^^^^^^^  engine root
        // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^ cache root
        // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ platform root
        {
            QMutexLocker locker(&m_processingJobMutex);
            auto found = m_processingProductInfoList.find(fullProductFile.toUtf8().constData());
            if (found != m_processingProductInfoList.end())
            {
                // if we get here because we just deleted a product file before we copy/move the new product file
                // than its totally safe to ignore this deletion.
                return;
            }
        }
        if (QFile::exists(fullProductFile))
        {
            // this is actually okay - it may have been temporarily deleted because it was in the process of being compiled.
            return;
        }

        //remove the cache root from the cached product path
        QString relativeProductFile = m_cacheRootDir.relativeFilePath(fullProductFile);

        //platform
        QString platform = relativeProductFile;// currently <platform>/<gamename>/<relative_asset_path>
        platform = platform.left(platform.indexOf('/')); // also consume the extra slash - remove PLATFORM

        //we are going to force the processor to re process the source file associated with this product
        //we do that by setting the fingerprint to some other value than which will be recomputed
        //we only want to notify any listeners that the product file was removed for this particular product
        AzToolsFramework::AssetDatabase::SourceDatabaseEntryContainer sources;
        if (!m_stateData->GetSourcesByProductName(relativeProductFile, sources))
        {
            return;
        }
        AzToolsFramework::AssetDatabase::JobDatabaseEntryContainer jobs;
        if (!m_stateData->GetJobsByProductName(relativeProductFile, jobs, AZ::Uuid::CreateNull(), QString(), platform))
        {
            return;
        }
        AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer products;
        if (!m_stateData->GetProductsByProductName(relativeProductFile, products, AZ::Uuid::CreateNull(), QString(), platform))
        {
            return;
        }

        // pretend that its source changed.  Add it to the things to keep watching so that in case MORE
        // products change. We don't start processing until all have been deleted
        for (auto& source : sources)
        {
            //we should only have one source
            AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry scanfolder;
            if (m_stateData->GetScanFolderByScanFolderID(source.m_scanFolderPK, scanfolder))
            {
                // there's one more thing to account for here, and thats the fact that the sourceName may have an outputPrefix appended on to it
                // so for example, the scan folder might be c:/ly/dev/Gems/Clouds/Assets
                // but the outputPrefix might be Clouds/Assets, meaning "put it in that folder in the cache instead of just at the root"
                // When we have an outputprefix, we prepend it to SourceName so that its a unique source
                // so for example, the sourceName might be Clouds/Assets/blah.tif
                // if you were to blindly concatenate them you'd end up with c:/ly/dev/Gems/Clouds/Assets/Clouds/Assets/blah.tif
                //                                                           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                //                                                              The watch folder is here
                //                                                                                       ^^^^^^^^^^^^^^
                //                                                                                        Prefix prepended
                //                                                                                                      ^^^^^^^^
                //                                                                                                      actual name
                // so remove the output prefix from sourcename if present before doing any source ops on it.

                AZStd::string sourceName(source.m_sourceName);

                if (!scanfolder.m_outputPrefix.empty())
                {
                    sourceName = sourceName.substr(scanfolder.m_outputPrefix.size() + 1);
                }
                AZStd::string fullSourcePath = AZStd::string::format("%s/%s", scanfolder.m_scanFolder.c_str(), sourceName.c_str());

                AssessFileInternal(fullSourcePath.c_str(), false);
            }
        }

        // currently <platform>/<gamename>/<relative_asset_path>
        // remove PLATFORM and GAMENAME so that we only have the relative asset path which should match the db
        QString relativePath(relativeProductFile);
        relativePath = relativePath.right(relativePath.length() - relativePath.indexOf('/') - 1); // also consume the extra slash - remove PLATFORM
        relativePath = relativePath.right(relativePath.length() - relativePath.indexOf('/') - 1); // also consume the extra slash - remove GAMENAME

        //set the fingerprint on the job that made this product
        for (auto& job : jobs)
        {
            for (auto& product : products)
            {
                if (job.m_jobID == product.m_jobPK)
                {
                    //set failed fingerprint
                    job.m_fingerprint = FAILED_FINGERPRINT;

                    // clear it and then queue reprocess on its parent:
                    m_stateData->SetJob(job);

                    // note that over here, we do not notify connected clients that their product has vanished
                    // this is because we have a record of its source file, and it is in the queue for processing.
                    // Even if the source has disappeared too, that will simply result in the rest of the code
                    // dealing with this issue later when it figures that out.
                    // If the source file is reprocessed and no longer outputs this product, the "AssetProcessed_impl" function will handle notifying
                    // of actually removed products.
                    // If the source file is gone, that will notify for the products right there and then.
                }
            }
        }
    }

    bool AssetProcessorManager::DeleteProducts(const AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer& products)
    {
        bool successfullyRemoved = true;
        // delete the products.
        // products have names like "pc/SamplesProject/textures/blah.dds" and do include platform roots!
        // this means the actual full path is something like
        // [cache root] / [platform] / [product name]
        for (const auto& product : products)
        {
            //get the source for this product
            AzToolsFramework::AssetDatabase::SourceDatabaseEntry source;
            if (!m_stateData->GetSourceByProductID(product.m_productID, source))
            {
                AZ_Error(AssetProcessor::ConsoleChannel, false, "Source for Product %s not found!!!", product.m_productName.c_str());
            }

            QString fullProductPath = m_cacheRootDir.absoluteFilePath(product.m_productName.c_str());
            QString relativeProductPath(product.m_productName.c_str());
            relativeProductPath = relativeProductPath.right(relativeProductPath.length() - relativeProductPath.indexOf('/') - 1); // also consume the extra slash - remove PLATFORM
            relativeProductPath = relativeProductPath.right(relativeProductPath.length() - relativeProductPath.indexOf('/') - 1); // also consume the extra slash - remove GAMENAME

            if (QFile::exists(fullProductPath))
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Deleting file %s because either its source file %s was removed or the builder did not emit this job.\n", fullProductPath.toUtf8().constData(), source.m_sourceName.c_str());

                successfullyRemoved &= QFile::remove(fullProductPath);

                if (successfullyRemoved)
                {
                    AzToolsFramework::AssetDatabase::JobDatabaseEntry job;
                    if (!m_stateData->GetJobByProductID(product.m_productID, job))
                    {
                        AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to find job for Product %s!!!", product.m_productName.c_str());
                    }

                    if (!m_stateData->RemoveProduct(product.m_productID))
                    {
                        AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to remove Product %s!!!", product.m_productName.c_str());
                    }

                    AZ::Data::AssetId assetId(source.m_sourceGuid, product.m_subID);
                    AZ::Data::AssetId legacyAssetId(product.m_legacyGuid, 0);
                    AZ::Data::AssetId legacySourceAssetId(AssetUtilities::CreateSafeSourceUUIDFromName(source.m_sourceName.c_str(), false), product.m_subID);

                    AssetNotificationMessage message(relativeProductPath.toUtf8().constData(), AssetNotificationMessage::AssetRemoved, product.m_assetType);
                    message.m_assetId = assetId;

                    if (legacyAssetId != assetId)
                    {
                        message.m_legacyAssetIds.push_back(legacyAssetId);
                    }

                    if (legacySourceAssetId != assetId)
                    {
                        message.m_legacyAssetIds.push_back(legacySourceAssetId);
                    }
                    Q_EMIT AssetMessage(job.m_platform.c_str(), message);

                    QString parentFolderName = QFileInfo(fullProductPath).absolutePath();
                    m_checkFoldersToRemove.insert(parentFolderName);
                }
            }
            else
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "An expected product %s was not present.\n", fullProductPath.toUtf8().constData());
            }
        }

        return successfullyRemoved;
    }

    void AssetProcessorManager::CheckDeletedSourceFile(QString normalizedPath, QString relativePath, QString databaseSourceFile)
    {
        // getting here means an input asset has been deleted
        // and no overrides exist for it.
        // we must delete its products.
        using namespace AzToolsFramework::AssetDatabase;

        // Check if this file causes any file types to be re-evaluated
        CheckMetaDataRealFiles(normalizedPath);

        // when a source is deleted, we also have to queue anything that depended on it, for re-processing:        
        SourceFileDependencyEntryContainer results;
        m_stateData->GetSourceFileDependenciesByDependsOnSource(databaseSourceFile, SourceFileDependencyEntry::DEP_Any, results);
        // the jobIdentifiers that have identified it as a job dependency
        for (SourceFileDependencyEntry& existingEntry : results)
        {
            // this row is [Source] --> [Depends on Source].
            QString absolutePath = m_platformConfig->FindFirstMatchingFile(QString::fromUtf8(existingEntry.m_source.c_str()));
            if (!absolutePath.isEmpty())
            {
                AssessFileInternal(absolutePath, false);
            }
            // also, update it in the database to be missing, ie, add the "missing file" prefix:
            existingEntry.m_dependsOnSource = QString(m_placeHolderFileName + relativePath).toUtf8().constData();
            m_stateData->RemoveSourceFileDependency(existingEntry.m_sourceDependencyID);
            m_stateData->SetSourceFileDependency(existingEntry);
        }

        // now that the right hand column (in terms of [thing] -> [depends on thing]) has been updated, eliminate anywhere its on the left hand side:
        results.clear();
        m_stateData->GetDependsOnSourceBySource(databaseSourceFile.toUtf8().constData(), SourceFileDependencyEntry::DEP_Any, results);
        m_stateData->RemoveSourceFileDependencies(results);

        AzToolsFramework::AssetDatabase::SourceDatabaseEntryContainer sources;
        if (m_stateData->GetSourcesBySourceName(databaseSourceFile, sources))
        {
            for (const auto& source : sources)
            {
                AzToolsFramework::AssetSystem::JobInfo jobInfo;
                jobInfo.m_sourceFile = databaseSourceFile.toUtf8().constData();

                AzToolsFramework::AssetDatabase::JobDatabaseEntryContainer jobs;
                if (m_stateData->GetJobsBySourceID(source.m_sourceID, jobs))
                {
                    for (auto& job : jobs)
                    {
                        // ToDo:  Add BuilderUuid here once we do the JobKey feature.
                        AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer products;
                        if (m_stateData->GetProductsByJobID(job.m_jobID, products))
                        {
                            if (!DeleteProducts(products))
                            {
                                // try again in a while.  Achieve this by recycling the item back into
                                // the queue as if it had been deleted again.
                                CheckSource(FileEntry(normalizedPath, true));
                                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Delete failed on %s. Will retry!. \n", normalizedPath.toUtf8().constData());
                            }
                        }
                        else
                        {
                            // even with no products, still need to clear the fingerprint:
                            job.m_fingerprint = FAILED_FINGERPRINT;
                            m_stateData->SetJob(job);
                        }

                        // notify the GUI to remove any failed jobs that are currently onscreen:
                        jobInfo.m_platform = job.m_platform;
                        jobInfo.m_jobKey = job.m_jobKey;
                        Q_EMIT JobRemoved(jobInfo);
                    }
                }
                // delete the source from the database too since otherwise it believes we have no products.
                m_stateData->RemoveSource(source.m_sourceID);
            }
        }

        Q_EMIT SourceDeleted(databaseSourceFile);  // note that this removes it from the RC Queue Model, also
    }

    void AssetProcessorManager::AddKnownFoldersRecursivelyForFile(QString fullFile, QString root)
    {
        QString normalizedRoot = AssetUtilities::NormalizeFilePath(root);

        // also track parent folders up to the specified root.
        QString parentFolderName = QFileInfo(fullFile).absolutePath();
        QString normalizedParentFolder = AssetUtilities::NormalizeFilePath(parentFolderName);

        if (!normalizedParentFolder.startsWith(normalizedRoot, Qt::CaseInsensitive))
        {
            return; // not interested in folders not in the root.
        }

        while (normalizedParentFolder.compare(normalizedRoot, Qt::CaseInsensitive) != 0)
        {
            // QSet does not actually have a function that tells us if the set already contained as well as inserts it
            // (unlike std::set and others) but an easy way to tell in o(1) is to just check if the size changed
            int priorSize = m_knownFolders.size();
            m_knownFolders.insert(normalizedParentFolder);
            if (m_knownFolders.size() == priorSize)
            {
                // this folder was already there, and thus there's no point in further recursion because
                // it would have already recursed the first time around.
                break;
            }

            int pos = normalizedParentFolder.lastIndexOf(QChar('/'));
            if (pos >= 0)
            {
                normalizedParentFolder = normalizedParentFolder.left(pos);
            }
            else
            {
                break; // no more slashes
            }
        }
    }

    void AssetProcessorManager::CheckMissingJobs(QString databasePathToFile, const ScanFolderInfo* scanFolder, const AZStd::vector<JobDetails>& jobsThisTime)
    {
        // Check to see if jobs were emitted last time by this builder, but are no longer being emitted this time - in which case we must eliminate old products.
        // whats going to be in the database is fingerprints for each job last time
        // this function is called once per source file, so in the array of jobsThisTime,
        // the relative path will always be the same.

        if ((databasePathToFile.length() == 0) && (jobsThisTime.empty()))
        {
            return;
        }

        // find all jobs from the last time of the platforms that are currently enabled
        JobInfoContainer jobsFromLastTime;
        for (const AssetBuilderSDK::PlatformInfo& platformInfo : scanFolder->GetPlatforms())
        {
            QString platform = QString::fromUtf8(platformInfo.m_identifier.c_str());
            m_stateData->GetJobInfoBySourceName(databasePathToFile.toUtf8().constData(), jobsFromLastTime, AZ::Uuid::CreateNull(), QString(), platform);
        }


        // so now we have jobsFromLastTime and jobsThisTime.  Whats in last time that is no longer being emitted now?
        if (jobsFromLastTime.empty())
        {
            return;
        }

        for (int oldJobIdx = azlossy_cast<int>(jobsFromLastTime.size()) - 1; oldJobIdx >= 0; --oldJobIdx)
        {
            const JobInfo& oldJobInfo = jobsFromLastTime[oldJobIdx];
            // did we find it this time?
            bool foundIt = false;
            for (const JobDetails& newJobInfo : jobsThisTime)
            {
                // the relative path is insensitive because some legacy data didn't have the correct case.
                if ((newJobInfo.m_jobEntry.m_builderGuid == oldJobInfo.m_builderGuid) &&
                    (QString::compare(newJobInfo.m_jobEntry.m_platformInfo.m_identifier.c_str(), oldJobInfo.m_platform.c_str()) == 0) &&
                    (QString::compare(newJobInfo.m_jobEntry.m_jobKey, oldJobInfo.m_jobKey.c_str()) == 0) &&
                    (QString::compare(newJobInfo.m_jobEntry.m_databaseSourceName, oldJobInfo.m_sourceFile.c_str(), Qt::CaseInsensitive) == 0)
                    )
                {
                    foundIt = true;
                    break;
                }
            }

            if (foundIt)
            {
                jobsFromLastTime.erase(jobsFromLastTime.begin() + oldJobIdx);
            }
        }

        // at this point, we contain only the jobs that are left over from last time and not found this time.
        //we want to remove all products for these jobs and the jobs
        for (const JobInfo& oldJobInfo : jobsFromLastTime)
        {
            // ToDo:  Add BuilderUuid here once we do the JobKey feature.
            AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer products;
            if (m_stateData->GetProductsBySourceName(databasePathToFile, products, oldJobInfo.m_builderGuid, oldJobInfo.m_jobKey.c_str(), oldJobInfo.m_platform.c_str()))
            {
                char tempBuffer[128];
                oldJobInfo.m_builderGuid.ToString(tempBuffer, AZ_ARRAY_SIZE(tempBuffer));

                AZ_TracePrintf(DebugChannel, "Removing products for job (%s, %s, %s, %s, %s) since it is no longer being emitted by its builder.\n",
                    oldJobInfo.m_sourceFile.c_str(),
                    oldJobInfo.m_platform.c_str(),
                    oldJobInfo.m_jobKey.c_str(),
                    oldJobInfo.m_builderGuid.ToString<AZStd::string>().c_str(),
                    tempBuffer);

                //delete products, which should remove them from the disk and database and send the notifications
                DeleteProducts(products);
            }

            //remove the jobs associated with these products
            m_stateData->RemoveJob(oldJobInfo.m_jobID);

            // note that JobRemoved is supposed to emit a jobinfo that is not output-prefixed
            if (!scanFolder->GetOutputPrefix().isEmpty())
            {
                JobInfo newJobInfo(oldJobInfo);
                QString sourcePathWithPrefix = QString::fromUtf8(oldJobInfo.m_sourceFile.c_str());
                newJobInfo.m_sourceFile = (sourcePathWithPrefix.right(sourcePathWithPrefix.length() - (scanFolder->GetOutputPrefix().length() + 1))).toUtf8().constData();
                Q_EMIT JobRemoved(newJobInfo);
            }
            else
            {
                Q_EMIT JobRemoved(oldJobInfo);
            }
        }
    }

    // clean all folders that are empty until you get to the root, or until you get to one that isn't empty.
    void AssetProcessorManager::CleanEmptyFolder(QString folder, QString root)
    {
        QString normalizedRoot = AssetUtilities::NormalizeFilePath(root);

        // also track parent folders up to the specified root.
        QString normalizedParentFolder = AssetUtilities::NormalizeFilePath(folder);
        QDir parentDir(folder);

        // keep walking up the tree until we either run out of folders or hit the root.
        while ((normalizedParentFolder.compare(normalizedRoot, Qt::CaseInsensitive) != 0) && (parentDir.exists()))
        {
            if (parentDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).empty())
            {
                if (!parentDir.rmdir(normalizedParentFolder))
                {
                    break; // if we fail to remove for any reason we don't push our luck.
                }
            }
            if (!parentDir.cdUp())
            {
                break;
            }
            normalizedParentFolder = AssetUtilities::NormalizeFilePath(parentDir.absolutePath());
        }
    }

    void AssetProcessorManager::CheckModifiedSourceFile(QString normalizedPath, QString databaseSourceFile, bool fromScanner, const ScanFolderInfo* scanFolder)
    {
        // a potential input file was modified or added.  We always pass these through our filters and potentially build it.
        // before we know what to do, we need to figure out if it matches some filter we care about.

        // note that if we get here during runtime, we've already eliminated overrides
        // so this is the actual file of importance.

        // check regexes.
        // get list of recognizers which match
        // for each platform in the recognizer:
        //    check the fingerprint and queue if appropriate!
        //    also queue if products missing.

        // Check if this file causes any file types to be re-evaluated
        CheckMetaDataRealFiles(normalizedPath);

        // keep track of its parent folders so that if a folder disappears or is renamed, and we get the notification that this has occurred
        // we will know that it *was* a folder before now (otherwise we'd have no idea)
        AddKnownFoldersRecursivelyForFile(normalizedPath, scanFolder->ScanPath());

        ++m_numTotalSourcesFound;
        if ((fromScanner) && (m_bAllowAnalysisSkippingFeature))
        {
            // if its from the scanner (ie, not the real time file monitor) its worth checking if we can just skip:
            if (CanEarlyOutSourceFile(normalizedPath, databaseSourceFile, scanFolder))
            {
                // no reason to go any further!
                return;
            }
        }

        AssetProcessor::BuilderInfoList builderInfoList;
        EBUS_EVENT(AssetProcessor::AssetBuilderInfoBus, GetMatchingBuildersInfo, normalizedPath.toUtf8().constData(), builderInfoList);

        if (builderInfoList.size())
        {
            ++m_numSourcesNeedingFullAnalysis;
            ProcessBuilders(normalizedPath, databaseSourceFile, scanFolder, builderInfoList);
        }
        else
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Non-processed file: %s\n", databaseSourceFile.toUtf8().constData());
            ++m_numSourcesNotHandledByAnyBuilder;
            // we could cache the source here so that its quick-skipped next time, but it would only be skipping the "getMatchingbuildersInfo" call
        }
    }

    bool AssetProcessorManager::AnalyzeJob(JobDetails& jobDetails)
    {
        // This function checks to see whether we need to process an asset or not, it returns true if we need to process it and false otherwise
        // It processes an asset if either there is a fingerprint mismatch between the computed and the last known fingerprint or if products are missing
        bool shouldProcessAsset = false;

        // First thing it checks is the computed fingerprint with its last known fingerprint in the database, if there is a mismatch than we need to process it
        AzToolsFramework::AssetDatabase::JobDatabaseEntryContainer jobs; //should only find one when we specify builder, job key, platform
        bool foundInDatabase = m_stateData->GetJobsBySourceName(jobDetails.m_jobEntry.m_databaseSourceName, jobs, jobDetails.m_jobEntry.m_builderGuid, jobDetails.m_jobEntry.m_jobKey, jobDetails.m_jobEntry.m_platformInfo.m_identifier.c_str());

        if (foundInDatabase && jobs[0].m_fingerprint == jobDetails.m_jobEntry.m_computedFingerprint)
        {
            // If the fingerprint hasn't changed, we won't process it.. unless...is it missing a product.
            AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer products;
            if (m_stateData->GetProductsBySourceName(jobDetails.m_jobEntry.m_databaseSourceName, products, jobDetails.m_jobEntry.m_builderGuid, jobDetails.m_jobEntry.m_jobKey, jobDetails.m_jobEntry.m_platformInfo.m_identifier.c_str()))
            {
                for (const auto& product : products)
                {
                    QString fullProductPath = m_cacheRootDir.absoluteFilePath(product.m_productName.c_str());
                    if (!QFile::exists(fullProductPath))
                    {
                        AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckModifiedInputAsset: (%s) is missing a product (%s) on %s\n", jobDetails.m_jobEntry.m_pathRelativeToWatchFolder.toUtf8().constData(), product.m_productName.c_str(), jobDetails.m_jobEntry.m_platformInfo.m_identifier.c_str());
                        shouldProcessAsset = true;
                    }
                    else
                    {
                        QString absoluteCacheRoot = m_cacheRootDir.absolutePath();
                        AddKnownFoldersRecursivelyForFile(fullProductPath, absoluteCacheRoot);
                    }
                }
            }
        }
        else
        {
            // The fingerprint for this job does not match last time the job was processed.
            // Thus, we need to queue a job to process it
            // If we are in this block of code, it means one of two things: either we didn't find it at all, or it doesn't match.
            // For debugging, it is useful to be able to tell those two code paths apart, so make output a message which cna differentiate.
            AZ_TracePrintf(AssetProcessor::DebugChannel, "AnalyzeJob: %s for source '%s' builder '%s' platform '%s' extra info '%s' job key '%s'\n",
                foundInDatabase ? "fingerprint mismatch" : "new job",
                jobDetails.m_jobEntry.m_databaseSourceName.toUtf8().constData(),
                jobDetails.m_assetBuilderDesc.m_name.c_str(),
                jobDetails.m_jobEntry.m_platformInfo.m_identifier.c_str(),
                jobDetails.m_extraInformationForFingerprinting.c_str(),
                jobDetails.m_jobEntry.m_jobKey.toUtf8().constData());

            // Check whether another job emitted this job as a job dependency and if true, queue the dependent job source file also
            JobDesc jobDesc(jobDetails.m_jobEntry.m_databaseSourceName.toUtf8().data(),
                jobDetails.m_jobEntry.m_jobKey.toUtf8().data(), jobDetails.m_jobEntry.m_platformInfo.m_identifier);
                       
            shouldProcessAsset = true;
            QFileInfo file(jobDetails.m_jobEntry.GetAbsoluteSourcePath());
            QDateTime dateTime(file.lastModified());
            qint64 mSecsSinceEpoch = dateTime.toMSecsSinceEpoch();
            auto foundSource = m_sourceFileModTimeMap.find(jobDetails.m_jobEntry.m_sourceFileUUID);

            if (foundSource == m_sourceFileModTimeMap.end() || foundSource->second != mSecsSinceEpoch)
            {
                // send a sourceFile notification message only if its last modified time changed or
                // we have not seen this source file before
                m_sourceFileModTimeMap[jobDetails.m_jobEntry.m_sourceFileUUID] = mSecsSinceEpoch;
                QString sourceFile(jobDetails.m_jobEntry.m_pathRelativeToWatchFolder);
                AZ::Uuid sourceUUID = AssetUtilities::CreateSafeSourceUUIDFromName(jobDetails.m_jobEntry.m_databaseSourceName.toUtf8().data());
                AzToolsFramework::AssetSystem::SourceFileNotificationMessage message(AZ::OSString(sourceFile.toUtf8().constData()), AZ::OSString(jobDetails.m_scanFolder->ScanPath().toUtf8().constData()), AzToolsFramework::AssetSystem::SourceFileNotificationMessage::FileChanged, sourceUUID);
                EBUS_EVENT(AssetProcessor::ConnectionBus, Send, 0, message);
            }
        }

        if (!shouldProcessAsset)
        {
            //AZ_TracePrintf(AssetProcessor::DebugChannel, "AnalyzeJob: UpToDate: not processing on %s.\n", jobDetails.m_jobEntry.m_platform.toUtf8().constData());
            return false;
        }
        else
        {
            // macOS requires that the cacheRootDir to not be all lowercase, otherwise file copies will not work correctly.
            // So use the lowerCasePath string to capture the parts that need to be lower case while keeping the cache root
            // mixed case.
            QString lowerCasePath = jobDetails.m_jobEntry.m_platformInfo.m_identifier.c_str();

            // this may seem odd, but m_databaseSourceName includes the output prefix up front, and we're trying to find where to put it in the cache
            // so we use the databaseSourceName instead of relpath.
            QString pathRel = QString("/") + QFileInfo(jobDetails.m_jobEntry.m_databaseSourceName).path();

            if (pathRel == "/.")
            {
                // if its in the current folder, avoid using ./ or /.
                pathRel = QString();
            }

            if (jobDetails.m_scanFolder->IsRoot())
            {
                // stuff which is found in the root continues to go to the root, rather than GAMENAME folder...
                lowerCasePath += pathRel;
            }
            else
            {
                lowerCasePath += "/" + AssetUtilities::ComputeGameName() + pathRel;
            }

            lowerCasePath = lowerCasePath.toLower();
            jobDetails.m_destinationPath = m_cacheRootDir.absoluteFilePath(lowerCasePath);
        }

        return true;
    }

    void AssetProcessorManager::CheckDeletedCacheFolder(QString normalizedPath)
    {
        QDir checkDir(normalizedPath);
        if (checkDir.exists())
        {
            // this is possible because it could have been moved back by the time we get here, in which case, we take no action.
            return;
        }

        // going to need to iterate on all files there, recursively, in order to emit them as having been deleted.
        // note that we don't scan here.  We use the asset database.
        QString cacheRootRemoved = m_cacheRootDir.relativeFilePath(normalizedPath);

        AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer products;
        m_stateData->GetProductsLikeProductName(cacheRootRemoved, AzToolsFramework::AssetDatabase::AssetDatabaseConnection::StartsWith, products);

        for (const auto& product : products)
        {
            QString fileFound = m_cacheRootDir.absoluteFilePath(product.m_productName.c_str());
            if (!QFile::exists(fileFound))
            {
                AssessDeletedFile(fileFound);
            }
        }

        m_knownFolders.remove(normalizedPath);
    }

    void AssetProcessorManager::CheckDeletedSourceFolder(QString normalizedPath, QString relativePath, const ScanFolderInfo* scanFolderInfo)
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckDeletedSourceFolder...\n");
        // we deleted a folder that is somewhere that is a watched input folder.

        QDir checkDir(normalizedPath);
        if (checkDir.exists())
        {
            // this is possible because it could have been moved back by the time we get here, in which case, we take no action.
            return;
        }

        AzToolsFramework::AssetDatabase::SourceDatabaseEntryContainer sources;
        QString sourceName = scanFolderInfo->GetOutputPrefix().isEmpty() ? relativePath : QDir::cleanPath(scanFolderInfo->GetOutputPrefix() + QDir::separator() + relativePath);
        m_stateData->GetSourcesLikeSourceName(sourceName, AzToolsFramework::AssetDatabase::AssetDatabaseConnection::StartsWith, sources);

        AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckDeletedSourceFolder: %i matching files.\n", sources.size());

        QDir scanFolder(scanFolderInfo->ScanPath());
        for (const auto& source : sources)
        {
            // reconstruct full path:
            QString actualRelativePath = source.m_sourceName.c_str();

            if (!scanFolderInfo->GetOutputPrefix().isEmpty())
            {
                actualRelativePath = actualRelativePath.right(actualRelativePath.length() - (scanFolderInfo->GetOutputPrefix().length() + 1)); // adding one for separator
            }

            QString finalPath = scanFolder.absoluteFilePath(actualRelativePath);

            if (!QFile::exists(finalPath))
            {
                AssessDeletedFile(finalPath);
            }
        }

        m_knownFolders.remove(normalizedPath);
    }

    namespace
    {
        void ScanFolderInternal(QString inputFolderPath, QStringList& outputs)
        {
            QDir inputFolder(inputFolderPath);
            QFileInfoList entries = inputFolder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files);

            for (const QFileInfo& entry : entries)
            {
                if (entry.isDir())
                {
                    //Entry is a directory
                    ScanFolderInternal(entry.absoluteFilePath(), outputs);
                }
                else
                {
                    //Entry is a file
                    outputs.push_back(entry.absoluteFilePath());
                }
            }
        }
    }

    void AssetProcessorManager::CheckMetaDataRealFiles(QString relativeSourceFile)
    {
        if (!m_platformConfig->IsMetaDataTypeRealFile(relativeSourceFile))
        {
            return;
        }

        QStringList extensions;
        for (int idx = 0; idx < m_platformConfig->MetaDataFileTypesCount(); idx++)
        {
            const auto& metaExt = m_platformConfig->GetMetaDataFileTypeAt(idx);
            if (!metaExt.second.isEmpty() && QString::compare(metaExt.first, relativeSourceFile, Qt::CaseInsensitive) == 0)
            {
                extensions.push_back(metaExt.second);
            }
        }

        AzToolsFramework::AssetDatabase::SourceDatabaseEntryContainer sources;
        for (const auto& ext : extensions)
        {
            m_stateData->GetSourcesLikeSourceName(ext, AzToolsFramework::AssetDatabase::AssetDatabaseConnection::EndsWith, sources);
        }

        QString fullMatchingSourceFile;
        for (const auto& source : sources)
        {
            fullMatchingSourceFile = m_platformConfig->FindFirstMatchingFile(source.m_sourceName.c_str());
            if (!fullMatchingSourceFile.isEmpty())
            {
                AssessFileInternal(fullMatchingSourceFile, false);
            }
        }
    }

    void AssetProcessorManager::CheckCreatedSourceFolder(QString fullSourceFile)
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "CheckCreatedSourceFolder...\n");
        // this could have happened because its a directory rename
        QDir checkDir(fullSourceFile);
        if (!checkDir.exists())
        {
            // this is possible because it could have been moved back by the time we get here.
            // find all assets that are products that have this as their normalized path and then indicate that they are all deleted.
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Directory (%s) does not exist.\n", fullSourceFile.toUtf8().data());
            return;
        }

        // we actually need to scan this folder, without invoking the whole asset scanner:

        const AssetProcessor::ScanFolderInfo* info = m_platformConfig->GetScanFolderForFile(fullSourceFile);
        if (!info)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "No scan folder found for the directory: (%s).\n", fullSourceFile.toUtf8().data());
            return; // early out, its nothing we care about.
        }

        QStringList files;
        ScanFolderInternal(fullSourceFile, files);

        for (const QString fileEntry : files)
        {
            AssessModifiedFile(fileEntry);
        }
    }

    void AssetProcessorManager::ProcessFilesToExamineQueue()
    {
        // it is assumed that files entering this function are already normalized
        // that is, the path is normalized
        // and only has forward slashes.

        if (!m_platformConfig)
        {
            // this cannot be recovered from
            qFatal("Platform config is missing, we cannot continue.");
            return;
        }

        if ((m_normalizedCacheRootPath.isEmpty()) && (!InitializeCacheRoot()))
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Cannot examine the queue yet - cache root is not ready!\n ");
            m_queuedExamination = true;
            QTimer::singleShot(250, this, SLOT(ProcessFilesToExamineQueue()));
            return;
        }

        if (m_isCurrentlyScanning)
        {
            // if we're currently scanning, then don't start processing yet, its not worth the IO thrashing.
            m_queuedExamination = true;
            QTimer::singleShot(250, this, SLOT(ProcessFilesToExamineQueue()));
            return;
        }

        QString canonicalRootDir = AssetUtilities::NormalizeFilePath(m_cacheRootDir.canonicalPath());
    
        FileExamineContainer swapped;
        m_filesToExamine.swap(swapped); // makes it okay to call CheckSource(...)

        QElapsedTimer elapsedTimer;
        elapsedTimer.start();

        int i = -1; // Starting at -1 so we can increment at the start of the loop instead of the end due to all the control flow that occurs inside the loop
        m_queuedExamination = false;
        for (const FileEntry& examineFile : swapped)
        {
            ++i;

            if (m_quitRequested)
            {
                return;
            }

            // CreateJobs can sometimes take a very long time, update the remaining count occasionally
            if (elapsedTimer.elapsed() >= MILLISECONDS_BETWEEN_CREATE_JOBS_STATUS_UPDATE)
            {
                int remainingInSwapped = swapped.size() - i;
                Q_EMIT NumRemainingJobsChanged(m_activeFiles.size() + remainingInSwapped + m_numOfJobsToAnalyze);
                elapsedTimer.restart();
            }

            // examination occurs here.
            // first, is it a source or is it a product in the cache folder?
            QString normalizedPath = examineFile.m_fileName;

            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessFilesToExamineQueue: %s delete: %s.\n", examineFile.m_fileName.toUtf8().constData(), examineFile.m_isDelete ? "true" : "false");

            // debug-only check to make sure our assumption about normalization is correct.
            Q_ASSERT(normalizedPath == AssetUtilities::NormalizeFilePath(normalizedPath));

            // if its in the cache root then its a product file:
            bool isProductFile = examineFile.m_fileName.startsWith(m_normalizedCacheRootPath, Qt::CaseInsensitive);
#if defined(AZ_PLATFORM_APPLE)
            // a case can occur on apple platforms in the temp folders
            // where there is a symlink and /var/folders/.../ is also known
            // as just /private/var/folders/...
            // this tends to happen for delete notifies and we can't canonicalize incoming delete notifies
            // because the file has already been deleted and thus its canonical path cannot be found.  Instead
            // we will use the canonical path of the cache root dir instead, and then alter the file
            // to have the current cache root dir instead.
            if ((!isProductFile)&&(!canonicalRootDir.isEmpty()))
            {
                // try the canonicalized form:
                isProductFile = examineFile.m_fileName.startsWith(canonicalRootDir);
                if (isProductFile)
                {
                    // found in canonical location, update its normalized path
                    QString withoutCachePath = normalizedPath.mid(canonicalRootDir.length() + 1);
                    // the extra +1 is to consume the slash that is after the root dir.
                    normalizedPath = AssetUtilities::NormalizeFilePath(m_cacheRootDir.absoluteFilePath(withoutCachePath));
                }
            }
#endif // AZ_PLATFORM_APPLE
            // strip the engine off it so that its a "normalized asset path" with appropriate slashes and such:
            if (isProductFile)
            {
                // its a product file.
                if (normalizedPath.length() >= AP_MAX_PATH_LEN)
                {
                    // if we are here it means that we have found a cache file whose filepath is greater than the maximum path length allowed
                    continue;
                }

                // we only care about deleted product files.
                if (examineFile.m_isDelete)
                {
                    if (normalizedPath.endsWith(QString(FENCE_FILE_EXTENSION), Qt::CaseInsensitive))
                    {
                        // its a fence file, now computing fenceId from it:
                        int startPos = normalizedPath.lastIndexOf("~");
                        int endPos = normalizedPath.lastIndexOf(".");
                        QString fenceIdString = normalizedPath.mid(startPos + 1, endPos - startPos - 1);
                        bool isNumber = false;
                        int fenceId = fenceIdString.toInt(&isNumber);
                        if (isNumber)
                        {
                            Q_EMIT FenceFileDetected(fenceId);
                        }
                        else
                        {
                            AZ_TracePrintf(AssetProcessor::DebugChannel, "AssetProcessor: Unable to compute fenceId from fenceFile name %s.\n", normalizedPath.toUtf8().data());
                        }
                        continue;
                    }
                    if (m_knownFolders.contains(normalizedPath))
                    {
                        CheckDeletedCacheFolder(normalizedPath);
                    }
                    else
                    {
                        CheckDeletedProductFile(normalizedPath);
                    }
                }
                else
                {
                    // a file was added or modified to the cache.
                    // we only care about the renames of folders, so cache folders here:
                    QFileInfo fileInfo(normalizedPath);
                    if (!fileInfo.isDir())
                    {
                        // keep track of its containing folder.
                        AddKnownFoldersRecursivelyForFile(normalizedPath, m_cacheRootDir.absolutePath());
                    }
                }
            }
            else
            {
                // its an source file.  check which scan folder it belongs to
                QString scanFolderName;
                QString databasePathToFile;
                QString relativePathToFile;

                // note that "ConvertToRelativePath" does add output prefix to it.
                if (!m_platformConfig->ConvertToRelativePath(normalizedPath, databasePathToFile, scanFolderName))
                {
                    AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessFilesToExamineQueue: Unable to find the relative path.\n");
                    continue;
                }
                
                const ScanFolderInfo* scanFolderInfo = m_platformConfig->GetScanFolderForFile(normalizedPath);

                relativePathToFile = databasePathToFile;
                // remove output prefix if present to generate relative path
                if ((scanFolderInfo)&&(!scanFolderInfo->GetOutputPrefix().isEmpty()))
                {
                    relativePathToFile = relativePathToFile.remove(0, static_cast<int>(scanFolderInfo->GetOutputPrefix().length()) + 1);
                }

                if (normalizedPath.length() >= AP_MAX_PATH_LEN)
                {
                    // if we are here it means that we have found a source file whose filepath is greater than the maximum path length allowed
                    AZ_TracePrintf(AssetProcessor::ConsoleChannel, "ProcessFilesToExamineQueue: %s filepath length %d exceeds the maximum path length (%d) allowed.\n", normalizedPath.toUtf8().constData(), normalizedPath.length(), AP_MAX_PATH_LEN);

                    JobInfoContainer jobInfos;
                    m_stateData->GetJobInfoBySourceName(databasePathToFile, jobInfos);

                    JobDetails job;
                    for (const auto& jobInfo : jobInfos)
                    {
                        const AssetBuilderSDK::PlatformInfo* const platformFromInfo = m_platformConfig->GetPlatformByIdentifier(jobInfo.m_platform.c_str());
                        AZ_Assert(platformFromInfo, "Error - somehow a job was created which was for a platform not in config.");
                        if (platformFromInfo)
                        {
                            job.m_jobEntry = JobEntry(
                                QString::fromUtf8(jobInfo.m_watchFolder.c_str()),
                                relativePathToFile,
                                databasePathToFile, // with outputprefix 
                                jobInfo.m_builderGuid, 
                                *platformFromInfo, 
                                jobInfo.m_jobKey.c_str(), 0, GenerateNewJobRunKey(), 
                                AZ::Uuid::CreateNull());

                            job.m_autoFail = true;
                            job.m_jobParam[AZ_CRC(AutoFailReasonKey)] = AZStd::string::format("Product file name would be too long: %s\n", normalizedPath.toUtf8().data());

                            UpdateAnalysisTrackerForFile(normalizedPath.toUtf8().constData(), AnalysisTrackerUpdateType::JobFailed);
                            Q_EMIT AssetToProcess(job);// forwarding this job to rccontroller to fail it
                        }
                    }

                    continue;
                }

                if (examineFile.m_isDelete)
                {
                    // if its a delete for a known folder, we handle it differently.
                    if (m_knownFolders.contains(normalizedPath))
                    {
                        CheckDeletedSourceFolder(normalizedPath, relativePathToFile, scanFolderInfo);
                        continue;
                    }
                }
                else
                {
                    // if we get here, we're either in a modify or add situation
                    QFileInfo fileInfo(normalizedPath);
                    if (!fileInfo.isDir())
                    {
                        if (!fileInfo.exists())
                        {
                            // it got deleted before we got to analyze it, we can ignore this.
                            continue;
                        }
                        // keep track of its parent folder so that if it is deleted later we know it is a folder
                        // delete and not a file delete.
                        m_knownFolders.insert(AssetUtilities::NormalizeFilePath(fileInfo.absolutePath()));
                    }
                    else
                    {
                        // if its a folder that was added or modified, we need to keep track of that too.
                        AddKnownFoldersRecursivelyForFile(normalizedPath, scanFolderName);
                        // we actually need to scan this folder now...
                        CheckCreatedSourceFolder(normalizedPath);
                        continue;
                    }
                }

                // is it being overridden by a higher priority file?
                QString overrider;
                if (examineFile.m_isDelete)
                {
                    // if we delete it, check if its revealed by an underlying file:
                    overrider = m_platformConfig->FindFirstMatchingFile(databasePathToFile);

                    if (!overrider.isEmpty()) // override found!
                    {
                        if (overrider.compare(normalizedPath, Qt::CaseInsensitive) == 0)
                        {
                            // if the overrider is the same file, it means that a file was deleted, then reappeared.
                            // if that happened there will be a message in the notification queue for that file reappearing, there
                            // is no need to add a double here.
                            overrider.clear();
                        }
                        else
                        {
                            // on the other hand, if we found a file it means that a deleted file revealed a file that
                            // was previously overridden by it.
                            // Because the deleted file may have "revealed" a file with different case, 
                            // we have to actually correct its case here.  This is rare, so it should be reasonable
                            // to call the expensive function to discover correct case.
                            QString pathRelativeToScanFolder;
                            QString scanFolderPath;
                            m_platformConfig->ConvertToRelativePath(overrider, pathRelativeToScanFolder, scanFolderPath, false /*include output prefix*/);
                            AssetUtilities::UpdateToCorrectCase(scanFolderPath, pathRelativeToScanFolder);
                            overrider = QDir(scanFolderPath).absoluteFilePath(pathRelativeToScanFolder);
                        }
                    }
                }
                else
                {
                    overrider = m_platformConfig->GetOverridingFile(databasePathToFile, scanFolderName);
                }

                if (!overrider.isEmpty())
                {
                    // this file is being overridden by an earlier file.
                    // ignore us, and pretend the other file changed:
                    AZ_TracePrintf(AssetProcessor::DebugChannel, "File overridden by %s.\n", overrider.toUtf8().constData());
                    CheckSource(FileEntry(overrider, false, examineFile.m_isFromScanner));
                    continue;
                }

                // its an input file or a file we don't care about...
                // note that if the file now exists, we have to treat it as an input asset even if it came in as a delete.
                if (examineFile.m_isDelete && !QFile::exists(examineFile.m_fileName))
                {
                    AZ_TracePrintf(AssetProcessor::DebugChannel, "Input was deleted and no overrider was found.\n");
                    const AssetProcessor::ScanFolderInfo* scanFolderInfo = m_platformConfig->GetScanFolderForFile(normalizedPath);
                    QString sourceFile(relativePathToFile);
                    AZ::Uuid sourceUUID = AssetUtilities::CreateSafeSourceUUIDFromName(databasePathToFile.toUtf8().data());
                    AzToolsFramework::AssetSystem::SourceFileNotificationMessage message(AZ::OSString(sourceFile.toUtf8().constData()), AZ::OSString(scanFolderInfo->ScanPath().toUtf8().constData()), AzToolsFramework::AssetSystem::SourceFileNotificationMessage::FileRemoved, sourceUUID);
                    EBUS_EVENT(AssetProcessor::ConnectionBus, Send, 0, message);
                    CheckDeletedSourceFile(normalizedPath, relativePathToFile, databasePathToFile);
                }
                else
                {
                    // log-spam-reduction - the lack of the prior tag (input was deleted) which is rare can infer that the above branch was taken
                    //AZ_TracePrintf(AssetProcessor::DebugChannel, "Input is modified or is overriding something.\n");
                    CheckModifiedSourceFile(normalizedPath, databasePathToFile, examineFile.m_isFromScanner, scanFolderInfo);
                }
            }
        }

        // instead of checking here, we place a message at the end of the queue.
        // this is because there may be additional scan or other results waiting in the queue
        // an example would be where the scanner found additional "copy" jobs waiting in the queue for finalization
        QueueIdleCheck();
    }

    void AssetProcessorManager::CheckForIdle()
    {
        m_alreadyQueuedCheckForIdle = false;
        if (IsIdle())
        {
            if (!m_hasProcessedCriticalAssets)
            {
                // only once, when we finish startup
                m_stateData->VacuumAndAnalyze();
                m_hasProcessedCriticalAssets = true;
            }

            if (!m_quitRequested && m_AssetProcessorIsBusy)
            {
                m_AssetProcessorIsBusy = false;
                Q_EMIT NumRemainingJobsChanged(m_activeFiles.size() + m_filesToExamine.size() + m_numOfJobsToAnalyze);
                Q_EMIT AssetProcessorManagerIdleState(true);
            }

            if (!m_reportedAnalysisMetrics)
            {
                // report these metrics only once per session.
                m_reportedAnalysisMetrics = true;
                AZ_TracePrintf(ConsoleChannel, "Builder optimization: %i / %i files required full analysis, %i sources found but not processed by anyone\n", m_numSourcesNeedingFullAnalysis, m_numTotalSourcesFound, m_numSourcesNotHandledByAnyBuilder);
            }

            QTimer::singleShot(20, this, SLOT(RemoveEmptyFolders()));
        }
        else
        {
            m_AssetProcessorIsBusy = true;
            Q_EMIT AssetProcessorManagerIdleState(false);

            // amount of jobs to evaluate right now (no deferred jobs)
            int numWorkRemainingNow = m_activeFiles.size() + m_filesToExamine.size();
            // total (GUI Shown) of work remaining (including jobs to do later)
            int numTotalWorkRemaining = numWorkRemainingNow + m_numOfJobsToAnalyze;
            Q_EMIT NumRemainingJobsChanged(numTotalWorkRemaining);

            // wake up if there's work to do and we haven't scheduled to do it.
            if ((!m_alreadyScheduledUpdate) && (numWorkRemainingNow > 0))
            {
                // schedule additional updates
                m_alreadyScheduledUpdate = true;
                QTimer::singleShot(1, this, SLOT(ScheduleNextUpdate()));
            }
            else if (numWorkRemainingNow == 0)  // if there are only jobs to process later remaining
            {
                // Process job entries and add jobs to process
                for (JobToProcessEntry& entry : m_jobEntries)
                {
                    if (entry.m_jobsToAnalyze.empty())
                    {
                        // no jobs were emitted this time around.
                        // we can assume that all jobs are done for this source file (because none were emitted)
                        QString absolutePath = QDir(entry.m_sourceFileInfo.m_scanFolder->ScanPath()).absoluteFilePath(entry.m_sourceFileInfo.m_pathRelativeToScanFolder);
                        QMetaObject::invokeMethod(this, "FinishAnalysis", Qt::QueuedConnection, Q_ARG(AZStd::string, absolutePath.toUtf8().constData()));
                    }
                    else
                    {
                        // All the jobs of the sourcefile needs to be bundled together to check for missing jobs.
                        CheckMissingJobs(entry.m_sourceFileInfo.m_databasePath, entry.m_sourceFileInfo.m_scanFolder, entry.m_jobsToAnalyze);
                        // Update source and job dependency list before forwarding the job to RCController
                        AnalyzeJobDetail(entry);
                    }
                }
                m_jobEntries.clear();
                ProcessJobs();
            }
        }
    }

    // ----------------------------------------------------
    // ------------- File change Queue --------------------
    // ----------------------------------------------------
    void AssetProcessorManager::AssessFileInternal(QString fullFile, bool isDelete, bool fromScanner)
    {
        if (m_quitRequested)
        {
            return;
        }

        QString normalizedFullFile = AssetUtilities::NormalizeFilePath(fullFile);
        if (!fromScanner) // the scanner already does exclusion and doesn't need to deal with metafiles.
        {
            if (m_platformConfig->IsFileExcluded(normalizedFullFile))
            {
                return;
            }

            // over here we also want to invalidate the metafiles on disk map if it COULD Be a metafile
            // note that there is no reason to do an expensive exacting computation here, it will be 
            // done later and cached when m_cachedMetaFilesExistMap is set to false, we just need to
            // know if its POSSIBLE that its a metafile, cheaply.
            // if its a metafile match, then invalidate the metafile table.
            for (int idx = 0; idx < m_platformConfig->MetaDataFileTypesCount(); idx++)
            {
                QPair<QString, QString> metaDataFileType = m_platformConfig->GetMetaDataFileTypeAt(idx);
                if (fullFile.endsWith(metaDataFileType.first, Qt::CaseInsensitive))
                {
                    m_cachedMetaFilesExistMap = false;
                    m_metaFilesWhichActuallyExistOnDisk.clear(); // invalidate the map, force a recompuation later.
                }
            }
            
        }

        m_AssetProcessorIsBusy = true;
        Q_EMIT AssetProcessorManagerIdleState(false);

        AZ_TracePrintf(AssetProcessor::DebugChannel, "AssesFileInternal: %s %s\n", normalizedFullFile.toUtf8().constData(), isDelete ? "true" : "false");

        // this function is the raw function that gets called from the file monitor
        // whenever an asset has been modified or added (not deleted)
        // it should place the asset on a grace period list and not considered until changes stop happening to it.
        // note that file Paths come in raw, full absolute paths.
        if (!m_sourceFilesInDatabase.empty())
        {
            if (!isDelete)
            {
                m_sourceFilesInDatabase.remove(normalizedFullFile);
            }
        }

        FileEntry newEntry(normalizedFullFile, isDelete, fromScanner);

        if (m_alreadyActiveFiles.find(normalizedFullFile) != m_alreadyActiveFiles.end())
        {
            auto entryIt = std::find_if(m_activeFiles.begin(), m_activeFiles.end(),
                    [&normalizedFullFile](const FileEntry& entry)
                    {
                        return entry.m_fileName == normalizedFullFile;
                    });

            if (entryIt != m_activeFiles.end())
            {
                m_activeFiles.erase(entryIt);
            }
        }

        m_AssetProcessorIsBusy = true;
        m_activeFiles.push_back(newEntry);
        m_alreadyActiveFiles.insert(normalizedFullFile);
        Q_EMIT NumRemainingJobsChanged(m_activeFiles.size() + m_filesToExamine.size() + m_numOfJobsToAnalyze);

        if (!m_alreadyScheduledUpdate)
        {
            m_alreadyScheduledUpdate = true;
            QTimer::singleShot(1, this, SLOT(ScheduleNextUpdate()));
        }
    }

    void AssetProcessorManager::AssessAddedFile(QString filePath)
    {
        if (filePath.startsWith(m_normalizedCacheRootPath, Qt::CaseInsensitive))
        {
            // modifies/adds to the cache are irrelevant.  Deletions are all we care about
            return;
        }

        AssessFileInternal(filePath, false);
    }

    void AssetProcessorManager::AssessModifiedFile(QString filePath)
    {
        // we don't care about modified folders at this time.
        // you'll get a "folder modified" whenever a file in a folder is removed or added or modified
        // but you'll also get the actual file modify itself.
        if (!QFileInfo(filePath).isDir())
        {
            // we also don't care if you modify files in the cache, only deletions matter.
            if (!filePath.startsWith(m_normalizedCacheRootPath, Qt::CaseInsensitive))
            {
                AssessFileInternal(filePath, false);
            }
        }
    }

    // this means a file is definitely coming from the file scanner, and not the file monitor.
    // the file scanner does not scan the cache.
    // the scanner should be ommitting directory changes.
    void AssetProcessorManager::AssessFilesFromScanner(QSet<QString> filePaths)
    {
        for (const QString& filePath : filePaths)
        {
            AssessFileInternal(filePath, false, true);
        }
    }

    void AssetProcessorManager::AssessDeletedFile(QString filePath)
    {
        {
            filePath = AssetUtilities::NormalizeFilePath(filePath);
            QMutexLocker locker(&m_processingJobMutex);
            // early-out on files that are in the deletion list to save some processing time and spam and prevent rebuild errors where you get stuck rebuilding things in a loop
            auto found = m_processingProductInfoList.find(filePath.toUtf8().constData());
            if (found != m_processingProductInfoList.end())
            {
                m_AssetProcessorIsBusy = true; // re-emit the idle state at least, for listeners waiting for it.
                QueueIdleCheck();
                return;
            }
        }

        AssessFileInternal(filePath, true);
    }

    void AssetProcessorManager::ScheduleNextUpdate()
    {
        m_alreadyScheduledUpdate = false;
        if (m_activeFiles.size() > 0)
        {
            DispatchFileChange();
        }
        else
        {
            QueueIdleCheck();
        }
    }

    void AssetProcessorManager::RemoveEmptyFolders()
    {
        if (!m_AssetProcessorIsBusy)
        {
            if (m_checkFoldersToRemove.size())
            {
                QString dir = *m_checkFoldersToRemove.begin();
                CleanEmptyFolder(dir, m_normalizedCacheRootPath);
                m_checkFoldersToRemove.remove(dir);
                QTimer::singleShot(20, this, SLOT(RemoveEmptyFolders()));
            }
        }
    }

    void AssetProcessorManager::DispatchFileChange()
    {
        Q_ASSERT(m_activeFiles.size() > 0);

        if (m_quitRequested)
        {
            return;
        }

        // This was added because we found out that the consumer was not able to keep up, which led to the app taking forever to shut down
        // we want to make sure that our queue has at least this many to eat in a single gulp, so it remains busy, but we cannot let this number grow too large
        // or else it never returns to the main message pump and thus takes a while to realize that quit has been signalled.
        // if the processing thread ever runs dry, then this needs to be increased.
        int maxPerIteration = 50;

        // Burn through all pending files
        const FileEntry* firstEntry = &m_activeFiles.front();
        while (m_filesToExamine.size() < maxPerIteration)
        {
            m_alreadyActiveFiles.remove(firstEntry->m_fileName);
            CheckSource(*firstEntry);
            m_activeFiles.pop_front();

            if (m_activeFiles.size() == 0)
            {
                break;
            }
            firstEntry = &m_activeFiles.front();
        }

        if (!m_alreadyScheduledUpdate)
        {
            // schedule additional updates
            m_alreadyScheduledUpdate = true;
            QTimer::singleShot(1, this, SLOT(ScheduleNextUpdate()));
        }
    }


    bool AssetProcessorManager::IsIdle()
    {
        if ((!m_queuedExamination) && (m_filesToExamine.isEmpty()) && (m_activeFiles.isEmpty()) &&
            !m_processedQueued && m_assetProcessedList.empty() && !m_numOfJobsToAnalyze)
        {
            return true;
        }

        return false;
    }

    bool AssetProcessorManager::HasProcessedCriticalAssets() const
    {
        return m_hasProcessedCriticalAssets;
    }

    void AssetProcessorManager::ProcessJobs()
    {
        // 1) Loop over all the jobs and analyze each job one by one.
        // 2) Analyzing should return true only when all the dependent jobs fingerprint's are known to APM, if true process that job.
        // 3) If anytime we were unable to analyze even one job even after looping over all the remaining jobs then
        //    we will process the first job and loop over the remaining jobs once again since that job might have unblocked other jobs.

        bool anyJobAnalyzed = false;

        QElapsedTimer elapsedTimer;
        elapsedTimer.start();
        for (auto jobIter = m_jobsToProcess.begin(); jobIter != m_jobsToProcess.end();)
        {
            JobDetails& job = *jobIter;
            if (CanAnalyzeJob(job))
            {
                anyJobAnalyzed = true;
                ProcessJob(job);
                jobIter = m_jobsToProcess.erase(jobIter);
                m_numOfJobsToAnalyze--;

                // Update the remaining job status occasionally 
                if (elapsedTimer.elapsed() >= MILLISECONDS_BETWEEN_PROCESS_JOBS_STATUS_UPDATE)
                {
                    Q_EMIT NumRemainingJobsChanged(m_activeFiles.size() + m_filesToExamine.size() + m_numOfJobsToAnalyze);
                    elapsedTimer.restart();
                }
            }
            else
            {
                ++jobIter;
            }
        }

        if (m_jobsToProcess.size())
        {
            if (!anyJobAnalyzed)
            {
                // Process the first job if no jobs were analyzed.
                auto jobIter = m_jobsToProcess.begin();
                JobDetails& job = *jobIter;
                AZ_Warning(AssetProcessor::DebugChannel, false, " Cyclic job dependency detected. Processing job (%s, %s, %s, %s) to unblock.", 
                    job.m_jobEntry.m_databaseSourceName.toUtf8().data(), job.m_jobEntry.m_jobKey.toUtf8().data(),
                    job.m_jobEntry.m_platformInfo.m_identifier.c_str(), job.m_jobEntry.m_builderGuid.ToString<AZStd::string>().c_str());
                ProcessJob(job);
                m_jobsToProcess.erase(jobIter);
                m_numOfJobsToAnalyze--;
            }

            QMetaObject::invokeMethod(this, "ProcessJobs", Qt::QueuedConnection);
        }
        else
        {
            QueueIdleCheck();
        }

        Q_EMIT NumRemainingJobsChanged(m_activeFiles.size() + m_filesToExamine.size() + m_numOfJobsToAnalyze);
    }

    void AssetProcessorManager::ProcessJob(JobDetails& job)
    {
        // Populate all the files needed for fingerprinting of this job.  Note that m_fingerprintFilesList is a sorted set
        // and thus will automatically eliminate duplicates and be sorted.  It is expected to contain the absolute paths to all
        // files that contribute to the fingerprint of the job.
        // this automatically adds the input file to the list, too.
        // note that for jobs, we only query source dependencies, here, not Source and Job dependencies.
        // this is because we want to take the fingerprint of SOURCE FILES for source dependencies
        // but for jobs we want the fingerprint of the job itself, not that job's source files.
        QueryAbsolutePathDependenciesRecursive(job.m_jobEntry.m_databaseSourceName, job.m_fingerprintFiles, AzToolsFramework::AssetDatabase::SourceFileDependencyEntry::DEP_SourceToSource, false);
        
        QString absoluteFullPath = QDir(job.m_jobEntry.m_watchFolderPath).absoluteFilePath(job.m_jobEntry.m_pathRelativeToWatchFolder);
        AddMetadataFilesForFingerprinting(absoluteFullPath, job.m_fingerprintFiles);
        
        // Check the current builder jobs with the previous ones in the database:
        job.m_jobEntry.m_computedFingerprint = AssetUtilities::GenerateFingerprint(job);
        JobIndentifier jobIndentifier(JobDesc(job.m_jobEntry.m_databaseSourceName.toUtf8().data(), job.m_jobEntry.m_jobKey.toUtf8().data(), job.m_jobEntry.m_platformInfo.m_identifier), job.m_jobEntry.m_builderGuid);

        {
            AZStd::lock_guard<AssetProcessor::ProcessingJobInfoBus::MutexType> lock(AssetProcessor::ProcessingJobInfoBus::GetOrCreateContext().m_contextMutex);
            m_jobFingerprintMap[jobIndentifier] = job.m_jobEntry.m_computedFingerprint;
        }
        job.m_jobEntry.m_computedFingerprintTimeStamp = QDateTime::currentMSecsSinceEpoch();
        if (job.m_jobEntry.m_computedFingerprint == 0)
        {
            // unable to fingerprint this file.
            AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessBuilders: Unable to fingerprint for platform: %s.\n", job.m_jobEntry.m_platformInfo.m_identifier.c_str());
        }

        // Check to see whether we need to process this asset
        if (AnalyzeJob(job))
        {
            Q_EMIT AssetToProcess(job);
        }
        else
        {
            // we're about to drop the job because its already up to date, so that's one job that is "Finished"
            UpdateAnalysisTrackerForFile(absoluteFullPath.toUtf8().constData(), AnalysisTrackerUpdateType::JobFinished);
        }
    }

    void AssetProcessorManager::UpdateJobDependency(JobDetails& job)
    {
        for(auto JobDependencyIter = job.m_jobDependencyList.begin(); JobDependencyIter != job.m_jobDependencyList.end();)
        {
            AssetBuilderSDK::SourceFileDependency& sourceFileDependency = JobDependencyIter->m_jobDependency.m_sourceFile;
            if (sourceFileDependency.m_sourceFileDependencyUUID.IsNull() && sourceFileDependency.m_sourceFileDependencyPath.empty())
            {
                AZ_Warning(AssetProcessor::DebugChannel, false, "Unable to resolve job dependency for job %s - %s\n", job.ToString().c_str(), sourceFileDependency.ToString().c_str());
                JobDependencyIter = job.m_jobDependencyList.erase(JobDependencyIter);
                continue;
            }

            QString databaseSourceName;

            if (!ResolveDependencyPath(sourceFileDependency, databaseSourceName))
            {
                AZ_Warning(AssetProcessor::DebugChannel, false, "Unable to resolve job dependency for job (%s, %s, %s)\n",
                    job.m_jobEntry.m_databaseSourceName.toUtf8().data(), job.m_jobEntry.m_jobKey.toUtf8().data(),
                    job.m_jobEntry.m_platformInfo.m_identifier.c_str(), sourceFileDependency.m_sourceFileDependencyUUID.ToString<AZStd::string>().c_str());
                continue;
            }

            sourceFileDependency.m_sourceFileDependencyPath = AssetUtilities::NormalizeFilePath(databaseSourceName).toUtf8().data();

            // Listing all the builderUuids that have the same (sourcefile,platform,jobKey) for this job dependency
            JobDesc jobDesc(JobDependencyIter->m_jobDependency.m_sourceFile.m_sourceFileDependencyPath,
                JobDependencyIter->m_jobDependency.m_jobKey, JobDependencyIter->m_jobDependency.m_platformIdentifier);
            auto buildersFound = m_jobDescToBuilderUuidMap.find(jobDesc);
            
            if (buildersFound != m_jobDescToBuilderUuidMap.end())
            {
                for (AZ::Uuid& builderUuid : buildersFound->second)
                {
                    JobDependencyIter->m_builderUuidList.insert({ builderUuid });
                }
            }

            JobDependencyIter++;

            // sorting job dependencies as they can effect the fingerprint of the job 
            AZStd::sort(job.m_jobDependencyList.begin(), job.m_jobDependencyList.end(),
                [](const AssetProcessor::JobDependencyInternal& lhs, const AssetProcessor::JobDependencyInternal& rhs)
            {
                return (lhs.ToString() < rhs.ToString());
            }
            );
        }
    }

    bool AssetProcessorManager::CanAnalyzeJob(const JobDetails& job)
    {
        for (const JobDependencyInternal& jobDependencyInternal : job.m_jobDependencyList)
        {
            // Loop over all the builderUuid and check whether the corresponding entry exists in the jobsFingerprint map.
            // If an entry exists, it implies than we have already send the job over to the RCController  
            for (auto builderIter = jobDependencyInternal.m_builderUuidList.begin(); builderIter != jobDependencyInternal.m_builderUuidList.end(); ++builderIter)
            {
                JobIndentifier jobIdentifier(JobDesc(jobDependencyInternal.m_jobDependency.m_sourceFile.m_sourceFileDependencyPath,
                    jobDependencyInternal.m_jobDependency.m_jobKey, jobDependencyInternal.m_jobDependency.m_platformIdentifier),
                    *builderIter);

                auto jobFound = m_jobFingerprintMap.find(jobIdentifier);
                if (jobFound == m_jobFingerprintMap.end())
                {
                    // Job cannot be processed, since one of its dependent job hasn't been fingerprinted 
                    return false;
                }
            }
        }

        // Either this job does not have any dependent jobs or all of its dependent jobs have been fingerprinted
        return true;
    }


    void AssetProcessorManager::ProcessBuilders(QString normalizedPath, QString databasePathToFile, const ScanFolderInfo* scanFolder, const AssetProcessor::BuilderInfoList& builderInfoList)
    {
        // this function gets called once for every source file.
        // it is expected to send the file to each builder registered to process that type of file
        // and call the CreateJobs function on the builder.
        // it bundles the results up in a JobToProcessEntry struct, while it is doing this:
        JobToProcessEntry entry;
        
        AZ::Uuid sourceUUID = AssetUtilities::CreateSafeSourceUUIDFromName(databasePathToFile.toUtf8().constData());

        // first, we put the source UUID in the map so that its present for any other queries:
        SourceInfo newSourceInfo;
        newSourceInfo.m_watchFolder = scanFolder->ScanPath();
        newSourceInfo.m_sourceDatabaseName = databasePathToFile;
        newSourceInfo.m_sourceRelativeToWatchFolder = databasePathToFile;
        if (!scanFolder->GetOutputPrefix().isEmpty())
        {
            newSourceInfo.m_sourceRelativeToWatchFolder = newSourceInfo.m_sourceRelativeToWatchFolder.remove(0, static_cast<int>(scanFolder->GetOutputPrefix().length()) + 1);
        }

        {
            // this scope exists only to narrow the range of m_sourceUUIDToSourceNameMapMutex
            AZStd::lock_guard<AZStd::mutex> lock(m_sourceUUIDToSourceInfoMapMutex);
            m_sourceUUIDToSourceInfoMap.insert(AZStd::make_pair(sourceUUID, newSourceInfo));
        }

        // insert the new entry into the analysis tracker:
        auto resultInsert = m_remainingJobsForEachSourceFile.insert_key(normalizedPath.toUtf8().constData());
        AnalysisTracker& analysisTracker = resultInsert.first->second;
        analysisTracker.m_databaseSourceName = databasePathToFile.toUtf8().constData();
        analysisTracker.m_databaseScanFolderId = scanFolder->ScanFolderID();
        analysisTracker.m_buildersInvolved.clear();
        for (const AssetBuilderSDK::AssetBuilderDesc& builderInfo : builderInfoList)
        {
            analysisTracker.m_buildersInvolved.insert(builderInfo.m_busId);
        }

        // collect all the jobs and responses
        for (const AssetBuilderSDK::AssetBuilderDesc& builderInfo : builderInfoList)
        {
            // If the builder's bus ID is null, then avoid processing (this should not happen)
            if (builderInfo.m_busId.IsNull())
            {
                AZ_TracePrintf(AssetProcessor::DebugChannel, "Skipping builder %s, no builder bus id defined.\n", builderInfo.m_name.data());
                continue;
            }

            // note that the relative path to file contains the output prefix since thats our data storage format in our database
            // however, that is an internal detail we do not want to expose to builders.
            // instead, we strip it out, before we send the request and if necessary, put it back after.
            QString actualRelativePath = newSourceInfo.m_sourceRelativeToWatchFolder;

            const AssetBuilderSDK::CreateJobsRequest createJobsRequest(builderInfo.m_busId, actualRelativePath.toUtf8().constData(), scanFolder->ScanPath().toUtf8().constData(), scanFolder->GetPlatforms(), sourceUUID);

            AssetBuilderSDK::CreateJobsResponse createJobsResponse;

            // Wrap with a log listener to redirect logging to a job specific log file and then send job request to the builder
            AZ::s64 runKey = GenerateNewJobRunKey();
            AssetProcessor::SetThreadLocalJobId(runKey);

            AZStd::string logFileName = AssetUtilities::ComputeJobLogFileName(createJobsRequest);
            {
                AssetUtilities::JobLogTraceListener jobLogTraceListener(logFileName, runKey, true);
                builderInfo.m_createJobFunction(createJobsRequest, createJobsResponse);
            }
            AssetProcessor::SetThreadLocalJobId(0);

            if (createJobsResponse.m_result == AssetBuilderSDK::CreateJobsResultCode::Failed)
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Createjobs Failed: %s.\n", normalizedPath.toUtf8().constData());

                AZStd::string fullPathToLogFile = AssetUtilities::ComputeJobLogFolder();
                fullPathToLogFile += "/";
                fullPathToLogFile += logFileName.c_str();
                char resolvedBuffer[AZ_MAX_PATH_LEN] = { 0 };

                AZ::IO::FileIOBase::GetInstance()->ResolvePath(fullPathToLogFile.c_str(), resolvedBuffer, AZ_MAX_PATH_LEN);
             
                JobDetails jobdetail;
                jobdetail.m_jobEntry = JobEntry(
                    scanFolder->ScanPath(),
                    actualRelativePath,
                    databasePathToFile,
                    builderInfo.m_busId,
                        {
                            "all", {}
                        },
                        QString("CreateJobs_%1").arg(builderInfo.m_busId.ToString<AZStd::string>().c_str()), 0, runKey, sourceUUID);
                jobdetail.m_scanFolder = scanFolder;
                jobdetail.m_autoFail = true;
                jobdetail.m_critical = true;
                jobdetail.m_priority = INT_MAX; // front of the queue.

                // try reading the log yourself.
                AssetJobLogResponse response;
                jobdetail.m_jobParam[AZ_CRC(AutoFailReasonKey)] = AZStd::string::format(
                        "CreateJobs of %s has failed.\n"
                        "This is often because the asset is corrupt.\n"
                        "Please load it in the editor to see what might be wrong.\n",
                        actualRelativePath.toUtf8().data());

                AssetUtilities::ReadJobLog(resolvedBuffer, response);
                jobdetail.m_jobParam[AZ_CRC(AutoFailLogFile)].swap(response.m_jobLog);
                jobdetail.m_jobParam[AZ_CRC(AutoFailOmitFromDatabaseKey)] = "true"; // omit this job from the database.

                UpdateAnalysisTrackerForFile(normalizedPath.toUtf8().constData(), AnalysisTrackerUpdateType::JobFailed);

                Q_EMIT AssetToProcess(jobdetail);// forwarding this job to rccontroller to fail it

                continue;
            }
            else if (createJobsResponse.m_result == AssetBuilderSDK::CreateJobsResultCode::ShuttingDown)
            {
                return;
            }
            else
            {
                // if we get here, we succeeded.
                {
                    // if we succeeded, we can erase any jobs that had failed createjobs last time for this builder:
                    AzToolsFramework::AssetSystem::JobInfo jobInfo;
                    jobInfo.m_sourceFile = actualRelativePath.toUtf8().constData();
                    jobInfo.m_platform = "all";
                    jobInfo.m_jobKey = AZStd::string::format("CreateJobs_%s", builderInfo.m_busId.ToString<AZStd::string>().c_str());
                    Q_EMIT JobRemoved(jobInfo);
                }

                int numJobDependencies = 0;

                for (AssetBuilderSDK::JobDescriptor& jobDescriptor : createJobsResponse.m_createJobOutputs)
                {
                    const AssetBuilderSDK::PlatformInfo* const infoForPlatform = m_platformConfig->GetPlatformByIdentifier(jobDescriptor.GetPlatformIdentifier().c_str());
                    AZ_Assert(infoForPlatform, "Somehow, a platform for a job was created in createjobs which cannot be found in the list of enabled platforms.");
                    if (infoForPlatform)
                    {
                        JobDetails newJob;
                        newJob.m_assetBuilderDesc = builderInfo;
                        newJob.m_critical = jobDescriptor.m_critical;
                        newJob.m_extraInformationForFingerprinting = AZStd::string::format("%i%s", builderInfo.m_version, jobDescriptor.m_additionalFingerprintInfo.c_str());
                        newJob.m_jobEntry = JobEntry(
                            scanFolder->ScanPath(),
                            actualRelativePath,
                            databasePathToFile,
                            builderInfo.m_busId, 
                            *infoForPlatform, 
                            jobDescriptor.m_jobKey.c_str(), 0, GenerateNewJobRunKey(), 
                            sourceUUID);
                        newJob.m_jobEntry.m_checkExclusiveLock = jobDescriptor.m_checkExclusiveLock;
                        newJob.m_jobParam = AZStd::move(jobDescriptor.m_jobParameters);
                        newJob.m_priority = jobDescriptor.m_priority;
                        newJob.m_scanFolder = scanFolder;

                        for (const AssetBuilderSDK::JobDependency& jobDependency : jobDescriptor.m_jobDependencyList)
                        {
                            newJob.m_jobDependencyList.push_back(JobDependencyInternal(jobDependency));
                            ++numJobDependencies;
                        }
                        
                        // note that until analysis completes, the jobId is not set and neither is the destination pat
                        JobDesc jobDesc(newJob.m_jobEntry.m_databaseSourceName.toUtf8().data(), newJob.m_jobEntry.m_jobKey.toUtf8().data(), newJob.m_jobEntry.m_platformInfo.m_identifier);
                        m_jobDescToBuilderUuidMap[jobDesc].insert(builderInfo.m_busId);

                        // until this job is analyzed, assume its fingerprint is not computed.
                        JobIndentifier jobIdentifier(jobDesc, builderInfo.m_busId);
                        {
                            AZStd::lock_guard<AssetProcessor::ProcessingJobInfoBus::MutexType> lock(AssetProcessor::ProcessingJobInfoBus::GetOrCreateContext().m_contextMutex);
                             m_jobFingerprintMap.erase(jobIdentifier); 
                        }

                        entry.m_jobsToAnalyze.push_back(AZStd::move(newJob));

                        // because we added / created a job for the queue, we increment the number of outstanding jobs for this item now.
                        // when it either later gets analyzed and done, or dropped (because its already up to date), we will decrement it.
                        UpdateAnalysisTrackerForFile(normalizedPath.toUtf8().constData(), AnalysisTrackerUpdateType::JobStarted);
                        m_numOfJobsToAnalyze++;
                    }
                }

                // detect if the configuration of the builder is correct:
                if ((!createJobsResponse.m_sourceFileDependencyList.empty()) || (numJobDependencies > 0))
                {
                    if ((builderInfo.m_flags & AssetBuilderSDK::AssetBuilderDesc::BF_EmitsNoDependencies) != 0)
                    {
                        AZ_WarningOnce(ConsoleChannel, "Asset builder '%s' registered itself using BF_EmitsNoDependencies flag, but actually emitted dependencies.  This will cause rebuilds to be inconsistent.\n", builderInfo.m_name.c_str());
                    }

                    // remember which builder emitted each dependency:
                    for (const AssetBuilderSDK::SourceFileDependency& sourceDependency : createJobsResponse.m_sourceFileDependencyList)
                    {
                        entry.m_sourceFileDependencies.push_back(AZStd::make_pair(builderInfo.m_busId, sourceDependency));
                    }
                }
            }
        }

        // Put the whole set into the 'process later' queue, so it runs after its dependencies
        entry.m_sourceFileInfo.m_databasePath = databasePathToFile;
        entry.m_sourceFileInfo.m_scanFolder = scanFolder;
        entry.m_sourceFileInfo.m_pathRelativeToScanFolder = newSourceInfo.m_sourceRelativeToWatchFolder;
        entry.m_sourceFileInfo.m_uuid = sourceUUID;

        // entry now contains, for one given source file, all jobs, dependencies, etc, created by ALL builders.
        // now we can update the database with this new information:
        UpdateSourceFileDependenciesDatabase(entry);
        m_jobEntries.push_back(entry);
    }

    bool AssetProcessorManager::ResolveDependencyPath(const AssetBuilderSDK::SourceFileDependency& sourceDependency, QString &resultDatabaseSourceName)
    {
        resultDatabaseSourceName.clear();
        if (!sourceDependency.m_sourceFileDependencyUUID.IsNull())
        {
            // if the UUID has been provided, we will use that, and attempt to resolve.
            SourceInfo resultSourceInfo;
            if (!SearchSourceInfoBySourceUUID(sourceDependency.m_sourceFileDependencyUUID, resultSourceInfo))
            {
                // unable to resolve it, encode it instead, force use of brackets:
                resultDatabaseSourceName = m_placeHolderFileName + sourceDependency.m_sourceFileDependencyUUID.ToString<AZStd::string>(true /*isBrackets*/).c_str();
            }
            else
            {
                resultDatabaseSourceName = resultSourceInfo.m_sourceDatabaseName;
            }
        }
        else if (!sourceDependency.m_sourceFileDependencyPath.empty())
        {
            // instead of a UUID, a path has been provided, prepare and use that.  We need to turn it into a database path
            QString encodedFileData = QString::fromUtf8(sourceDependency.m_sourceFileDependencyPath.c_str());
            if (QFileInfo(encodedFileData).isAbsolute())
            {
                // attempt to split:
                QString scanFolderName;
                if (!m_platformConfig->ConvertToRelativePath(encodedFileData, resultDatabaseSourceName, scanFolderName))
                {
                    AZ_Warning(AssetProcessor::ConsoleChannel, false, "'%s' does not appear to be in any input folder.  Use relative paths instead.", sourceDependency.m_sourceFileDependencyPath.c_str());
                    resultDatabaseSourceName.clear();
                }
            }
            else
            {
                // its a relative path.  We want a database source name if possible, but we'll settle for relative path if we have to:
                QString absolutePath = m_platformConfig->FindFirstMatchingFile(encodedFileData);
                if (absolutePath.isEmpty())
                {
                    resultDatabaseSourceName = m_placeHolderFileName + encodedFileData;
                }
                else
                {
                    // we have found the actual file, so we know what the scan folder and thus database path will be.
                    QString scanFolderName;
                    m_platformConfig->ConvertToRelativePath(absolutePath, resultDatabaseSourceName, scanFolderName);
                }
            }
        }
        else
        {
            AZ_Warning(AssetProcessor::ConsoleChannel, false, "The dependency fields were empty.");
            resultDatabaseSourceName.clear();
        }

        return (!resultDatabaseSourceName.isEmpty());
    }
    
    void AssetProcessorManager::UpdateSourceFileDependenciesDatabase(JobToProcessEntry& entry)
    {
        using namespace AzToolsFramework::AssetDatabase;
        AZ_TraceContext("Source File", entry.m_sourceFileInfo.m_pathRelativeToScanFolder.toUtf8().constData());
        // entry is all of the collected CreateJobs responses and other info for a given single source file.
        // we are going to erase the prior entries in the database for this source file and replace them with the new ones
        // we are also going to find any unresolved entries in the database for THIS source, and update them

        // the database contains the following columns
        // ID         BuilderID       SOURCE     WhatItDependsOn    TypeOfDependency

        // note that NEITHER columns (source / what it depends on) are database names (ie, they do not have the output prefix prepended)
        // where "whatitdependson" is either a relative path to a source file, or, if the source's UUID is unknown, a UUID in curly braces format.
        // collect all dependencies, of every type of dependency:
        AzToolsFramework::AssetDatabase::SourceFileDependencyEntryContainer newDependencies;
        for (const AZStd::pair<AZ::Uuid, AssetBuilderSDK::SourceFileDependency>& sourceDependency : entry.m_sourceFileDependencies)
        {
            // figure out whether we can resolve the dependency or not:
            QString resolvedDependency;
            
            if (!ResolveDependencyPath(sourceDependency.second, resolvedDependency))
            {
                // ResolveDependencyPath should only fail in a data error, otherwise it always outputs something, 
                // even if that something starts with the placeholder.
                continue;
            }

            // add the new dependency:
            SourceFileDependencyEntry newDependencyEntry(
                sourceDependency.first, 
                entry.m_sourceFileInfo.m_databasePath.toUtf8().constData(),
                resolvedDependency.toUtf8().constData(), 
                SourceFileDependencyEntry::DEP_SourceToSource);
            newDependencies.push_back(AZStd::move(newDependencyEntry));
        }

        // gather the job dependencies, too:
        for (const JobDetails& jobToCheck : entry.m_jobsToAnalyze)
        {
            const AZ::Uuid& builderId = jobToCheck.m_assetBuilderDesc.m_busId;
            for (const AssetProcessor::JobDependencyInternal& jobDependency : jobToCheck.m_jobDependencyList)
            {
                // figure out whether we can resolve the dependency or not:
                QString resolvedDependency;

                if (!ResolveDependencyPath(jobDependency.m_jobDependency.m_sourceFile, resolvedDependency))
                {
                    continue;
                }

                SourceFileDependencyEntry newDependencyEntry(
                    builderId,
                    entry.m_sourceFileInfo.m_databasePath.toUtf8().constData(),
                    resolvedDependency.toUtf8().constData(),
                    SourceFileDependencyEntry::DEP_JobToJob);   // significant line in this code block
                newDependencies.push_back(AZStd::move(newDependencyEntry));
            }
        }

        // get all the old dependencies and remove them. This function is comprehensive on all dependencies
        // for a given source file so we can just eliminate all of them from that same source file and replace 
        // them with all of the  new ones for the given source file:
        AZStd::unordered_set<AZ::s64> oldDependencies;
        m_stateData->QueryDependsOnSourceBySourceDependency(
            entry.m_sourceFileInfo.m_databasePath.toUtf8().constData(), // find all rows in the database where this is the source column
            nullptr, // no filter
            SourceFileDependencyEntry::DEP_Any,    // significant line in this code block
            [&](SourceFileDependencyEntry& existingEntry)
        {
            oldDependencies.insert(existingEntry.m_sourceDependencyID);
            return true; // return true to keep stepping to additional rows
        });

        m_stateData->RemoveSourceFileDependencies(oldDependencies);
        oldDependencies.clear();

        // set the new dependencies:
        m_stateData->SetSourceFileDependencies(newDependencies);

        // we also have to make sure that anything that was a placeholder (right hand column only) on this file, either by relative path, or by guid, is updated
        // if we find anything, we have to re-queue it.
        // so do another search - this time, on our placeholder.
        // note that if it IS a place holder, it won't have an output prefix, so we use the relative path, not the database path.
        QString ourNameWithPlaceholder = m_placeHolderFileName + entry.m_sourceFileInfo.m_pathRelativeToScanFolder;
        QString ourUUIDWithPlaceholder = m_placeHolderFileName + entry.m_sourceFileInfo.m_uuid.ToString<AZStd::string>().c_str();

        SourceFileDependencyEntryContainer results;
        m_stateData->GetSourceFileDependenciesByDependsOnSource(ourNameWithPlaceholder, SourceFileDependencyEntry::DEP_Any, results);
        m_stateData->GetSourceFileDependenciesByDependsOnSource(ourUUIDWithPlaceholder, SourceFileDependencyEntry::DEP_Any, results);

        AZStd::string databaseNameEncoded = entry.m_sourceFileInfo.m_databasePath.toUtf8().constData();
        // process the results by replacing them with the resolved value and pushing any sources into the list.
        for (SourceFileDependencyEntry& resultEntry : results)
        {
            resultEntry.m_dependsOnSource = databaseNameEncoded;
            // we also have to re-queue the source for analysis, if it exists, since it means something it depends on
            // has suddenly appeared on disk:
            QString absPath = m_platformConfig->FindFirstMatchingFile(QString::fromUtf8(resultEntry.m_source.c_str()));
            if (!absPath.isEmpty())
            {
                // add it to the queue for analysis:
                AssessFileInternal(absPath, false, false);
            }
        }

        // remove the old ones:
        m_stateData->RemoveSourceFileDependencies(results);

        // replace the changed lines:
        m_stateData->SetSourceFileDependencies(results);
    }

    AZStd::shared_ptr<AssetDatabaseConnection> AssetProcessorManager::GetDatabaseConnection() const
    {
        return m_stateData;
    }

    void AssetProcessorManager::BeginIgnoringCacheFileDelete(const char* productPath)
    {
        QMutexLocker locker(&m_processingJobMutex);
        m_processingProductInfoList.insert(productPath);
    }

    void AssetProcessorManager::StopIgnoringCacheFileDelete(const char* productPath, bool queueAgainForProcessing)
    {
        QMutexLocker locker(&m_processingJobMutex);
        m_processingProductInfoList.erase(productPath);
        if (queueAgainForProcessing)
        {
            QMetaObject::invokeMethod(this, "AssessDeletedFile", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(productPath)));
        }
    }

    AZ::u32 AssetProcessorManager::GetJobFingerprint(const AssetProcessor::JobIndentifier& jobIndentifier)
    {
        auto jobFound = m_jobFingerprintMap.find(jobIndentifier);
        if (jobFound == m_jobFingerprintMap.end())
        {
            // fingerprint of this job is missing
            return 0;
        }
        else
        {
            return jobFound->second;
        }
    }

    AZ::s64 AssetProcessorManager::GenerateNewJobRunKey()
    {
        return m_highestJobRunKeySoFar++;
    }

    bool AssetProcessorManager::EraseLogFile(const char* fileName)
    {
        AZ_Assert(fileName, "Invalid call to EraseLogFile with a nullptr filename.");
        if ((!fileName) || (fileName[0] == 0))
        {
            // Sometimes logs are empty / missing already in the DB or empty in the "log" column.
            // this counts as success since there is no log there.
            return true;
        }
        // try removing it immediately - even if it doesn't exist, its quicker to delete it and notice it failed.
        if (!AZ::IO::FileIOBase::GetInstance()->Remove(fileName))
        {
            // we couldn't remove it.  Is it because it was already gone?  Because in that case, there's no problem.
            // we only worry if we were unable to delete it and it exists
            if (AZ::IO::FileIOBase::GetInstance()->Exists(fileName))
            {
                AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Was unable to delete log file %s...\n", fileName);
                return false;
            }
        }

        return true; // if the file was either successfully removed or never existed in the first place, its gone, so we return true;
    }

    bool AssetProcessorManager::MigrateScanFolders()
    {
        // Migrate Scan Folders retrieves the last list of scan folders from the DB
        // it then finds out what scan folders SHOULD be in the database now, by matching the portable key

        // start with all of the scan folders that are currently in the database.
        m_stateData->QueryScanFoldersTable([this](AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry& entry)
            {
                // the database is case-insensitive, so we should emulate that here in our find()
                AZStd::string portableKey = entry.m_portableKey;
                AZStd::to_lower(portableKey.begin(), portableKey.end());
                m_scanFoldersInDatabase.insert(AZStd::make_pair(portableKey, entry));
                return true;
            });

        // now update them based on whats in the config file.
        for (int i = 0; i < m_platformConfig->GetScanFolderCount(); ++i)
        {
            AssetProcessor::ScanFolderInfo& scanFolderFromConfigFile = m_platformConfig->GetScanFolderAt(i);

            // for each scan folder in the config file, see if its port key already exists
            AZStd::string scanFolderFromConfigFileKeyLower = scanFolderFromConfigFile.GetPortableKey().toLower().toUtf8().constData();
            auto found = m_scanFoldersInDatabase.find(scanFolderFromConfigFileKeyLower.c_str());

            AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry scanFolderToWrite;
            if (found != m_scanFoldersInDatabase.end())
            {
                // portable key was found, this means we have an existing database entry for this config file entry.
                scanFolderToWrite = AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry(
                        found->second.m_scanFolderID,
                        scanFolderFromConfigFile.ScanPath().toUtf8().constData(),
                        scanFolderFromConfigFile.GetDisplayName().toUtf8().constData(),
                        scanFolderFromConfigFile.GetPortableKey().toUtf8().constData(),
                        scanFolderFromConfigFile.GetOutputPrefix().toUtf8().constData(),
                        scanFolderFromConfigFile.IsRoot());
                //remove this scan path from the scan folders so what is left can deleted
                m_scanFoldersInDatabase.erase(found);
            }
            else
            {
                // no such key exists, its a new entry.
                scanFolderToWrite = AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry(
                        scanFolderFromConfigFile.ScanPath().toUtf8().constData(),
                        scanFolderFromConfigFile.GetDisplayName().toUtf8().constData(),
                        scanFolderFromConfigFile.GetPortableKey().toUtf8().constData(),
                        scanFolderFromConfigFile.GetOutputPrefix().toUtf8().constData(),
                        scanFolderFromConfigFile.IsRoot());
            }

            // update the database.
            bool res = m_stateData->SetScanFolder(scanFolderToWrite);

            AZ_Assert(res, "Failed to set a scan folder.");
            if (!res)
            {
                return false;
            }

            // update the in-memory value of the scan folder id from the above query.
            scanFolderFromConfigFile.SetScanFolderID(scanFolderToWrite.m_scanFolderID);
        }
        return true;
    }

    bool AssetProcessorManager::SearchSourceInfoBySourceUUID(const AZ::Uuid& sourceUuid, AssetProcessorManager::SourceInfo& result)
    {
        {
            // check the map first, it will be faster than checking the DB:
            AZStd::lock_guard<AZStd::mutex> lock(m_sourceUUIDToSourceInfoMapMutex);

            // Checking whether AP know about this source file, this map contain uuids of all known sources encountered in this session.
            auto foundSource = m_sourceUUIDToSourceInfoMap.find(sourceUuid);
            if (foundSource != m_sourceUUIDToSourceInfoMap.end())
            {
                result = foundSource->second;
                return true;
            }
        }

        // try the database next:
        AzToolsFramework::AssetDatabase::SourceDatabaseEntry sourceDatabaseEntry;
        if (m_stateData->GetSourceBySourceGuid(sourceUuid, sourceDatabaseEntry))
        {
            AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry scanFolder;
            if (m_stateData->GetScanFolderByScanFolderID(sourceDatabaseEntry.m_scanFolderPK, scanFolder))
            {
                result.m_sourceDatabaseName = QString::fromUtf8(sourceDatabaseEntry.m_sourceName.c_str());
                result.m_watchFolder = QString::fromUtf8(scanFolder.m_scanFolder.c_str());
                result.m_sourceRelativeToWatchFolder = result.m_sourceDatabaseName;
                if (!scanFolder.m_outputPrefix.empty())
                {
                    result.m_sourceRelativeToWatchFolder = result.m_sourceRelativeToWatchFolder.remove(0, static_cast<int>(scanFolder.m_outputPrefix.size()) + 1);
                }
                
                {   
                    // this scope exists to restrict the duration of the below lock.
                    AZStd::lock_guard<AZStd::mutex> lock(m_sourceUUIDToSourceInfoMapMutex);
                    m_sourceUUIDToSourceInfoMap.insert(AZStd::make_pair(sourceUuid, result));
                }
            }
            return true;
        }

        AZ_TracePrintf(AssetProcessor::DebugChannel, "Unable to find source file having uuid %s", sourceUuid.ToString<AZStd::string>().c_str());
        return false;
    }

    
    void AssetProcessorManager::AnalyzeJobDetail(JobToProcessEntry& jobEntry)
    {
        // each jobEntry is all the jobs collected for a given single source file, this is our opportunity to update the Job Dependencies table
        // since we need all of the ones for a given source.
        using namespace AzToolsFramework::AssetDatabase;

        for (JobDetails& jobDetail : jobEntry.m_jobsToAnalyze)
        {
            // update the job with whatever info it needs about dependencies to proceed:
            UpdateJobDependency(jobDetail);

           auto jobPair = m_jobsToProcess.insert(AZStd::move(jobDetail));
            if (!jobPair.second)
            {
                // if we are here it means that this job was already found in the jobs to process list 
                // and therefore insert failed, we will try to update the iterator manually here.
                // Note that if insert fails the original object is not destroyed and therefore we can use move again. 
                // we just replaced a job, so we have to decrement its count.
                UpdateAnalysisTrackerForFile(jobPair.first->m_jobEntry, AnalysisTrackerUpdateType::JobFinished);

                --m_numOfJobsToAnalyze;
                *jobPair.first = AZStd::move(jobDetail);
            }
        }
    }
    
    QStringList AssetProcessorManager::GetSourceFilesWhichDependOnSourceFile(const QString& sourcePath)
    {
        // The purpose of this function is to find anything that depends on this given file, so that they can be added to the queue.
        // this is NOT a recursive query, because recursion will happen automatically as those files are in turn
        // analyzed.
        // It is generally called when a source file modified in any way, including when it is added or deleted.
        // note that this is a "reverse" dependency query - it looks up what depends on a file, not what the file depends on
        using namespace AzToolsFramework::AssetDatabase;
        QStringList absoluteSourceFilePathQueue;
        QString databasePath;
        QString scanFolder;
 
        auto callbackFunction = [&](AzToolsFramework::AssetDatabase::SourceFileDependencyEntry& entry)
        {
            QString relativeDatabaseName = QString::fromUtf8(entry.m_source.c_str());
            QString absolutePath = m_platformConfig->FindFirstMatchingFile(relativeDatabaseName);
            if (!absolutePath.isEmpty())
            {
                absoluteSourceFilePathQueue.push_back(absolutePath);
            }
            return true;
        };

        // convert to a database path so that the standard function can be called.

        if (m_platformConfig->ConvertToRelativePath(sourcePath, databasePath, scanFolder))
        {
            m_stateData->QuerySourceDependencyByDependsOnSource(databasePath.toUtf8().constData(), nullptr, SourceFileDependencyEntry::DEP_Any, callbackFunction);
        }

        return absoluteSourceFilePathQueue;
    }

    void AssetProcessorManager::AddSourceToDatabase(AzToolsFramework::AssetDatabase::SourceDatabaseEntry& sourceDatabaseEntry, const ScanFolderInfo* scanFolder, QString relativeSourceFilePath)
    {
        sourceDatabaseEntry.m_scanFolderPK = scanFolder->ScanFolderID();

        if (!scanFolder->GetOutputPrefix().isEmpty())
        {
            // replace the "output prefix" part of the file name with the one from the ini file to sort out case sensitivity problems.
            QString withoutOutputPrefix = relativeSourceFilePath.remove(0, scanFolder->GetOutputPrefix().length() + 1);
            sourceDatabaseEntry.m_sourceName = AZStd::string::format("%s/%s", scanFolder->GetOutputPrefix().toUtf8().constData(), withoutOutputPrefix.toUtf8().data());
        }
        else
        {
            sourceDatabaseEntry.m_sourceName = relativeSourceFilePath.toUtf8().constData();
        }


        sourceDatabaseEntry.m_sourceGuid = AssetUtilities::CreateSafeSourceUUIDFromName(sourceDatabaseEntry.m_sourceName.c_str());

        if (!m_stateData->SetSource(sourceDatabaseEntry))
        {
            //somethings wrong...
            AZ_Error(AssetProcessor::ConsoleChannel, false, "Failed to add source to the database!!!");
            return;
        }
    }

    void AssetProcessorManager::CheckAssetProcessorIdleState()
    {
        Q_EMIT AssetProcessorManagerIdleState(IsIdle());
    }

    void AssetProcessorManager::OnBuildersRegistered()
    {
        ComputeBuilderDirty();
    }

    void AssetProcessorManager::ComputeBuilderDirty()
    {
        using BuilderInfoEntry = AzToolsFramework::AssetDatabase::BuilderInfoEntry;
        using BuilderInfoEntryContainer = AzToolsFramework::AssetDatabase::BuilderInfoEntryContainer;
        using AssetBuilderDesc = AssetBuilderSDK::AssetBuilderDesc;
        using AssetBuilderPattern = AssetBuilderSDK::AssetBuilderPattern;

        const char* currentAnalysisVersionString = "0";
        AZ_TracePrintf(DebugChannel, "Computing builder differences from last time...\n")
        m_builderDataCache.clear();
        // note that it counts as an addition or removal if the patterns that a builder uses have changed since it may now apply
        // to new files even if the files themselves have not changed.
        m_buildersAddedOrRemoved = false;
        m_anyBuilderChange = false;

        AssetProcessor::BuilderInfoList currentBuilders; // queried from AP
        BuilderInfoEntryContainer priorBuilders; // queried from the DB

        // the following fields are built using the above data.
        BuilderInfoEntryContainer newBuilders;
        // each entr is a pairy of <Fingerprint For Analysis, Pattern Fingerprint>
        using FingerprintPair = AZStd::pair<AZ::Uuid, AZ::Uuid>;
        AZStd::unordered_map<AZ::Uuid, FingerprintPair > newBuilderFingerprints;
        AZStd::unordered_map<AZ::Uuid, FingerprintPair > priorBuilderFingerprints;

        // query the database to retrieve the prior builders:
        m_stateData->QueryBuilderInfoTable([&priorBuilders](BuilderInfoEntry&& result)
            {
                priorBuilders.push_back(result);
                return true;
            });

        // query the AP to retrieve the current builders:
        AssetProcessor::AssetBuilderInfoBus::Broadcast(&AssetProcessor::AssetBuilderInfoBus::Events::GetAllBuildersInfo, currentBuilders);

        // digest the info into maps for easy lookup
        // the map is of the form
        // [BuilderUUID] = <analysisFingerprint, patternFingerprint>
        // first, digest the current builder info:
        for (const AssetBuilderDesc& currentBuilder : currentBuilders)
        {
            // this makes sure that the version of the builder is included in the analysis fingerprint data:
            AZStd::string analysisFingerprintString = AZStd::string::format("%i:%s", currentBuilder.m_version, currentBuilder.m_analysisFingerprint.c_str());
            AZStd::string patternFingerprintString;

            for (const AssetBuilderPattern& pattern : currentBuilder.m_patterns)
            {
                patternFingerprintString += pattern.ToString();
            }

            //CreateName hashes the data and makes a UUID out of the hash
            AZ::Uuid newAnalysisFingerprint = AZ::Uuid::CreateName(analysisFingerprintString.c_str());
            AZ::Uuid newPatternFingerprint = AZ::Uuid::CreateName(patternFingerprintString.c_str());

            newBuilderFingerprints[currentBuilder.m_busId] = AZStd::make_pair(newAnalysisFingerprint, newPatternFingerprint);
            // in the end, these are just two fingerprints that are part of the same.
            // its 'data version:analysisfingerprint:patternfingerprint'
            AZStd::string finalFingerprintString = AZStd::string::format("%s:%s:%s",
                    currentAnalysisVersionString,
                    newAnalysisFingerprint.ToString<AZStd::string>().c_str(),
                    newPatternFingerprint.ToString<AZStd::string>().c_str());

            newBuilders.push_back(BuilderInfoEntry(-1, currentBuilder.m_busId, finalFingerprintString.c_str()));
            BuilderData newBuilderData;
            newBuilderData.m_fingerprint = AZ::Uuid::CreateName(finalFingerprintString.c_str());
            newBuilderData.m_flags = currentBuilder.m_flags;
            m_builderDataCache[currentBuilder.m_busId] = newBuilderData;

            AZ_TracePrintf(DebugChannel, "Builder %s: %s.\n", currentBuilder.m_flags & AssetBuilderDesc::BF_EmitsNoDependencies ? "does not emit dependencies" : "emits dependencies", currentBuilder.m_name.c_str());
        }

        // now digest the prior builder info from the database:
        for (const BuilderInfoEntry& priorBuilder : priorBuilders)
        {
            AZStd::vector<AZStd::string> tokens;
            AZ::Uuid analysisFingerprint = AZ::Uuid::CreateNull();
            AZ::Uuid patternFingerprint = AZ::Uuid::CreateNull();

            AzFramework::StringFunc::Tokenize(priorBuilder.m_analysisFingerprint.c_str(), tokens, ":");
            // note that the above call to Tokenize will drop empty tokens, so tokens[n] will never be the empty string.
            if ((tokens.size() == 3) && (tokens[0] == currentAnalysisVersionString))
            {
                // CreateString interprets the data as an actual UUID instead of hashing it.
                analysisFingerprint = AZ::Uuid::CreateString(tokens[1].c_str());
                patternFingerprint = AZ::Uuid::CreateString(tokens[2].c_str());
            }
            priorBuilderFingerprints[priorBuilder.m_builderUuid] = AZStd::make_pair(analysisFingerprint, patternFingerprint);
        }

        // now we have the two maps we need to compare and find out which have changed and what is new and old.
        for (const auto& priorBuilderFingerprint : priorBuilderFingerprints)
        {
            const AZ::Uuid& priorBuilderUUID = priorBuilderFingerprint.first;
            const AZ::Uuid& priorBuilderAnalysisFingerprint = priorBuilderFingerprint.second.first;
            const AZ::Uuid& priorBuilderPatternFingerprint = priorBuilderFingerprint.second.second;

            const auto& found = newBuilderFingerprints.find(priorBuilderUUID);
            if (found != newBuilderFingerprints.end())
            {
                const AZ::Uuid& newBuilderAnalysisFingerprint = found->second.first;
                const AZ::Uuid& newBuilderPatternFingerprint = found->second.second;

                bool patternFingerprintIsDirty  = (priorBuilderPatternFingerprint != newBuilderPatternFingerprint);
                bool analysisFingerprintIsDirty = (priorBuilderAnalysisFingerprint != newBuilderAnalysisFingerprint);
                bool builderIsDirty             = (patternFingerprintIsDirty || analysisFingerprintIsDirty);

                // altering the pattern a builder uses to decide which files it affects counts as builder addition or removal
                // because it causes existing files to potentially map to a new set of builders and thus they need re-analysis
                m_buildersAddedOrRemoved = m_buildersAddedOrRemoved || patternFingerprintIsDirty;

                if (patternFingerprintIsDirty)
                {
                    AZ_TracePrintf(DebugChannel, "Builder %s matcher pattern changed.  This will cause a full re-analysis of all assets.\n", priorBuilderUUID.ToString<AZStd::string>().c_str());
                }
                else if (analysisFingerprintIsDirty)
                {
                    AZ_TracePrintf(DebugChannel, "Builder %s analysis fingerprint changed.  Files assigned to it will be re-analyzed.\n", priorBuilderUUID.ToString<AZStd::string>().c_str());
                }
                
                if (builderIsDirty)
                {
                    m_anyBuilderChange = true;
                    m_builderDataCache[priorBuilderUUID].m_isDirty = true;
                }
            }
            else
            {
                // if we get here, it means that a prior builder existed, but no longer exists.
                AZ_TracePrintf(DebugChannel, "Builder with UUID %s no longer exists, full analysis will be done.\n", priorBuilderUUID.ToString<AZStd::string>().c_str());
                m_buildersAddedOrRemoved = true;
                m_anyBuilderChange = true;
            }
        }

        for (const auto& newBuilderFingerprint : newBuilderFingerprints)
        {
            const auto& found = priorBuilderFingerprints.find(newBuilderFingerprint.first);
            if (found == priorBuilderFingerprints.end())
            {
                // if we get here, it means that a new builder exists that did not exist before.
                m_buildersAddedOrRemoved = true;
                m_anyBuilderChange = true;
                m_builderDataCache[newBuilderFingerprint.first].m_isDirty = true;
            }
        }

        // note that we do this in this order, so that the data is INVALIDATED before we write the new builders
        // even if power is lost, we are ensured correct database integrity (ie, the worst case scenario is that we re-analyze)
        if (m_buildersAddedOrRemoved)
        {
            AZ_TracePrintf(ConsoleChannel, "At least one builder has been added or removed or has changed its filter - full analysis needs to be performed\n");
            // when this happens we immediately invalidate every source hash of every files so that if hte user
            m_stateData->InvalidateSourceAnalysisFingerprints();
        }

        // update the database:
        m_stateData->SetBuilderInfoTable(newBuilders);

        if (m_anyBuilderChange)
        {
            // notify the console so that logs contain forensics about this.
            for (const AssetBuilderDesc& builder : currentBuilders)
            {
                if (m_builderDataCache[builder.m_busId].m_isDirty)
                {
                    AZ_TracePrintf(ConsoleChannel, "Builder is new or has changed: %s (%s)\n", builder.m_name.c_str(), builder.m_busId.ToString<AZStd::string>().c_str());
                }
            }
        }
    }

    void AssetProcessorManager::FinishAnalysis(AZStd::string fileToCheck)
    {
        using namespace AzToolsFramework::AssetDatabase;

        auto foundTrackingInfo = m_remainingJobsForEachSourceFile.find(fileToCheck);

        if (foundTrackingInfo == m_remainingJobsForEachSourceFile.end())
        {
            return;
        }

        AnalysisTracker* analysisTracker = &foundTrackingInfo->second;

        if (analysisTracker->failedStatus)
        {
            // if the job failed, we need to wipe the tracking column so that the next time we start the app we will try it again.
            // it may not be necessary to actually alter the database here.
            m_remainingJobsForEachSourceFile.erase(foundTrackingInfo);
            return;
        }

        // if we get here, it succeded, but it may have remaining jobs
        if (analysisTracker->m_remainingJobsSpawned > 0)
        {
            // don't write the fingerprint to the database if there are still remaining jobs to be finished.
            // we only write it when theres no work left to do whatsoever for this asset.
            return;
        }

        // if we get here, we succeeded and there are no more remaining jobs.
        SourceDatabaseEntry source;
        SourceDatabaseEntryContainer sources;

        bool found = false;
        m_stateData->QuerySourceBySourceNameScanFolderID(analysisTracker->m_databaseSourceName.c_str(),
            analysisTracker->m_databaseScanFolderId,
            [&](SourceDatabaseEntry& sourceData)
            {
                source = AZStd::move(sourceData);
                found = true;
                return false; // stop iterating after the first one.  There should actually only be one entry anyway.
            });

        if (found)
        {
            // construct the analysis fingerprint
            // the format for this data is "modtimefingerprint:builder0:builder1:builder2:...:buildern"
            source.m_analysisFingerprint.clear();
            // compute mod times:
            // get the appropriate modtimes:
            AZStd::string modTimeArray;

            // QSet is not ordered.
            SourceFilesForFingerprintingContainer knownDependenciesAbsolutePaths;
            // this automatically adds the input file to the list:
            QueryAbsolutePathDependenciesRecursive(QString::fromUtf8(analysisTracker->m_databaseSourceName.c_str()), knownDependenciesAbsolutePaths, SourceFileDependencyEntry::DEP_Any, false);
            AddMetadataFilesForFingerprinting(QString::fromUtf8(fileToCheck.c_str()), knownDependenciesAbsolutePaths);

            // reserve 17 chars for each since its a 64 bit hex number, and then one more for the dash inbetween each.
            modTimeArray.reserve((knownDependenciesAbsolutePaths.size() * 17));

            for (const auto& element : knownDependenciesAbsolutePaths)
            {
                // if its a placeholder then don't bother hitting the disk to find it.
                modTimeArray.append(AssetUtilities::GetFileFingerprint(element.first, element.second));
                modTimeArray.append("-");
            }
            // to keep this from growing out of hand, we don't use the full string, we use a hash of it:
            source.m_analysisFingerprint = AZ::Uuid::CreateName(modTimeArray.c_str()).ToString<AZStd::string>();
            for (const AZ::Uuid& builderID : analysisTracker->m_buildersInvolved)
            {
                source.m_analysisFingerprint.append(":");
                // for each builder, we write a combination of
                // its ID and its fingerprint.
                AZ::Uuid builderFP = m_builderDataCache[builderID].m_fingerprint;
                source.m_analysisFingerprint.append(builderID.ToString<AZStd::string>());
                source.m_analysisFingerprint.append("~");
                source.m_analysisFingerprint.append(builderFP.ToString<AZStd::string>());
            }
        }

        m_stateData->SetSource(source);

        m_remainingJobsForEachSourceFile.erase(foundTrackingInfo);
    }

    void AssetProcessorManager::SetEnableAnalysisSkippingFeature(bool enable)
    {
        m_bAllowAnalysisSkippingFeature = enable;
    }

    void AssetProcessorManager::QueryAbsolutePathDependenciesRecursive(QString inputDatabasePath, SourceFilesForFingerprintingContainer& finalDependencyList, AzToolsFramework::AssetDatabase::SourceFileDependencyEntry::TypeOfDependency dependencyType, bool reverseQuery)
    {
        // then we add database dependencies.  We have to query this recursively so that we get dependencies of dependencies:
        QSet<QString> results;
        QSet<QString> queryQueue;
        queryQueue.insert(inputDatabasePath);

        while (!queryQueue.isEmpty())
        {
            QString toSearch = *queryQueue.begin();
            queryQueue.erase(queryQueue.begin());

            if (toSearch.startsWith(m_placeHolderFileName))
            {
                if (!reverseQuery)
                {
                    // a placeholder means that it could not be resolved because the file does not exist.
                    // we still add it to the queue so recursion can happen:
                    toSearch = toSearch.mid(m_placeHolderFileName.length());
                }
            }

            // if we've already queried it, dont do it again (breaks recursion)
            if (results.contains(toSearch))
            {
                continue;
            }
            results.insert(toSearch);

            auto callbackFunction = [&](AzToolsFramework::AssetDatabase::SourceFileDependencyEntry& entry)
            {
                if (reverseQuery)
                {
                    queryQueue.insert(QString::fromUtf8(entry.m_source.c_str()));
                }
                else
                {
                    queryQueue.insert(QString::fromUtf8(entry.m_dependsOnSource.c_str()));
                }
                return true;
            };

            if (reverseQuery)
            {
                m_stateData->QuerySourceDependencyByDependsOnSource(toSearch.toUtf8().constData(), nullptr, dependencyType, callbackFunction);
            }
            else
            {
                m_stateData->QueryDependsOnSourceBySourceDependency(toSearch.toUtf8().constData(), nullptr, dependencyType, callbackFunction);
            }
        }

        for (const QString& dep : results)
        {
            // note that 'results' contains the database paths (or placeholder ones), we need to find the real absolute ones
            if (dep.startsWith(m_placeHolderFileName))
            {
                continue;
            }
            
            QString firstMatchingFile = m_platformConfig->FindFirstMatchingFile(dep);
            if (firstMatchingFile.isEmpty())
            {
                continue;
            }
            finalDependencyList.insert(AZStd::make_pair(firstMatchingFile.toUtf8().constData(), dep.toUtf8().constData()));
        }
    }

    bool AssetProcessorManager::CanEarlyOutSourceFile(const QString normalizedPath, QString databaseSourceFileName, const ScanFolderInfo* scanFolder)
    {
        using namespace AzToolsFramework::AssetDatabase;
        // Can we early out and drop this file on the floor right now?
        // we can if:
        // * There are no new builders that may process it or old builders that have gone away.
        // * The builders that do process it have not emitted any new analysis versions.
        // * The file's own modtime has not changed and any dependencies it has modtime (hard deps or metafiles) has not changed.
        // on the other hand, if builders have been added or removed
        // we cannot early out at all because the analysis step also cleans up any left over products from jobs
        // that no longer exist.
        if (m_buildersAddedOrRemoved)
        {
            return false; // new builders can register to watch files that were previously not being processed.
        }

        AZ_Assert(scanFolder, "Scanfolder cannot be null.");

        // this is an extremely hot path, and we want this to be as quick as possible.
        // first, retrieve any information from last time that we have about this file.  This query should be as narrow as possible.

        AZStd::string fingerprintFromDatabase;
        if (!m_stateData->QuerySourceAnalysisFingerprint(databaseSourceFileName.toUtf8().constData(), scanFolder->ScanFolderID(), fingerprintFromDatabase))
        {
            return false;
        }

        if (fingerprintFromDatabase.empty())
        {
            return false; // we have no prior data for this entry, so we can't skip anything.
        }

        // the format for this data is a series of UUIDs colon-seperated.
        // it starts with the mod time fingerprint, followed by a series of entries for each builder that works on the file.
        // "modTimeFingerprint:Builder0Uuid~Builder0Fingerprint:Builder1Uuid~Builder1Fingerprint..."
        // the length of each string is predictable (38 characters)
        const AZStd::size_t lengthOfUUID = 38;
        const AZStd::size_t databaseFingerprintLength = fingerprintFromDatabase.size();

        if ((databaseFingerprintLength < lengthOfUUID) || (fingerprintFromDatabase[0] != '{'))
        {
            return false;
        }

        AZ::Uuid modtimeFingerprint = AZ::Uuid::CreateString(fingerprintFromDatabase.c_str(), lengthOfUUID);
        if (modtimeFingerprint.IsNull())
        {
            return false;
        }

        AZStd::string_view builderEntries(fingerprintFromDatabase.begin() + lengthOfUUID + 1, fingerprintFromDatabase.end());
        int numBuildersEmittingSourceDependencies = 0;
        // each entry here is of the format "builderID~builderFingerprint"
        // each part is exactly the size of a UUID, so we can check size instead of having to find or search.
        const size_t sizeOfOneEntry = (lengthOfUUID * 2) + 1;

        while (!builderEntries.empty())
        {
            if (builderEntries.size() < sizeOfOneEntry)
            {
                // corrupt data
                return false;
            }

            AZStd::string_view builderFPString(builderEntries.begin() + lengthOfUUID + 1, builderEntries.end());

            if ((builderEntries[0] != '{') || (builderFPString[0] != '{'))
            {
                return false; // corrupt or bad format.  We chose bracket guids for a reason!
            }

            AZ::Uuid builderID = AZ::Uuid::CreateString(builderEntries.data(), lengthOfUUID);
            AZ::Uuid builderFP = AZ::Uuid::CreateString(builderFPString.data(), lengthOfUUID);

            if ((builderID.IsNull()) || (builderFP.IsNull()))
            {
                return false;
            }

            // is it different?
            auto foundBuilder = m_builderDataCache.find(builderID);
            if (foundBuilder == m_builderDataCache.end())
            {
                // this file doesn't recognize the builder it was built with last time in the new list of builders, it definitely needs analysis!
                return false;
            }

            const BuilderData& data = foundBuilder->second;

            if (builderFP != data.m_fingerprint)
            {
                return false; // the builder changed!
            }

            // if we get here, its not dirty, but we need to know, does it emit deps?
            if ((data.m_flags & AssetBuilderSDK::AssetBuilderDesc::BF_EmitsNoDependencies) == 0)
            {
                numBuildersEmittingSourceDependencies++;
            }
            // advance to the next one.
            builderEntries = AZStd::string_view(builderEntries.begin() + sizeOfOneEntry, builderEntries.end());
            if (!builderEntries.empty())
            {
                // We add one for the colon that is the token that separates these entries.
                builderEntries = AZStd::string_view(builderEntries.begin() + 1, builderEntries.end());
            }
        }

        // if we get here it means that no builders which process this file have changed.
        // it also means that no new builders have been added or removed, and this file does have metadata that indicates
        // it had a fingeprint from last time.  So as long as its modtime and the modtime of any of its deps hasn't changed
        // we do not have to re-analyze it.

        // note this is an ordered set to ensure stability and is intentional.
        SourceFilesForFingerprintingContainer allDependencies;
        if (numBuildersEmittingSourceDependencies > 0)
        {
            // this automatically adds the original file to the dependency list:
            QueryAbsolutePathDependenciesRecursive(databaseSourceFileName, allDependencies, SourceFileDependencyEntry::DEP_Any, false);
        }
        else
        {
            // just add ourselves:
            allDependencies.insert(AZStd::make_pair(normalizedPath.toUtf8().constData(), databaseSourceFileName.toUtf8().constData()));
        }

        // add any metadata files too:
        AddMetadataFilesForFingerprinting(normalizedPath, allDependencies);

        // get the appropriate modtimes:
        AZStd::string modTimeArray;
        // reserve 17 chars for each, since its a series of 64-bit hex numbers, with a dash after each.
        modTimeArray.reserve((allDependencies.size() * 17));

        for (const auto& element : allDependencies)
        {
            modTimeArray.append(AssetUtilities::GetFileFingerprint(element.first, element.second));
            modTimeArray.append("-");
        }

        AZ::Uuid finalModTimeHash = AZ::Uuid::CreateName(modTimeArray.c_str());

        if (modtimeFingerprint != finalModTimeHash)
        {
            return false;
        }

        // mod time of self and depencencies is the same too, drop it!
        return true;
    }

    // given a file, add all the metadata files that could be related to it to an output vector
    void AssetProcessorManager::AddMetadataFilesForFingerprinting(QString absolutePathToFileToCheck, SourceFilesForFingerprintingContainer& outFilesToFingerprint)
    {
        QString metaDataFileName;
        QDir assetRoot;
        AssetUtilities::ComputeAssetRoot(assetRoot);
        QString gameName = AssetUtilities::ComputeGameName();
        QString fullPathToFile(absolutePathToFileToCheck);

        if (!m_cachedMetaFilesExistMap)
        {
            // one-time cache the actually existing metafiles.  These are files where its an actual path to a file
            // like "animations/skeletoninfo.xml" as the metafile, not when its a file thats next to each such file of a given type.
            for (int idx = 0; idx < m_platformConfig->MetaDataFileTypesCount(); idx++)
            {
                QPair<QString, QString> metaDataFileType = m_platformConfig->GetMetaDataFileTypeAt(idx);
                QString fullMetaPath = assetRoot.filePath(gameName + "/" + metaDataFileType.first);
                if (QFileInfo::exists(fullMetaPath))
                {
                    m_metaFilesWhichActuallyExistOnDisk.insert(metaDataFileType.first);
                }
            }
            m_cachedMetaFilesExistMap = true;
        }

        for (int idx = 0; idx < m_platformConfig->MetaDataFileTypesCount(); idx++)
        {
            QPair<QString, QString> metaDataFileType = m_platformConfig->GetMetaDataFileTypeAt(idx);

            if (!metaDataFileType.second.isEmpty() && !fullPathToFile.endsWith(metaDataFileType.second, Qt::CaseInsensitive))
            {
                continue;
            }

            if (m_metaFilesWhichActuallyExistOnDisk.find(metaDataFileType.first) != m_metaFilesWhichActuallyExistOnDisk.end())
            {
                QString fullMetaPath = assetRoot.filePath(gameName + "/" + metaDataFileType.first);
                metaDataFileName = fullMetaPath;
            }
            else
            {
                if (metaDataFileType.second.isEmpty())
                {
                    // ADD the metadata file extension to the end of the filename
                    metaDataFileName = fullPathToFile + "." + metaDataFileType.first;
                }
                else
                {
                    // REPLACE the file's extension with the metadata file extension.
                    QFileInfo fileInfo(absolutePathToFileToCheck);
                    metaDataFileName = fileInfo.path() + '/' + fileInfo.completeBaseName() + "." + metaDataFileType.first;
                }
            }

            QString databasePath;
            QString scanFolderPath;
            m_platformConfig->ConvertToRelativePath(metaDataFileName, databasePath, scanFolderPath, true);
            outFilesToFingerprint.insert(AZStd::make_pair(metaDataFileName.toUtf8().constData(), databasePath.toUtf8().constData()));
        }
    }

    // this function gets called whenever something changes about a file being processed, and checks to see
    // if it needs to write the fingerprint to the database.
    void AssetProcessorManager::UpdateAnalysisTrackerForFile(const char* fullPathToFile, AnalysisTrackerUpdateType updateType)
    {
        auto foundTrackingInfo = m_remainingJobsForEachSourceFile.find(fullPathToFile);
        if (foundTrackingInfo != m_remainingJobsForEachSourceFile.end())
        {
            // clear our the information about analysis on failed jobs.
            AnalysisTracker& analysisTracker = foundTrackingInfo->second;
            switch (updateType)
            {
            case AnalysisTrackerUpdateType::JobFailed:
                if (!analysisTracker.failedStatus)
                {
                    analysisTracker.failedStatus = true;
                    analysisTracker.m_remainingJobsSpawned = 0;
                    QMetaObject::invokeMethod(this, "FinishAnalysis", Qt::QueuedConnection, Q_ARG(AZStd::string, fullPathToFile));
                }
                break;
            case AnalysisTrackerUpdateType::JobStarted:
                if (!analysisTracker.failedStatus)
                {
                    ++analysisTracker.m_remainingJobsSpawned;
                }
                break;
            case AnalysisTrackerUpdateType::JobFinished:
            {
                if (!analysisTracker.failedStatus)
                {
                    --analysisTracker.m_remainingJobsSpawned;
                    if (analysisTracker.m_remainingJobsSpawned == 0)
                    {
                        QMetaObject::invokeMethod(this, "FinishAnalysis", Qt::QueuedConnection, Q_ARG(AZStd::string, fullPathToFile));
                    }
                }
            }
            break;
            }
        }
    }

    void AssetProcessorManager::UpdateAnalysisTrackerForFile(const JobEntry &entry, AnalysisTrackerUpdateType updateType)
    {
        // it is assumed that watch folder path / path relative to watch folder are already normalized and such.
        QString absolutePath = QDir(entry.m_watchFolderPath).absoluteFilePath(entry.m_pathRelativeToWatchFolder);
        UpdateAnalysisTrackerForFile(absolutePath.toUtf8().constData(), updateType);
    }

} // namespace AssetProcessor

#include <native/AssetManager/assetProcessorManager.moc>
