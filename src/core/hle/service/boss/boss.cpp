// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/file_sys/errors.h"
// Needed to prevent conflicts with system macros when httplib is included on windows
ResultCode error_file_not_found = FileSys::ERROR_NOT_FOUND;
#ifdef ENABLE_WEB_SERVICE
#if defined(__ANDROID__)
#include <ifaddrs.h>
#endif
#include <httplib.h>
#ifdef WIN32
// Needed to prevent conflicts with system macros when httplib is included on windows
#undef CreateEvent
#undef CreateFile
#endif
#endif
#include <core/file_sys/archive_systemsavedata.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/boss/boss.h"
#include "core/hle/service/boss/boss_p.h"
#include "core/hle/service/boss/boss_u.h"
#include "core/hw/aes/key.h"

namespace Service::BOSS {

void Module::Interface::InitializeSession(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 2, 2);
    const u64 programID = rp.Pop<u64>();
    rp.PopPID();

    cur_props = BossTaskProperties();
    // I'm putting this here for now because I don't know where else to put it;
    // the BOSS service saves data in its BOSS_A(Archive? A list of program ids and some
    // properties that are keyed on program), BOSS_SS (Saved Strings? Includes the url and the
    // other string properties, and also some other properties?, keyed on task_id) and BOSS_SV
    // (Saved Values? Includes task id and most properties, keyed on task_id) databases in the
    // following format: A four byte header (always 00 80 34 12?) followed by any number of
    // 0x800(BOSS_A) and 0xC00(BOSS_SS and BOSS_SV) entries.
    u64 program_id = 0;
    if (programID == 0) {
        Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id);
    } else {
        program_id = programID;
    }

    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_SystemSaveData systemsavedata_factory(nand_directory);

    // Open the SystemSaveData archive 0x00010034
    FileSys::Path archive_path(boss_system_savedata_id);
    auto archive_result = systemsavedata_factory.Open(archive_path, 0);

    std::unique_ptr<FileSys::ArchiveBackend> boss_system_save_data_archive;

    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == error_file_not_found) {
        // Format the archive to create the directories
        systemsavedata_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0);

        // Open it again to get a valid archive now that the folder exists
        boss_system_save_data_archive = systemsavedata_factory.Open(archive_path, 0).Unwrap();
    } else if (!archive_result.Succeeded()) {
        LOG_ERROR(Service_BOSS, "could not open boss savedata");
    } else {
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the BOSS SystemSaveData archive!");

        boss_system_save_data_archive = std::move(archive_result).Unwrap();
    }

    FileSys::Path boss_a_path("/BOSS_A.db");
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);

    auto boss_a_result = boss_system_save_data_archive->OpenFile(boss_a_path, open_mode);

    // Read the file if it already exists
    if (boss_a_result.Succeeded()) {
        auto boss_a = std::move(boss_a_result).Unwrap();
        if (boss_a->GetSize() > boss_save_header_size &&
            ((boss_a->GetSize() - boss_save_header_size) % boss_a_entry_size) == 0) {
            u64 num_entries = (boss_a->GetSize() - boss_save_header_size) / boss_a_entry_size;
            for (u64 i = 0; i < num_entries; i++) {
                u64 prog_id;
                boss_a->Read(i * boss_a_entry_size + boss_save_header_size, sizeof(prog_id),
                             (u8*)&prog_id);
                LOG_DEBUG(Service_BOSS, "id in entry {} is {:#018X}", i, prog_id);
            }
        }
    }
    FileSys::Path boss_sv_path("/BOSS_SV.db");

    auto boss_sv_result = boss_system_save_data_archive->OpenFile(boss_sv_path, open_mode);

    FileSys::Path boss_ss_path("/BOSS_SS.db");

    auto boss_ss_result = boss_system_save_data_archive->OpenFile(boss_ss_path, open_mode);

    // Read the files if they already exists
    if (boss_sv_result.Succeeded() && boss_ss_result.Succeeded()) {
        auto boss_sv = std::move(boss_sv_result).Unwrap();
        auto boss_ss = std::move(boss_ss_result).Unwrap();
        if (boss_sv->GetSize() > boss_save_header_size &&
            ((boss_sv->GetSize() - boss_save_header_size) % boss_s_entry_size) == 0 &&
            boss_sv->GetSize() == boss_ss->GetSize()) {
            u64 num_entries = (boss_sv->GetSize() - boss_save_header_size) / boss_s_entry_size;
            for (u64 i = 0; i < num_entries; i++) {
                u64 prog_id;
                const u64 prog_id_offset = 0x10;
                boss_sv->Read(i * boss_s_entry_size + boss_save_header_size + prog_id_offset,
                              sizeof(prog_id), (u8*)&prog_id);
                LOG_DEBUG(Service_BOSS, "id sv in entry {} is {:#018X}", i, prog_id);
                std::string task_id(task_id_size, 0);
                const u64 task_id_offset = 0x18;
                boss_sv->Read(i * boss_s_entry_size + boss_save_header_size + task_id_offset,
                              task_id_size, (u8*)task_id.data());
                size_t task_id_len = strnlen(task_id.c_str(), task_id_size);
                task_id.resize(task_id_len < task_id_size ? task_id_len + 1 : task_id_size);
                LOG_DEBUG(Service_BOSS, "task id in entry {} is {}", i,
                          std::string(task_id, 0, task_id.size() - 1));
                std::string url(sizeof(cur_props.x7), 0);
                const u64 url_offset = 0x21C;
                boss_ss->Read(i * boss_s_entry_size + boss_save_header_size + url_offset,
                              sizeof(cur_props.x7), (u8*)url.data());
                size_t url_len = strnlen(url.c_str(), url.size());
                url.resize(url_len < url.size() ? url_len + 1 : url.size());
                LOG_DEBUG(Service_BOSS, "url for task {} is {}",
                          std::string(task_id, 0, task_id.size() - 1),
                          std::string(url, 0, url.size() - 1));
                if (prog_id == program_id) {
                    LOG_DEBUG(Service_BOSS, "storing for this session");
                    std::memcpy(cur_props.x7, url.data(), sizeof(cur_props.x7));
                    if (task_id_list.contains(task_id)) {
                        LOG_WARNING(Service_BOSS, "Task id already in list, will be replaced");
                        task_id_list.erase(task_id);
                    }
                    task_id_list.emplace(task_id, cur_props);
                    cur_props = BossTaskProperties();
                }
            }
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) programID={:#018X}", programID);
}

void Module::Interface::SetStorageInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 4, 0);
    const u64 extdata_id = rp.Pop<u64>();
    const u32 boss_size = rp.Pop<u32>();
    const u8 extdata_type = rp.Pop<u8>(); /// 0 = NAND, 1 = SD

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) extdata_id={:#018X}, boss_size={:#010X}, extdata_type={:#04X}",
                extdata_id, boss_size, extdata_type);
}

