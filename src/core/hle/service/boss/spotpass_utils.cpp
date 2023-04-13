// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef ENABLE_WEB_SERVICE
#if defined(__ANDROID__)
#include <ifaddrs.h>
#endif
#include <httplib.h>
#ifdef WIN32
// Needed to prevent conflicts with system macros when httplib is included on windows
#undef CreateEvent
#undef CreateFile
#undef ERROR_NOT_FOUND
#undef ERROR_FILE_NOT_FOUND
#undef ERROR_PATH_NOT_FOUND
#undef ERROR_ALREADY_EXISTS
#endif
#endif
#include <boost/url/src.hpp>
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
#include "core/hw/aes/key.h"

namespace Service::BOSS {

FileSys::Path GetBossDataDir(u64 extdata_id) {

    const u32 high = static_cast<u32>(extdata_id >> 32);
    const u32 low = static_cast<u32>(extdata_id & 0xFFFFFFFF);

    return FileSys::ConstructExtDataBinaryPath(1, high, low);
}

bool WriteBossFile(u64 extdata_id, FileSys::Path file_path, u64 size, const u8* buffer,
                   std::string_view description) {
    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false, true);

    const FileSys::Path boss_path{GetBossDataDir(extdata_id)};

    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);
    if (!archive_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata opening failed");
        return false;
    }
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
    auto boss_archive = std::move(archive_result).Unwrap().get();

    auto file_create_result = boss_archive->CreateFile(file_path, size);
    if (file_create_result.is_error) {
        LOG_WARNING(Service_BOSS, "{} could not be created, it may already exist", description);
    }
    FileSys::Mode file_open_mode = {};
    file_open_mode.write_flag.Assign(1);
    auto file_result = boss_archive->OpenFile(file_path, file_open_mode);
    if (!file_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Could not open {} for writing", description);
        return false;
    }
    auto file = std::move(file_result).Unwrap();
    file->Write(0, size, true, buffer);
    file->Close();
    return true;
}

bool SendNewsMessage(std::vector<u8> decrypted_data, const u32 payload_size, u64 extdata_id,
                     std::string_view file_name_str) {
    // TODO: Actually add a notification to the news service
    // Looks like it has some sort of header(only 0x60 bytes, datetime and unknown at 0x20
    // missing?),  https://www.3dbrew.org/wiki/NEWSS:AddNotification#Header_structure , then
    // the message, then the image
    LOG_INFO(Service_BOSS, "News message was received");
    constexpr u32 news_header_size = 0x60;
    constexpr u32 news_title_offset = 0x20;
    constexpr u32 news_title_size = news_header_size - news_title_offset;
    constexpr u32 news_message_size = 0x1780;
    constexpr u32 news_total_size = news_header_size + news_message_size;

    struct NewsMessage {
        INSERT_PADDING_BYTES(news_title_offset);
        std::array<u8, news_title_size> news_title;
        std::array<u8, news_message_size> news_message;
    };
    static_assert(sizeof(NewsMessage) == news_total_size, "NewsMessage has incorrect size");

    if (payload_size < news_total_size) {
        LOG_ERROR(Service_BOSS, "Payload is too short to contain news message");
        return false;
    }

    NewsMessage* message =
        reinterpret_cast<NewsMessage*>(decrypted_data.data() + BOSS_ENTIRE_HEADER_LENGTH);

    std::u16string_view news_title_string(reinterpret_cast<char16_t*>(message->news_title.data()),
                                          news_title_size / 2);
    std::u16string_view news_message_string(
        reinterpret_cast<char16_t*>(message->news_message.data()), news_message_size / 2);

    LOG_INFO(Service_BOSS, "News title is: {}", Common::UTF16ToUTF8(news_title_string));
    LOG_INFO(Service_BOSS, "News message is:\n{}", Common::UTF16ToUTF8(news_message_string));

    if (payload_size > news_total_size) {
        LOG_INFO(Service_BOSS, "Image is present in news, dumping...");
        if (!WriteBossFile(extdata_id, fmt::format("{}_news_image.jpg", file_name_str),
                           payload_size - news_total_size,
                           decrypted_data.data() + BOSS_ENTIRE_HEADER_LENGTH + news_total_size,
                           "image file")) {
            LOG_WARNING(Service_BOSS, "Could not write image file");
        }
    }
    return true;
}

