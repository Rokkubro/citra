// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/boss/boss.h"
#include "core/hle/service/boss/boss_p.h"
#include "core/hle/service/boss/boss_u.h"

namespace Service::BOSS {

void Module::Interface::InitializeSession(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 2, 2);
    const u64 programID = rp.Pop<u64>();
    rp.PopPID();

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

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
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
std::vector<NsDataEntry> Module::Interface::GetNsDataEntries(u32 max_entries) {
    std::vector<NsDataEntry> ns_data;
    u32 entry_count = 0;
    u32 files_to_read = 100;
    FileSys::Entry boss_files[100];

    entry_count = GetBossExtDataFiles(files_to_read, boss_files);

    if (entry_count > max_entries) {
        LOG_WARNING(Service_BOSS, "Number of output entries has exceeded maximum");
        entry_count = max_entries;
    }

    for (u32 i = 0; i < entry_count; i++) {
        if (boss_files[i].is_directory || boss_files[i].file_size < 52) {
            LOG_WARNING(Service_BOSS, "Directory or too-short file in spotpass extdata");
            continue;
        }

        NsDataEntry entry;
        std::string filename{Common::UTF16ToUTF8(boss_files[i].filename)};
        filename = "/" + filename;
        FileSys::Path file_path = filename.c_str();
        LOG_DEBUG(Service_BOSS, "Spotpass filename={}", filename);
        entry.filename = filename;

        FileSys::Mode mode{};
        mode.read_flag.Assign(1);
        FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(
            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false);
        FileSys::Path boss_path{GetBossDataDir()};
        auto archive_result = extdata_archive_factory.OpenSpotpass(boss_path, 0);

        if (archive_result.Succeeded()) {
            LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
            auto boss_archive = std::move(archive_result).Unwrap().get();
            auto file_result = boss_archive->OpenFile(file_path, mode);

            if (file_result.Succeeded()) {
                auto file = std::move(file_result).Unwrap();
                LOG_DEBUG(Service_BOSS, "Opening Spotpass file succeeded!");

                // File header info from
                // https://www.3dbrew.org/wiki/SpotPass#Payload_Content_Header It looks like the
                // non-shared spotpass data on sd does not include the SHA hash and its
                // signature in the header So the total header is only 52 bytes long
                u8 header_length[1];
                file->Read(0, 1, header_length);
                if (header_length[0] != 0x18) {
                    LOG_WARNING(Service_BOSS, "Incorrect header length or non-spotpass file");
                    continue;
                }

                u64 program_id = 0;
                Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id);
                u8 spotpass_program_id_array[8];
                u64 spotpass_program_id = 0;
                file->Read(0x18, 8, spotpass_program_id_array);
                std::memcpy(&spotpass_program_id, spotpass_program_id_array, 8);
                spotpass_program_id = Common::swap64(spotpass_program_id);
                if (spotpass_program_id != program_id) {
                    LOG_WARNING(Service_BOSS,
                                "Mismatched program ID in spotpass data. Was expecting "
                                "{:#018X}, found {:#018X}",
                                program_id, spotpass_program_id);
                    continue;
                }
                entry.program_id = program_id;

                u8 ns_datatype_array[4];
                u32 ns_datatype = 0;
                file->Read(0x24, 4, ns_datatype_array);
                std::memcpy(&ns_datatype, ns_datatype_array, 4);
                ns_datatype = Common::swap32(ns_datatype);
                LOG_DEBUG(Service_BOSS, "Datatype is {:#010X}", ns_datatype);
                entry.datatype = ns_datatype;

                u8 ns_data_size_array[4];
                u32 ns_data_size = 0;
                file->Read(0x28, 4, ns_data_size_array);
                std::memcpy(&ns_data_size, ns_data_size_array, 4);
                ns_data_size = Common::swap32(ns_data_size);
                // Check the payload size is correct, excluding header
                if (ns_data_size != boss_files[i].file_size - 0x34) {
                    LOG_WARNING(Service_BOSS,
                                "Mismatched file size, was expecting {:#010X}, found {:#010X}",
                                ns_data_size, boss_files[i].file_size - 0x34);
                    continue;
                }
                LOG_DEBUG(Service_BOSS, "Payload size is {:#010X}", ns_data_size);
                entry.payload_size = ns_data_size;

                u8 ns_data_id_array[4];
                u32 ns_data_id = 0;
                file->Read(0x2C, 4, ns_data_id_array);
                std::memcpy(&ns_data_id, ns_data_id_array, 4);
                ns_data_id = Common::swap32(ns_data_id);
                LOG_DEBUG(Service_BOSS, "NsDataID is {:#010X}", ns_data_id);
                entry.ns_data_id = ns_data_id;

                ns_data.push_back(entry);
            } else {
                LOG_WARNING(Service_BOSS, "Opening Spotpass file failed.");
            }
        } else {
            LOG_WARNING(Service_BOSS, "Extdata opening failed");
        }
    }
    return ns_data;
}

u32 Module::Interface::GetBossExtDataFiles(u32 files_to_read, auto* files) {
    u32 entry_count = 0;

    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false);

    FileSys::Path boss_path{GetBossDataDir()};

    auto archive_result = extdata_archive_factory.OpenSpotpass(boss_path, 0);
    if (archive_result.Succeeded()) {
        LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
        auto boss_archive = std::move(archive_result).Unwrap().get();

        FileSys::Path dir_path = "/";

        auto dir_result = boss_archive->OpenDirectory(dir_path);
        if (dir_result.Succeeded()) {
            LOG_DEBUG(Service_BOSS, "Spotpass Extdata directory opened successfully!");
            auto dir = std::move(dir_result).Unwrap();
            entry_count = dir->Read(files_to_read, files);
            LOG_DEBUG(Service_BOSS, "Spotpass Extdata directory contains {} files", entry_count);
        } else {
            LOG_WARNING(Service_BOSS, "Extdata directory opened unsuccessfully :(");
        }
    } else {
        LOG_WARNING(Service_BOSS, "Extdata opening failed");
    }
    return entry_count;
}