void Module::Interface::UnregisterStorage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::GetStorageInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::RegisterPrivateRootCa(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 1, 2);
    [[maybe_unused]] const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED)");
}

void Module::Interface::RegisterPrivateClientCert(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 2, 4);
    const u32 buffer1_size = rp.Pop<u32>();
    const u32 buffer2_size = rp.Pop<u32>();
    auto& buffer1 = rp.PopMappedBuffer();
    auto& buffer2 = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer1);
    rb.PushMappedBuffer(buffer2);

    LOG_WARNING(Service_BOSS, "(STUBBED) buffer1_size={:#010X}, buffer2_size={:#010X}, ",
                buffer1_size, buffer2_size);
}

void Module::Interface::GetNewArrivalFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(new_arrival_flag);

    LOG_WARNING(Service_BOSS, "(STUBBED) new_arrival_flag={}", new_arrival_flag);
}

void Module::Interface::RegisterNewArrivalEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 0, 2);
    [[maybe_unused]] const auto event = rp.PopObject<Kernel::Event>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED)");
}

void Module::Interface::SetOptoutFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x09, 1, 0);
    output_flag = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "output_flag={}", output_flag);
}

void Module::Interface::GetOptoutFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0A, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(output_flag);

    LOG_WARNING(Service_BOSS, "output_flag={}", output_flag);
}

void Module::Interface::RegisterTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0B, 3, 2);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    const u8 unk_param3 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    std::string task_id(size, 0);
    buffer.Read(task_id.data(), 0, size);
    if (task_id_list.contains(task_id)) {
        LOG_WARNING(Service_BOSS, "Task id already in list, will be replaced");
        task_id_list.erase(task_id);
    }
    task_id_list.emplace(task_id, cur_props);
    LOG_DEBUG(Service_BOSS, "read task id {}", task_id);
    cur_props = BossTaskProperties();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X}, unk_param3={:#04X}",
                size, unk_param2, unk_param3);
}

void Module::Interface::UnregisterTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0C, 2, 2);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    u32 result = 1;

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        if (task_id_list.erase(task_id) == 0) {
            LOG_WARNING(Service_BOSS, "Task Id not in list");
        } else {
            LOG_DEBUG(Service_BOSS, "Task Id erased");
            result = 0;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(result);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X}", size, unk_param2);
}

void Module::Interface::ReconfigureTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0D, 2, 2);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X}", size, unk_param2);
}

void Module::Interface::GetTaskIdList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0E, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::GetStepIdList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

auto Module::Interface::GetBossDataDir() {
    u64 extdata_id = 0;
    Core::System::GetInstance().GetAppLoader().ReadExtdataId(extdata_id);

    const u32 high = static_cast<u32>(extdata_id >> 32);
    const u32 low = static_cast<u32>(extdata_id & 0xFFFFFFFF);

    return FileSys::ConstructExtDataBinaryPath(1, high, low);
}

std::vector<NsDataEntry> Module::Interface::GetNsDataEntries() {
    std::vector<NsDataEntry> ns_data;
    const u32 files_to_read = 100;
    std::vector<FileSys::Entry> boss_files(files_to_read);

    u32 entry_count = GetBossExtDataFiles(files_to_read, boss_files.data());

    boss_files.resize(entry_count);
    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false, true);
    FileSys::Path boss_path{GetBossDataDir()};
    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);

    if (!archive_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata opening failed");
        return ns_data;
    }
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
    auto boss_archive = std::move(archive_result).Unwrap().get();

    for (auto const& cur_file : boss_files) {
        if (cur_file.is_directory || cur_file.file_size < boss_header_length) {
            LOG_WARNING(Service_BOSS, "Directory or too-short file in spotpass extdata");
            continue;
        }

        NsDataEntry entry;
        std::string filename{Common::UTF16ToUTF8(cur_file.filename)};
        FileSys::Path file_path = ("/" + filename).c_str();
        LOG_DEBUG(Service_BOSS, "Spotpass filename={}", filename);
        entry.filename = filename;

        FileSys::Mode mode{};
        mode.read_flag.Assign(1);

        auto file_result = boss_archive->OpenFile(file_path, mode);

        if (!file_result.Succeeded()) {
            LOG_WARNING(Service_BOSS, "Opening Spotpass file failed.");
            continue;
        }
        auto file = std::move(file_result).Unwrap();
        LOG_DEBUG(Service_BOSS, "Opening Spotpass file succeeded!");
        file->Read(0, boss_header_length, (u8*)&entry.header);
        // Extdata header should have size 0x18:
        // https://www.3dbrew.org/wiki/SpotPass#Payload_Content_Header
        if (entry.header.header_length != 0x18) {
            LOG_WARNING(
                Service_BOSS,
                "Incorrect header length or non-spotpass file; expected 0x18, found {:#010X}",
                entry.header.header_length);
            continue;
        }
#if COMMON_LITTLE_ENDIAN
        entry.header.unknown = Common::swap32(entry.header.unknown);
        entry.header.download_date = Common::swap32(entry.header.download_date);
        entry.header.program_id = Common::swap64(entry.header.program_id);
        entry.header.datatype = Common::swap32(entry.header.datatype);
        entry.header.payload_size = Common::swap32(entry.header.payload_size);
        entry.header.ns_data_id = Common::swap32(entry.header.ns_data_id);
        entry.header.version = Common::swap32(entry.header.version);
#endif
        u64 program_id = 0;
        Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id);
        if (entry.header.program_id != program_id) {
            LOG_WARNING(Service_BOSS,
                        "Mismatched program ID in spotpass data. Was expecting "
                        "{:#018X}, found {:#018X}",
                        program_id, entry.header.program_id);
            continue;
        }
        LOG_DEBUG(Service_BOSS, "Datatype is {:#010X}", entry.header.datatype);
        // Check the payload size is correct, excluding header
        if (entry.header.payload_size != cur_file.file_size - 0x34) {
            LOG_WARNING(Service_BOSS,
                        "Mismatched file size, was expecting {:#010X}, found {:#010X}",
                        entry.header.payload_size, cur_file.file_size - 0x34);
            continue;
        }
        LOG_DEBUG(Service_BOSS, "Payload size is {:#010X}", entry.header.payload_size);
        LOG_DEBUG(Service_BOSS, "NsDataID is {:#010X}", entry.header.ns_data_id);

        ns_data.push_back(entry);
    }
    return ns_data;
}

u32 Module::Interface::GetBossExtDataFiles(u32 files_to_read, auto* files) {
    u32 entry_count = 0;

    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false, true);

    FileSys::Path boss_path{GetBossDataDir()};

    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);
    if (!archive_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata opening failed");
        return entry_count;
    }
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
    auto boss_archive = std::move(archive_result).Unwrap().get();

    FileSys::Path dir_path = "/";

    auto dir_result = boss_archive->OpenDirectory(dir_path);
    if (!dir_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata directory opening failed");
        return entry_count;
    }
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata directory opened successfully!");
    auto dir = std::move(dir_result).Unwrap();
    entry_count = dir->Read(files_to_read, files);
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata directory contains {} files", entry_count);
    return entry_count;
}

