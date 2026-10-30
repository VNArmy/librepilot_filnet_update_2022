/**
 ******************************************************************************
 * @addtogroup LibrePilotModules LibrePilot Modules
 * @{
 * @addtogroup VisionTracker Vision tracker module
 * @{
 *
 * @file       vtprotocol.h
 * @author     The LibrePilot Project, http://www.librepilot.org Copyright (C) 2026.
 * @brief      Wire format of the serial link between a companion computer
 *             running an object tracker and the flight controller.
 *             See PROTOCOL.md in the module directory.
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
#ifndef VTPROTOCOL_H
#define VTPROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Frame layout, all multi byte values little endian:
 *
 *   SYNC1 SYNC2 MSG_ID LEN PAYLOAD[LEN] CRC8
 *
 * CRC8 is PIOS_CRC_updateCRC() (polynomial 0x07, initial value 0, the same
 * CRC as UAVTalk) over MSG_ID, LEN and PAYLOAD.
 */
#define VT_SYNC1                 0xA5
#define VT_SYNC2                 0x5A
#define VT_FRAME_OVERHEAD        5 // sync1, sync2, msg_id, len, crc
#define VT_MAX_PAYLOAD_LEN       32
#define VT_MAX_FRAME_LEN         (VT_MAX_PAYLOAD_LEN + VT_FRAME_OVERHEAD)

// companion -> flight controller
#define VT_MSG_TRACK             0x01
#define VT_TRACK_PAYLOAD_LEN     16
#define VT_TRACK_FLAG_VALID      0x01

// flight controller -> companion
#define VT_MSG_FC_STATUS         0x81
#define VT_FC_STATUS_PAYLOAD_LEN 9

// normalized image coordinates and sizes are transferred as value * VT_NORM_SCALE
#define VT_NORM_SCALE            10000

typedef struct {
    uint8_t  seq; // incremented by the companion for every frame
    uint8_t  flags; // VT_TRACK_FLAG_*
    uint16_t track_id;
    int16_t  x; // bbox center, -10000 (left edge) .. 10000 (right edge)
    int16_t  y; // bbox center, -10000 (top edge) .. 10000 (bottom edge)
    uint16_t w; // bbox width / image width * 10000
    uint16_t h; // bbox height / image height * 10000
    uint8_t  conf; // detection confidence, 0..255
    uint16_t distance_cm; // 0 when unknown
} vt_track_t;

typedef struct {
    uint8_t armed; // FlightStatus.Armed
    uint8_t flightmode; // FlightStatus.FlightMode
    uint8_t followme_state; // FollowMeStatus.State
    int16_t roll; // centidegrees
    int16_t pitch; // centidegrees
    int16_t yaw; // centidegrees
} vt_fc_status_t;

#endif /* VTPROTOCOL_H */

/**
 * @}
 * @}
 */
