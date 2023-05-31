// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <any>
#include <future>
#include <memory>
#include <boost/serialization/shared_ptr.hpp>
#include <core/loader/loader.h>
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/directory_backend.h"
#include "core/global.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::BOSS {
// File header info from
// https://www.3dbrew.org/wiki/SpotPass#Payload_Content_Header
// So the total header is only 52 bytes long

constexpr u32 boss_header_length = 0x34;
// 52 bytes doesn't align nicely into 8-byte words
#pragma pack(push, 4)
struct BossHeader {
    u8 header_length;
    std::array<u8, 11> zero1;
    u32_be unknown;
    u32_be download_date;
    std::array<u8, 4> zero2;
    u64_be program_id;
    std::array<u8, 4> zero3;
    u32_be datatype;
    u32_be payload_size;
    u32_be ns_data_id;
    u32_be version;
};
#pragma pack(pop)

static_assert(sizeof(BossHeader) == 0x34, "BossHeader struct isn't exactly 0x34 bytes long!");

// Payload header info from
// https://www.3dbrew.org/wiki/SpotPass#Content_Container
// So the total header is only 40 bytes long

constexpr u32 boss_payload_header_length = 0x28;
constexpr u32 boss_magic = Loader::MakeMagic('b', 'o', 's', 's');
constexpr u32 boss_payload_magic = 0x10001;
constexpr u64 news_prog_id = 0x0004013000003502;
// 40 bytes doesn't align nicely into 8-byte words either
#pragma pack(push, 4)
struct BossPayloadHeader {
    u32_le boss;
    u32_be magic;
    u32_be filesize;
    u64_be release_date;
    u16_be one;
    INSERT_PADDING_BYTES(2);
    u16_be hash_type;
    u16_be rsa_size;
    std::array<u8, 0xC> iv_start;
};
#pragma pack(pop)

static_assert(sizeof(BossPayloadHeader) == 0x28,
              "BossPayloadHeader struct isn't exactly 0x28 bytes long!");

constexpr u32 boss_content_header_length = 0x132;
constexpr u32 boss_header_with_hash_length = 0x13C;
constexpr u32 boss_entire_header_length = boss_content_header_length + boss_header_with_hash_length;
constexpr u32 boss_extdata_header_length = 0x18;
constexpr u32 boss_a_entry_size = 0x800;
constexpr u32 boss_s_entry_size = 0xC00;
constexpr u32 boss_save_header_size = 4;
constexpr u32 boss_s_prog_id_offset = 0x10;
constexpr u32 boss_s_task_id_offset = 0x18;
constexpr u32 boss_s_url_offset = 0x21C;

struct NsDataEntry {
    std::string filename;
    BossHeader header;
};

constexpr u8 task_id_size = 8;
constexpr u32 files_to_read = 100;

enum NsDataHeaderInfoType : u8 {
    PROGRAM_ID,
    UNKNOWN,
    DATATYPE,
    PAYLOAD_SIZE,
    NS_DATA_ID,
    VERSION,
    EVERYTHING
};

enum TaskStatus : u8 { TASK_SUCCESS = 0, TASK_RUNNING = 2, TASK_NOT_STARTED = 5, TASK_FAILED = 7 };

constexpr u16 interval_id = 0x03;
constexpr u16 duration_id = 0x04;
constexpr u16 url_id = 0x07;
constexpr size_t url_size = 0x200;
constexpr u16 headers_id = 0x0D;
constexpr size_t headers_size = 0x360;
constexpr u16 certid_id = 0x0E;
constexpr u16 certidlist_id = 0x0F;
constexpr size_t certidlist_size = 3;
constexpr u16 loadcert_id = 0x10;
constexpr u16 loadrootcert_id = 0x11;
constexpr u16 totaltasks_id = 0x35;
constexpr u16 taskidlist_id = 0x36;
constexpr size_t taskidlist_size = 0x400;

