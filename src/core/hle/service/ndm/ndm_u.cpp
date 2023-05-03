// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/ndm/ndm_u.h"

SERIALIZE_EXPORT_IMPL(Service::NDM::NDM_U)

namespace Service::NDM {

void NDM_U::EnterExclusiveState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 1, 2);
    exclusive_state = rp.PopEnum<ExclusiveState>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) exclusive_state=0x{:08X}", exclusive_state);
}

void NDM_U::LeaveExclusiveState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 0, 2);
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::QueryExclusiveMode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(exclusive_state);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::LockState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 0, 2);
    rp.PopPID();
    daemon_lock_enabled = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::UnlockState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 0, 2);
    rp.PopPID();
    daemon_lock_enabled = false;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::SuspendDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 1, 0);
    u32 bit_mask = rp.Pop<u32>() & 0xF;
    daemon_bit_mask =
        static_cast<DaemonMask>(static_cast<u32>(default_daemon_bit_mask) & ~bit_mask);
    for (std::size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Suspended;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x{:08X}", bit_mask);
}

void NDM_U::ResumeDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 1, 0);
    u32 bit_mask = rp.Pop<u32>() & 0xF;
    daemon_bit_mask = static_cast<DaemonMask>(static_cast<u32>(daemon_bit_mask) & ~bit_mask);
    for (std::size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Idle;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x{:08X}", bit_mask);
}

void NDM_U::SuspendScheduler(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 1, 0);
    bool perform_in_background = rp.Pop<bool>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) perform_in_background={}", perform_in_background);
}

void NDM_U::ResumeScheduler(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x09, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::QueryStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0D, 1, 0);
    u8 daemon = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(daemon_status.at(daemon));
    LOG_WARNING(Service_NDM, "(STUBBED) daemon=0x{:02X}", daemon);
}

void NDM_U::GetDaemonDisableCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0E, 1, 0);
    u8 daemon = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // current process disable count
    rb.Push<u32>(0); // total disable count
    LOG_WARNING(Service_NDM, "(STUBBED) daemon=0x{:02X}", daemon);
}

void NDM_U::GetSchedulerDisableCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // current process disable count
    rb.Push<u32>(0); // total disable count
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::SetScanInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 1, 0);
    scan_interval = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) scan_interval=0x{:08X}", scan_interval);
}

void NDM_U::GetScanInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(scan_interval);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::SetRetryInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 1, 0);
    retry_interval = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) retry_interval=0x{:08X}", retry_interval);
}

void NDM_U::GetRetryInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x13, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(retry_interval);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::OverrideDefaultDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 1, 0);
    u32 bit_mask = rp.Pop<u32>() & 0xF;
    default_daemon_bit_mask = static_cast<DaemonMask>(bit_mask);
    daemon_bit_mask = default_daemon_bit_mask;
    for (std::size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Idle;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x{:08X}", bit_mask);
}

void NDM_U::ResetDefaultDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x15, 0, 0);
    default_daemon_bit_mask = DaemonMask::Default;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::GetDefaultDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x16, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(default_daemon_bit_mask);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::ClearHalfAwakeMacFilter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

NDM_U::NDM_U() : ServiceFramework("ndm:u", 6) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 1, 2), &NDM_U::EnterExclusiveState, "EnterExclusiveState"},
        {IPC::MakeHeader(0x0002, 0, 2), &NDM_U::LeaveExclusiveState, "LeaveExclusiveState"},
        {IPC::MakeHeader(0x0003, 0, 0), &NDM_U::QueryExclusiveMode, "QueryExclusiveMode"},
        {IPC::MakeHeader(0x0004, 0, 2), &NDM_U::LockState, "LockState"},
        {IPC::MakeHeader(0x0005, 0, 2), &NDM_U::UnlockState, "UnlockState"},
        {IPC::MakeHeader(0x0006, 1, 0), &NDM_U::SuspendDaemons, "SuspendDaemons"},
        {IPC::MakeHeader(0x0007, 1, 0), &NDM_U::ResumeDaemons, "ResumeDaemons"},
        {IPC::MakeHeader(0x0008, 1, 0), &NDM_U::SuspendScheduler, "SuspendScheduler"},
        {IPC::MakeHeader(0x0009, 0, 0), &NDM_U::ResumeScheduler, "ResumeScheduler"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "GetCurrentState"},
        {IPC::MakeHeader(0x000B, 0, 0), nullptr, "GetTargetState"},
        {IPC::MakeHeader(0x000C, 0, 0), nullptr, "<Stubbed>"},
        {IPC::MakeHeader(0x000D, 1, 0), &NDM_U::QueryStatus, "QueryStatus"},
        {IPC::MakeHeader(0x000E, 1, 0), &NDM_U::GetDaemonDisableCount, "GetDaemonDisableCount"},
        {IPC::MakeHeader(0x000F, 0, 0), &NDM_U::GetSchedulerDisableCount, "GetSchedulerDisableCount"},
        {IPC::MakeHeader(0x0010, 1, 0), &NDM_U::SetScanInterval, "SetScanInterval"},
        {IPC::MakeHeader(0x0011, 0, 0), &NDM_U::GetScanInterval, "GetScanInterval"},
        {IPC::MakeHeader(0x0012, 1, 0), &NDM_U::SetRetryInterval, "SetRetryInterval"},
        {IPC::MakeHeader(0x0013, 0, 0), &NDM_U::GetRetryInterval, "GetRetryInterval"},
        {IPC::MakeHeader(0x0014, 1, 0), &NDM_U::OverrideDefaultDaemons, "OverrideDefaultDaemons"},
        {IPC::MakeHeader(0x0015, 0, 0), &NDM_U::ResetDefaultDaemons, "ResetDefaultDaemons"},
        {IPC::MakeHeader(0x0016, 0, 0), &NDM_U::GetDefaultDaemons, "GetDefaultDaemons"},
        {IPC::MakeHeader(0x0017, 0, 0), &NDM_U::ClearHalfAwakeMacFilter, "ClearHalfAwakeMacFilter"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<NDM_U>()->InstallAsService(service_manager);
}

} // namespace Service::NDM