u16 Module::Interface::GetOutputEntries(u32 filter, u32 max_entries, auto* buffer) {
    std::vector<NsDataEntry> ns_data = GetNsDataEntries();
    std::vector<u32> output_entries;
    for (auto const& cur_entry : ns_data) {
        const u16 datatype_high = static_cast<u16>(cur_entry.header.datatype >> 16);
        const u16 datatype_low = static_cast<u16>(cur_entry.header.datatype & 0xFFFF);
        const u16 filter_high = static_cast<u16>(filter >> 16);
        const u16 filter_low = static_cast<u16>(filter & 0xFFFF);
        if (filter != 0xFFFFFFFF &&
            (filter_high != datatype_high || (filter_low & datatype_low) == 0)) {
            LOG_DEBUG(
                Service_BOSS,
                "Filtered out NsDataID {:#010X}; failed filter {:#010X} with datatype {:#010X}",
                cur_entry.header.ns_data_id, filter, cur_entry.header.datatype);
            continue;
        }
        if (output_entries.size() >= max_entries) {
            LOG_WARNING(Service_BOSS, "Reached maximum number of entries");
            break;
        }
        output_entries.push_back(cur_entry.header.ns_data_id);
    }
    buffer->Write(output_entries.data(), 0, sizeof(u32) * output_entries.size());
    LOG_DEBUG(Service_BOSS, "{} usable entries returned", output_entries.size());
    return static_cast<u16>(output_entries.size());
}

void Module::Interface::GetNsDataIdList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 4, 2);
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    const u16 entries_count = GetOutputEntries(filter, max_entries, &buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(entries_count); /// Actual number of output entries
    rb.Push<u16>(0);             /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) filter={:#010X}, max_entries={:#010X}, "
                "word_index_start={:#06X}, start_ns_data_id={:#010X}",
                filter, max_entries, word_index_start, start_ns_data_id);
}

void Module::Interface::GetNsDataIdList1(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 4, 2);
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    const u16 entries_count = GetOutputEntries(filter, max_entries, &buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(entries_count); /// Actual number of output entries
    rb.Push<u16>(0);             /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) filter={:#010X}, max_entries={:#010X}, "
                "word_index_start={:#06X}, start_ns_data_id={:#010X}",
                filter, max_entries, word_index_start, start_ns_data_id);
}

void Module::Interface::GetNsDataIdList2(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 4, 2);
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    const u16 entries_count = GetOutputEntries(filter, max_entries, &buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(entries_count); /// Actual number of output entries
    rb.Push<u16>(0);             /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) filter={:#010X}, max_entries={:#010X}, "
                "word_index_start={:#06X}, start_ns_data_id={:#010X}",
                filter, max_entries, word_index_start, start_ns_data_id);
}

void Module::Interface::GetNsDataIdList3(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x13, 4, 2);
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    const u16 entries_count = GetOutputEntries(filter, max_entries, &buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(entries_count); /// Actual number of output entries
    rb.Push<u16>(0);             /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) filter={:#010X}, max_entries={:#010X}, "
                "word_index_start={:#06X}, start_ns_data_id={:#010X}",
                filter, max_entries, word_index_start, start_ns_data_id);
}

