// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <common/common_paths.h>
#include <core/file_sys/archive_systemsavedata.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/boss/boss.h"
#include "core/hle/service/boss/boss_p.h"
#include "core/hle/service/boss/boss_u.h"
#include "core/hw/aes/key.h"

namespace Service::BOSS {

void Module::Interface::InitializeSession(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u64 provided_program_id = rp.Pop<u64>();
    rp.PopPID();

    util.cur_props = BossTaskProperties();
    // I'm putting this here for now because I don't know where else to put it;
    // the BOSS service saves data in its BOSS_A(Archive? A list of program ids and some
    // properties that are keyed on program), BOSS_SS (Saved Strings? Includes the url and the
    // other string properties, and also some other properties?, keyed on task_id) and BOSS_SV
    // (Saved Values? Includes task id and most properties, keyed on task_id) databases in the
    // following format: A four byte header (always 00 80 34 12?) followed by any number of
    // 0x800(BOSS_A) and 0xC00(BOSS_SS and BOSS_SV) entries.

    if (provided_program_id != 0) {
        util.program_id = provided_program_id;
    }

    LOG_DEBUG(Service_BOSS, "called, program_id={:#018X}", util.program_id);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    // We can always return success from this as the boss dbs are not properly used atm
    rb.Push(RESULT_SUCCESS);

    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_SystemSaveData systemsavedata_factory(nand_directory);

    // Open the SystemSaveData archive 0x00010034
    FileSys::Path archive_path(BOSS_SYSTEM_SAVEDATA_ID);
    auto archive_result = systemsavedata_factory.Open(archive_path, 0);

    std::unique_ptr<FileSys::ArchiveBackend> boss_system_save_data_archive;

    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ERROR_NOT_FOUND) {
        // Format the archive to create the directories
        systemsavedata_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0);