bool DecryptBossData(const BossPayloadHeader* payload_header, const u32 data_size,
                     const u8* encrypted_data, u8* decrypted_data) {
    // AES details here: https://www.3dbrew.org/wiki/SpotPass#Content_Container
    // IV is data in payload + 32 bit Big Endian 1
    const u32_be one = 1;
    std::vector<u8> iv(sizeof(payload_header->iv_start) + sizeof(one));
    std::memcpy(iv.data(), payload_header->iv_start.data(), sizeof(payload_header->iv_start));
    std::memcpy(iv.data() + sizeof(payload_header->iv_start), &one, sizeof(one));
    LOG_DEBUG(Service_BOSS, "IV is {:#018X}{:16X}",
              static_cast<u64>(*reinterpret_cast<u64_be*>(iv.data())),
              static_cast<u64>(*reinterpret_cast<u64_be*>(iv.data() + sizeof(u64_be))));

    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption aes;
    HW::AES::AESKey key = HW::AES::GetNormalKey(0x38);
    if (key == HW::AES::AESKey{}) {
        LOG_WARNING(Service_BOSS, "AES Key 0x38 not found");
        return false;
    }

    aes.SetKeyWithIV(key.data(), CryptoPP::AES::BLOCKSIZE, iv.data());
    aes.ProcessData(decrypted_data, encrypted_data, data_size);
    return true;
}