bool Module::Interface::DownloadBossDataFromURL(std::string url, std::string file_name) {
#ifdef ENABLE_WEB_SERVICE
    size_t scheme_end = url.find("://") + 3;
    std::string scheme = url.substr(0, scheme_end);
    LOG_DEBUG(Service_BOSS, "Scheme is {}", scheme);
    std::string host = url.substr(scheme_end, url.size());
    std::string path = host.substr(host.find("/"), host.size());
    host = host.substr(0, host.find("/"));
    LOG_DEBUG(Service_BOSS, "host is {}", host);
    LOG_DEBUG(Service_BOSS, "path is {}", path);
    std::unique_ptr<httplib::Client> client = std::make_unique<httplib::Client>(scheme + host);
    if (client == nullptr) {
        LOG_ERROR(Service_BOSS, "Invalid URL {}{}", scheme, host);
        return false;
    }

    httplib::Request request{
        .method = "GET",
        .path = path,
        .matches = Match(),
    };
    LOG_DEBUG(Service_BOSS, "Got client");
    client->set_follow_location(true);
    client->enable_server_certificate_verification(false);

    const auto result = client->send(request);
    if (!result) {
        LOG_ERROR(Service_BOSS, "GET to {}{}{} returned null", scheme, host, path);
        auto err = result.error();
        LOG_DEBUG(Service_BOSS, "error {}", httplib::to_string(err));
        return false;
    }
    LOG_DEBUG(Service_BOSS, "Got result");
    const auto& response = result.value();
    if (response.status >= 400) {
        LOG_ERROR(Service_BOSS, "GET to {}{}{} returned error status code: {}", scheme, host, path,
                  response.status);
        return false;
    }
    if (!response.headers.contains("content-type")) {
        LOG_ERROR(Service_BOSS, "GET to {}{}{} returned no content", scheme, host, path);
    }
    LOG_DEBUG(Service_BOSS, "Downloaded content is: {}", response.body);

    if (response.body.size() < boss_payload_header_length) {
        LOG_WARNING(Service_BOSS, "Payload size of {} too short for boss payload",
                    response.body.size());
        return false;
    }
    BossPayloadHeader payload_header;
    std::memcpy(&payload_header, response.body.data(), boss_payload_header_length);
    u32 one = 1;
#if COMMON_LITTLE_ENDIAN
    payload_header.magic = Common::swap32(payload_header.magic);
    payload_header.filesize = Common::swap32(payload_header.filesize);
    payload_header.release_date = Common::swap64(payload_header.release_date);
    payload_header.one = Common::swap16(payload_header.one);
    payload_header.hash_type = Common::swap16(payload_header.hash_type);
    payload_header.rsa_size = Common::swap16(payload_header.rsa_size);
    one = Common::swap32(one);
#endif
    std::string boss_string = std::string((char*)payload_header.boss, sizeof(payload_header.boss));
    if (boss_string.compare("boss") != 0) {
        LOG_WARNING(Service_BOSS, "Start of file is not 'boss', it's '{}'", boss_string);
        return false;
    }
    LOG_DEBUG(Service_BOSS, "Magic boss number is {}", boss_string);
    if (payload_header.magic != 0x10001) {
        LOG_WARNING(Service_BOSS, "Magic number mismatch");
        return false;
    }
    LOG_DEBUG(Service_BOSS, "Magic number is {:#010X}", payload_header.magic);
    if (payload_header.filesize != response.body.size()) {
        LOG_WARNING(Service_BOSS, "Expecting response to be size {}, actual size is {}",
                    payload_header.filesize, response.body.size());
        return false;
    }
    LOG_DEBUG(Service_BOSS, "Filesize is {:#010X}", payload_header.filesize);
    const u32 data_size = payload_header.filesize - boss_payload_header_length;
    std::vector<u8> encrypted_data(data_size);
    std::vector<u8> decrypted_data(data_size);
    std::memcpy(encrypted_data.data(), response.body.data() + boss_payload_header_length,
                data_size);
    std::string encrypted_string((char*)encrypted_data.data(), data_size);
    LOG_DEBUG(Service_BOSS, "encrypted data {}", encrypted_string);
    // AES details here: https://www.3dbrew.org/wiki/SpotPass#Content_Container
    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption aes;
    HW::AES::AESKey key = HW::AES::GetNormalKey(0x38);
    std::vector<u8> iv(sizeof(payload_header.iv_start) + sizeof(one));
    std::memcpy(iv.data(), payload_header.iv_start, sizeof(payload_header.iv_start));
    std::memcpy(iv.data() + sizeof(payload_header.iv_start), &one, sizeof(one));
    u64 iv_high = 0;
    u64 iv_low = 0;
    std::memcpy(&iv_high, iv.data(), sizeof(iv_high));
    std::memcpy(&iv_low, iv.data() + sizeof(iv_high), sizeof(iv_low));
#if COMMON_LITTLE_ENDIAN
    iv_high = Common::swap64(iv_high);
    iv_low = Common::swap64(iv_low);
#endif
    LOG_DEBUG(Service_BOSS, "IV is {:#018X},{:#018X}", iv_high, iv_low);
    aes.SetKeyWithIV(key.data(), CryptoPP::AES::BLOCKSIZE, iv.data());
    aes.ProcessData(decrypted_data.data(), encrypted_data.data(), data_size);
    std::string decrypted_string((char*)decrypted_data.data(), data_size);
    LOG_DEBUG(Service_BOSS, "decrypted data {}", decrypted_string);

    if (decrypted_data.size() < boss_content_header_length + boss_header_with_hash_length) {
        LOG_WARNING(Service_BOSS, "Payload size to small to be boss data: {}",
                    decrypted_data.size());
        return false;
    }

    BossHeader header;
    std::memcpy(&header.program_id, decrypted_data.data() + boss_content_header_length,
                boss_header_length - boss_extdata_header_length);
#if COMMON_LITTLE_ENDIAN
    header.program_id = Common::swap64(header.program_id);
    header.datatype = Common::swap32(header.datatype);
    header.payload_size = Common::swap32(header.payload_size);
    header.ns_data_id = Common::swap32(header.ns_data_id);
    header.version = Common::swap32(header.version);
#endif
    u32 payload_size =
        (u32)(decrypted_data.size() - (boss_content_header_length + boss_header_with_hash_length));
    if (header.payload_size != payload_size) {
        LOG_WARNING(Service_BOSS, "Payload has incorrect size, was expecting {}, found {}",
                    header.payload_size, payload_size);
        return false;
    }
    std::vector<u8> payload(payload_size);
    std::memcpy(payload.data(),
                decrypted_data.data() + boss_content_header_length + boss_header_with_hash_length,
                payload_size);

    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false, true);

    FileSys::Path boss_path{GetBossDataDir()};

    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);
    if (!archive_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata opening failed");
    }
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
    auto boss_archive = std::move(archive_result).Unwrap().get();
    // Temporarily also write raw data
    FileSys::Path raw_file_path = ("/" + file_name + "_raw_data").c_str();
    auto raw_create_result = boss_archive->CreateFile(raw_file_path, decrypted_data.size());
    if (raw_create_result.is_error) {
        LOG_WARNING(Service_BOSS, "Spotpass file could not be created, it may already exist");
    }
    FileSys::Mode raw_open_mode = {};
    raw_open_mode.write_flag.Assign(1);
    auto raw_file_result = boss_archive->OpenFile(raw_file_path, raw_open_mode);
    if (!raw_file_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Could not open spotpass file for writing");
        return false;
    }
    auto raw_file = std::move(raw_file_result).Unwrap();
    raw_file->Write(0, decrypted_data.size(), true, decrypted_data.data());
    raw_file->Close();
    // end raw data block
    u64 program_id = 0;
    Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id);
    if (program_id != header.program_id) {
        LOG_WARNING(Service_BOSS, "Mismatched program id, was expecting {:#018X}, found {:#018X}",
                    program_id, header.program_id);
        if (header.program_id == 0x0004013000003502) {
            LOG_DEBUG(Service_BOSS, "Looks like this is a news message");
            std::string news_string((char*)payload.data(), payload_size);
            news_string.erase(std::remove(news_string.begin(), news_string.end(), '\0'),
                              news_string.end());
            LOG_DEBUG(Service_BOSS, "News string might be {}", news_string);
        }
        return false;
    }
    FileSys::Path file_path = ("/" + file_name).c_str();
    auto create_result = boss_archive->CreateFile(file_path, boss_header_length + payload_size);
    if (create_result.is_error) {
        LOG_WARNING(Service_BOSS, "Spotpass file could not be created, it may already exist");
    }
    FileSys::Mode open_mode = {};
    open_mode.write_flag.Assign(1);
    auto file_result = boss_archive->OpenFile(file_path, open_mode);
    if (!file_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Could not open spotpass file for writing");
        return false;
    }
    auto file = std::move(file_result).Unwrap();
    header.header_length = 0x18;
#if COMMON_LITTLE_ENDIAN
    header.program_id = Common::swap64(header.program_id);
    header.datatype = Common::swap32(header.datatype);
    header.payload_size = Common::swap32(header.payload_size);
    header.ns_data_id = Common::swap32(header.ns_data_id);
    header.version = Common::swap32(header.version);
#endif
    file->Write(0, boss_header_length, true, (u8*)&header);
    file->Write(boss_header_length, payload_size, true, payload.data());
    file->Close();
    return true;
#else
    LOG_ERROR(Service_BOSS, "Cannot download data as web services are not enabled");
    return false;
#endif
}