        // Open it again to get a valid archive now that the folder exists
        auto create_archive_result = systemsavedata_factory.Open(archive_path, 0);
        if (create_archive_result.Succeeded()) {
            boss_system_save_data_archive = std::move(create_archive_result).Unwrap();
        } else {
            LOG_ERROR(Service_BOSS, "Could not open boss savedata");
            return;
        }
    } else if (archive_result.Succeeded()) {
        boss_system_save_data_archive = std::move(archive_result).Unwrap();
    } else {
        LOG_ERROR(Service_BOSS, "Could not open boss savedata");
        return;
    }
    FileSys::Path boss_a_path("/BOSS_A.db");
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);
    auto boss_a_result = boss_system_save_data_archive->OpenFile(boss_a_path, open_mode);

    // Read the file if it already exists
    if (boss_a_result.Succeeded()) {
        auto boss_a = std::move(boss_a_result).Unwrap();
        const u64 boss_a_size = boss_a->GetSize();
        // Check the file has a valid size (multiple of the number of entries, plus header)
        if (!(boss_a_size > BOSS_SAVE_HEADER_SIZE &&
              ((boss_a_size - BOSS_SAVE_HEADER_SIZE) % BOSS_A_ENTRY_SIZE) == 0)) {
        } else {
            u64 num_entries = (boss_a_size - BOSS_SAVE_HEADER_SIZE) / BOSS_A_ENTRY_SIZE;
            // Print the program ids of the entries
            for (u64 i = 0; i < num_entries; i++) {
                u64 entry_offset = i * BOSS_A_ENTRY_SIZE + BOSS_SAVE_HEADER_SIZE;
                u64 prog_id;
                boss_a->Read(entry_offset, sizeof(prog_id), reinterpret_cast<u8*>(&prog_id));
                LOG_DEBUG(Service_BOSS, "Id in entry {} is {:#018X}", i, prog_id);
            }
        }
    }

    FileSys::Path boss_sv_path("/BOSS_SV.db");
    auto boss_sv_result = boss_system_save_data_archive->OpenFile(boss_sv_path, open_mode);

    FileSys::Path boss_ss_path("/BOSS_SS.db");
    auto boss_ss_result = boss_system_save_data_archive->OpenFile(boss_ss_path, open_mode);

    // Read the files if they already exist
    if (boss_sv_result.Succeeded() && boss_ss_result.Succeeded()) {
        auto boss_sv = std::move(boss_sv_result).Unwrap();
        auto boss_ss = std::move(boss_ss_result).Unwrap();

        // Check the file length is valid. Both files should have the same number of entries
        if (!(boss_sv->GetSize() > BOSS_SAVE_HEADER_SIZE &&
              ((boss_sv->GetSize() - BOSS_SAVE_HEADER_SIZE) % BOSS_S_ENTRY_SIZE) == 0 &&
              boss_sv->GetSize() == boss_ss->GetSize())) {
            LOG_WARNING(Service_BOSS, "Boss dbs have incorrect size");
            return;
        } else {
            u64 num_entries = (boss_sv->GetSize() - BOSS_SAVE_HEADER_SIZE) / BOSS_S_ENTRY_SIZE;
            for (u64 i = 0; i < num_entries; i++) {
                u64 entry_offset = i * BOSS_S_ENTRY_SIZE + BOSS_SAVE_HEADER_SIZE;

                // Print the program id and task id to debug
                u64 prog_id;
                boss_sv->Read(entry_offset + BOSS_S_PROG_ID_OFFSET, sizeof(prog_id),
                              reinterpret_cast<u8*>(&prog_id));
                LOG_DEBUG(Service_BOSS, "Id sv in entry {} is {:#018X}", i, prog_id);

                std::string task_id(TASK_ID_SIZE, 0);
                boss_sv->Read(entry_offset + BOSS_S_TASK_ID_OFFSET, TASK_ID_SIZE,
                              reinterpret_cast<u8*>(task_id.data()));
                LOG_DEBUG(Service_BOSS, "Task id in entry {} is {}", i, task_id);

                std::vector<u8> url(URL_SIZE);
                boss_ss->Read(entry_offset + BOSS_S_URL_OFFSET, URL_SIZE, url.data());
                LOG_DEBUG(Service_BOSS, "Url for task {} is {}", task_id,
                          std::string_view(reinterpret_cast<char*>(url.data()), url.size()));

                // If the task is for the current program, store the download url for the session.
                // In the future we could store more properties, including from the sv db.
                if (prog_id == util.program_id) {
                    LOG_DEBUG(Service_BOSS, "Storing download url");
                    util.cur_props.props[PropertyID::Url] = url;
                    if (util.task_id_list.contains(task_id)) {
                        LOG_WARNING(Service_BOSS, "Task id already in list, will be replaced");
                        util.task_id_list.erase(task_id);
                    }
                    util.task_id_list.emplace(task_id, std::move(util.cur_props));
                    util.cur_props = BossTaskProperties();
                }
            }
        }
    }
}

void Module::Interface::SetStorageInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::GetStorageInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::RegisterPrivateRootCa(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED)");
}

void Module::Interface::RegisterPrivateClientCert(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(new_arrival_flag);

    LOG_WARNING(Service_BOSS, "(STUBBED) new_arrival_flag={}", new_arrival_flag);
}

void Module::Interface::RegisterNewArrivalEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const auto event = rp.PopObject<Kernel::Event>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED)");
}

void Module::Interface::SetOptoutFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    output_flag = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "output_flag={}", output_flag);
}

void Module::Interface::GetOptoutFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(output_flag);

    LOG_WARNING(Service_BOSS, "output_flag={}", output_flag);
}

void Module::Interface::RegisterTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    const u8 unk_param3 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    std::string task_id(size, 0);
    buffer.Read(task_id.data(), 0, size);
    if (util.task_id_list.contains(task_id)) {
        LOG_WARNING(Service_BOSS, "Task id already in list, will be replaced");
        util.task_id_list.erase(task_id);
    }
    util.task_id_list.emplace(task_id, std::move(util.cur_props));
    util.cur_props = BossTaskProperties();
    LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS, "called, size={:#010X}, unk_param2={:#04X}, unk_param3={:#04X}", size,
              unk_param2, unk_param3);
}

