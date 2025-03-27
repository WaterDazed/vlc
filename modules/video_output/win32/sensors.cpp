/*****************************************************************************
 * sensors.cpp: Windows sensor handling
 *****************************************************************************
 * Copyright © 2017 Steve Lhomme
 * Copyright © 2017-2025 VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
 *          Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_gyroscope.h>
#include <vlc_plugin.h>

#include "events.h"
#include "sensors.h"

#include <initguid.h>
#include <wrl/client.h>
#include <propsys.h> /* stupid mingw headers don't include this */
#include <sensors.h>
#include <sensorsapi.h>
#include <functional>

#include <new>

using Microsoft::WRL::ComPtr;

class SensorReceiver : public ISensorEvents
{
public:
    template<typename T>
    SensorReceiver(const vlc_viewpoint_t & init_viewpoint, T fnOnViewpointChanged)
        : on_viewpoint_changed(std::move(fnOnViewpointChanged))
        , current_pos(init_viewpoint)
    {}

    virtual ~SensorReceiver()
    {}

    STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
    {
        if (ppv == NULL)
        {
            return E_POINTER;
        }
        if (iid == __uuidof(IUnknown))
        {
            *ppv = static_cast<IUnknown*>(this);
        }
        else if (iid == __uuidof(ISensorEvents))
        {
            *ppv = static_cast<ISensorEvents*>(this);
        }
        else
        {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    STDMETHODIMP_(ULONG) Release()
    {
        ULONG count = InterlockedDecrement(&m_cRef);
        if (count == 0)
        {
            delete this;
            return 0;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE OnStateChanged(ISensor *pSensor, SensorState state)
    {
        (void)pSensor;
        (void)state;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDataUpdated(ISensor *pSensor, ISensorDataReport *pNewData)
    {
        (void)pSensor;
        vlc_viewpoint_t old_pos = current_pos;
        HRESULT hr;
        PROPVARIANT pvRot;

        float yaw = 0.f, pitch = 90.f, roll = 0.f;
        PropVariantInit(&pvRot);
        hr = pNewData->GetSensorValue(SENSOR_DATA_TYPE_TILT_X_DEGREES, &pvRot);
        if (SUCCEEDED(hr) && pvRot.vt == VT_R4)
        {
            pitch = pvRot.fltVal;
            PropVariantClear(&pvRot);
        }
        hr = pNewData->GetSensorValue(SENSOR_DATA_TYPE_TILT_Y_DEGREES, &pvRot);
        if (SUCCEEDED(hr) && pvRot.vt == VT_R4)
        {
            roll = pvRot.fltVal;
            PropVariantClear(&pvRot);
        }
        hr = pNewData->GetSensorValue(SENSOR_DATA_TYPE_TILT_Z_DEGREES, &pvRot);
        if (SUCCEEDED(hr) && pvRot.vt == VT_R4)
        {
            yaw = pvRot.fltVal;
            PropVariantClear(&pvRot);
        }

        // TODO: use current_pos directly?
        /* Zero initialize vp for field of view. */
        vlc_viewpoint_t vp {};
        vp.fov = FIELD_OF_VIEW_DEGREES_DEFAULT;
        vlc_viewpoint_from_euler(&vp, yaw, pitch, roll);

        if (this->on_viewpoint_changed)
            this->on_viewpoint_changed(&vp);

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnEvent(ISensor *pSensor, REFGUID eventID, IPortableDeviceValues *pEventData)
    {
        (void)pSensor;
        (void)eventID;
        (void)pEventData;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnLeave(REFSENSOR_ID ID)
    {
        (void)ID;
        return S_OK;
    }

    void ReadViewpoint(vlc_viewpoint_t *vp)
    {
        // TODO
        memcpy(vp->quat, m_quat, sizeof vp->quat);
    }

private:
    std::function<void(vlc_viewpoint_t *)> on_viewpoint_changed;
    vlc_viewpoint_t current_pos;
    long m_cRef;
    float m_quat[4];
};

void *HookWindowsSensorsInternal(vlc_logger *vd, std::function<void(const vlc_viewpoint_t *)> onViewpointChanged, HWND hwnd)
{
    ComPtr<ISensorManager> pSensorManager;
    HRESULT hr = CoCreateInstance( __uuidof(SensorManager),
                      NULL, CLSCTX_INPROC_SERVER,
                      IID_PPV_ARGS(pSensorManager.GetAddressOf()) );
    if (FAILED(hr))
        return NULL;

    ComPtr<ISensorCollection> pInclinometers;
    hr = pSensorManager->GetSensorsByType(SENSOR_TYPE_INCLINOMETER_3D, &pInclinometers);
    if (FAILED(hr))
    {
        vlc_debug(vd, "inclinometer not found. (hr=0x%lX)", hr);
        return NULL;
    }

    ULONG count;
    pInclinometers->GetCount(&count);
    vlc_debug(vd, "Found %lu inclinometer", count);
    for (ULONG i=0; i<count; ++i)
    {
        ComPtr<ISensor> pSensor;
        hr = pInclinometers->GetAt(i, &pSensor);
        if (FAILED(hr))
            continue;

        SensorState state = SENSOR_STATE_NOT_AVAILABLE;
        hr = pSensor->GetState(&state);
        if (FAILED(hr))
            continue;

        if (state == SENSOR_STATE_ACCESS_DENIED)
            hr = pSensorManager->RequestPermissions(hwnd, pInclinometers.Get(), TRUE);

        if (FAILED(hr))
            continue;

        vlc_viewpoint_t start_viewpoint;
        vlc_viewpoint_init(&start_viewpoint);
        PROPVARIANT pvRot;
        PropVariantInit(&pvRot);
        float yaw = 0.f, pitch = 0.f, roll = 0.f;
        hr = pSensor->GetProperty(SENSOR_DATA_TYPE_TILT_X_DEGREES, &pvRot);
        if (SUCCEEDED(hr) && pvRot.vt == VT_R4)
        {
            pitch = pvRot.fltVal;
            PropVariantClear(&pvRot);
        }
        hr = pSensor->GetProperty(SENSOR_DATA_TYPE_TILT_Y_DEGREES, &pvRot);
        if (SUCCEEDED(hr) && pvRot.vt == VT_R4)
        {
            roll = pvRot.fltVal;
            PropVariantClear(&pvRot);
        }
        hr = pSensor->GetProperty(SENSOR_DATA_TYPE_TILT_Z_DEGREES, &pvRot);
        if (SUCCEEDED(hr) && pvRot.vt == VT_R4)
        {
            yaw = pvRot.fltVal;
            PropVariantClear(&pvRot);
        }
        vlc_viewpoint_from_euler(&start_viewpoint, yaw, pitch, roll);

        SensorReceiver *received = new(std::nothrow) SensorReceiver(start_viewpoint, std::move(onViewpointChanged));
        if (received == NULL)
        {
            pSensor->SetEventSink(received);
            return pSensor.Detach();
        }
    }
    return NULL;
}

void *HookWindowsSensors(vlc_logger *logger, const vout_display_owner_t *owner, HWND hwnd)
{
    HookWindowsSensorsInternal(logger, [owner](const vlc_viewpoint_t *vp){
        if (owner && owner->viewpoint_moved)
            owner->viewpoint_moved(owner->sys, vp);
    }, hwnd);

    return NULL;
}

void UnhookWindowsSensors(void *vSensor)
{
    if (!vSensor)
        return;

    ISensor *pSensor = static_cast<ISensor*>(vSensor);
    pSensor->SetEventSink(NULL);
    pSensor->Release();
}

static void SensorsEnable(struct vlc_gyroscope *gyroscope, bool enabled)
{
    (void)gyroscope; (void)enabled;
    /* No enable / disable function implemented right now. */
}

static void SensorsGetViewpoint(struct vlc_gyroscope *gyroscope, vlc_viewpoint_t *out)
{
    auto *sensors = static_cast<SensorReceiver*>(gyroscope->sys);
    sensors->ReadViewpoint(out);
}

static void SensorsDestroy(struct vlc_gyroscope *gyroscope)
{
    UnhookWindowsSensors(gyroscope->sys);
}

static int OpenSensors(struct vlc_gyroscope *gyroscope)
{
    struct vlc_logger *logger = vlc_object_logger(gyroscope);

    HWND window = nullptr;
    //if (gyroscope->surface != NULL && gyroscope->surface->type == VLC_WINDOW_TYPE_HWND)
    //    window = gyroscope->surface->handle.hwnd;

    void *sensor = HookWindowsSensorsInternal(logger, nullptr, window);
    if (sensor== nullptr)
        return VLC_EGENERIC;

    static const struct vlc_gyroscope_operations ops = {
        .enable = SensorsEnable,
        .get_viewpoint = SensorsGetViewpoint,
        .destroy = SensorsDestroy,
    };

    gyroscope->ops = &ops;
    gyroscope->sys = sensor;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_description("Windows gyroscope")
    set_callback(OpenSensors)
    set_capability("gyroscope", 100)
vlc_module_end()
