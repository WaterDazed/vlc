/*****************************************************************************
 * Copyright (C) 2012-2023 VLC authors and VideoLAN
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

#include "mmdevice_volume_control.h"
#include <vlc_aout.h>
#include "mmdevice.h"

typedef struct
{
    mmdevice_volume_controler_t iface;

    audio_output_t *aout;

    float requested_volume; /**< volume */
    signed char requested_mute; /**< mute */

    bool report_mute;
    bool report_volume;

    float volume;
    float gain; /**< gain */

    HANDLE work_event;

} mmdevice_player_volume_controler_t;

#define VOLUME_CONTROL_IMPL_FROM_IFACE(obj) \
    container_of((obj), mmdevice_player_volume_controler_t, iface)

static int RequestMute(mmdevice_volume_controler_t* controler, bool mute)
{
    mmdevice_player_volume_controler_t *sys = VOLUME_CONTROL_IMPL_FROM_IFACE(controler);
    sys->requested_mute = mute;
    SetEvent(sys->work_event);
    return 0;
}

static int RequestVolume(mmdevice_volume_controler_t* controler, float value, float* outGain)
{
    mmdevice_player_volume_controler_t *sys = VOLUME_CONTROL_IMPL_FROM_IFACE(controler);

    float gain = 1.f;

    value = value * value * value; /* ISimpleAudioVolume is tapered linearly. */

    if (value > 1.f)
    {
        gain = value;
        value = 1.f;
    }

    sys->requested_volume = value;
    sys->gain =gain;
    if (outGain != NULL)
        *outGain = gain;
    SetEvent(sys->work_event);

    return 0;
}

static void SetAllStreamVolume(IAudioStreamVolume *stream_volume, UINT32 chan_count, float volume)
{
    float chan_volumes[chan_count];
    for (UINT32 i = 0; i < chan_count; ++i)
        chan_volumes[i] = volume;
    IAudioStreamVolume_SetAllVolumes(stream_volume, chan_count, chan_volumes);
}

static void Process(mmdevice_volume_controler_t* controler, struct aout_stream_owner *stream)
{
    mmdevice_player_volume_controler_t *sys = VOLUME_CONTROL_IMPL_FROM_IFACE(controler);

    if (!stream
        || (sys->requested_mute < 0 &&  sys->requested_volume < 0))
        return;

    IAudioStreamVolume *stream_volume = NULL;

    void* pv;
    HRESULT hr = aout_stream_GetService(stream, &IID_IAudioStreamVolume, &pv);
    if (FAILED(hr))
        goto error;
    stream_volume = pv;

    UINT32 chan_count;
    hr = IAudioStreamVolume_GetChannelCount(stream_volume, &chan_count);
    if (FAILED(hr))
        goto error;
    assert(chan_count <= 64);

    if (chan_count == 0)
        goto error;

    if (sys->requested_mute >= 0)
    {
        if (sys->requested_mute)
            SetAllStreamVolume(stream_volume, chan_count, 0.f);
        else
            SetAllStreamVolume(stream_volume, chan_count, sys->volume);

        sys->report_mute = true;
        sys->requested_mute = -1;
    }

    if (sys->requested_volume >= 0.f)
    {
        SetAllStreamVolume(stream_volume, chan_count, sys->requested_volume);

        sys->volume = sys->requested_volume;
        sys->report_volume = true;
        sys->requested_volume = -1.f;
    }

    float currentVolume;
    hr = IAudioStreamVolume_GetChannelVolume(stream_volume, 0,
                                     &currentVolume);
    if (FAILED(hr))
        goto error;

    if (sys->report_mute)
    {
        aout_MuteReport(sys->aout, currentVolume == 0.f);
        sys->report_mute =false;
    }

    if (sys->report_volume)
    {
        aout_VolumeReport(sys->aout, cbrtf(currentVolume * sys->gain));
        sys->report_volume =false;
    }

error:
    if (stream_volume)
        IAudioStreamVolume_Release(stream_volume);

}

static bool Initialize(struct mmdevice_volume_controler_t* controler, IAudioSessionManager *manager, LPCGUID guid)
{
    VLC_UNUSED(controler);
    VLC_UNUSED(manager);
    VLC_UNUSED(guid);
    return true;
}


static void Release(mmdevice_volume_controler_t* controler)
{
    mmdevice_player_volume_controler_t *sys = VOLUME_CONTROL_IMPL_FROM_IFACE(controler);
    free(sys);
}

mmdevice_volume_controler_t* createMMDevicePlayerVolumeControler(audio_output_t *aout, HANDLE work_event)
{
    mmdevice_player_volume_controler_t* sys = malloc(sizeof(mmdevice_player_volume_controler_t));
    if (!sys)
        return NULL;

    sys->aout = aout;

    sys->requested_mute = -1;
    sys->requested_volume = -1.f;
    //report initial volume and mute state
    sys->report_mute = true;
    sys->report_volume = true;

    sys->volume = 1.f;
    sys->gain = 1.f;
    sys->work_event = work_event;

    sys->iface.release = Release;

    sys->iface.initialize = Initialize;
    sys->iface.process = Process;

    sys->iface.requestMute = RequestMute;
    sys->iface.requestVolume = RequestVolume;

    return &sys->iface;

}
