/**
 ******************************************************************************
 * @addtogroup LibrePilotModules LibrePilot Modules
 * @{
 * @addtogroup VisionTracker Vision tracker module
 * @{
 *
 * @file       vtparser.h
 * @author     The LibrePilot Project, http://www.librepilot.org Copyright (C) 2026.
 * @brief      Byte stream parser and encoder for the vision tracker protocol.
 *             Free of OS and UAVObject dependencies so it can be unit tested.
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
#ifndef VTPARSER_H
#define VTPARSER_H

#include "vtprotocol.h"

typedef enum {
    VT_STATE_SYNC1 = 0,
    VT_STATE_SYNC2,
    VT_STATE_MSG_ID,
    VT_STATE_LEN,
    VT_STATE_PAYLOAD,
    VT_STATE_CRC,
} vt_parser_state_t;

typedef struct {
    vt_parser_state_t state;
    uint8_t  msg_id;
    uint8_t  len;
    uint8_t  idx;
    uint8_t  crc;
    uint8_t  payload[VT_MAX_PAYLOAD_LEN];
    uint32_t frames; // frames with a valid CRC
    uint32_t crc_errors; // frames dropped because of a CRC mismatch
    uint32_t len_errors; // frames dropped because LEN exceeded VT_MAX_PAYLOAD_LEN
} vt_parser_t;

void vt_parser_init(vt_parser_t *p);

/**
 * Feed one received byte.
 * @return true when this byte completed a frame with a valid CRC. The frame
 *         (p->msg_id, p->len, p->payload) stays valid until the next call.
 */
bool vt_parser_feed(vt_parser_t *p, uint8_t b);

/**
 * Decode a TRACK payload. Longer payloads are accepted so that fields can be
 * appended in later protocol revisions.
 * @return false when the payload is too short
 */
bool vt_decode_track(const uint8_t *payload, uint8_t len, vt_track_t *out);

void vt_encode_track(const vt_track_t *track, uint8_t payload[VT_TRACK_PAYLOAD_LEN]);
void vt_encode_fc_status(const vt_fc_status_t *status, uint8_t payload[VT_FC_STATUS_PAYLOAD_LEN]);

/**
 * Wrap a payload into a complete frame.
 * @return frame length, or 0 when the payload or the output buffer is too large/small
 */
uint8_t vt_encode_frame(uint8_t msg_id, const uint8_t *payload, uint8_t len, uint8_t *out, uint8_t out_len);

#endif /* VTPARSER_H */

/**
 * @}
 * @}
 */