void Module::Interface::UnregisterTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    ResultCode result = RESULT_FAILED;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);

    if (size > TASK_ID_SIZE) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
        rb.Push(result);
        rb.PushMappedBuffer(buffer);
        return;
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        if (util.task_id_list.erase(task_id) == 0) {
            LOG_WARNING(Service_BOSS, "Task Id not in list");
        } else {
            LOG_DEBUG(Service_BOSS, "Task Id erased");
            result = RESULT_SUCCESS;
        }
    }

    rb.Push(result);
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS, "called, size={:#010X}, unk_param2={:#04X}", size, unk_param2);
}

void Module::Interface::ReconfigureTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X}", size, unk_param2);
}

void Module::Interface::GetTaskIdList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    const u16 num_task_ids = static_cast<u16>(util.task_id_list.size());
    util.cur_props.props[PropertyID::TotalTasks] = num_task_ids;
    LOG_DEBUG(Service_BOSS, "Prepared total_tasks = {}", num_task_ids);

    u16 num_returned_task_ids = 0;
    std::vector<std::array<u8, TASK_ID_SIZE>> task_ids(TASKIDLIST_SIZE / TASK_ID_SIZE);

    for (const auto& iter : util.task_id_list) {
        const std::string_view cur_task_id = iter.first;
        if (cur_task_id.size() > TASK_ID_SIZE ||
            num_returned_task_ids >= TASKIDLIST_SIZE / TASK_ID_SIZE) {
            LOG_WARNING(Service_BOSS, "task id {} too long or too many task ids", cur_task_id);
        } else {
            std::memcpy(task_ids[num_returned_task_ids].data(), cur_task_id.data(), TASK_ID_SIZE);
            num_returned_task_ids++;
            LOG_TRACE(Service_BOSS, "wrote task id {}", cur_task_id);
        }
    }

    const auto task_list_prop =
        std::get_if<std::vector<u8>>(&util.cur_props.props[PropertyID::TaskIdList]);
    if (task_list_prop && task_list_prop->size() == TASKIDLIST_SIZE) {

        std::memcpy(task_list_prop->data(), task_ids.data(), TASKIDLIST_SIZE);
        LOG_DEBUG(Service_BOSS, "wrote out {} task ids", num_returned_task_ids);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_BOSS, "called");
}

void Module::Interface::GetStepIdList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetNsDataIdList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    const u16 entries_count = util.GetOutputEntries(filter, max_entries, buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(entries_count); /// Actual number of output entries
    rb.Push<u16>(0);             /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS,
              "filter={:#010X}, max_entries={:#010X}, "
              "word_index_start={:#06X}, start_ns_data_id={:#010X}",
              filter, max_entries, word_index_start, start_ns_data_id);
}

void Module::Interface::GetNsDataIdList1(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    const u16 entries_count = util.GetOutputEntries(filter, max_entries, buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(entries_count); /// Actual number of output entries
    rb.Push<u16>(0);             /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS,
              "filter={:#010X}, max_entries={:#010X}, "
              "word_index_start={:#06X}, start_ns_data_id={:#010X}",
              filter, max_entries, word_index_start, start_ns_data_id);
}

void Module::Interface::GetNsDataIdList2(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    const u16 entries_count = util.GetOutputEntries(filter, max_entries, buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(entries_count); /// Actual number of output entries
    rb.Push<u16>(0);             /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS,
              "filter={:#010X}, max_entries={:#010X}, "
              "word_index_start={:#06X}, start_ns_data_id={:#010X}",
              filter, max_entries, word_index_start, start_ns_data_id);
}

