/*****************************************************************************
 * android_sensors.c: Android sensor handling
 *****************************************************************************
 * Copyright © 2024-2025 VideoLabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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
#include <vlc_threads.h>
#include <vlc_plugin.h>
#include <vlc_viewpoint.h>

#include <android/sensor.h>

struct android_sensors {
    vlc_thread_t thread;

    struct {
        bool error;
        vlc_sem_t sem_wait;
    } startup;

    ASensorManager *mgr;
    const ASensor *gyroscope;
    vlc_viewpoint_t viewpoint;
    vlc_mutex_t lock;
};

static void MultiplyQuat(const float q1[4], const float q2[4],
                         float qO[4])
{
    qO[3] = q1[3] * q2[3] - q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2];
    qO[0] = q1[0] * q2[3] + q1[3] * q2[0] + q1[1] * q2[2] - q1[2] * q2[1];
    qO[1] = q1[1] * q2[3] + q1[3] * q2[1] - q1[0] * q2[2] + q1[2] * q2[0]; 
    qO[2] = q1[2] * q2[3] + q2[3] * q2[2] + q1[0] * q2[1] - q1[1] * q2[0];
}

static void* EventQueueThread(void *opaque)
{
    struct vlc_gyroscope *gyroscope = opaque;
    struct android_sensors *sys = gyroscope->sys;

    /* We need the ALOOPER_PREPARE_ALLOW_NON_CALLBACKS flag to get the
     * updates from the sensors. */
    ALooper *looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if (looper == NULL)
    {
        sys->startup.error = true;
        vlc_sem_post(&sys->startup.sem_wait);
        return NULL;
    }

    ASensorEventQueue *queue = 
        ASensorManager_createEventQueue(sys->mgr, looper, 0, NULL, NULL);
    if (queue == NULL)
    {
        sys->startup.error = true;
        vlc_sem_post(&sys->startup.sem_wait);
        goto release_looper;
    }

    /* Enable and setup the device, both are required. */
    if (ASensorEventQueue_enableSensor(queue, sys->gyroscope) != 0)
    {
        sys->startup.error = true;
        vlc_sem_post(&sys->startup.sem_wait);
        goto release_queue;
    }

    /* Set sampling frequency to every 2ms or more if the gyroscope cannot */
    int min_delay = ASensor_getMinDelay(sys->gyroscope);
    ASensorEventQueue_setEventRate(queue, sys->gyroscope, __MAX(2000, min_delay));

    /* Unblock the Open function, no error declared. */
    vlc_sem_post(&sys->startup.sem_wait);

    while (true)
    {
        ASensorEvent ev[1];

        /* TODO: error case? what to do there */
        int ret = ALooper_pollOnce(-1, NULL, NULL, NULL);
        if (ret == ALOOPER_POLL_ERROR) {}

        ret = ASensorEventQueue_getEvents(queue, ev, ARRAY_SIZE(ev));
        /* TODO: error case */

        /**
         * Conventions for the renderer in VLC are:
         *
         *             Y (+ is top, - is bottom)
         *             |
         *             |
         *             |
         *            (+)-----> X (+ is right, - is left)
         *            /
         *           /
         *          /
         *         Z (+ is back, - is front)
         *
         * However, conventions for the android sensors are, with the
         * phone on its back on the ground
         * https://developer.android.com/develop/sensors-and-location/sensors/sensors_overview
         *
         *             Y (+ is top, - is bottom)
         *             |
         *             |
         *             |
         *            (+)-----> X (+ is right, + is left)
         *            /
         *           /
         *          /
         *         Z (+ is back, - is front)
         *
         * Thus, we need to switch Y and Z (data[1] and data[2]) and
         * because we switch from a left-handed to a right-handed system,
         * we need to reverse the rotations by conjugate of the quaternion.
         */

        //float quat[4] = { -ev[0].data[0],  -ev[0].data[1],
        //                  -ev[0].data[2],   ev[0].data[3] };

        //float quat[4] = {  ev[0].data[0],   ev[0].data[1],
        //                   ev[0].data[2],  -ev[0].data[3] };

        //float norm = sqrt(ev[0].data[0] * ev[0].data[0] +
        //                  ev[0].data[1] * ev[0].data[1] +
        //                  ev[0].data[2] * ev[0].data[2] - 1.f);

        // Yaw reversed
        //  Roll ok
        //  Pitch ok, but 90° to the bottom
        //float source[4] = { ev[0].data[0],   ev[0].data[1],
        //                    ev[0].data[2],   ev[0].data[3] };

        // Same but roll is reversed
        //float source[4] = {-ev[0].data[0],   ev[0].data[1],
        //                    ev[0].data[2],   ev[0].data[3] };

        // Same but pitch reversed, and yaw is ok now
        //float source[4] = { ev[0].data[0],  -ev[0].data[1],
        //                    ev[0].data[2],   ev[0].data[3] };

        /* We need to conjugate the source quaternion */
        float source[4] = { -ev[0].data[0],   -ev[0].data[1],
                            -ev[0].data[2],   ev[0].data[3] };

        float quat[4]; /* Will contain the final computed quaternion */

        /* The axis are not aligned so use those from ...*/
        float rot[4] = { sin(M_PI_4), 0.f, 0.f, cos(M_PI_4)};
        MultiplyQuat(source, rot, quat);

        // TODO: compute quat[4] when SDK < 18, it's not computed otherwise

        vlc_mutex_lock(&sys->lock);
        memcpy(sys->viewpoint.quat, quat, sizeof sys->viewpoint.quat);
        // TODO: add mechanism to trigger notification
        vlc_mutex_unlock(&sys->lock);
    }