void Module::Interface::SendProperty(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 2, 2);
    const u16 property_id = rp.Pop<u16>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();
    // if (size == 1) {
    //     u8 property = 0;
    //     buffer.Read(&property, 0, size);
    //     LOG_DEBUG(Service_BOSS, "content of property {:#06X} is {:#06X}", property_id,
    //     property);
    // } else if (size == 4) {
    //     u32 property = 0;
    //     buffer.Read(&property, 0, size);
    //     LOG_DEBUG(Service_BOSS, "content of property {:#06X} is {:#010X}", property_id,
    //     property);
    // } else {
    //     if (property_id == 0x0007) {
    //         if (size != sizeof(cur_props.x7)) {
    //             LOG_WARNING(Service_BOSS, "Property {:#06X} is {}, was expecting {}",
    //             property_id,
    //                         size, sizeof(cur_props.x7));
    //         } else {
    //             buffer.Read(cur_props.x7, 0, size);
    //         }
    //     }
    //     std::string property(size, 0);
    //     buffer.Read(property.data(), 0, size);

    //    LOG_DEBUG(Service_BOSS, "content of property {:#06X} is {}", property_id, property);
    //}

    void* cur_prop = NULLPTR;
    switch (property_id) {
        // byte-sized properties
    case 0x0:
        cur_prop = &cur_props.x0;
        break;
    case 0x1:
        cur_prop = &cur_props.x1;
        break;
    case 0x5:
        cur_prop = &cur_props.x5;
        break;
    case 0x6:
        cur_prop = &cur_props.x6;
        break;
    case 0x9:
        cur_prop = &cur_props.x9;
        break;
    case 0x10:
        cur_prop = &cur_props.x10;
        break;
    case 0x11:
        cur_prop = &cur_props.x11;
        break;
    case 0x12:
        cur_prop = &cur_props.x12;
        break;
    case 0x18:
        cur_prop = &cur_props.x18;
        break;
    case 0x19:
        cur_prop = &cur_props.x19;
        break;
    case 0x1A:
        cur_prop = &cur_props.x1A;
        break;
    case 0x3F:
        cur_prop = &cur_props.x3F;
    }
    if (cur_prop != NULLPTR) {
        if (size != sizeof(u8)) {
            LOG_WARNING(Service_BOSS, "Property Id {:#06X} expects size of {}, found {}",
                        property_id, sizeof(u8), size);
        } else {
            buffer.Read(cur_prop, 0, size);
            LOG_DEBUG(Service_BOSS, "Read property {:#06X}, value {:#06X}", property_id,
                      *(u8*)cur_prop);
        }
    }
    cur_prop = NULLPTR;
    switch (property_id) {
    // word-sized properties
    case 0x2:
        cur_prop = &cur_props.x2;
        break;
    case 0x3:
        cur_prop = &cur_props.x3;
        break;
    case 0x4:
        cur_prop = &cur_props.x4;
        break;
    case 0x8:
        cur_prop = &cur_props.x8;
        break;
    case 0xC:
        cur_prop = &cur_props.xC;
        break;
    case 0xE:
        cur_prop = &cur_props.xE;
        break;
    case 0x13:
        cur_prop = &cur_props.x13;
        break;
    case 0x14:
        cur_prop = &cur_props.x14;
        break;
    case 0x16:
        cur_prop = &cur_props.x16;
        break;
    case 0x1B:
        cur_prop = &cur_props.x1B;
        break;
    case 0x1C:
        cur_prop = &cur_props.x1C;
        break;
    case 0x3B:
        cur_prop = &cur_props.x3B;
    }
    if (cur_prop != NULLPTR) {

        if (size != sizeof(u32)) {
            LOG_WARNING(Service_BOSS, "Property Id {:#06X} expects size of {}, found {}",
                        property_id, sizeof(u32), size);
        } else {
            buffer.Read(cur_prop, 0, size);
            LOG_DEBUG(Service_BOSS, "Read property {:#06X}, value {:#010X}", property_id,
                      *(u32*)cur_prop);
        }
    }
    cur_prop = NULLPTR;
    u32 expected_size = 0;
    switch (property_id) {
        // string properties
    case 0x7:
        cur_prop = &cur_props.x7;
        expected_size = 0x200;
        break;
    case 0xA:
        cur_prop = &cur_props.xA;
        expected_size = 0x100;
        break;
    case 0xB:
        cur_prop = &cur_props.xB;
        expected_size = 0x200;
        break;
    case 0xD:
        cur_prop = &cur_props.xD;
        expected_size = 0x360;
        break;
    case 0x15:
        cur_prop = &cur_props.x15;
        expected_size = 0x40;
        break;
    case 0x3E:
        cur_prop = &cur_props.x3E;
        expected_size = 0x200;
        break;
    }
    if (cur_prop != NULLPTR) {

        if (size != expected_size) {
            LOG_WARNING(Service_BOSS, "Property Id {:#06X} expects size of {}, found {}",
                        property_id, expected_size, size);
        } else {
            buffer.Read(cur_prop, 0, size);
            LOG_DEBUG(Service_BOSS, "Read property {:#06X}, value {}", property_id,
                      std::string((char*)cur_prop, size));
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) property_id={:#06X}, size={:#010X}", property_id, size);
}

void Module::Interface::SendPropertyHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x15, 1, 2);
    const u16 property_id = rp.Pop<u16>();
    [[maybe_unused]] const std::shared_ptr<Kernel::Object> object = rp.PopGenericObject();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) property_id={:#06X}", property_id);
}

void Module::Interface::ReceiveProperty(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x16, 2, 2);
    const u16 property_id = rp.Pop<u16>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    u32 result = 1;
    u16 num_returned_task_ids = 0;

    switch (property_id) {
    case 0x35:
        if (size != 0x2) {
            LOG_WARNING(Service_BOSS, "Invalid size {} for property id {}", size, property_id);
        } else {
            u16 task_id_list_size = static_cast<u16>(task_id_list.size());
            buffer.Write(&task_id_list_size, 0, size);
            result = 0;
            LOG_DEBUG(Service_BOSS, "Wrote out total_tasks {}", task_id_list_size);
        }
        break;
    case 0x36:
        if (size != 0x400) {
            LOG_WARNING(Service_BOSS, "Invalid size {} for property id {}", size, property_id);
            break;
        }
        for (auto const& iter : task_id_list) {
            std::string cur_task_id = iter.first;
            if (cur_task_id.size() > task_id_size ||
                num_returned_task_ids * task_id_size + task_id_size > 0x400) {
                LOG_WARNING(Service_BOSS, "task id {} too long or would write past buffer",
                            cur_task_id);
            } else {
                buffer.Write(cur_task_id.data(), num_returned_task_ids * task_id_size,
                             task_id_size);
                num_returned_task_ids++;
                LOG_DEBUG(Service_BOSS, "wrote task id {}", cur_task_id);
            }
        }
        LOG_DEBUG(Service_BOSS, "wrote out {} task ids", num_returned_task_ids);
        result = 0;
        break;
    default:
        LOG_WARNING(Service_BOSS, "Unknown property id {}", property_id);
        result = 0;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(result);
    rb.Push<u32>(size); /// Should be actual read size; However, for property 0x36 FEA will not
                        /// attempt to read from the buffer unless the size returned is 0x400,
                        /// regardless of how many title ids are returned
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) property_id={:#06X}, size={:#010X}", property_id, size);
}

void Module::Interface::UpdateTaskInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 2, 2);
    const u32 size = rp.Pop<u32>();
    const u16 unk_param2 = rp.Pop<u16>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#06X}", size, unk_param2);
}

void Module::Interface::UpdateTaskCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x18, 2, 2);
    const u32 size = rp.Pop<u32>();
    const u32 unk_param2 = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#010X}", size, unk_param2);
}