void Module::Interface::GetNsDataIdList3(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 filter = rp.Pop<u32>();
    const u32 max_entries = rp.Pop<u32>(); /// buffer size in words
    const u16 word_index_start = rp.Pop<u16>();
    const u32 start_ns_data_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    const u16 entries_count = util.GetOutputEntries(filter, max_entries, buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(entries_count); /// Actual number of output entries
    rb.Push<u16>(0);             /// Last word-index copied to output in the internal NsDataId list.
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS,
              "filter={:#010X}, max_entries={:#010X}, "
              "word_index_start={:#06X}, start_ns_data_id={:#010X}",
              filter, max_entries, word_index_start, start_ns_data_id);
}

void Module::Interface::SendProperty(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const PropertyID property_id = static_cast<PropertyID>(rp.Pop<u16>());
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_BOSS, "called, property_id={:#06X}, size={:#010X}", property_id, size);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);

    ResultCode result = RESULT_FAILED;

    if (util.ReadWriteProperties(false, property_id, size, buffer)) {
        result = RESULT_SUCCESS;
    }

    rb.Push(result);
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::SendPropertyHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u16 property_id = rp.Pop<u16>();
    [[maybe_unused]] const std::shared_ptr<Kernel::Object> object = rp.PopGenericObject();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) property_id={:#06X}", property_id);
}

void Module::Interface::ReceiveProperty(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const PropertyID property_id = static_cast<PropertyID>(rp.Pop<u16>());
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_BOSS, "called, property_id={:#06X}, size={:#010X}", property_id, size);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    ResultCode result = RESULT_FAILED;

    if (util.ReadWriteProperties(true, property_id, size, buffer)) {
        result = RESULT_SUCCESS;
    }

    rb.Push(result);
    rb.Push<u32>(size); // The size of the property per id, not how much data
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::UpdateTaskInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    const u16 unk_param2 = rp.Pop<u16>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#06X}", size, unk_param2);
}

void Module::Interface::UpdateTaskCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    const u32 unk_param2 = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > TASK_ID_SIZE) {
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
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 ( 32bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetTaskCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > TASK_ID_SIZE) {
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
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    // Not sure what this is but it's not the task status. Maybe it's the status of the service
    // after running the task?
    u8 task_service_status = 1;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(task_service_status); // stub 1 ( 8bit value) this is not taskstatus
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::StartTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > TASK_ID_SIZE) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        if (!util.task_id_list.contains(task_id)) {
            LOG_WARNING(Service_BOSS, "Task Id {} not found", task_id);
        } else {
            util.task_id_list[task_id].times_checked = 0;
            if (const auto* url_prop = util.task_id_list[task_id].props.contains(PropertyID::Url)
                                           ? std::get_if<std::vector<u8>>(
                                                 &util.task_id_list[task_id].props[PropertyID::Url])
                                           : nullptr;
                url_prop && url_prop->size() == URL_SIZE) {
                const char* url_pointer = reinterpret_cast<const char*>(url_prop->data());
                std::string_view url(url_pointer, strnlen(url_pointer, URL_SIZE));
                std::string_view file_name(util.task_id_list.find(task_id)->first.c_str(),
                                           strnlen(task_id.c_str(), TASK_ID_SIZE));
                util.task_id_list[task_id].download_task =
                    std::async(std::launch::async, DownloadBossDataFromURL, url, file_name,
                               util.program_id, util.extdata_id);
            } else {
                LOG_ERROR(Service_BOSS, "URL property is invalid");
            }
        }
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS, "size={:#010X}", size);
}

void Module::Interface::StartTaskImmediate(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_BOSS, "StartTaskImmediate called");
    // StartTask and StartTaskImmediate do much the same thing
    StartTask(ctx);
    LOG_DEBUG(Service_BOSS, "called");
}

void Module::Interface::CancelTask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > TASK_ID_SIZE) {
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
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects<Kernel::Event>(boss->task_finish_event);

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::GetTaskState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    const s8 state = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    TaskStatus task_status = TaskStatus::Failed;
    u32 duration;

    if (size > TASK_ID_SIZE) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
        duration = 0;
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        std::tie(task_status, duration) = util.GetTaskStatusAndDuration(task_id, false);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(static_cast<u8>(task_status)); /// TaskStatus
    rb.Push<u32>(duration);                    /// Current state value for task PropertyID 0x4
    rb.Push<u8>(0);                            /// unknown, usually 0
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS, "size={:#010X}, state={:#06X}", size, state);
}