struct BossTaskProperties {
    std::future<bool> download_task;
    bool task_result;
    u32 times_checked;
    std::map<u16, std::any> props{
        {0x00, u8()},
        {0x01, u8()},
        {0x02, u32()},
        // interval
        {interval_id, u32()},
        // duration
        {duration_id, u32()},
        {0x05, u8()},
        {0x06, u8()},
        // url
        {url_id, std::vector<u8>(url_size)},
        {0x08, u32()},
        {0x09, u8()},
        {0x0A, std::vector<u8>(0x100)},
        {0x0B, std::vector<u8>(0x200)},
        {0x0C, u32()},
        // headers
        {headers_id, std::vector<u8>(headers_size)},
        // certid
        {certid_id, u32()},
        // certidlist
        {certidlist_id, std::vector<u32>(certidlist_size)},
        // loadcert (bool)
        {loadcert_id, u8()},
        // loadrootcert (bool)
        {loadrootcert_id, u8()},
        {0x12, u8()},
        {0x13, u32()},
        {0x14, u32()},
        {0x15, std::vector<u8>(0x40)},
        {0x16, u32()},
        {0x18, u8()},
        {0x19, u8()},
        {0x1A, u8()},
        {0x1B, u32()},
        {0x1C, u32()},
        // totaltasks
        {totaltasks_id, u16()},
        // taskidlist
        {taskidlist_id, std::vector<u8>(taskidlist_size)},
        {0x3B, u32()},
        {0x3E, std::vector<u8>(0x200)},
        {0x3F, u8()},
    };
};

constexpr std::array<u8, 8> boss_system_savedata_id{
    0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x01, 0x00,
};

