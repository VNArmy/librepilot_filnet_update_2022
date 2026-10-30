/**
 ******************************************************************************
 * @addtogroup LibrePilotModules LibrePilot Modules
 * @{
 * @addtogroup VisionTracker Vision tracker module
 * @{
 *
 * @file       vtparser.c
 * @author     The LibrePilot Project, http://www.librepilot.org Copyright (C) 2026.
 * @brief      Byte stream parser and encoder for the vision tracker protocol.
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

#include <string.h>
#include "vtparser.h"
#include "pios_crc.h"

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

void vt_parser_init(vt_parser_t *p)
{
    memset(p, 0, sizeof(*p));
    p->state = VT_STATE_SYNC1;
}

bool vt_parser_feed(vt_parser_t *p, uint8_t b)
{
    switch (p->state) {
    case VT_STATE_SYNC1:
        if (b == VT_SYNC1) {
            p->state = VT_STATE_SYNC2;
        }
        break;

    case VT_STATE_SYNC2:
        if (b == VT_SYNC2) {
            p->state = VT_STATE_MSG_ID;
        } else if (b != VT_SYNC1) {
            // a repeated SYNC1 may still be the start of a frame
            p->state = VT_STATE_SYNC1;
        }
        break;

    case VT_STATE_MSG_ID:
        p->msg_id = b;
        p->crc    = PIOS_CRC_updateByte(0, b);
        p->state  = VT_STATE_LEN;
        break;

    case VT_STATE_LEN:
        if (b > VT_MAX_PAYLOAD_LEN) {
            p->len_errors++;
            p->state = VT_STATE_SYNC1;
            break;
        }
        p->len   = b;
        p->idx   = 0;
        p->crc   = PIOS_CRC_updateByte(p->crc, b);
        p->state = (b == 0) ? VT_STATE_CRC : VT_STATE_PAYLOAD;
        break;

    case VT_STATE_PAYLOAD:
        p->payload[p->idx++] = b;
        p->crc = PIOS_CRC_updateByte(p->crc, b);
        if (p->idx >= p->len) {
            p->state = VT_STATE_CRC;
        }
        break;

    case VT_STATE_CRC:
        p->state = VT_STATE_SYNC1;
        if (b == p->crc) {
            p->frames++;
            return true;
        }
        p->crc_errors++;
        break;

    default:
        p->state = VT_STATE_SYNC1;
        break;
    }

    return false;
}

bool vt_decode_track(const uint8_t *payload, uint8_t len, vt_track_t *out)
{
    if (len < VT_TRACK_PAYLOAD_LEN) {
        return false;
    }

    out->seq         = payload[0];
    out->flags       = payload[1];
    out->track_id    = get_u16(&payload[2]);
    out->x           = (int16_t)get_u16(&payload[4]);
    out->y           = (int16_t)get_u16(&payload[6]);
    out->w           = get_u16(&payload[8]);
    out->h           = get_u16(&payload[10]);
    out->conf        = payload[12];
    // payload[13] is reserved
    out->distance_cm = get_u16(&payload[14]);

    return true;
}

void vt_encode_track(const vt_track_t *track, uint8_t payload[VT_TRACK_PAYLOAD_LEN])
{
    payload[0] = track->seq;
    payload[1] = track->flags;
    put_u16(&payload[2], track->track_id);
    put_u16(&payload[4], (uint16_t)track->x);
    put_u16(&payload[6], (uint16_t)track->y);
    put_u16(&payload[8], track->w);
    put_u16(&payload[10], track->h);
    payload[12] = track->conf;
    payload[13] = 0;
    put_u16(&payload[14], track->distance_cm);
}

void vt_encode_fc_status(const vt_fc_status_t *status, uint8_t payload[VT_FC_STATUS_PAYLOAD_LEN])
{
    payload[0] = status->armed;
    payload[1] = status->flightmode;
    payload[2] = status->followme_state;
    put_u16(&payload[3], (uint16_t)status->roll);
    put_u16(&payload[5], (uint16_t)status->pitch);
    put_u16(&payload[7], (uint16_t)status->yaw);
}

uint8_t vt_encode_frame(uint8_t msg_id, const uint8_t *payload, uint8_t len, uint8_t *out, uint8_t out_len)
{
    if (len > VT_MAX_PAYLOAD_LEN || out_len < len + VT_FRAME_OVERHEAD) {
        return 0;
    }

    out[0] = VT_SYNC1;
    out[1] = VT_SYNC2;
    out[2] = msg_id;
    out[3] = len;
    memcpy(&out[4], payload, len);
    out[4 + len] = PIOS_CRC_updateCRC(0, &out[2], len + 2);

    return len + VT_FRAME_OVERHEAD;
}

/**
 * @}
 * @}
 */