bool DownloadBossDataFromURL(std::string_view url, std::string_view file_name, u64 program_id,
                             u64 extdata_id) {
#ifdef ENABLE_WEB_SERVICE
    const auto url_parse_result = boost::urls::parse_uri(url);
    const std::string file_name_str = fmt::format("/{}", file_name);
    if (url_parse_result.has_error()) {
        LOG_ERROR(Service_BOSS, "Invalid URL {}", url);
        return false;
    }
    const auto url_parsed = url_parse_result.value();
    const std::string scheme = url_parsed.scheme();
    const std::string host = url_parsed.host();
    const std::string path = url_parsed.path();
    LOG_DEBUG(Service_BOSS, "Scheme is {}, host is {}, path is {}", scheme, host, path);
    const std::unique_ptr<httplib::Client> client =
        std::make_unique<httplib::Client>(fmt::format("{}://{}", scheme, host));
    httplib::Request request{
        .method = "GET",
        .path = path,
        // Needed when httplib is included on android
        .matches = httplib::Match(),
    };
    client->set_follow_location(true);
    client->enable_server_certificate_verification(false);

    const auto result = client->send(request);
    if (!result) {
        LOG_ERROR(Service_BOSS, "GET to {}://{}{} returned error {}", scheme, host, path,
                  httplib::to_string(result.error()));
        return false;
    }
    const auto& response = result.value();
    if (response.status >= 400) {
        LOG_ERROR(Service_BOSS, "GET to {}://{}{} returned error status code: {}", scheme, host,
                  path, response.status);
        return false;
    }
    if (!response.headers.contains("content-type")) {
        LOG_ERROR(Service_BOSS, "GET to {}://{}{} returned no content", scheme, host, path);
        return false;
    }

    if (response.body.size() < BOSS_PAYLOAD_HEADER_LENGTH) {
        LOG_WARNING(Service_BOSS, "Payload size of {} too short for boss payload",
                    response.body.size());
        return false;
    }
    const BossPayloadHeader* payload_header =
        reinterpret_cast<const BossPayloadHeader*>(response.body.data());

    if (BOSS_MAGIC != payload_header->boss) {
        LOG_WARNING(Service_BOSS, "Start of file is not '{}', it's '{}'", BOSS_MAGIC,
                    static_cast<u32>(payload_header->boss));
        return false;
    }

    if (payload_header->magic != BOSS_PAYLOAD_MAGIC) {
        LOG_WARNING(Service_BOSS, "Magic number mismatch, expecting {}, found {}",
                    BOSS_PAYLOAD_MAGIC, static_cast<u32>(payload_header->magic));
        return false;
    }

    if (payload_header->filesize != response.body.size()) {
        LOG_WARNING(Service_BOSS, "Expecting response to be size {}, actual size is {}",
                    static_cast<u32>(payload_header->filesize), response.body.size());
        return false;
    }

    // Temporarily also write payload (maybe for re-implementing spotpass when it goes down in the
    // future?)

    if (!WriteBossFile(extdata_id, fmt::format("{}_payload", file_name_str), response.body.size(),
                       reinterpret_cast<const u8*>(response.body.data()), "payload file")) {
        LOG_WARNING(Service_BOSS, "Could not write payload file");
    }

    // end payload block
    const u32 data_size = payload_header->filesize - BOSS_PAYLOAD_HEADER_LENGTH;

    std::vector<u8> decrypted_data(data_size);

    if (!DecryptBossData(
            payload_header, data_size,
            reinterpret_cast<const u8*>(response.body.data() + BOSS_PAYLOAD_HEADER_LENGTH),
            decrypted_data.data())) {
        LOG_ERROR(Service_BOSS, "Could not decrypt payload");
        return false;
    }

    // Temporarily also write raw data

    if (!WriteBossFile(extdata_id, fmt::format("{}_raw_data", file_name_str), decrypted_data.size(),
                       decrypted_data.data(), "raw data file")) {
        LOG_WARNING(Service_BOSS, "Could not write raw data file");
    }

    // end raw data block

    if (decrypted_data.size() < BOSS_ENTIRE_HEADER_LENGTH) {
        LOG_WARNING(Service_BOSS, "Payload size to small to be boss data: {}",
                    decrypted_data.size());
        return false;
    }

    BossHeader header{};
    std::memcpy(&header.program_id, decrypted_data.data() + BOSS_CONTENT_HEADER_LENGTH,
                BOSS_HEADER_LENGTH - BOSS_EXTDATA_HEADER_LENGTH);

    const u32 payload_size = static_cast<u32>(decrypted_data.size() - BOSS_ENTIRE_HEADER_LENGTH);
    if (header.payload_size != payload_size) {
        LOG_WARNING(Service_BOSS, "Payload has incorrect size, was expecting {}, found {}",
                    static_cast<u32>(header.payload_size), payload_size);
        return false;
    }

    if (program_id != header.program_id) {
        LOG_WARNING(Service_BOSS, "Mismatched program id, was expecting {:#018X}, found {:#018X}",
                    program_id, u64(header.program_id));
        if (header.program_id == NEWS_PROG_ID) {
            SendNewsMessage(decrypted_data, payload_size, extdata_id, file_name_str);
        }
        return false;
    }

    std::vector<u8> data_file(BOSS_HEADER_LENGTH + payload_size);
    header.header_length = BOSS_EXTDATA_HEADER_LENGTH;
    std::memcpy(data_file.data(), &header, BOSS_HEADER_LENGTH);
    std::memcpy(data_file.data() + BOSS_HEADER_LENGTH,
                decrypted_data.data() + BOSS_ENTIRE_HEADER_LENGTH, payload_size);

    if (!WriteBossFile(extdata_id, file_name_str, data_file.size(), data_file.data(),
                       "spotpass file")) {
        LOG_WARNING(Service_BOSS, "Could not write spotpass file");
    }
    return true;
#else
    LOG_ERROR(Service_BOSS, "Cannot download data as web services are not enabled");
    return false;
#endif
}