void Module::Interface::GetTaskInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x19, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 ( 32bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetTaskCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1A, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 ( 32bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetTaskServiceStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1B, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    // Not sure what this is but it's not the task status. Maybe it's the status of the service
    // after running the task?
    u8 task_service_status = 1;

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        if (!task_id_list.contains(task_id)) {
            LOG_WARNING(Service_BOSS, "Could not find task_id in list");
        } else {
            LOG_DEBUG(Service_BOSS, "Found currently running task id");
            if (task_id_list[task_id].success) {
                LOG_DEBUG(Service_BOSS, "Task ran successfully");
            } else {
                LOG_WARNING(Service_BOSS, "Task failed");
            }
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(task_service_status); // stub 0 ( 8bit value) this is not taskstatus
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::StartTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1C, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        if (!task_id_list.contains(task_id)) {
            LOG_WARNING(Service_BOSS, "Task Id {} not found", task_id);
        } else {
            task_id_list[task_id].times_checked = 0;
            std::string url(
                (char*)task_id_list[task_id].x7,
                strnlen((char*)task_id_list[task_id].x7, sizeof(task_id_list[task_id].x7)));
            std::string file_name(task_id, 0, strnlen(task_id.c_str(), task_id.size()));
            if (DownloadBossDataFromURL(url, file_name)) {
                LOG_DEBUG(Service_BOSS, "Downloaded from {} successfully", url);
                task_id_list[task_id].success = true;
            } else {
                LOG_WARNING(Service_BOSS, "Failed to download from {}", url);
            }
        }
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::StartTaskImmediate(Kernel::HLERequestContext& ctx) {
    // IPC::RequestParser rp(ctx, 0x1D, 1, 2);
    // const u32 size = rp.Pop<u32>();
    // auto& buffer = rp.PopMappedBuffer();

    // if(size>0x8){
    // LOG_WARNING(Service_BOSS,"Task Id cannot be longer than 8");
    // }
    // else {
    // std::string task_id(size,0);
    // buffer.Read(task_id.data(),0,size);
    // LOG_DEBUG(Service_BOSS,"Read task id {}",task_id);
    // }

    // IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    // rb.Push(RESULT_SUCCESS);
    // rb.PushMappedBuffer(buffer);
    LOG_WARNING(Service_BOSS, "StartTaskImmediate called");
    StartTask(ctx);

    // LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::CancelTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1E, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetTaskFinishHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects<Kernel::Event>(boss->task_finish_event);

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::GetTaskState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x20, 2, 2);
    const u32 size = rp.Pop<u32>();
    const s8 state = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();
    u8 task_status = 0;

    u32 duration = 30;

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        if (!task_id_list.contains(task_id)) {
            LOG_WARNING(Service_BOSS, "Could not find task_id in list");
        } else {
            LOG_DEBUG(Service_BOSS, "Found currently running task id");
            // Get the duration from the task if available
            duration = task_id_list[task_id].x4;
            if (task_id_list[task_id].times_checked == 0) {
                LOG_DEBUG(Service_BOSS, "Emulating task not started");
                task_status = 5;
            } else if (task_id_list[task_id].times_checked < 200) {
                LOG_DEBUG(Service_BOSS, "Emulating task running");
                task_status = 2;
            } else {
                if (task_id_list[task_id].success) {
                    LOG_DEBUG(Service_BOSS, "Task ran successfully");
                } else {
                    LOG_WARNING(Service_BOSS, "Task failed");
                    task_status = 7;
                }
            }
            task_id_list[task_id].times_checked++;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(task_status); /// TaskStatus
    rb.Push<u32>(duration);   /// Current state value for task PropertyID 0x4
    rb.Push<u8>(0);           /// unknown, usually 0
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, state={:#06X}", size, state);
}

void Module::Interface::GetTaskResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x21, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    u8 task_status = 0;
    u32 duration = 30;

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        if (!task_id_list.contains(task_id)) {
            LOG_WARNING(Service_BOSS, "Could not find task_id in list");
        } else {
            LOG_DEBUG(Service_BOSS, "Found currently running task id");
            // Get the duration from the task if available
            duration = task_id_list[task_id].x4;
            if (task_id_list[task_id].success) {
                LOG_DEBUG(Service_BOSS, "Task ran successfully");
            } else {
                LOG_WARNING(Service_BOSS, "Task failed");
                task_status = 7;
            }
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(task_status); // This might be task_status; however it is considered a failure if
                              // anything other than 0 is returned, apps won't call this method
                              // unless they have previously determined the task has ended
    rb.Push<u32>(duration);   // stub 0 (32 bit value)
    rb.Push<u8>(0);           // stub 0 (8 bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetTaskCommErrorCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x22, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        if (!task_id_list.contains(task_id)) {
            LOG_WARNING(Service_BOSS, "Could not find task_id in list");
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 (32 bit value)
    rb.Push<u32>(0); // stub 0 (32 bit value)
    rb.Push<u8>(0);  // stub 0 (8 bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetTaskStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x23, 3, 2);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    const u8 unk_param3 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    u8 task_status = 0;

    if (size > 0x8) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        if (!task_id_list.contains(task_id)) {
            LOG_WARNING(Service_BOSS, "Could not find task_id in list");
        } else {
            LOG_DEBUG(Service_BOSS, "Found currently running task id");
            if (task_id_list[task_id].times_checked == 0) {
                LOG_DEBUG(Service_BOSS, "Emulating task not started");
                task_status = 5;
            } else if (task_id_list[task_id].times_checked < 200) {
                LOG_DEBUG(Service_BOSS, "Emulating task running");
                task_status = 2;
            } else {
                if (task_id_list[task_id].success) {
                    LOG_DEBUG(Service_BOSS, "Task ran successfully");
                } else {
                    LOG_WARNING(Service_BOSS, "Task failed");
                    task_status = 7;
                }
            }
            task_id_list[task_id].times_checked++;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(task_status); // stub 0 (8 bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X}, unk_param3={:#04X}",
                size, unk_param2, unk_param3);
}

void Module::Interface::GetTaskError(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x24, 2, 2);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0); // stub 0 (8 bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X}", size, unk_param2);
}

void Module::Interface::GetTaskInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x25, 2, 2);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X}", size, unk_param2);
}

bool Module::Interface::GetNsDataEntryFromID(u32 ns_data_id, auto* entry) {
    std::vector<NsDataEntry> ns_data = GetNsDataEntries();
    for (auto const& cur_entry : ns_data) {
        if (cur_entry.header.ns_data_id == ns_data_id) {
            *entry = cur_entry;
            return true;
        }
    }
    LOG_WARNING(Service_BOSS, "Could not find NsData with ID {:#010X}", ns_data_id);
    return false;
}

void Module::Interface::DeleteNsData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x26, 1, 0);
    const u32 ns_data_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) ns_data_id={:#010X}", ns_data_id);
}