void Module::Interface::GetTaskResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    TaskStatus task_status = TaskStatus::Failed;
    u32 duration;

    if (size > TASK_ID_SIZE) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
        duration = 0;
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        std::tie(task_status, duration) = util.GetTaskStatusAndDuration(task_id, true);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 2);
    rb.Push(RESULT_SUCCESS);
    // This might be task_status; however it is considered a failure if
    // anything other than 0 is returned, apps won't call this method
    // unless they have previously determined the task has ended
    rb.Push<u8>(static_cast<u8>(task_status));
    rb.Push<u32>(duration); // return duration (number of times to check)
    rb.Push<u8>(0);         // stub 0 (8 bit value)
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS, "size={:#010X}", size);
}

void Module::Interface::GetTaskCommErrorCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    if (size > TASK_ID_SIZE) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        if (!util.task_id_list.contains(task_id)) {
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
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    const u8 unk_param3 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    TaskStatus task_status;

    if (size > TASK_ID_SIZE) {
        LOG_WARNING(Service_BOSS, "Task Id cannot be longer than 8");
        task_status = TaskStatus::Failed;
    } else {
        std::string task_id(size, 0);
        buffer.Read(task_id.data(), 0, size);
        LOG_DEBUG(Service_BOSS, "Read task id {}", task_id);
        task_status = util.GetTaskStatusAndDuration(task_id, false).first;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(static_cast<u8>(task_status)); // return current task status
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS, "size={:#010X}, unk_param2={:#04X}, unk_param3={:#04X}", size,
              unk_param2, unk_param3);
}

void Module::Interface::GetTaskError(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    const u8 unk_param2 = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}, unk_param2={:#04X}", size, unk_param2);
}

void Module::Interface::DeleteNsData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ns_data_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) ns_data_id={:#010X}", ns_data_id);
}

void Module::Interface::GetNsDataHeaderInfo(Kernel::HLERequestContext& ctx) {
    NsDataHeaderInfo info{};

    IPC::RequestParser rp(ctx);
    info.ns_data_id = rp.Pop<u32>();
    const NsDataHeaderInfoType type = static_cast<NsDataHeaderInfoType>(rp.Pop<u8>());
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    ResultCode result = RESULT_FAILED;
    u32 zero = 0;
    std::optional<NsDataEntry> entry = util.GetNsDataEntryFromID(info.ns_data_id);
    if (entry.has_value()) {
        info.program_id = entry->header.program_id;
        info.datatype = entry->header.datatype;
        info.payload_size = entry->header.payload_size;
        info.version = entry->header.version;

        switch (type) {
        case NsDataHeaderInfoType::ProgramId:
            if (size != sizeof(info.program_id)) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&info.program_id, 0, size);
            result = RESULT_SUCCESS;
            LOG_DEBUG(Service_BOSS, "Wrote out program id {}", info.program_id);
            break;
        case NsDataHeaderInfoType::Unknown:
            if (size != sizeof(u32)) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&zero, 0, size);
            result = RESULT_SUCCESS;
            LOG_DEBUG(Service_BOSS, "Wrote out unknown as zero");
            break;
        case NsDataHeaderInfoType::Datatype:
            if (size != sizeof(info.datatype)) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&info.datatype, 0, size);
            result = RESULT_SUCCESS;
            LOG_DEBUG(Service_BOSS, "Wrote out content datatype {}", info.datatype);
            break;
        case NsDataHeaderInfoType::PayloadSize:
            if (size != sizeof(info.payload_size)) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&info.payload_size, 0, size);
            result = RESULT_SUCCESS;
            LOG_DEBUG(Service_BOSS, "Wrote out payload size {}", info.payload_size);
            break;
        case NsDataHeaderInfoType::NsDataId:
            if (size != sizeof(info.ns_data_id)) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&info.ns_data_id, 0, size);
            result = RESULT_SUCCESS;
            LOG_DEBUG(Service_BOSS, "Wrote out NsDataID {}", info.ns_data_id);
            break;
        case NsDataHeaderInfoType::Version:
            if (size != sizeof(info.version)) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&info.version, 0, size);
            result = RESULT_SUCCESS;
            LOG_DEBUG(Service_BOSS, "Wrote out version {}", info.version);
            break;
        case NsDataHeaderInfoType::Everything:
            if (size != sizeof(info)) {
                LOG_WARNING(Service_BOSS, "Invalid size {} for type {}", size, type);
                break;
            }
            buffer.Write(&info, 0, size);
            result = RESULT_SUCCESS;
            LOG_DEBUG(Service_BOSS,
                      "Wrote out unknown with program id {:#018X}, unknown zero, "
                      "datatype {:#010X}, "
                      "payload size {:#010X}, NsDataID {:#010X}, version "
                      "{:#010X} and unknown zero",
                      info.program_id, info.datatype, info.payload_size, info.ns_data_id,
                      info.version);
            break;
        default:
            LOG_WARNING(Service_BOSS, "Unknown header info type {}", type);
            result = RESULT_FAILED;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(result);
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS, "ns_data_id={:#010X}, type={:#04X}, size={:#010X}", info.ns_data_id,
              type, size);
}

