#!/usr/bin/env python3
"""
Send simulated TRACK frames to the flight controller for bench tests of the
VisionTracker port and the FollowMe flight mode, and print FC_STATUS replies.

    python3 vt_sim.py /dev/ttyUSB0 --rate 20
    python3 vt_sim.py COM5 --drop 5:3 --garbage 0.05

Requires pyserial.
"""

import argparse
import math
import random
import sys
import time

import serial

import vt_protocol as vt


def parse_drop(text):
    """'every:duration' in seconds, e.g. '10:2' drops the track for 2 s every 10 s."""
    every, duration = (float(v) for v in text.split(":"))
    return every, duration


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--rate", type=float, default=20.0, help="frames per second")
    ap.add_argument("--amp-x", type=float, default=0.5, help="horizontal amplitude, -1..1")
    ap.add_argument("--amp-y", type=float, default=0.3, help="vertical amplitude, -1..1")
    ap.add_argument("--period", type=float, default=8.0, help="seconds per sine period")
    ap.add_argument("--static", nargs=2, type=float, metavar=("X", "Y"),
                    help="hold the target at a fixed offset instead of moving it")
    ap.add_argument("--conf", type=float, default=0.9)
    ap.add_argument("--drop", type=parse_drop, metavar="EVERY:DURATION",
                    help="periodically report the target as lost")
    ap.add_argument("--garbage", type=float, default=0.0,
                    help="probability per frame of inserting random bytes or corrupting the frame")
    args = ap.parse_args()

    link = serial.Serial(args.port, args.baud, timeout=0)
    parser = vt.Parser()
    period = 1.0 / args.rate
    start = time.monotonic()
    next_send = start
    last_status_print = 0.0
    seq = 0

    print("sending to %s @ %d baud, %.1f Hz (Ctrl+C to stop)" % (args.port, args.baud, args.rate))
    try:
        while True:
            now = time.monotonic()
            t = now - start

            if now >= next_send:
                next_send += period
                if args.static:
                    x, y = args.static
                else:
                    phase = 2.0 * math.pi * t / args.period
                    x = args.amp_x * math.sin(phase)
                    y = args.amp_y * math.sin(2.0 * phase)
                valid = True
                if args.drop:
                    every, duration = args.drop
                    valid = (t % every) >= duration

                frame = bytearray(vt.encode_track(seq, valid, x, y, 0.2, 0.4, args.conf, track_id=1))
                if args.garbage and random.random() < args.garbage:
                    if random.random() < 0.5:
                        frame = bytes(random.getrandbits(8) for _ in range(random.randint(1, 8))) + frame
                    else:
                        frame[random.randint(2, len(frame) - 1)] ^= 0xFF
                link.write(frame)
                seq = (seq + 1) & 0xFF

            for msg_id, payload in parser.feed(link.read(256)):
                if msg_id == vt.MSG_FC_STATUS and now - last_status_print > 1.0:
                    last_status_print = now
                    st = vt.decode_fc_status(payload)
                    print("FC: armed=%(armed)d mode=%(flightmode)d state=%(followme_state)d "
                          "rpy=%(roll).1f/%(pitch).1f/%(yaw).1f" % st)

            time.sleep(0.002)
    except KeyboardInterrupt:
        pass
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