void Module::Interface::GetNsDataHeaderInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x27, 3, 2);
    const u32 ns_data_id = rp.Pop<u32>();
    const u8 type = rp.Pop<u8>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    // This is the error code for NsDataID not found
    u32 result = 0xC8A0F843;
    u32 zero = 0;
    NsDataEntry entry;
    bool entry_success = GetNsDataEntryFromID(ns_data_id, &entry);
    if (entry_success) {

        switch (type) {
        case 0x00:
            if (size != 8) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&entry.header.program_id, 0, size);
            result = 0;
            LOG_DEBUG(Service_BOSS, "Wrote out program id {}", entry.header.program_id);
            break;
        case 0x01:
            if (size != 4) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&zero, 0, size);
            result = 0;
            LOG_DEBUG(Service_BOSS, "Wrote out unknown as zero");
            break;
        case 0x02:
            if (size != 4) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&entry.header.datatype, 0, size);
            result = 0;
            LOG_DEBUG(Service_BOSS, "Wrote out content datatype {}", entry.header.datatype);
            break;
        case 0x03:
            if (size != 4) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&entry.header.payload_size, 0, size);
            result = 0;
            LOG_DEBUG(Service_BOSS, "Wrote out payload size {}", entry.header.payload_size);
            break;
        case 0x04:
            if (size != 4) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&entry.header.ns_data_id, 0, size);
            result = 0;
            LOG_DEBUG(Service_BOSS, "Wrote out NsDataID {}", entry.header.ns_data_id);
            break;
        case 0x05:
            if (size != 4) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&entry.header.version, 0, size);
            result = 0;
            LOG_DEBUG(Service_BOSS, "Wrote out version {}", entry.header.version);
            break;
        case 0x06:
            if (size != 0x20) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&entry.header.program_id, 0x0, 8);
            buffer.Write(&zero, 0x8, 4);
            buffer.Write(&entry.header.datatype, 0xC, 4);
            buffer.Write(&entry.header.payload_size, 0x10, 4);
            buffer.Write(&entry.header.ns_data_id, 0x14, 4);
            buffer.Write(&entry.header.version, 0x18, 4);
            buffer.Write(&zero, 0x1C, 4);
            result = 0;
            LOG_DEBUG(
                Service_BOSS,
                "Wrote out unknown with program id {:#018X}, unknown zero, datatype {:#010X}, "
                "payload size {:#010X}, NsDataID {:#010X}, version {:#010X} and unknown zero",
                entry.header.program_id, entry.header.datatype, entry.header.payload_size,
                entry.header.ns_data_id, entry.header.version);
            break;
        default:
            LOG_WARNING(Service_BOSS, "Unknown header info type {}", type);
            result = 0;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(result);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) ns_data_id={:#010X}, type={:#04X}, size={:#010X}",
                ns_data_id, type, size);
}

void Module::Interface::ReadNsData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x28, 4, 2);
    const u32 ns_data_id = rp.Pop<u32>();
    const u64 offset = rp.Pop<u64>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    // This is the error code for NsDataID not found
    u32 result = 0xC8A0F843;
    u32 read_size = 0;
    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false, true);
    FileSys::Path boss_path{GetBossDataDir()};
    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);
    NsDataEntry entry;
    bool entry_success = GetNsDataEntryFromID(ns_data_id, &entry);
    if (!archive_result.Succeeded() || !entry_success) {
        LOG_WARNING(Service_BOSS, "Opening Spotpass Extdata failed.");
    } else {
        LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
        auto boss_archive = std::move(archive_result).Unwrap().get();
        FileSys::Path file_path = ("/" + entry.filename).c_str();
        FileSys::Mode mode{};
        mode.read_flag.Assign(1);
        auto file_result = boss_archive->OpenFile(file_path, mode);

        if (!file_result.Succeeded()) {
            LOG_WARNING(Service_BOSS, "Opening Spotpass file failed.");
        } else {
            auto file = std::move(file_result).Unwrap();
            LOG_DEBUG(Service_BOSS, "Opening Spotpass file succeeded!");
            if (entry.header.payload_size < size + offset) {
                LOG_WARNING(Service_BOSS,
                            "Request to read {:#010X} bytes at offset {:#010X}, payload "
                            "length is {:#010X}",
                            size, offset, entry.header.payload_size);
            } else {
                std::vector<u8> ns_data_array(size);
                file->Read(boss_header_length + offset, size, ns_data_array.data());
                buffer.Write(ns_data_array.data(), 0, size);
                result = 0;
                read_size = size;
                LOG_DEBUG(Service_BOSS, "Read {:#010X} bytes from file {}", read_size,
                          entry.filename);
            }
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(result);
    rb.Push<u32>(read_size); /// Should be actual read size
    rb.Push<u32>(0);         /// unknown
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) ns_data_id={:#010X}, offset={:#018X}, size={:#010X}",
                ns_data_id, offset, size);
}

void Module::Interface::SetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x29, 2, 0);
    const u32 unk_param1 = rp.Pop<u32>();
    const u32 unk_param2 = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1={:#010X}, unk_param2={:#010X}", unk_param1,
                unk_param2);
}

void Module::Interface::GetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2A, 1, 0);
    const u32 unk_param1 = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 (32bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1={:#010X}", unk_param1);
}

void Module::Interface::SetNsDataNewFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2B, 2, 0);
    const u32 ns_data_id = rp.Pop<u32>();
    ns_data_new_flag = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) ns_data_id={:#010X}, ns_data_new_flag={:#04X}", ns_data_id,
                ns_data_new_flag);
}

void Module::Interface::GetNsDataNewFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2C, 1, 0);
    const u32 ns_data_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(ns_data_new_flag);

    LOG_WARNING(Service_BOSS, "(STUBBED) ns_data_id={:#010X}, ns_data_new_flag={:#04X}", ns_data_id,
                ns_data_new_flag);
}

void Module::Interface::GetNsDataLastUpdate(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2D, 1, 0);
    const u32 ns_data_id = rp.Pop<u32>();

    u32 last_update = 0;

    NsDataEntry entry;
    bool entry_success = GetNsDataEntryFromID(ns_data_id, &entry);
    if (entry_success) {
        last_update = entry.header.download_date;
        LOG_DEBUG(Service_BOSS, "Last update: {}", last_update);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);           // stub 0 (32bit value)
    rb.Push<u32>(last_update); // stub 0 (32bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) ns_data_id={:#010X}", ns_data_id);
}

void Module::Interface::GetErrorCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2E, 1, 0);
    const u8 input = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// output value

    LOG_WARNING(Service_BOSS, "(STUBBED) input={:#010X}", input);
}

void Module::Interface::RegisterStorageEntry(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2F, 5, 0);
    const u32 unk_param1 = rp.Pop<u32>();
    const u32 unk_param2 = rp.Pop<u32>();
    const u32 unk_param3 = rp.Pop<u32>();
    const u32 unk_param4 = rp.Pop<u32>();
    const u8 unk_param5 = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS,
                "(STUBBED)  unk_param1={:#010X}, unk_param2={:#010X}, unk_param3={:#010X}, "
                "unk_param4={:#010X}, unk_param5={:#04X}",
                unk_param1, unk_param2, unk_param3, unk_param4, unk_param5);
}

