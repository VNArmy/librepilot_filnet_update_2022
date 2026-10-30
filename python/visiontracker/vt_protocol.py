"""
Vision tracker serial protocol (companion computer <-> LibrePilot flight controller).

Frame, all multi byte values little endian:

    0xA5 0x5A MSG_ID LEN PAYLOAD[LEN] CRC8

CRC8 uses polynomial 0x07 with initial value 0 (the UAVTalk CRC) and covers
MSG_ID, LEN and PAYLOAD. Keep in sync with
flight/modules/VisionTracker/inc/vtprotocol.h.
"""

import struct

SYNC1 = 0xA5
SYNC2 = 0x5A
MAX_PAYLOAD_LEN = 32

MSG_TRACK = 0x01
MSG_FC_STATUS = 0x81

TRACK_FLAG_VALID = 0x01
NORM_SCALE = 10000

_TRACK_FMT = "<BBHhhHHBBH"  # 16 bytes
_FC_STATUS_FMT = "<BBBhhh"  # 9 bytes


def crc8(data, crc=0):
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def encode_frame(msg_id, payload):
    if len(payload) > MAX_PAYLOAD_LEN:
        raise ValueError("payload too long")
    body = bytes([msg_id, len(payload)]) + bytes(payload)
    return bytes([SYNC1, SYNC2]) + body + bytes([crc8(body)])


def _norm(value, lo, hi):
    return int(round(max(lo, min(hi, value)) * NORM_SCALE))


def encode_track(seq, valid, x, y, w, h, conf, track_id=0, distance_m=0.0):
    """Build a TRACK frame.

    x, y: bbox center relative to the image center, -1..1 (right / down positive)
    w, h: bbox size relative to the image size, 0..1
    conf: detection confidence, 0..1
    """
    payload = struct.pack(
        _TRACK_FMT,
        seq & 0xFF,
        TRACK_FLAG_VALID if valid else 0,
        track_id & 0xFFFF,
        _norm(x, -1.0, 1.0),
        _norm(y, -1.0, 1.0),
        _norm(w, 0.0, 1.0),
        _norm(h, 0.0, 1.0),
        int(round(max(0.0, min(1.0, conf)) * 255)),
        0,
        int(round(max(0.0, min(655.35, distance_m)) * 100)),
    )
    return encode_frame(MSG_TRACK, payload)


def decode_track(payload):
    seq, flags, track_id, x, y, w, h, conf, _, dist = struct.unpack_from(_TRACK_FMT, payload)
    return {
        "seq": seq,
        "valid": bool(flags & TRACK_FLAG_VALID),
        "track_id": track_id,
        "x": x / NORM_SCALE,
        "y": y / NORM_SCALE,
        "w": w / NORM_SCALE,
        "h": h / NORM_SCALE,
        "conf": conf / 255.0,
        "distance_m": dist / 100.0,
    }


def decode_fc_status(payload):
    armed, flightmode, state, roll, pitch, yaw = struct.unpack_from(_FC_STATUS_FMT, payload)
    return {
        "armed": armed,
        "flightmode": flightmode,
        "followme_state": state,
        "roll": roll / 100.0,
        "pitch": pitch / 100.0,
        "yaw": yaw / 100.0,
    }


class Parser:
    """Incremental frame parser, mirrors vt_parser_feed() in vtparser.c."""

    def __init__(self):
        self._buf = bytearray()
        self.crc_errors = 0

    def feed(self, data):
        """Feed received bytes, return a list of (msg_id, payload) tuples."""
        self._buf += data
        frames = []
        while True:
            start = self._buf.find(bytes([SYNC1, SYNC2]))
            if start < 0:
                # keep a trailing SYNC1, it may start the next frame
                self._buf = self._buf[-1:] if self._buf[-1:] == bytes([SYNC1]) else bytearray()
                return frames
            del self._buf[:start]
            if len(self._buf) < 4:
                return frames
            length = self._buf[3]
            if length > MAX_PAYLOAD_LEN:
                del self._buf[:2]
                continue
            if len(self._buf) < length + 5:
                return frames
            body = bytes(self._buf[2:4 + length])
            if crc8(body) == self._buf[4 + length]:
                frames.append((body[0], body[2:]))
                del self._buf[:length + 5]
            else:
                self.crc_errors += 1
                del self._buf[:2]