constexpr std::array<u8, 4> boss_system_savedata_header{0x00, 0x80, 0x34, 0x12};

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module() = default;

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> boss, const char* name, u32 max_session);
        ~Interface() = default;

    protected:
        /**
         * BOSS::InitializeSession service function
         *  Inputs:
         *      0 : Header Code[0x00010082]
         *    1-2 : programID, normally zero for using the programID determined from the input PID
         *      3 : 0x20, ARM11-kernel processID translate-header.
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void InitializeSession(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterStorage service function
         *  Inputs:
         *      0 : Header Code[0x00020100]
         *    1-2 : u64 extdataID
         *      3 : u32 boss_size
         *      4 : u8 extdata_type: 0 = NAND, 1 = SD
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetStorageInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::UnregisterStorage service function
         *  Inputs:
         *      0 : Header Code[0x00030000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void UnregisterStorage(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetStorageInfo service function
         *  Inputs:
         *      0 : Header Code[0x00040000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         */
        void GetStorageInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterPrivateRootCa service function
         *  Inputs:
         *      0 : Header Code[0x00050042]
         *      1 : u32 Size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void RegisterPrivateRootCa(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterPrivateClientCert service function
         *  Inputs:
         *      0 : Header Code[0x00060084]
         *      1 : u32 buffer 1 size
         *      2 : u32 buffer 2 size
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : u32 buff_addr1
         *      5 : MappedBufferDesc2(permission = R)
         *      6 : u32 buff_addr2
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff1_size << 4 | 0xA
         *      3 : u32 buff_addr1
         *      4 : buff2_size << 4 | 0xA
         *      5 : u32 buff_addr2
         */
        void RegisterPrivateClientCert(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNewArrivalFlag service function
         *  Inputs:
         *      0 : Header Code[0x00070000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 flag
         */
        void GetNewArrivalFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterNewArrivalEvent service function
         *  Inputs:
         *      0 : Header Code[0x00080002]
         *      1 : u32 unknown1
         *      2 : u32 unknown2
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void RegisterNewArrivalEvent(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetOptoutFlag service function
         *  Inputs:
         *      0 : Header Code[0x00090040]
         *      1 : u8 output_flag
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetOptoutFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetOptoutFlag service function
         *  Inputs:
         *      0 : Header Code[0x000A0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 output_flag
         */
        void GetOptoutFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterTask service function
         *  Inputs:
         *      0 : Header Code[0x000B00C2]
         *      1 : TaskID buffer size
         *      2 : u8 unknown value, Usually zero, regardless of HTTP GET/POST.
         *      3 : u8 unknown value, Usually zero, regardless of HTTP GET/POST.
         *      4 : MappedBufferDesc1(permission = R)
         *      5 : buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void RegisterTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::UnregisterTask service function
         *  Inputs:
         *      0 : Header Code[0x000C0082]
         *      1 : TaskID buffer size
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void UnregisterTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::ReconfigureTask service function
         *  Inputs:
         *      0 : Header Code[0x000D0082]
         *      1 : TaskID buffer size
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void ReconfigureTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskIdList service function
         *  Inputs:
         *      0 : Header Code[0x000E0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetTaskIdList(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetStepIdList service function
         *  Inputs:
         *      0 : Header Code[0x000F0042]
         *      1 : u32 buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void GetStepIdList(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataIdList service function
         *  Inputs:
         *      0 : Header Code[0x00100102]
         *      1 : u32 filter
         *      2 : u32 Buffer size in words(max entries)
         *      3 : u16, starting word-index in the internal NsDataId list
         *      4 : u32, start_NsDataId
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdList(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataIdList1 service function
         *  Inputs:
         *      0 : Header Code[0x00110102]
         *      1 : u32 filter
         *      2 : u32 Buffer size in words(max entries)
         *      3 : u16, starting word-index in the internal NsDataId list
         *      4 : u32, start_NsDataId
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdList1(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataIdList2 service function
         *  Inputs:
         *      0 : Header Code[0x00120102]
         *      1 : u32 filter
         *      2 : u32 Buffer size in words(max entries)
         *      3 : u16, starting word-index in the internal NsDataId list
         *      4 : u32, start_NsDataId
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdList2(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataIdList3 service function
         *  Inputs:
         *      0 : Header Code[0x00130102]
         *      1 : u32 filter
         *      2 : u32 Buffer size in words(max entries)
         *      3 : u16, starting word-index in the internal NsDataId list
         *      4 : u32, start_NsDataId
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr

         */
        void GetNsDataIdList3(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SendProperty service function
         *  Inputs:
         *      0 : Header Code[0x00140082]
         *      1 : u16 PropertyID
         *      2 : u32 size
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void SendProperty(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SendPropertyHandle service function
         *  Inputs:
         *      0 : Header Code[0x00150042]
         *      2 : u16 PropertyID
         *      3 : 0x0
         *      4 : Handle
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SendPropertyHandle(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::ReceiveProperty service function
         *  Inputs:
         *      0 : Header Code[0x00160082]
         *      1 : u16 PropertyID
         *      2 : u32 Size
         *      3 : MappedBufferDesc(permission = W)
         *      4 : u32 buff addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Actual read size
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void ReceiveProperty(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::UpdateTaskInterval service function
         *  Inputs:
         *      0 : Header Code[0x00170082]
         *      1 : u32 unknown value
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void UpdateTaskInterval(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::UpdateTaskCount service function
         *  Inputs:
         *      0 : Header Code[0x00180082]
         *      1 : u32 buff_size
         *      2 : u32 unknown2
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void UpdateTaskCount(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskInterval service function
         *  Inputs:
         *      0 : Header Code[0x00190042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskInterval(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskCount service function
         *  Inputs:
         *      0 : Header Code[0x001A0042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskCount(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskServiceStatus service function
         *  Inputs:
         *      0 : Header Code[0x001B0042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskServiceStatus(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::StartTask service function
         *  Inputs:
         *      0 : Header Code[0x001C0042]
         *      1 : TaskID buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void StartTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::StartTaskImmediate service function
         *  Inputs:
         *      0 : Header Code[0x001D0042]
         *      1 : TaskID buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void StartTaskImmediate(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::CancelTask service function
         *  Inputs:
         *      0 : Header Code[0x001E0042]
         *      1 : TaskID buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void CancelTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskFinishHandle service function
         *  Inputs:
         *      0 : Header Code[0x001F0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : 0x0
         *      3 : Task Finish Handle
         */
        void GetTaskFinishHandle(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskState service function
         *  Inputs:
         *      0 : Header Code[0x00200082]
         *      1 : TaskID buffer size
         *      2 : u8 state
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 TaskStatus
         *      3 : u32 Current state value for task PropertyID 0x4
         *      4 : u8 unknown value
         *      5 : buff_size << 4 | 0xA
         *      6 : u32 buff_addr
         */
        void GetTaskState(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskResult service function
         *  Inputs:
         *      0 : Header Code[0x00210042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : u32 unknown value
         *      4 : u8 unknown value
         *      5 : buff_size << 4 | 0xA
         *      6 : u32 buff_addr
         */
        void GetTaskResult(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskCommErrorCode service function
         *  Inputs:
         *      0 : Header Code[0x00220042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : u32 unknown value
         *      4 : u8 unknown value
         *      5 : buff_size << 4 | 0xA
         *      6 : u32 buff_addr
         */
        void GetTaskCommErrorCode(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskStatus service function
         *  Inputs:
         *      0 : Header Code[0x002300C2]
         *      1 : u32 size
         *      2 : u8 unknown value
         *      3 : u8 unknown value
         *      4 : MappedBufferDesc(permission = R)
         *      5 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskStatus(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskError service function
         *  Inputs:
         *      0 : Header Code[0x00240082]
         *      1 : u32 size
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskError(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskInfo service function
         *  Inputs:
         *      0 : Header Code[0x00250082]
         *      1 : u32 size
         *      2 : u8 unknown value
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void GetTaskInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::DeleteNsData service function
         *  Inputs:
         *      0 : Header Code[0x00260040]
         *      1 : u32 NsDataID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DeleteNsData(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataHeaderInfo service function
         *  Inputs:
         *      0 : Header Code[0x002700C2]
         *      1 : u32, NsDataID
         *      2 : u8, type
         *      3 : u32, Size
         *      4 : MappedBufferDesc(permission = W)
         *      5 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xC
         *      3 : u32, buff_addr
         */
        void GetNsDataHeaderInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::ReadNsData service function
         *  Inputs:
         *      0 : Header Code[0x00280102]
         *      1 : u32, NsDataID
         *    2-3 : u64, offset
         *      4 : u32, Size
         *      5 : MappedBufferDesc(permission = W)
         *      6 : u32, buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 Actual read size
         *      3 : u32, unknown value
         *      4 : buff_size << 4 | 0xC
         *      5 : u32, buff_addr
         */
        void ReadNsData(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetNsDataAdditionalInfo service function
         *  Inputs:
         *      0 : Header Code[0x00290080]
         *      1 : u32 unknown value
         *      2 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataAdditionalInfo service function
         *  Inputs:
         *      0 : Header Code[0x002A0040]
         *      1 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         */
        void GetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetNsDataNewFlag service function
         *  Inputs:
         *      0 : Header Code[0x002B0080]
         *      1 : u32 unknown value
         *      2 : u8 flag
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNsDataNewFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataNewFlag service function
         *  Inputs:
         *      0 : Header Code[0x002C0040]
         *      1 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 flag
         */
        void GetNsDataNewFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetNsDataLastUpdate service function
         *  Inputs:
         *      0 : Header Code[0x002D0040]
         *      1 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : u32 unknown value
         */
        void GetNsDataLastUpdate(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetErrorCode service function
         *  Inputs:
         *      0 : Header Code[0x002E0040]
         *      1 : u8 input
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         */
        void GetErrorCode(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterStorageEntry service function
         *  Inputs:
         *      0 : Header Code[0x002F0140]
         *      1 : u32 unknown value
         *      2 : u32 unknown value
         *      3 : u32 unknown value
         *      4 : u16 unknown value
         *      5 : u8 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void RegisterStorageEntry(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetStorageEntryInfo service function
         *  Inputs:
         *      0 : Header Code[0x00300000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 unknown value
         *      3 : u16 unknown value
         */
        void GetStorageEntryInfo(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetStorageOption service function
         *  Inputs:
         *      0 : Header Code[0x00310100]
         *      1 : u8 unknown value
         *      2 : u32 unknown value
         *      3 : u16 unknown value
         *      4 : u16 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetStorageOption(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetStorageOption service function
         *  Inputs:
         *      0 : Header Code[0x00320000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 unknown value
         *      3 : u32 unknown value
         *      4 : u16 unknown value
         *      5 : u16 unknown value
         */
        void GetStorageOption(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::StartBgImmediate service function
         *  Inputs:
         *      0 : Header Code[0x00330042]
         *      1 : TaskID buffer size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32, buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32, buff_addr
         */
        void StartBgImmediate(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskProperty0 service function
         *  Inputs:
         *      0 : Header Code[0x00340042]
         *      1 : u32 size
         *      2 : MappedBufferDesc(permission = R)
         *      3 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 current state
         *      3 : buff_size << 4 | 0xA
         *      4 : u32 buff_addr
         */
        void GetTaskProperty0(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::RegisterImmediateTask service function
         *  Inputs:
         *      0 : Header Code[0x003500C2]
         *      1 : u32 size
         *      2 : u8 unknown value
         *      3 : u8 unknown value
         *      4 : MappedBufferDesc(permission = R)
         *      5 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void RegisterImmediateTask(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::SetTaskQuery service function
         *  Inputs:
         *      0 : Header Code[0x00360084]
         *      1 : u32 buffer1 size
         *      2 : u32 buffer2 size
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : u32 buff1_addr
         *      5 : MappedBufferDesc2(permission = R)
         *      6 : u32 buff2_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff1_size << 4 | 0xA
         *      3 : u32 buff1_addr
         *      4 : buff2_size << 4 | 0xA
         *      5 : u32 buff2_addr
         */
        void SetTaskQuery(Kernel::HLERequestContext& ctx);

        /**
         * BOSS::GetTaskQuery service function
         *  Inputs:
         *      0 : Header Code[0x00370084]
         *      1 : u32 buffer1 size
         *      2 : u32 buffer2 size
         *      3 : MappedBufferDesc1(permission = R)
         *      4 : u32 buff1_addr
         *      5 : MappedBufferDesc2(permission = W)
         *      6 : u32 buff2_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff1_size << 4 | 0xA
         *      3 : u32 buff1_addr
         *      4 : buff2_size << 4 | 0xC
         *      5 : u32 buff2_addr
         */
        void GetTaskQuery(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::InitializeSessionPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x04010082]
         *    1-2 : programID, normally zero for using the programID determined from the input PID
         *      3 : 0x20, ARM11-kernel processID translate-header.
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void InitializeSessionPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetAppNewFlag service function
         *  Inputs:
         *      0 : Header Code[0x04040080]
         *    1-2 : u64 ProgramID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 flag, 0 = nothing new, 1 = new content
         */
        void GetAppNewFlag(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetNsDataIdListPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x040D0182]
         *    1-2 : u64 ProgramID
         *      3 : u32 filter
         *      4 : u32 Buffer size in words(max entries)
         *      5 : u16, starting word-index in the internal NsDataId list
         *      6 : u32, start_NsDataId
         *      7 : MappedBufferDesc(permission = W)
         *      8 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdListPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetNsDataIdListPrivileged1 service function
         *  Inputs:
         *      0 : Header Code[0x040E0182]
         *    1-2 : u64 ProgramID
         *      3 : u32 filter
         *      4 : u32 Buffer size in words(max entries)
         *      5 : u16, starting word-index in the internal NsDataId list
         *      6 : u32, start_NsDataId
         *      7 : MappedBufferDesc(permission = W)
         *      8 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u16 Actual number of output entries
         *      3 : u16 Last word-index copied to output in the internal NsDataId list
         *      4 : buff_size << 4 | 0xC
         *      5 : u32 buff_addr
         */
        void GetNsDataIdListPrivileged1(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::SendPropertyPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x04130082]
         *      1 : u16 PropertyID
         *      2 : u32 size
         *      3 : MappedBufferDesc(permission = R)
         *      4 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xA
         *      3 : u32 buff_addr
         */
        void SendPropertyPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::DeleteNsDataPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x041500C0]
         *    1-2 : u64 ProgramID
         *      3 : u32 NsDataID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DeleteNsDataPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetNsDataHeaderInfoPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x04160142]
         *    1-2 : u64 ProgramID
         *      3 : u32, NsDataID
         *      4 : u8, type
         *      5 : u32, Size
         *      6 : MappedBufferDesc(permission = W)
         *      7 : u32 buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : buff_size << 4 | 0xC
         *      3 : u32, buff_addr
         */
        void GetNsDataHeaderInfoPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::ReadNsDataPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x04170182]
         *    1-2 : u64 ProgramID
         *      3 : u32, NsDataID
         *    4-5 : u64, offset
         *      6 : u32, Size
         *      7 : MappedBufferDesc(permission = W)
         *      8 : u32, buff_addr
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u32 Actual read size
         *      3 : u32, unknown value
         *      4 : buff_size << 4 | 0xC
         *      5 : u32, buff_addr
         */
        void ReadNsDataPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::SetNsDataNewFlagPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x041A0100]
         *    1-2 : u64 ProgramID
         *      3 : u32 unknown value
         *      4 : u8 flag
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNsDataNewFlagPrivileged(Kernel::HLERequestContext& ctx);

        /**
         * BOSS_P::GetNsDataNewFlagPrivileged service function
         *  Inputs:
         *      0 : Header Code[0x041B00C0]
         *    1-2 : u64 ProgramID
         *      3 : u32 unknown value
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 flag
         */
        void GetNsDataNewFlagPrivileged(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> boss;

    private:
        u8 new_arrival_flag;
        u8 ns_data_new_flag;
        u8 ns_data_new_flag_privileged;
        u8 output_flag;
        std::map<std::string, BossTaskProperties> task_id_list;
        BossTaskProperties cur_props;

        static FileSys::Path GetBossDataDir();
        static bool DownloadBossDataFromURL(std::string url, std::string file_name);
        std::vector<NsDataEntry> GetNsDataEntries();
        u32 GetBossExtDataFiles(std::vector<FileSys::Entry>& boss_files);
        u16 GetOutputEntries(u32 filter, u32 max_entries, Kernel::MappedBuffer& buffer);
        bool GetNsDataEntryFromID(u32 ns_data_id, NsDataEntry& entry);
        std::pair<u8, u32> GetTaskStatusAndDuration(std::string task_id, bool wait_on_result);

        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& new_arrival_flag;
            ar& ns_data_new_flag;
            ar& ns_data_new_flag_privileged;
            ar& output_flag;
        }
        friend class boost::serialization::access;
    };

private:
    std::shared_ptr<Kernel::Event> task_finish_event;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& task_finish_event;
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::BOSS

namespace boost::serialization {
template <class Archive>
void load_construct_data(Archive& ar, Service::BOSS::Module* t, const unsigned int) {
    ::new (t) Service::BOSS::Module(Core::Global<Core::System>());
}
} // namespace boost::serialization