void Module::Interface::GetStorageEntryInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x30, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 (32bit value)
    rb.Push<u16>(0); // stub 0 (16bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::SetStorageOption(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x31, 4, 0);
    const u8 unk_param1 = rp.Pop<u8>();
    const u32 unk_param2 = rp.Pop<u32>();
    const u16 unk_param3 = rp.Pop<u16>();
    const u16 unk_param4 = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS,
                "(STUBBED)  unk_param1={:#04X}, unk_param2={:#010X}, "
                "unk_param3={:#08X}, unk_param4={:#08X}",
                unk_param1, unk_param2, unk_param3, unk_param4);
}

void Module::Interface::GetStorageOption(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x32, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 (32bit value)
    rb.Push<u8>(0);  // stub 0 (8bit value)
    rb.Push<u16>(0); // stub 0 (16bit value)
    rb.Push<u16>(0); // stub 0 (16bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::StartBgImmediate(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x33, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetTaskProperty0(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x34, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0); /// current state of PropertyID 0x0 stub 0 (8bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::RegisterImmediateTask(Kernel::HLERequestContext& ctx) {
    /*     IPC::RequestParser rp(ctx, 0x35, 3, 2);
        const u32 size = rp.Pop<u32>();
        const u8 unk_param2 = rp.Pop<u8>();
        const u8 unk_param3 = rp.Pop<u8>();
        auto& buffer = rp.PopMappedBuffer();

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(RESULT_SUCCESS);
        rb.PushMappedBuffer(buffer);

        LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X},
       unk_param3={:#04X}", size, unk_param2, unk_param3); */

    LOG_WARNING(Service_BOSS, "RegisterImmediateTask called");
    // These seem to do the same thing...
    RegisterTask(ctx);
}

void Module::Interface::SetTaskQuery(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x36, 2, 4);
    const u32 buffer1_size = rp.Pop<u32>();
    const u32 buffer2_size = rp.Pop<u32>();
    auto& buffer1 = rp.PopMappedBuffer();
    auto& buffer2 = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer1);
    rb.PushMappedBuffer(buffer2);

    LOG_WARNING(Service_BOSS, "(STUBBED) buffer1_size={:#010X}, buffer2_size={:#010X}",
                buffer1_size, buffer2_size);
}

void Module::Interface::GetTaskQuery(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x37, 2, 4);
    const u32 buffer1_size = rp.Pop<u32>();
    const u32 buffer2_size = rp.Pop<u32>();
    auto& buffer1 = rp.PopMappedBuffer();
    auto& buffer2 = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer1);
    rb.PushMappedBuffer(buffer2);

    LOG_WARNING(Service_BOSS, "(STUBBED) buffer1_size={:#010X}, buffer2_size={:#010X}",
                buffer1_size, buffer2_size);
}

void Module::Interface::InitializeSessionPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x401, 2, 2);
    const u64 programID = rp.Pop<u64>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) programID={:#018X}", programID);
}

void Module::Interface::GetAppNewFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x404, 2, 0);
    const u64 programID = rp.Pop<u64>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0); // 0 = nothing new, 1 = new content

    LOG_WARNING(Service_BOSS, "(STUBBED) programID={:#018X}", programID);
}

void Module::Interface::GetNsDataIdListPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x40D, 6, 2);
    const u64 programID = rp.Pop<u64>();
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(0); /// Actual number of output entries
    rb.Push<u16>(0); /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) programID={:#018X}, filter={:#010X}, max_entries={:#010X}, "
                "word_index_start={:#06X}, start_ns_data_id={:#010X}",
                programID, filter, max_entries, word_index_start, start_ns_data_id);
}

void Module::Interface::GetNsDataIdListPrivileged1(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x40E, 6, 2);
    const u64 programID = rp.Pop<u64>();
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(0); /// Actual number of output entries
    rb.Push<u16>(0); /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) programID={:#018X}, filter={:#010X}, max_entries={:#010X}, "
                "word_index_start={:#06X}, start_ns_data_id={:#010X}",
                programID, filter, max_entries, word_index_start, start_ns_data_id);
}

void Module::Interface::SendPropertyPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x413, 2, 2);
    const u16 property_id = rp.Pop<u16>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) property_id={:#06X}, size={:#010X}", property_id, size);
}

void Module::Interface::DeleteNsDataPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x415, 3, 0);
    const u64 programID = rp.Pop<u64>();
    const u32 ns_data_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) programID={:#018X}, ns_data_id={:#010X}", programID,
                ns_data_id);
}

void Module::Interface::GetNsDataHeaderInfoPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x416, 5, 2);
    const u64 programID = rp.Pop<u64>();
    const u32 ns_data_id = rp.Pop<u32>();
    const u8 type = rp.Pop<u8>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) programID={:#018X} ns_data_id={:#010X}, type={:#04X}, size={:#010X}",
                programID, ns_data_id, type, size);
}

void Module::Interface::ReadNsDataPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x417, 6, 2);
    const u64 programID = rp.Pop<u64>();
    const u32 ns_data_id = rp.Pop<u32>();
    const u64 offset = rp.Pop<u64>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(size); /// Should be actual read size
    rb.Push<u32>(0);    /// unknown
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) programID={:#018X}, ns_data_id={:#010X}, offset={:#018X}, size={:#010X}",
                programID, ns_data_id, offset, size);
}

void Module::Interface::SetNsDataNewFlagPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x41A, 4, 0);
    const u64 programID = rp.Pop<u64>();
    const u32 unk_param1 = rp.Pop<u32>();
    ns_data_new_flag_privileged = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) programID={:#018X}, unk_param1={:#010X}, "
                "ns_data_new_flag_privileged={:#04X}",
                programID, unk_param1, ns_data_new_flag_privileged);
}

void Module::Interface::GetNsDataNewFlagPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x41B, 3, 0);
    const u64 programID = rp.Pop<u64>();
    const u32 unk_param1 = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(ns_data_new_flag_privileged);

    LOG_WARNING(Service_BOSS,
                "(STUBBED) programID={:#018X}, unk_param1={:#010X}, "
                "ns_data_new_flag_privileged={:#04X}",
                programID, unk_param1, ns_data_new_flag_privileged);
}

Module::Interface::Interface(std::shared_ptr<Module> boss, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), boss(std::move(boss)) {}

Module::Module(Core::System& system) {
    using namespace Kernel;
    // TODO: verify ResetType
    task_finish_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "BOSS::task_finish_event");
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto boss = std::make_shared<Module>(system);
    std::make_shared<BOSS_P>(boss)->InstallAsService(service_manager);
    std::make_shared<BOSS_U>(boss)->InstallAsService(service_manager);
}

} // namespace Service::BOSS
