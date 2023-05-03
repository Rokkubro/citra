// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_s.h"

namespace Service::CAM {

CAM_S::CAM_S(std::shared_ptr<Module> cam) : Module::Interface(std::move(cam), "cam:s", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 1, 0), &CAM_S::StartCapture, "StartCapture"},
        {IPC::MakeHeader(0x0002, 1, 0), &CAM_S::StopCapture, "StopCapture"},
        {IPC::MakeHeader(0x0003, 1, 0), &CAM_S::IsBusy, "IsBusy"},
        {IPC::MakeHeader(0x0004, 1, 0), &CAM_S::ClearBuffer, "ClearBuffer"},
        {IPC::MakeHeader(0x0005, 1, 0), &CAM_S::GetVsyncInterruptEvent, "GetVsyncInterruptEvent"},
        {IPC::MakeHeader(0x0006, 1, 0), &CAM_S::GetBufferErrorInterruptEvent, "GetBufferErrorInterruptEvent"},
        {IPC::MakeHeader(0x0007, 4, 2), &CAM_S::SetReceiving, "SetReceiving"},
        {IPC::MakeHeader(0x0008, 1, 0), &CAM_S::IsFinishedReceiving, "IsFinishedReceiving"},
        {IPC::MakeHeader(0x0009, 4, 0), &CAM_S::SetTransferLines, "SetTransferLines"},
        {IPC::MakeHeader(0x000A, 2, 0), &CAM_S::GetMaxLines, "GetMaxLines"},
        {IPC::MakeHeader(0x000B, 4, 0), &CAM_S::SetTransferBytes, "SetTransferBytes"},
        {IPC::MakeHeader(0x000C, 1, 0), &CAM_S::GetTransferBytes, "GetTransferBytes"},
        {IPC::MakeHeader(0x000D, 2, 0), &CAM_S::GetMaxBytes, "GetMaxBytes"},
        {IPC::MakeHeader(0x000E, 2, 0), &CAM_S::SetTrimming, "SetTrimming"},
        {IPC::MakeHeader(0x000F, 1, 0), &CAM_S::IsTrimming, "IsTrimming"},
        {IPC::MakeHeader(0x0010, 5, 0), &CAM_S::SetTrimmingParams, "SetTrimmingParams"},
        {IPC::MakeHeader(0x0011, 1, 0), &CAM_S::GetTrimmingParams, "GetTrimmingParams"},
        {IPC::MakeHeader(0x0012, 5, 0), &CAM_S::SetTrimmingParamsCenter, "SetTrimmingParamsCenter"},
        {IPC::MakeHeader(0x0013, 1, 0), &CAM_S::Activate, "Activate"},
        {IPC::MakeHeader(0x0014, 2, 0), &CAM_S::SwitchContext, "SwitchContext"},
        {IPC::MakeHeader(0x0015, 2, 0), nullptr, "SetExposure"},
        {IPC::MakeHeader(0x0016, 2, 0), nullptr, "SetWhiteBalance"},
        {IPC::MakeHeader(0x0017, 2, 0), nullptr, "SetWhiteBalanceWithoutBaseUp"},
        {IPC::MakeHeader(0x0018, 2, 0), nullptr, "SetSharpness"},
        {IPC::MakeHeader(0x0019, 2, 0), nullptr, "SetAutoExposure"},
        {IPC::MakeHeader(0x001A, 1, 0), nullptr, "IsAutoExposure"},
        {IPC::MakeHeader(0x001B, 2, 0), nullptr, "SetAutoWhiteBalance"},
        {IPC::MakeHeader(0x001C, 1, 0), nullptr, "IsAutoWhiteBalance"},
        {IPC::MakeHeader(0x001D, 3, 0), &CAM_S::FlipImage, "FlipImage"},
        {IPC::MakeHeader(0x001E, 8, 0), &CAM_S::SetDetailSize, "SetDetailSize"},
        {IPC::MakeHeader(0x001F, 3, 0), &CAM_S::SetSize, "SetSize"},
        {IPC::MakeHeader(0x0020, 2, 0), &CAM_S::SetFrameRate, "SetFrameRate"},
        {IPC::MakeHeader(0x0021, 2, 0), nullptr, "SetPhotoMode"},
        {IPC::MakeHeader(0x0022, 3, 0), &CAM_S::SetEffect, "SetEffect"},
        {IPC::MakeHeader(0x0023, 2, 0), nullptr, "SetContrast"},
        {IPC::MakeHeader(0x0024, 2, 0), nullptr, "SetLensCorrection"},
        {IPC::MakeHeader(0x0025, 3, 0), &CAM_S::SetOutputFormat, "SetOutputFormat"},
        {IPC::MakeHeader(0x0026, 5, 0), nullptr, "SetAutoExposureWindow"},
        {IPC::MakeHeader(0x0027, 5, 0), nullptr, "SetAutoWhiteBalanceWindow"},
        {IPC::MakeHeader(0x0028, 2, 0), nullptr, "SetNoiseFilter"},
        {IPC::MakeHeader(0x0029, 2, 0), &CAM_S::SynchronizeVsyncTiming, "SynchronizeVsyncTiming"},
        {IPC::MakeHeader(0x002A, 2, 0), &CAM_S::GetLatestVsyncTiming, "GetLatestVsyncTiming"},
        {IPC::MakeHeader(0x002B, 0, 0), &CAM_S::GetStereoCameraCalibrationData, "GetStereoCameraCalibrationData"},
        {IPC::MakeHeader(0x002C, 16, 0), nullptr, "SetStereoCameraCalibrationData"},
        {IPC::MakeHeader(0x002D, 3, 0), nullptr, "WriteRegisterI2c"},
        {IPC::MakeHeader(0x002E, 3, 0), nullptr, "WriteMcuVariableI2c"},
        {IPC::MakeHeader(0x002F, 2, 0), nullptr, "ReadRegisterI2cExclusive"},
        {IPC::MakeHeader(0x0030, 2, 0), nullptr, "ReadMcuVariableI2cExclusive"},
        {IPC::MakeHeader(0x0031, 6, 0), nullptr, "SetImageQualityCalibrationData"},
        {IPC::MakeHeader(0x0032, 0, 0), nullptr, "GetImageQualityCalibrationData"},
        {IPC::MakeHeader(0x0033, 11, 0), &CAM_S::SetPackageParameterWithoutContext, "SetPackageParameterWithoutContext"},
        {IPC::MakeHeader(0x0034, 5, 0), &CAM_S::SetPackageParameterWithContext, "SetPackageParameterWithContext"},
        {IPC::MakeHeader(0x0035, 7, 0), &CAM_S::SetPackageParameterWithContextDetail, "SetPackageParameterWithContextDetail"},
        {IPC::MakeHeader(0x0036, 0, 0), &CAM_S::GetSuitableY2rStandardCoefficient, "GetSuitableY2rStandardCoefficient"},
        {IPC::MakeHeader(0x0037, 8, 2), nullptr, "PlayShutterSoundWithWave"},
        {IPC::MakeHeader(0x0038, 1, 0), &CAM_S::PlayShutterSound, "PlayShutterSound"},
        {IPC::MakeHeader(0x0039, 0, 0), &CAM_S::DriverInitialize, "DriverInitialize"},
        {IPC::MakeHeader(0x003A, 0, 0), &CAM_S::DriverFinalize, "DriverFinalize"},
        {IPC::MakeHeader(0x003B, 0, 0), nullptr, "GetActivatedCamera"},
        {IPC::MakeHeader(0x003C, 0, 0), nullptr, "GetSleepCamera"},
        {IPC::MakeHeader(0x003D, 1, 0), nullptr, "SetSleepCamera"},
        {IPC::MakeHeader(0x003E, 1, 0), nullptr, "SetBrightnessSynchronization"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CAM

SERIALIZE_EXPORT_IMPL(Service::CAM::CAM_S)