u32 Module::Interface::GetOutputEntries(u32 filter, u32 max_entries, auto* buffer) {
    std::vector<NsDataEntry> ns_data = GetNsDataEntries(max_entries);
    std::vector<u32> output_entries;
    u32 entry_count = ns_data.size();
    for (u32 i = 0; i < entry_count; i++) {
        output_entries.push_back(ns_data[i].ns_data_id);
    }
    buffer->Write(&output_entries[0], 0, sizeof(u32) * entry_count);
    LOG_DEBUG(Service_BOSS, "{} usable entries returned", entry_count);
    return entry_count;
}

void Module::Interface::GetNsDataIdList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 4, 2);
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    const u32 entries_count = GetOutputEntries(filter, max_entries, &buffer);

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

    const u32 entries_count = GetOutputEntries(filter, max_entries, &buffer);

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

    const u32 entries_count = GetOutputEntries(filter, max_entries, &buffer);

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

    const u32 entries_count = GetOutputEntries(filter, max_entries, &buffer);

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

void Module::Interface::SendProperty(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 2, 2);
    const u16 property_id = rp.Pop<u16>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

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

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(size); /// Should be actual read size
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

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0); // stub 0 ( 8bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::StartTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1C, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::StartTaskImmediate(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1D, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::CancelTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1E, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

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
    const u8 state = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0);  /// TaskStatus
    rb.Push<u32>(0); /// Current state value for task PropertyID 0x4
    rb.Push<u8>(0);  /// unknown, usually 0
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, state={:#06X}", size, state);
}

void Module::Interface::GetTaskResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x21, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0);  // stub 0 (8 bit value)
    rb.Push<u32>(0); // stub 0 (32 bit value)
    rb.Push<u8>(0);  // stub 0 (8 bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetTaskCommErrorCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x22, 1, 2);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

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

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0); // stub 0 (8 bit value)
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

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
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

    std::vector<NsDataEntry> ns_data = GetNsDataEntries(100);

    // This is the error code for NsDataID not found
    u32 result = 0xC8A0F843;
    u32 read_size = 0;

    for (u32 i = 0; i < ns_data.size(); i++) {
        NsDataEntry entry = ns_data[i];
        if (entry.ns_data_id == ns_data_id) {
            FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(
                FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false);
            FileSys::Path boss_path{GetBossDataDir()};
            auto archive_result = extdata_archive_factory.OpenSpotpass(boss_path, 0);

            if (archive_result.Succeeded()) {
                LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
                auto boss_archive = std::move(archive_result).Unwrap().get();
                FileSys::Path file_path = entry.filename.c_str();
                FileSys::Mode mode{};
                mode.read_flag.Assign(1);
                auto file_result = boss_archive->OpenFile(file_path, mode);

                if (file_result.Succeeded()) {
                    auto file = std::move(file_result).Unwrap();
                    LOG_DEBUG(Service_BOSS, "Opening Spotpass file succeeded!");
                    if (entry.payload_size < size + offset) {
                        LOG_WARNING(Service_BOSS,
                                    "Request to read {:#010X} bytes at offset {:#010X}, payload "
                                    "length is {:#010X}",
                                    size, offset, entry.payload_size);
                        continue;
                    }
                    u8* ns_data_array = new u8[size];
                    file->Read(0x34 + offset, size, ns_data_array);
                    buffer.Write(ns_data_array, 0, size);
                    delete[] ns_data_array;
                    result = 0;
                    read_size = size;
                    LOG_DEBUG(Service_BOSS, "Read {:#010X} bytes from file {}", read_size,
                              entry.filename);
                } else {
                    LOG_WARNING(Service_BOSS, "Opening Spotpass file failed.");
                }
            } else {
                LOG_WARNING(Service_BOSS, "Opening Spotpass Extdata failed.");
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
    const u32 unk_param1 = rp.Pop<u32>();
    ns_data_new_flag = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1={:#010X}, ns_data_new_flag={:#04X}", unk_param1,
                ns_data_new_flag);
}

void Module::Interface::GetNsDataNewFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2C, 1, 0);
    const u32 unk_param1 = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(ns_data_new_flag);

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1={:#010X}, ns_data_new_flag={:#04X}", unk_param1,
                ns_data_new_flag);
}

void Module::Interface::GetNsDataLastUpdate(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2D, 1, 0);
    const u32 unk_param1 = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 (32bit value)
    rb.Push<u32>(0); // stub 0 (32bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1={:#010X}", unk_param1);
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
    IPC::RequestParser rp(ctx, 0x35, 3, 2);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    const u8 unk_param3 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X}, unk_param3={:#04X}",
                size, unk_param2, unk_param3);
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

    LOG_WARNING(
        Service_BOSS,
        "(STUBBED) programID={:#018X}, unk_param1={:#010X}, ns_data_new_flag_privileged={:#04X}",
        programID, unk_param1, ns_data_new_flag_privileged);
}

void Module::Interface::GetNsDataNewFlagPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x41B, 3, 0);
    const u64 programID = rp.Pop<u64>();
    const u32 unk_param1 = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(ns_data_new_flag_privileged);

    LOG_WARNING(
        Service_BOSS,
        "(STUBBED) programID={:#018X}, unk_param1={:#010X}, ns_data_new_flag_privileged={:#04X}",
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