std::vector<NsDataEntry> Util::GetNsDataEntries() {
    std::vector<NsDataEntry> ns_data;
    std::vector<FileSys::Entry> boss_files = GetBossExtDataFiles();
    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false, true);
    const FileSys::Path boss_path{GetBossDataDir(extdata_id)};
    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);

    if (!archive_result.Succeeded()) {
        LOG_ERROR(Service_BOSS, "Extdata opening failed");
        return ns_data;
    }
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
    auto boss_archive = std::move(archive_result).Unwrap().get();

    for (const auto& cur_file : boss_files) {
        if (cur_file.is_directory || cur_file.file_size < BOSS_HEADER_LENGTH) {
            LOG_WARNING(Service_BOSS, "Spotpass extdata contains directory or file is too short");
            continue;
        }

        NsDataEntry entry{};
        entry.filename = Common::UTF16ToUTF8(cur_file.filename);
        const FileSys::Path file_path = fmt::format("/{}", entry.filename);
        LOG_DEBUG(Service_BOSS, "Spotpass filename={}", entry.filename);

        FileSys::Mode mode{};
        mode.read_flag.Assign(1);

        auto file_result = boss_archive->OpenFile(file_path, mode);

        if (!file_result.Succeeded()) {
            LOG_WARNING(Service_BOSS, "Opening Spotpass file failed.");
            continue;
        }
        auto file = std::move(file_result).Unwrap();
        LOG_DEBUG(Service_BOSS, "Opening Spotpass file succeeded!");
        file->Read(0, BOSS_HEADER_LENGTH, reinterpret_cast<u8*>(&entry.header));
        // Extdata header should have size 0x18:
        // https://www.3dbrew.org/wiki/SpotPass#Payload_Content_Header
        if (entry.header.header_length != BOSS_EXTDATA_HEADER_LENGTH) {
            LOG_WARNING(Service_BOSS,
                        "Incorrect header length or non-spotpass file; expected {:#010X}, "
                        "found {:#010X}",
                        BOSS_EXTDATA_HEADER_LENGTH, entry.header.header_length);
            continue;
        }
        if (entry.header.program_id != program_id) {
            LOG_WARNING(Service_BOSS,
                        "Mismatched program ID in spotpass data. Was expecting "
                        "{:#018X}, found {:#018X}",
                        program_id, u64(entry.header.program_id));
            continue;
        }
        // Check the payload size is correct, excluding header
        if (entry.header.payload_size != cur_file.file_size - BOSS_HEADER_LENGTH) {
            LOG_WARNING(Service_BOSS,
                        "Mismatched file size, was expecting {:#010X}, found {:#010X}",
                        u32(entry.header.payload_size), cur_file.file_size - BOSS_HEADER_LENGTH);
            continue;
        }
        LOG_DEBUG(
            Service_BOSS, "Datatype is {:#010X}, Payload size is {:#010X}, NsDataID is {:#010X}",
            static_cast<u32>(entry.header.datatype), static_cast<u32>(entry.header.payload_size),
            static_cast<u32>(entry.header.ns_data_id));

        ns_data.push_back(entry);
    }
    return ns_data;
}

std::vector<FileSys::Entry> Util::GetBossExtDataFiles() {

    std::vector<FileSys::Entry> boss_files;

    FileSys::ArchiveFactory_ExtSaveData boss_extdata_archive_factory(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), false, true);
    const FileSys::Path boss_path{GetBossDataDir(extdata_id)};

    auto archive_result = boss_extdata_archive_factory.Open(boss_path, 0);
    if (!archive_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata opening failed");
        return boss_files;
    }
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata opened successfully!");
    auto boss_archive = std::move(archive_result).Unwrap().get();

    auto dir_result = boss_archive->OpenDirectory(DIR_SEP);
    if (!dir_result.Succeeded()) {
        LOG_WARNING(Service_BOSS, "Extdata directory opening failed");
        return boss_files;
    }
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata directory opened successfully!");
    const auto dir = std::move(dir_result).Unwrap();
    // Keep reading the directory 32 files at a time until all files have been checked
    constexpr u32 files_to_read = 32;
    u32 entry_count = 0;
    size_t i = 0;
    do {
        boss_files.resize(boss_files.size() + files_to_read);
        entry_count = dir->Read(files_to_read, boss_files.data() + (i * files_to_read));
    } while (files_to_read <= entry_count && ++i);
    LOG_DEBUG(Service_BOSS, "Spotpass Extdata directory contains {} files",
              i * files_to_read + entry_count);
    boss_files.resize(i * files_to_read + entry_count);
    return boss_files;
}

