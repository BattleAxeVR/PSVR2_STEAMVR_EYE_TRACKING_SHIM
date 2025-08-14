// MIT License
//
// Copyright(c) 2025 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pch.h"

#include "ShimDriverManager.h"
#include "DetourUtils.h"
#include "Tracing.h"

#include "defines.h"

#if ENABLE_PSVR2_EYE_TRACKING
#include "psvr2_eye_tracking.h"
#endif

namespace vr {
    struct VREyeTrackingData_t {
        uint16_t flag1;
        uint8_t flag2;
        DirectX::XMVECTOR vector;
    };

    struct IVRDriverInputInternal_XXX {
        virtual vr::EVRInputError CreateEyeTrackingComponent(vr::PropertyContainerHandle_t ulContainer,
                                                             const char* pchName,
                                                             vr::VRInputComponentHandle_t* pHandle) = 0;

        virtual vr::EVRInputError UpdateEyeTrackingComponent(vr::VRInputComponentHandle_t ulComponent,
                                                             VREyeTrackingData_t* data) = 0;
    };

    struct IVRDriverInput_XXX {
        virtual void dummy00() = 0;
        virtual void dummy01() = 0;
        virtual void dummy02() = 0;
        virtual void dummy03() = 0;
        virtual void dummy04() = 0;
        virtual void dummy05() = 0;
        virtual void dummy06() = 0;
        // IVRDriverInput_004 starts here
        virtual void dummy07() = 0;
        virtual void dummy08() = 0;
        virtual vr::EVRInputError CreateEyeTrackingComponent(vr::PropertyContainerHandle_t ulContainer,
                                                             const char* pchName,
                                                             vr::VRInputComponentHandle_t* pHandle) = 0;
        virtual vr::EVRInputError UpdateEyeTrackingComponent(vr::VRInputComponentHandle_t ulComponent,
                                                             VREyeTrackingData_t* data) = 0;
    };
} // namespace vr

namespace 
{
    using namespace driver_shim;

    // The HmdShimDriver driver wraps another ITrackedDeviceServerDriver instance with the intent to override
    // properties and behaviors.
    struct HmdShimDriver : public vr::ITrackedDeviceServerDriver 
    {
        HmdShimDriver(vr::ITrackedDeviceServerDriver* shimmedDevice)
            : m_shimmedDevice(shimmedDevice)
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "HmdShimDriver_Ctor");
            TraceLoggingWriteStop(local, "HmdShimDriver_Ctor");
        }

        vr::EVRInitError Activate(uint32_t unObjectId) override 
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "HmdShimDriver_Activate", TLArg(unObjectId, "ObjectId"));

            // Activate the real device driver.
            m_shimmedDevice->Activate(unObjectId);

            m_deviceIndex = unObjectId;

            const vr::PropertyContainerHandle_t container =
                vr::VRProperties()->TrackedDeviceToPropertyContainer(m_deviceIndex);

            // Advertise that we will pass eye tracking data. This is an undocumented property.
            vr::VRProperties()->SetBoolProperty(container, (vr::ETrackedDeviceProperty)6009, true);

            // Get the internal interface.
            vr::EVRInitError eError;
            {
                IVRDriverInput_XXX =
                    (vr::IVRDriverInput_XXX*)vr::VRDriverContext()->GetGenericInterface("IVRDriverInput_004", &eError);

                if (IVRDriverInput_XXX) 
                {
                    const void** vtable = *((const void***)IVRDriverInput_XXX);
                    if (((intptr_t)vtable[9] - (intptr_t)vtable[0]) > 0x10000) {
                        IVRDriverInput_XXX = nullptr;
                    }
                }
                if (!IVRDriverInput_XXX) 
                {
                    DriverLog("IVRDriverInput_004 appears ineligible for eye tracking");
                }
            }
            if (!IVRDriverInput_XXX) 
            {
                IVRDriverInputInternal_XXX =
                    (vr::IVRDriverInputInternal_XXX*)vr::VRDriverContext()->GetGenericInterface(
                        "IVRDriverInputInternal_XXX", &eError);

                if (IVRDriverInputInternal_XXX) 
                {
                    const void** vtable = *((const void***)IVRDriverInputInternal_XXX);
                    if (((intptr_t)vtable[1] - (intptr_t)vtable[0]) > 0x10000) {
                        IVRDriverInputInternal_XXX = nullptr;
                    }
                }
                if (!IVRDriverInputInternal_XXX) 
                {
                    DriverLog("IVRDriverInputInternal appears ineligible for eye tracking");
                }
            }

            if (IVRDriverInput_XXX) 
            {
                IVRDriverInput_XXX->CreateEyeTrackingComponent(container, "/eyetracking", &m_eyeTrackingComponent);
            } 
            else if (IVRDriverInputInternal_XXX) 
            {
                IVRDriverInputInternal_XXX->CreateEyeTrackingComponent(container, "/eyetracking", &m_eyeTrackingComponent);
            } 
            else 
            {
                DriverLog("No usable interface for eye tracking");
            }
            DriverLog("Eye Gaze Component: %lld", m_eyeTrackingComponent);

            // Schedule updates in a background thread.
            m_active = true;
            m_updateThread = std::thread(&HmdShimDriver::UpdateThread, this);

            TraceLoggingWriteStop(local, "HmdShimDriver_Activate");

            return vr::VRInitError_None;
        }

        void Deactivate() override 
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "HmdShimDriver_Deactivate", TLArg(m_deviceIndex, "ObjectId"));

            if (m_active.exchange(false)) 
            {
                m_updateThread.join();
            }

            m_deviceIndex = vr::k_unTrackedDeviceIndexInvalid;

            m_shimmedDevice->Deactivate();

            DriverLog("Deactivated device shimmed with HmdShimDriver");

            TraceLoggingWriteStop(local, "HmdShimDriver_Deactivate");
        }

        void EnterStandby() override 
        {
            m_shimmedDevice->EnterStandby();
        }

        void* GetComponent(const char* pchComponentNameAndVersion) override 
        {
            return m_shimmedDevice->GetComponent(pchComponentNameAndVersion);
        }

        vr::DriverPose_t GetPose() override 
        {
            return m_shimmedDevice->GetPose();
        }

        void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override 
        {
            m_shimmedDevice->DebugRequest(pchRequest, pchResponseBuffer, unResponseBufferSize);
        }

