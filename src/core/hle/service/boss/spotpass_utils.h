// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <future>
#include <memory>
#include <variant>
#include <boost/serialization/shared_ptr.hpp>
#include <core/loader/loader.h>
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/directory_backend.h"
#include "core/global.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/service.h"

namespace Service::BOSS {

// File header info from
// https://www.3dbrew.org/wiki/SpotPass#Payload_Content_Header
// So the total header is only 52 bytes long

constexpr u32 BOSS_HEADER_LENGTH = 0x34;
// 52 bytes doesn't align nicely into 8-byte words
#pragma pack(push, 4)
struct BossHeader {
    u8 header_length;
    INSERT_PADDING_BYTES(11);
    u32_be unknown;
    u32_be download_date;
    INSERT_PADDING_BYTES(4);
    u64_be program_id;
    INSERT_PADDING_BYTES(4);
    u32_be datatype;
    u32_be payload_size;
    u32_be ns_data_id;
    u32_be version;
};
#pragma pack(pop)

static_assert(sizeof(BossHeader) == 0x34, "BossHeader has incorrect size");

// Payload header info from
// https://www.3dbrew.org/wiki/SpotPass#Content_Container
// So the total header is only 40 bytes long

constexpr u32 BOSS_PAYLOAD_HEADER_LENGTH = 0x28;
constexpr u32 BOSS_MAGIC = Loader::MakeMagic('b', 'o', 's', 's');
constexpr u32 BOSS_PAYLOAD_MAGIC = 0x10001;
constexpr u64 NEWS_PROG_ID = 0x0004013000003502;
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

static_assert(sizeof(BossPayloadHeader) == 0x28, "BossPayloadHeader has incorrect size");

constexpr u32 BOSS_CONTENT_HEADER_LENGTH = 0x132;
constexpr u32 BOSS_HEADER_WITH_HASH_LENGTH = 0x13C;
constexpr u32 BOSS_ENTIRE_HEADER_LENGTH = BOSS_CONTENT_HEADER_LENGTH + BOSS_HEADER_WITH_HASH_LENGTH;
constexpr u32 BOSS_EXTDATA_HEADER_LENGTH = 0x18;
constexpr u32 BOSS_A_ENTRY_SIZE = 0x800;
constexpr u32 BOSS_S_ENTRY_SIZE = 0xC00;
constexpr u32 BOSS_SAVE_HEADER_SIZE = 4;
constexpr u32 BOSS_S_PROG_ID_OFFSET = 0x10;
constexpr u32 BOSS_S_TASK_ID_OFFSET = 0x18;
constexpr u32 BOSS_S_URL_OFFSET = 0x21C;

struct NsDataEntry {
    std::string filename;
    BossHeader header;
};

constexpr ResultCode RESULT_FAILED(1);
constexpr u8 TASK_ID_SIZE = 8;

enum class NsDataHeaderInfoType : u8 {
    ProgramId,
    Unknown,
    Datatype,
    PayloadSize,
    NsDataId,
    Version,
    Everything,
};

struct NsDataHeaderInfo {
    u64 program_id;
    INSERT_PADDING_BYTES(4);
    u32 datatype;
    u32 payload_size;
    u32 ns_data_id;
    u32 version;
    INSERT_PADDING_BYTES(4);
};

static_assert(sizeof(NsDataHeaderInfo) == 0x20, "NsDataHeaderInfo has incorrect size");

enum class TaskStatus : u8 {
    Success = 0,
    Running = 2,
    NotStarted = 5,
    Failed = 7,
};

enum class PropertyID : u16 {
    Interval = 0x03,
    Duration = 0x04,
    Url = 0x07,
    Headers = 0x0D,
    CertId = 0x0E,
    CertIdList = 0x0F,
    LoadCert = 0x10,
    LoadRootCert = 0x11,
    TotalTasks = 0x35,
    TaskIdList = 0x36,
};

constexpr size_t URL_SIZE = 0x200;
constexpr size_t HEADERS_SIZE = 0x360;
constexpr size_t CERTIDLIST_SIZE = 3;
constexpr size_t TASKIDLIST_SIZE = 0x400;

struct BossTaskProperties {
    std::future<bool> download_task;
    bool task_result;
    u32 times_checked;
    std::map<PropertyID, std::variant<u8, u16, u32, std::vector<u8>, std::vector<u32>>> props{
        {static_cast<PropertyID>(0x00), u8()},
        {static_cast<PropertyID>(0x01), u8()},
        {static_cast<PropertyID>(0x02), u32()},
        // interval
        {PropertyID::Interval, u32()},
        // duration
        {PropertyID::Duration, u32()},
        {static_cast<PropertyID>(0x05), u8()},
        {static_cast<PropertyID>(0x06), u8()},
        // url
        {PropertyID::Url, std::vector<u8>(URL_SIZE)},
        {static_cast<PropertyID>(0x08), u32()},
        {static_cast<PropertyID>(0x09), u8()},
        {static_cast<PropertyID>(0x0A), std::vector<u8>(0x100)},
        {static_cast<PropertyID>(0x0B), std::vector<u8>(0x200)},
        {static_cast<PropertyID>(0x0C), u32()},
        // headers
        {PropertyID::Headers, std::vector<u8>(HEADERS_SIZE)},
        // certid
        {PropertyID::CertId, u32()},
        // certidlist
        {PropertyID::CertIdList, std::vector<u32>(CERTIDLIST_SIZE)},
        // loadcert (bool)
        {PropertyID::LoadCert, u8()},
        // loadrootcert (bool)
        {PropertyID::LoadRootCert, u8()},
        {static_cast<PropertyID>(0x12), u8()},
        {static_cast<PropertyID>(0x13), u32()},
        {static_cast<PropertyID>(0x14), u32()},
        {static_cast<PropertyID>(0x15), std::vector<u8>(0x40)},
        {static_cast<PropertyID>(0x16), u32()},
        {static_cast<PropertyID>(0x18), u8()},
        {static_cast<PropertyID>(0x19), u8()},
        {static_cast<PropertyID>(0x1A), u8()},
        {static_cast<PropertyID>(0x1B), u32()},
        {static_cast<PropertyID>(0x1C), u32()},
        // totaltasks
        {PropertyID::TotalTasks, u16()},
        // taskidlist
        {PropertyID::TaskIdList, std::vector<u8>(TASKIDLIST_SIZE)},
        {static_cast<PropertyID>(0x3B), u32()},
        {static_cast<PropertyID>(0x3E), std::vector<u8>(0x200)},
        {static_cast<PropertyID>(0x3F), u8()},
    };
};

constexpr std::array<u8, 8> BOSS_SYSTEM_SAVEDATA_ID{
    0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x01, 0x00,
};

constexpr std::array<u8, 4> BOSS_SYSTEM_SAVEDATA_HEADER{0x00, 0x80, 0x34, 0x12};

FileSys::Path GetBossDataDir(u64 extdata_id);
bool DownloadBossDataFromURL(std::string_view url, std::string_view file_name, u64 program_id,
                             u64 extdata_id);

class Util {
public:
    std::vector<NsDataEntry> GetNsDataEntries();
    std::vector<FileSys::Entry> GetBossExtDataFiles();
    u16 GetOutputEntries(u32 filter, u32 max_entries, Kernel::MappedBuffer& buffer);
    bool ReadWriteProperties(bool write, PropertyID property_id, u32 size,
                             Kernel::MappedBuffer& buffer);
    std::optional<NsDataEntry> GetNsDataEntryFromID(u32 ns_data_id);
    std::pair<TaskStatus, u32> GetTaskStatusAndDuration(std::string task_id, bool wait_on_result);

public:
    u64 program_id;
    u64 extdata_id;
    std::map<std::string, BossTaskProperties> task_id_list;
    BossTaskProperties cur_props;
};
} // namespace Service::BOSS