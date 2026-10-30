/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{
 * @addtogroup ManualControl
 * @brief Interpretes the control input in ManualControlCommand
 * @{
 *
 * @file       followmehandler.c
 * @author     The LibrePilot Project, http://www.librepilot.org Copyright (C) 2026.
 * @brief      FollowMe flight mode. Keeps the object reported by the vision
 *             tracker (@ref TrackingData) in the center of a fixed, forward
 *             looking camera by yawing and climbing/descending. Roll and pitch
 *             are held level and all pilot stick input is ignored.
 *
 * @see        The GNU Public License (GPL) Version 3
 *
 ******************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "inc/manualcontrol.h"

#ifndef PIOS_EXCLUDE_ADVANCED_FEATURES

#include <mathmisc.h>
#include <stabilizationdesired.h>
#include <attitudestate.h>
#include <positionstate.h>
#include <altitudeholdsettings.h>
#include <trackingdata.h>
#include <followmesettings.h>
#include <followmestatus.h>

// Private constants

// ALTITUDEVARIO thrust input, see stabilizationAltitudeHold(): 0.4 .. 0.6 holds
// the altitude, 0.6 .. 1.0 and 0.4 .. 0.0 command a climb/descent rate
#define VARIO_NEUTRAL      0.5f
#define VARIO_DEADBAND     0.1f
#define VARIO_SPAN         0.4f
// keep the thrust input above 0 so that CutThrustWhenZero never triggers
#define VARIO_MIN_THRUST   0.05f
#define VARIO_ITERATIONS   12
// climb rates below this hold the altitude, m/s
#define VARIO_MIN_RATE     0.001f

// filters for the frame interval and the derivative of the horizontal angle
#define FRAME_DT_ALPHA     0.1f
#define DERIVATIVE_ALPHA   0.5f
#define FRAME_DT_MIN       0.01f

// Private types
typedef struct {
    float    entryDown; // PositionState.Down when entering the flight mode
    uint32_t lastRxFrames; // TrackingData.RxFrames of the last processed frame
    portTickType lastFrameTime; // arrival of the last frame
    portTickType trackedTime; // last time the object was tracked
    bool     tracking; // the last frame contained a usable object
    bool     prevAxValid; // prevAx can be used for the derivative
    float    ax; // horizontal angle to the object at the last frame, deg
    float    ay; // vertical angle to the object at the last frame, deg, positive is down
    float    yawAtFrame; // AttitudeState.Yaw at the last frame
    float    prevAx;
    float    axDot; // filtered derivative of ax, deg/s
    float    frameDt; // filtered frame interval, s
} followMeState_t;

// Private variables
static followMeState_t fm;

// Private functions
static float wrap180(float angle);
static float climbRateToVarioThrust(float climbRate, const AltitudeHoldSettingsData *altSettings);

/**
 * @brief Handler for the FollowMe flight mode
 * @input: TrackingData, FollowMeSettings, AttitudeState, PositionState
 * @output: StabilizationDesired, FollowMeStatus
 */
void followMeHandler(bool newinit)
{
    static bool inited = false;

    if (!inited) {
        inited = true;
        StabilizationDesiredInitialize();
        AttitudeStateInitialize();
        PositionStateInitialize();
        AltitudeHoldSettingsInitialize();
        TrackingDataInitialize();
        FollowMeSettingsInitialize();
        FollowMeStatusInitialize();
    }

    FollowMeSettingsData settings;
    FollowMeSettingsGet(&settings);
    TrackingDataData track;
    TrackingDataGet(&track);
    AttitudeStateData attitude;
    AttitudeStateGet(&attitude);
    float down;
    PositionStateDownGet(&down);

    portTickType now = xTaskGetTickCount();

    if (newinit) {
        memset(&fm, 0, sizeof(fm));
        fm.entryDown     = down;
        fm.lastRxFrames  = track.RxFrames; // do not use a frame received before entering the mode
        fm.lastFrameTime = now;
        fm.trackedTime   = now;
    }

    // process a new frame
    if (track.RxFrames != fm.lastRxFrames) {
        float frameDt = (float)((now - fm.lastFrameTime) * portTICK_RATE_MS) * 1e-3f;
        bool continuous = (frameDt * 1000.0f) <= settings.TrackTimeout;

        fm.lastRxFrames  = track.RxFrames;
        fm.lastFrameTime = now;
        fm.tracking = (track.Valid == TRACKINGDATA_VALID_TRUE) && (track.Confidence >= settings.MinConfidence);

        if (fm.tracking) {
            fm.ax = RAD2DEG(atanf(boundf(track.X, -1.0f, 1.0f) * tanf(DEG2RAD(settings.CameraFOV.Horizontal * 0.5f))));
            fm.ay = RAD2DEG(atanf(boundf(track.Y, -1.0f, 1.0f) * tanf(DEG2RAD(settings.CameraFOV.Vertical * 0.5f))));
            fm.yawAtFrame  = attitude.Yaw;
            fm.trackedTime = now;

            if (fm.prevAxValid && continuous) {
                // the handler runs at the receiver rate, so the measured interval jitters; use a filtered one
                fm.frameDt = (fm.frameDt > 0.0f) ? (1.0f - FRAME_DT_ALPHA) * fm.frameDt + FRAME_DT_ALPHA * frameDt : frameDt;
                float axDot = (fm.ax - fm.prevAx) / fmaxf(fm.frameDt, FRAME_DT_MIN);
                fm.axDot = (1.0f - DERIVATIVE_ALPHA) * fm.axDot + DERIVATIVE_ALPHA * axDot;
            } else {
                fm.axDot = 0.0f;
            }
            fm.prevAx = fm.ax;
            fm.prevAxValid = true;
        } else {
            fm.prevAxValid = false;
        }
    }

    // a frame that is too old does not count, also when the link died
    bool fresh = ((now - fm.lastFrameTime) * portTICK_RATE_MS) <= settings.TrackTimeout;
    bool tracking = fresh && fm.tracking;

    if (!tracking) {
        fm.prevAxValid = false;
        fm.axDot = 0.0f;
    }

    FollowMeStatusData status;
    float yawRate   = 0.0f;
    float climbRate = 0.0f;
    float altitude  = fm.entryDown - down; // relative to the altitude when entering the mode, positive up

    status.YawError = 0.0f;
    status.ElevationError = 0.0f;

    if (tracking) {
        status.State = FOLLOWMESTATUS_STATE_TRACKING;

        // the frame shows the object relative to the heading at that time, correct for the yaw since then
        float yawError = fm.ax - wrap180(attitude.Yaw - fm.yawAtFrame);
        yawRate = boundf(settings.YawPID.Kp * yawError + settings.YawPID.Kd * fm.axDot, -settings.MaxYawRate, settings.MaxYawRate);

        // positive when the object is above the image center, corrected for the body pitch
        float elevationError = attitude.Pitch - fm.ay;
        if (fabsf(elevationError) > settings.ElevationDeadband) {
            float error = elevationError - copysignf(settings.ElevationDeadband, elevationError);
            climbRate = boundf(settings.ElevationKp * error, -settings.MaxClimbRate, settings.MaxClimbRate);
        }

        // stay within the altitude window around the entry altitude
        if ((climbRate > 0.0f && altitude >= settings.AltitudeWindow.Climb) ||
            (climbRate < 0.0f && altitude <= -settings.AltitudeWindow.Descent)) {
            climbRate = 0.0f;
        }

        status.YawError = yawError;
        status.ElevationError = elevationError;
    } else {
        status.State = (fresh) ? FOLLOWMESTATUS_STATE_LOST : FOLLOWMESTATUS_STATE_NODATA;

        // hold heading and altitude, after LostTimeout optionally descend until landed
        if (settings.LostAction == FOLLOWMESETTINGS_LOSTACTION_DESCEND &&
            ((now - fm.trackedTime) * portTICK_RATE_MS) > settings.LostTimeout) {
            climbRate = -fabsf(settings.LostDescentRate);
        }
    }

    AltitudeHoldSettingsData altSettings;
    AltitudeHoldSettingsGet(&altSettings);

    StabilizationDesiredData stabilization;
    StabilizationDesiredGet(&stabilization);

    stabilization.Roll   = 0.0f;
    stabilization.Pitch  = 0.0f;
    stabilization.Yaw    = yawRate;
    stabilization.Thrust = climbRateToVarioThrust(climbRate, &altSettings);
    stabilization.StabilizationMode.Roll   = STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE;
    stabilization.StabilizationMode.Pitch  = STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE;
    stabilization.StabilizationMode.Yaw    = STABILIZATIONDESIRED_STABILIZATIONMODE_RATE;
    stabilization.StabilizationMode.Thrust = STABILIZATIONDESIRED_STABILIZATIONMODE_ALTITUDEVARIO;
    StabilizationDesiredSet(&stabilization);

    status.YawRateCmd     = yawRate;
    status.ClimbRateCmd   = climbRate;
    status.AltitudeOffset = altitude;
    FollowMeStatusSet(&status);
}

static float wrap180(float angle)
{
    while (angle > 180.0f) {
        angle -= 360.0f;
    }
    while (angle < -180.0f) {
        angle += 360.0f;
    }
    return angle;
}

/**
 * Invert the ALTITUDEVARIO stick curve of stabilizationAltitudeHold():
 * rate = (k * x^3 + (255 - k) * x) / 255 * ThrustRate, k = ThrustExp, x = 0..1
 * @param climbRate desired climb rate, m/s, positive up
 * @return thrust input for StabilizationDesired in ALTITUDEVARIO mode
 */
static float climbRateToVarioThrust(float climbRate, const AltitudeHoldSettingsData *altSettings)
{
    if (fabsf(climbRate) < VARIO_MIN_RATE || altSettings->ThrustRate <= 0.0f) {
        return VARIO_NEUTRAL;
    }

    const float k    = (float)altSettings->ThrustExp;
    const float rate = fabsf(climbRate) / altSettings->ThrustRate;
    float x = 1.0f;

    if (rate < 1.0f) {
        // the curve is monotonic, bisect
        float lo = 0.0f;
        float hi = 1.0f;
        for (int i = 0; i < VARIO_ITERATIONS; i++) {
            x = 0.5f * (lo + hi);
            if ((k * x * x * x + (255.0f - k) * x) / 255.0f < rate) {
                lo = x;
            } else {
                hi = x;
            }
        }
        x = hi;
    }

    float thrust = (climbRate > 0.0f) ?
                   VARIO_NEUTRAL + VARIO_DEADBAND + VARIO_SPAN * x :
                   VARIO_NEUTRAL - VARIO_DEADBAND - VARIO_SPAN * x;

    return boundf(thrust, VARIO_MIN_THRUST, 1.0f);
}

#endif /* ifndef PIOS_EXCLUDE_ADVANCED_FEATURES */

/**
 * @}
 * @}
 */