u16 Util::GetOutputEntries(u32 filter, u32 max_entries, Kernel::MappedBuffer& buffer) {
    std::vector<NsDataEntry> ns_data = GetNsDataEntries();
    std::vector<u32> output_entries;
    for (const auto& cur_entry : ns_data) {
        const u16 datatype_high =
            static_cast<u16>(static_cast<u32>(cur_entry.header.datatype) >> 16);
        const u16 datatype_low =
            static_cast<u16>(static_cast<u32>(cur_entry.header.datatype) & 0xFFFF);
        const u16 filter_high = static_cast<u16>(filter >> 16);
        const u16 filter_low = static_cast<u16>(filter & 0xFFFF);
        if (filter != 0xFFFFFFFF &&
            (filter_high != datatype_high || (filter_low & datatype_low) == 0)) {
            LOG_DEBUG(
                Service_BOSS,
                "Filtered out NsDataID {:#010X}; failed filter {:#010X} with datatype {:#010X}",
                static_cast<u32>(cur_entry.header.ns_data_id), filter,
                static_cast<u32>(cur_entry.header.datatype));
            continue;
        }
        if (output_entries.size() >= max_entries) {
            LOG_WARNING(Service_BOSS, "Reached maximum number of entries");
            break;
        }
        output_entries.push_back(cur_entry.header.ns_data_id);
    }
    buffer.Write(output_entries.data(), 0, sizeof(u32) * output_entries.size());
    LOG_DEBUG(Service_BOSS, "{} usable entries returned", output_entries.size());
    return static_cast<u16>(output_entries.size());
}

std::pair<TaskStatus, u32> Util::GetTaskStatusAndDuration(std::string task_id,
                                                          bool wait_on_result) {
    // Default duration is zero -> means no more runs of the task are allowed
    u32 duration = 0;

    if (!task_id_list.contains(task_id)) {
        LOG_WARNING(Service_BOSS, "Could not find task_id in list");
        return {TaskStatus::Failed, duration};
    }
    LOG_DEBUG(Service_BOSS, "Found currently running task id");
    task_id_list[task_id].times_checked++;

    // Get the duration from the task if available
    if (const auto* dur_prop =
            task_id_list[task_id].props.contains(PropertyID::Duration)
                ? std::get_if<u32>(&task_id_list[task_id].props[PropertyID::Duration])
                : nullptr;
        dur_prop) {
        duration = *dur_prop;
    }

    if (task_id_list[task_id].download_task.valid()) {
        LOG_DEBUG(Service_BOSS, "Task is still running");
        auto status = task_id_list[task_id].download_task.wait_for(std::chrono::microseconds(0));
        if (status == std::future_status::ready || wait_on_result) {
            LOG_DEBUG(Service_BOSS,
                      wait_on_result ? "Waiting for result..." : "Task just finished");

            task_id_list[task_id].task_result = task_id_list[task_id].download_task.get();
            if (task_id_list[task_id].task_result) {
                LOG_DEBUG(Service_BOSS, "Task ran successfully");
                return {TaskStatus::Success, duration};
            }

            LOG_WARNING(Service_BOSS, "Task failed");
            return {TaskStatus::Failed, duration};
        }

        LOG_DEBUG(Service_BOSS, "Task is still running");
        return {TaskStatus::Running, duration};
    }

    LOG_DEBUG(Service_BOSS, "Task has finished running or is invalid");

    if (task_id_list[task_id].task_result) {
        LOG_DEBUG(Service_BOSS, "Task ran successfully");
        return {TaskStatus::Success, duration};
    }

    LOG_WARNING(Service_BOSS, "Task failed");
    return {TaskStatus::Failed, duration};
}