void Module::Interface::ReadNsData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ns_data_id = rp.Pop<u32>();
    const u64 offset = rp.Pop<u64>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    // This is the error code for NsDataID not found
    ResultCode result = RESULT_FAILED;
    u32 read_size = 0;
    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false, true);
    const FileSys::Path boss_path{GetBossDataDir(util.extdata_id)};
    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 2);

    std::optional<NsDataEntry> entry = util.GetNsDataEntryFromID(ns_data_id);
    if (!archive_result.Succeeded() || !entry.has_value()) {
        LOG_WARNING(Service_BOSS, "Opening Spotpass Extdata failed.");
        rb.Push(result);
        rb.Push<u32>(read_size);
        rb.Push<u32>(0);
        rb.PushMappedBuffer(buffer);
        LOG_DEBUG(Service_BOSS, "ns_data_id={:#010X}, offset={:#018X}, size={:#010X}", ns_data_id,
                  offset, size);
        return;
    }
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
    auto boss_archive = std::move(archive_result).Unwrap().get();
    FileSys::Path file_path = fmt::format("/{}", entry->filename);
    FileSys::Mode mode{};
    mode.read_flag.Assign(1);
    auto file_result = boss_archive->OpenFile(file_path, mode);

    if (!file_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Opening Spotpass file failed.");
        rb.Push(result);
        rb.Push<u32>(read_size);
        rb.Push<u32>(0);
        rb.PushMappedBuffer(buffer);
        LOG_DEBUG(Service_BOSS, "ns_data_id={:#010X}, offset={:#018X}, size={:#010X}", ns_data_id,
                  offset, size);
        return;
    }
    auto file = std::move(file_result).Unwrap();
    LOG_DEBUG(Service_BOSS, "Opening Spotpass file succeeded!");
    if (entry->header.payload_size < size + offset) {
        LOG_WARNING(Service_BOSS,
                    "Request to read {:#010X} bytes at offset {:#010X}, payload "
                    "length is {:#010X}",
                    size, offset, u32(entry->header.payload_size));
        rb.Push(result);
        rb.Push<u32>(read_size);
        rb.Push<u32>(0);
        rb.PushMappedBuffer(buffer);
        LOG_DEBUG(Service_BOSS, "ns_data_id={:#010X}, offset={:#018X}, size={:#010X}", ns_data_id,
                  offset, size);
        return;
    }
    std::vector<u8> ns_data_array(size);
    file->Read(BOSS_HEADER_LENGTH + offset, size, ns_data_array.data());
    buffer.Write(ns_data_array.data(), 0, size);
    result = RESULT_SUCCESS;
    read_size = size;
    LOG_DEBUG(Service_BOSS, "Read {:#010X} bytes from file {}", read_size, entry->filename);

    rb.Push(result);
    rb.Push<u32>(read_size); /// Should be actual read size
    rb.Push<u32>(0);         /// unknown
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_BOSS, "ns_data_id={:#010X}, offset={:#018X}, size={:#010X}", ns_data_id,
              offset, size);
}

