/**
 ******************************************************************************
 * @addtogroup LibrePilotModules LibrePilot Modules
 * @{
 * @addtogroup VisionTracker Vision tracker module
 * @{
 *
 * @file       visiontracker.c
 * @author     The LibrePilot Project, http://www.librepilot.org Copyright (C) 2026.
 * @brief      Receives object tracking results from a companion computer over
 *             a serial port and publishes them as @ref TrackingData. Sends a
 *             short status frame back to the companion computer.
 *             The protocol is described in PROTOCOL.md.
 *
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
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

#include "openpilot.h"
#include "hwsettings.h"
#include "trackingdata.h"
#include "followmestatus.h"
#include "flightstatus.h"
#include "attitudestate.h"
#include "taskinfo.h"

#include <pios_board_io.h>

#include "vtparser.h"

// Private constants
#if defined(PIOS_VISIONTRACKER_STACK_SIZE)
#define STACK_SIZE_BYTES  PIOS_VISIONTRACKER_STACK_SIZE
#else
#define STACK_SIZE_BYTES  512
#endif
#define TASK_PRIORITY     (tskIDLE_PRIORITY + 1)

#define RX_BUFFER_SIZE    16
#define RX_TIMEOUT_MS     10
#define STATUS_PERIOD_MS  200

// Private variables
static uint32_t vtComPort;
static bool moduleEnabled;
static vt_parser_t parser;

// Private functions
static void visionTrackerTask(void *parameters);
static void handleFrame(void);
static void sendStatus(void);
static uint32_t rxErrors(void);
static uint32_t visionTrackerSpeedToBaud(uint8_t speed);

/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
static int32_t VisionTrackerInitialize(void)
{
    vtComPort = PIOS_COM_VISIONTRACKER;

    if (!vtComPort) {
        return -1;
    }

    TrackingDataInitialize();
    FollowMeStatusInitialize();

    HwSettingsVisionTrackerSpeedOptions speed;
    HwSettingsVisionTrackerSpeedGet(&speed);
    PIOS_COM_ChangeBaud(vtComPort, visionTrackerSpeedToBaud(speed));

    vt_parser_init(&parser);

    moduleEnabled = true;
    return 0;
}

/**
 * Start the module, called on startup
 * \returns 0 on success or -1 if the module is not enabled
 */
static int32_t VisionTrackerStart(void)
{
    if (!moduleEnabled) {
        return -1;
    }

    xTaskHandle taskHandle;

    xTaskCreate(visionTrackerTask, "VisionTracker", STACK_SIZE_BYTES / 4, NULL, TASK_PRIORITY, &taskHandle);
    PIOS_TASK_MONITOR_RegisterTask(TASKINFO_RUNNING_VISIONTRACKER, taskHandle);

    return 0;
}
MODULE_INITCALL(VisionTrackerInitialize, VisionTrackerStart);

/**
 * Main task. Feeds received bytes into the parser and sends the status frame
 * every STATUS_PERIOD_MS.
 */
static void visionTrackerTask(__attribute__((unused)) void *parameters)
{
    uint8_t buf[RX_BUFFER_SIZE];
    portTickType lastStatus = xTaskGetTickCount();
    uint32_t lastErrors     = 0;

    while (1) {
        uint16_t count = PIOS_COM_ReceiveBuffer(vtComPort, buf, sizeof(buf), RX_TIMEOUT_MS);

        for (uint16_t i = 0; i < count; i++) {
            if (vt_parser_feed(&parser, buf[i])) {
                handleFrame();
            }
        }

        portTickType now = xTaskGetTickCount();
        if ((now - lastStatus) * portTICK_RATE_MS >= STATUS_PERIOD_MS) {
            lastStatus = now;
            sendStatus();

            // publish errors also while no valid frame arrives, e.g. wrong baud rate
            uint32_t errors = rxErrors();
            if (errors != lastErrors) {
                lastErrors = errors;
                TrackingDataRxErrorsSet(&errors);
            }
        }
    }
}

/**
 * Handle a frame with a valid CRC. Unknown messages are ignored.
 */
static void handleFrame(void)
{
    vt_track_t track;

    if (parser.msg_id != VT_MSG_TRACK || !vt_decode_track(parser.payload, parser.len, &track)) {
        return;
    }

    TrackingDataData data;

    data.Valid      = (track.flags & VT_TRACK_FLAG_VALID) ? TRACKINGDATA_VALID_TRUE : TRACKINGDATA_VALID_FALSE;
    data.Seq        = track.seq;
    data.TrackId    = track.track_id;
    data.X          = (float)track.x / VT_NORM_SCALE;
    data.Y          = (float)track.y / VT_NORM_SCALE;
    data.Width      = (float)track.w / VT_NORM_SCALE;
    data.Height     = (float)track.h / VT_NORM_SCALE;
    data.Confidence = (float)track.conf / 255.0f;
    data.Distance   = (float)track.distance_cm / 100.0f;
    data.RxFrames   = parser.frames;
    data.RxErrors   = rxErrors();

    TrackingDataSet(&data);
}

/**
 * Send FC_STATUS to the companion computer. Dropped when the tx buffer is full.
 */
static void sendStatus(void)
{
    FlightStatusData flightStatus;
    AttitudeStateData attitude;
    FollowMeStatusStateOptions state;

    FlightStatusGet(&flightStatus);
    AttitudeStateGet(&attitude);
    FollowMeStatusStateGet(&state);

    vt_fc_status_t status = {
        .armed          = flightStatus.Armed,
        .flightmode     = flightStatus.FlightMode,
        .followme_state = state,
        .roll           = (int16_t)(attitude.Roll * 100.0f),
        .pitch          = (int16_t)(attitude.Pitch * 100.0f),
        .yaw            = (int16_t)(attitude.Yaw * 100.0f),
    };

    uint8_t payload[VT_FC_STATUS_PAYLOAD_LEN];
    uint8_t frame[VT_MAX_FRAME_LEN];

    vt_encode_fc_status(&status, payload);
    uint8_t len = vt_encode_frame(VT_MSG_FC_STATUS, payload, sizeof(payload), frame, sizeof(frame));
    if (len) {
        PIOS_COM_SendBufferNonBlocking(vtComPort, frame, len);
    }
}

static uint32_t rxErrors(void)
{
    return parser.crc_errors + parser.len_errors;
}

static uint32_t visionTrackerSpeedToBaud(uint8_t speed)
{
    switch (speed) {
    case HWSETTINGS_VISIONTRACKERSPEED_57600:
        return 57600;

    case HWSETTINGS_VISIONTRACKERSPEED_230400:
        return 230400;

    default:
    case HWSETTINGS_VISIONTRACKERSPEED_115200:
        return 115200;
    }
}

/**
 * @}
 * @}
 */