#if ENABLE_PSVR2_EYE_TRACKING
        BVR::PSVR2EyeTracker psvr2_eye_tracker_;
#endif

        void UpdateThread() 
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "HmdShimDriver_UpdateThread");

            DriverLog("Hello from HmdShimDriver::UpdateThread");
            SetThreadDescription(GetCurrentThread(), L"HmdShimDriver_UpdateThread");

            const vr::PropertyContainerHandle_t container =
                vr::VRProperties()->TrackedDeviceToPropertyContainer(m_deviceIndex);

            vr::VREyeTrackingData_t data{};

            while (true) 
            {
                // Wait for the next time to update.
                {
                    TraceLocalActivity(sleep);
                    TraceLoggingWriteStart(sleep, "HmdShimDriver_UpdateThread_Sleep");

                    // We refresh the data at this frequency.
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));

                    TraceLoggingWriteStop(sleep, "HmdShimDriver_UpdateThread_Sleep", TLArg(m_active.load(), "Active"));

                    if (!m_active) 
                    {
                        break;
                    }
                }

                data.vector = DirectX::XMVectorSet(0, 0, -1, 1);

#if ENABLE_PSVR2_EYE_TRACKING
                if(!psvr2_eye_tracker_.is_connected())
                {
                    psvr2_eye_tracker_.connect();
                }

                BVR::XrVector3f combined_gaze;
                const bool isEyeTrackingDataAvailable = psvr2_eye_tracker_.is_connected() && psvr2_eye_tracker_.update_gazes() && psvr2_eye_tracker_.get_combined_gaze(combined_gaze, false);

                if(isEyeTrackingDataAvailable)
                {
                    data.vector = DirectX::XMVectorSet(combined_gaze.x, combined_gaze.y, combined_gaze.z, 1.0f);
                }
#else
                const bool isEyeTrackingDataAvailable = true;//&& state.TimeInSeconds > 0;
#endif
                

                if (isEyeTrackingDataAvailable) 
                {
					data.flag1 = 0x101;
					data.flag2 = 0x1;
                } 
                else 
                {
                    data.flag1 = 0;
                    data.flag2 = 0;
                }

                if (IVRDriverInput_XXX) 
                {
                    IVRDriverInput_XXX->UpdateEyeTrackingComponent(m_eyeTrackingComponent, &data);
                } 
                else if (IVRDriverInputInternal_XXX) 
                {
                    IVRDriverInputInternal_XXX->UpdateEyeTrackingComponent(m_eyeTrackingComponent, &data);
                }
            }

            DriverLog("Bye from HmdShimDriver::UpdateThread");

            TraceLoggingWriteStop(local, "HmdShimDriver_UpdateThread");
        }

        vr::ITrackedDeviceServerDriver* const m_shimmedDevice;

        vr::TrackedDeviceIndex_t m_deviceIndex = vr::k_unTrackedDeviceIndexInvalid;

        std::atomic<bool> m_active = false;
        std::thread m_updateThread;

        vr::VRInputComponentHandle_t m_eyeTrackingComponent = 0;
        vr::IVRDriverInputInternal_XXX* IVRDriverInputInternal_XXX = nullptr;
        vr::IVRDriverInput_XXX* IVRDriverInput_XXX = nullptr;
    };
} // namespace

namespace driver_shim {

    vr::ITrackedDeviceServerDriver* CreateHmdShimDriver(vr::ITrackedDeviceServerDriver* shimmedDriver) 
    {
        return new HmdShimDriver(shimmedDriver);
    }

} // namespace driver_shim