void Module::Interface::SetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 unk_param1 = rp.Pop<u32>();
    const u32 unk_param2 = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1={:#010X}, unk_param2={:#010X}", unk_param1,
                unk_param2);
}

void Module::Interface::GetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 unk_param1 = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 (32bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1={:#010X}", unk_param1);
}

void Module::Interface::SetNsDataNewFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ns_data_id = rp.Pop<u32>();
    ns_data_new_flag = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) ns_data_id={:#010X}, ns_data_new_flag={:#04X}", ns_data_id,
                ns_data_new_flag);
}

void Module::Interface::GetNsDataNewFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ns_data_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(ns_data_new_flag);

    LOG_WARNING(Service_BOSS, "(STUBBED) ns_data_id={:#010X}, ns_data_new_flag={:#04X}", ns_data_id,
                ns_data_new_flag);
}

void Module::Interface::GetNsDataLastUpdate(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ns_data_id = rp.Pop<u32>();

    u32 last_update = 0;

    std::optional<NsDataEntry> entry = util.GetNsDataEntryFromID(ns_data_id);
    if (entry.has_value()) {
        last_update = entry->header.download_date;
        LOG_DEBUG(Service_BOSS, "Last update: {}", last_update);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
    rb.Push<u32>(last_update); // return the download date from the ns data

    LOG_DEBUG(Service_BOSS, "ns_data_id={:#010X}", ns_data_id);
}

void Module::Interface::GetErrorCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u8 input = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); /// output value

    LOG_WARNING(Service_BOSS, "(STUBBED) input={:#010X}", input);
}

void Module::Interface::RegisterStorageEntry(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 (32bit value)
    rb.Push<u16>(0); // stub 0 (16bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::SetStorageOption(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // stub 0 (32bit value)
    rb.Push<u8>(0);  // stub 0 (8bit value)
    rb.Push<u16>(0); // stub 0 (16bit value)
    rb.Push<u16>(0); // stub 0 (16bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void Module::Interface::StartBgImmediate(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::GetTaskProperty0(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0); /// current state of PropertyID 0x0 stub 0 (8bit value)
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) size={:#010X}", size);
}

void Module::Interface::RegisterImmediateTask(Kernel::HLERequestContext& ctx) {
    RegisterTask(ctx);
    LOG_DEBUG(Service_BOSS, "called");
}

void Module::Interface::SetTaskQuery(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
    const u64 programID = rp.Pop<u64>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) programID={:#018X}", programID);
}

void Module::Interface::GetAppNewFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u64 programID = rp.Pop<u64>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0); // 0 = nothing new, 1 = new content

    LOG_WARNING(Service_BOSS, "(STUBBED) programID={:#018X}", programID);
}

void Module::Interface::GetNsDataIdListPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
    const u16 property_id = rp.Pop<u16>();
    const u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_BOSS, "(STUBBED) property_id={:#06X}, size={:#010X}", property_id, size);
}

void Module::Interface::DeleteNsDataPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u64 programID = rp.Pop<u64>();
    const u32 ns_data_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_BOSS, "(STUBBED) programID={:#018X}, ns_data_id={:#010X}", programID,
                ns_data_id);
}

void Module::Interface::GetNsDataHeaderInfoPrivileged(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
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
    : ServiceFramework(name, max_session), boss(std::move(boss)) {
    util = Util();
    this->boss->loader.ReadProgramId(util.program_id);
    this->boss->loader.ReadExtdataId(util.extdata_id);
}

Module::Module(Core::System& system) : loader(system.GetAppLoader()) {
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
