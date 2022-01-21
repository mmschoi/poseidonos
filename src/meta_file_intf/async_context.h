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

#pragma once

#include <functional>

#include "meta_file_include.h"

namespace pos
{
class AsyncMetaFileIoCtx;

using MetaIoCbPtr = std::function<void(AsyncMetaFileIoCtx*)>;
using MetaFileIoCbPtr = std::function<int(void*)>;

class AsyncMetaFileIoCtx
{
public:
    AsyncMetaFileIoCtx(void);
// LCOV_EXCL_START
    virtual ~AsyncMetaFileIoCtx(void) = default;
// LCOV_EXCL_STOP

    virtual void HandleIoComplete(void* data);
    virtual int GetError(void) const;
    virtual uint64_t GetLength(void) const;

    virtual void SetTopPriority(void);
    virtual void ClearTopPriority(void);
    virtual bool IsTopPriority(void) const;

    MetaFsIoOpcode opcode;
    int fd;
    uint64_t fileOffset;
    uint64_t length;
    char* buffer;
    MetaIoCbPtr callback;

    int error;
    MetaFileIoCbPtr ioDoneCheckCallback;
    uint32_t vsid;
    bool topPriority;
};

} // namespace pos
