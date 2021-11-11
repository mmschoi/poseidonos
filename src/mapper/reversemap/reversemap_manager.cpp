/*
 *   BSD LICENSE
 *   Copyright (c) 2021 Samsung Electronics Corporation
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Corporation nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "src/metafs/include/metafs_service.h"
#include "src/array_mgmt/array_manager.h"
#include "src/mapper/reversemap/reversemap_manager.h"
#include "src/meta_file_intf/mock_file_intf.h"
#ifndef IBOF_CONFIG_USE_MOCK_FS
#include "src/metafs/metafs_file_intf.h"
#endif

#include <string>

namespace pos
{

ReverseMapManager::ReverseMapManager(VSAMapManager* ivsaMap, IStripeMap* istripeMap, IVolumeManager* vol, MapperAddressInfo* addrInfo_)
: mpageSize(0),
  numMpagesPerStripe(0),
  fileSizePerStripe(0),
  fileSizeWholeRevermap(0),
  revMapPacks(nullptr),
  revMapWholefile(nullptr),
  iVSAMap(ivsaMap),
  iStripeMap(istripeMap),
  volumeManager(vol),
  addrInfo(addrInfo_)
{
}

ReverseMapManager::~ReverseMapManager(void)
{
    if (revMapWholefile != nullptr)
    {
        revMapWholefile->Close();
        delete revMapWholefile;
        revMapWholefile = nullptr;
    }

    if (revMapPacks != nullptr)
    {
        delete [] revMapPacks;
        revMapPacks = nullptr;
    }
}

void
ReverseMapManager::Init(void)
{
    if (volumeManager == nullptr)
    {
        volumeManager = VolumeServiceSingleton::Instance()->GetVolumeManager(addrInfo->GetArrayName());
    }

    mpageSize = addrInfo->GetMpageSize();
    _SetNumMpages();

    int maxVsid = addrInfo->GetMaxVSID();
    fileSizeWholeRevermap = fileSizePerStripe * maxVsid;

    // Create MFS and Open the file for whole reverse map
    if (addrInfo->IsUT() == false)
    {
        revMapWholefile = new FILESTORE("RevMapWhole", addrInfo->GetArrayId());
    }
    else
    {
        revMapWholefile = new MockFileIntf("RevMapWhole", addrInfo->GetArrayId());
    }
    if (revMapWholefile->DoesFileExist() == false)
    {
        POS_TRACE_INFO(EID(REVMAP_FILE_SIZE), "fileSizePerStripe:{}  maxVsid:{}  fileSize:{} for RevMapWhole",
                        fileSizePerStripe, maxVsid, fileSizeWholeRevermap);

        int ret = revMapWholefile->Create(fileSizeWholeRevermap);
        if (ret != 0)
        {
            POS_TRACE_ERROR(EID(REVMAP_FILE_SIZE), "RevMapWhole file Create failed, ret:{}", ret);
            assert(false);
        }
    }
    revMapWholefile->Open();

    // Make ReverseMapPack:: objects by the number of WriteBuffer stripes
    uint32_t numWbStripes = addrInfo->GetNumWbStripes();
    revMapPacks = new ReverseMapPack[numWbStripes]();
    for (StripeId wbLsid = 0; wbLsid < numWbStripes; ++wbLsid)
    {
        std::string arrName = addrInfo->GetArrayName();
        revMapPacks[wbLsid].Init(mpageSize, numMpagesPerStripe, revMapWholefile, arrName);
        revMapPacks[wbLsid].Init(volumeManager, wbLsid, iVSAMap, iStripeMap);
    }
}

void
ReverseMapManager::Dispose(void)
{
    if (revMapWholefile != nullptr)
    {
        if (revMapWholefile->IsOpened() == true)
        {
            revMapWholefile->Close();
        }
        delete revMapWholefile;
        revMapWholefile = nullptr;
    }
    if (revMapPacks != nullptr)
    {
        delete [] revMapPacks;
        revMapPacks = nullptr;
    }
}
//----------------------------------------------------------------------------//
ReverseMapPack*
ReverseMapManager::GetReverseMapPack(StripeId wbLsid)
{
    return &revMapPacks[wbLsid];
}

ReverseMapPack*
ReverseMapManager::AllocReverseMapPack(bool gcDest)
{
    ReverseMapPack* obj = new ReverseMapPack;
    std::string arrName = addrInfo->GetArrayName();
    obj->Init(mpageSize, numMpagesPerStripe, revMapWholefile, arrName);
    if (gcDest == true)
    {
        obj->Init(volumeManager, UNMAP_STRIPE, iVSAMap, iStripeMap);
    }
    return obj;
}

uint64_t
ReverseMapManager::GetReverseMapPerStripeFileSize(void)
{
    return fileSizePerStripe;
}

uint64_t
ReverseMapManager::GetWholeReverseMapFileSize(void)
{
    return fileSizeWholeRevermap;
}

int
ReverseMapManager::LoadWholeReverseMap(char* pBuffer)
{
    int ret = revMapWholefile->IssueIO(MetaFsIoOpcode::Read, 0, fileSizeWholeRevermap, pBuffer);
    return ret;
}

int
ReverseMapManager::StoreWholeReverseMap(char* pBuffer)
{
    int ret = revMapWholefile->IssueIO(MetaFsIoOpcode::Write, 0, fileSizeWholeRevermap, pBuffer);
    return ret;
}

void
ReverseMapManager::WaitAllPendingIoDone(void)
{
    uint32_t numWbStripes = addrInfo->GetNumWbStripes();
    POS_TRACE_INFO(EID(MAP_FLUSH_COMPLETED), "[Mapper ReverseMapMAnager] Wait For Pending IO");
    for (StripeId wbLsid = 0; wbLsid < numWbStripes; ++wbLsid)
    {
        revMapPacks[wbLsid].WaitPendingIoDone();
    }
}

int
ReverseMapManager::_SetNumMpages(void)
{
    uint32_t blksPerStripe = addrInfo->GetBlksPerStripe();
    uint32_t entriesPerNormalPage = (mpageSize / REVMAP_SECTOR_SIZE) * (REVMAP_SECTOR_SIZE / REVMAP_ENTRY_SIZE);
    uint32_t entriesPerFirstPage = entriesPerNormalPage - (REVMAP_SECTOR_SIZE / REVMAP_ENTRY_SIZE);

    if (blksPerStripe <= entriesPerFirstPage)
    {
        numMpagesPerStripe = 1;
    }
    else
    {
        numMpagesPerStripe = 1 + DivideUp(blksPerStripe - entriesPerFirstPage, entriesPerNormalPage);
    }

    // Set fileSize
    fileSizePerStripe = mpageSize * numMpagesPerStripe;

    POS_TRACE_INFO(EID(REVMAP_FILE_SIZE),
        "[ReverseMap Info] entriesPerNormalPage:{}  entriesPerFirstPage:{}  numMpagesPerStripe:{}  fileSizePerStripe:{}",
        entriesPerNormalPage, entriesPerFirstPage, numMpagesPerStripe,
        fileSizePerStripe);

    return 0;
}

} // namespace pos
