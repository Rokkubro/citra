// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ptm/ptm_u.h"

SERIALIZE_EXPORT_IMPL(Service::PTM::PTM_U)

namespace Service::PTM {

PTM_U::PTM_U(std::shared_ptr<Module> ptm) : Module::Interface(std::move(ptm), "ptm:u", 26) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 0, 2), nullptr, "RegisterAlarmClient"},
        {IPC::MakeHeader(0x0002, 2, 0), nullptr, "SetRtcAlarm"},
        {IPC::MakeHeader(0x0003, 0, 0), nullptr, "GetRtcAlarm"},
        {IPC::MakeHeader(0x0004, 0, 0), nullptr, "CancelRtcAlarm"},
        {IPC::MakeHeader(0x0005, 0, 0), &PTM_U::GetAdapterState, "GetAdapterState"},
        {IPC::MakeHeader(0x0006, 0, 0), &PTM_U::GetShellState, "GetShellState"},
        {IPC::MakeHeader(0x0007, 0, 0), &PTM_U::GetBatteryLevel, "GetBatteryLevel"},
        {IPC::MakeHeader(0x0008, 0, 0), &PTM_U::GetBatteryChargeState, "GetBatteryChargeState"},
        {IPC::MakeHeader(0x0009, 0, 0), &PTM_U::GetPedometerState, "GetPedometerState"},
        {IPC::MakeHeader(0x000A, 1, 2), nullptr, "GetStepHistoryEntry"},
        {IPC::MakeHeader(0x000B, 3, 2), &PTM_U::GetStepHistory, "GetStepHistory"},
        {IPC::MakeHeader(0x000C, 0, 0), &PTM_U::GetTotalStepCount, "GetTotalStepCount"},
        {IPC::MakeHeader(0x000D, 1, 0), nullptr, "SetPedometerRecordingMode"},
        {IPC::MakeHeader(0x000E, 0, 0), nullptr, "GetPedometerRecordingMode"},
        {IPC::MakeHeader(0x000F, 2, 4), nullptr, "GetStepHistoryAll"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::PTM