std::optional<NsDataEntry> Util::GetNsDataEntryFromID(u32 ns_data_id) {
    std::vector<NsDataEntry> ns_data = GetNsDataEntries();
    const auto entry_iter = std::find_if(ns_data.begin(), ns_data.end(), [ns_data_id](auto entry) {
        return entry.header.ns_data_id == ns_data_id;
    });
    if (entry_iter == ns_data.end()) {
        LOG_WARNING(Service_BOSS, "Could not find NsData with ID {:#010X}", ns_data_id);
        return std::nullopt;
    }
    return *entry_iter;
}

bool Util::ReadWriteProperties(bool write, PropertyID property_id, u32 size,
                               Kernel::MappedBuffer& buffer) {
    if (!cur_props.props.contains(property_id)) {
        LOG_ERROR(Service_BOSS, "Unknown property with id {:#06X}", property_id);
        return false;
    }

    auto& prop = cur_props.props[property_id];

    auto readwrite_pod = [&]<typename T>(T& cur_prop) {
        static_assert(std::is_trivial<T>::value,
                      "Only trivial types are allowed for readwrite_pod");
        if (size != sizeof(cur_prop)) {
            LOG_ERROR(Service_BOSS, "Unexpected size of property {:#06x}, was expecting {}, got {}",
                      property_id, sizeof(cur_prop), size);
        }
        if (write) {
            buffer.Write(&cur_prop, 0, size);
        } else {
            T new_prop = 0;
            buffer.Read(&new_prop, 0, size);
            prop = new_prop;
            LOG_DEBUG(Service_BOSS, "Read property {:#06X}, value {:#010X}", property_id, new_prop);
        }
    };

    auto readwrite_vector = [&]<typename T>(std::vector<T>& cur_prop) {
        if (size != cur_prop.size() * sizeof(T)) {
            LOG_ERROR(Service_BOSS, "Unexpected size of property {:#06x}, was expecting {}, got {}",
                      property_id, cur_prop.size(), size);
        }
        LOG_DEBUG(Service_BOSS,
                  "Vector property contains {} elements of size {} for a total size of {}",
                  cur_prop.size(), sizeof(T), size);
        if (write) {
            buffer.Write(cur_prop.data(), 0, size);
        } else {
            std::vector<T> new_prop(cur_prop.size());
            buffer.Read(new_prop.data(), 0, size);
            prop = new_prop;
            if (sizeof(T) == sizeof(u8)) {
                LOG_DEBUG(Service_BOSS, "Read property {:#06X}, value {}", property_id,
                          std::string_view(reinterpret_cast<char*>(new_prop.data()), size));
            } else if (sizeof(T) == sizeof(u32) && new_prop.size() == CERTIDLIST_SIZE) {
                LOG_DEBUG(Service_BOSS, "Read property {:#06X}, values {:#010X},{:#010X},{:#010X}",
                          property_id, new_prop[0], new_prop[1], new_prop[2]);
            }
        }
    };

    if (const auto char_prop = std::get_if<u8>(&prop))
        readwrite_pod(*char_prop);
    else if (const auto short_prop = std::get_if<u16>(&prop))
        readwrite_pod(*short_prop);
    else if (const auto int_prop = std::get_if<u32>(&prop))
        readwrite_pod(*int_prop);
    else if (const auto charvec_prop = std::get_if<std::vector<u8>>(&prop))
        readwrite_vector(*charvec_prop);
    else if (const auto intvec_prop = std::get_if<std::vector<u32>>(&prop))
        readwrite_vector(*intvec_prop);

    return true;
}

} // namespace Service::BOSS