release_queue:
    ASensorManager_destroyEventQueue(sys->mgr, queue);
release_looper:
    ALooper_release(looper);
    return NULL;
}

static void GetViewpoint(struct vlc_gyroscope *gyroscope,
                         struct vlc_viewpoint_t *vp_out)
{
    struct android_sensors *sys = gyroscope->sys;
    vlc_mutex_lock(&sys->lock);
    *vp_out = sys->viewpoint;
    vlc_mutex_unlock(&sys->lock);
}

static void DestroySensors(struct vlc_gyroscope *gyroscope)
{
    struct android_sensors *sys = gyroscope->sys;

    /* TODO: stop thread with fd */

    vlc_join(sys->thread, NULL);
    free(gyroscope->sys);
}

static int OpenSensors(struct vlc_gyroscope *gyroscope)
{
    struct vlc_logger *logger = vlc_object_logger(gyroscope);

    ASensorManager *mgr = ASensorManager_getInstance();
    //mgr = ASensorManager_getInstanceForPackage(package_name):
 
    ASensorList sensors;
    int count = ASensorManager_getSensorList(mgr, &sensors);
    if (count == 0)
        return VLC_ENOTSUP;

    if (count <= 0)
        return VLC_EGENERIC;

    const ASensor *gyro_sensor = NULL;
    for (size_t i = 0; i < (size_t)count; ++i)
    {
        const ASensor *probe = sensors[i];

        const char *name = ASensor_getName(probe);
        const char *vendor = ASensor_getVendor(probe);
        msg_Dbg(&gyroscope->obj, " - sensor '%s' from vendor '%s'", name, vendor);

        if (ASensor_getType(probe) == ASENSOR_TYPE_ROTATION_VECTOR)
        {
            if (gyro_sensor == NULL)
                gyro_sensor = probe;
        }
    }

    //gyro_sensor = ASensorManager_getDefaultSensor(mgr, ASENSOR_TYPE_ROTATION_VECTOR);
    
    if (gyro_sensor == NULL)
        return VLC_ENOTSUP;

    const char *name = ASensor_getName(gyro_sensor);
    const char *vendor = ASensor_getVendor(gyro_sensor);
    msg_Dbg(&gyroscope->obj, "Using gyroscope sensor %s from vendor %s", name, vendor);

    struct android_sensors *sys = malloc(sizeof *sys);
    *sys = (struct android_sensors){
        .mgr = mgr,
        .gyroscope = gyro_sensor,
        .viewpoint.fov = FIELD_OF_VIEW_DEGREES_DEFAULT,
    };
    gyroscope->sys = sys;
    vlc_mutex_init(&sys->lock);
    vlc_sem_init(&sys->startup.sem_wait, 0);

    int ret = vlc_clone(&sys->thread, EventQueueThread, gyroscope);
    if (ret != VLC_SUCCESS)
    {
        gyroscope->sys = NULL;
        free(sys);
        return VLC_EGENERIC;
    }
    vlc_sem_wait(&sys->startup.sem_wait);

    static const struct vlc_gyroscope_operations ops = {
        .get_viewpoint = GetViewpoint,
        .destroy = DestroySensors,
    };
    gyroscope->ops = &ops;

    return VLC_SUCCESS;
}
vlc_module_begin()
    set_subcategory(SUBCAT_VIDEO_GENERAL)
    set_description("Android gyroscope sensors")
    set_callback(OpenSensors)
    set_capability("gyroscope", 100)
vlc_module_end()
