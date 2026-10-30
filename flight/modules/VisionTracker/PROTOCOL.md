# Vision tracker serial protocol

Link between a companion computer that runs an object detector/tracker (for
example a camera with YOLO) and a LibrePilot flight controller. The flight
controller uses the data in the `FollowMe` flight mode to keep the tracked
object in the center of a fixed, forward looking camera.

Reference implementation: `python/visiontracker/vt_protocol.py`.
Bench test sender: `python/visiontracker/vt_sim.py`.

## Physical layer

* UART, 8N1, 3.3 V logic. Default 115200 baud (`HwSettings.VisionTrackerSpeed`).
* Select `VisionTracker` (or `PPM+VisionTracker`) for the port in
  `HwSettings` (Main, Flexi or Receiver port on Revolution) and reboot.

## Frame

All multi byte values are little endian.

| Offset | Size | Field                                   |
|--------|------|-----------------------------------------|
| 0      | 1    | `0xA5`                                  |
| 1      | 1    | `0x5A`                                  |
| 2      | 1    | message id                              |
| 3      | 1    | payload length `LEN` (0..32)            |
| 4      | LEN  | payload                                 |
| 4+LEN  | 1    | CRC8 over message id, length and payload |

CRC8: polynomial 0x07, initial value 0x00, no reflection, no final XOR (the
UAVTalk CRC). Frames with a bad CRC or a length above 32 are dropped.

## 0x01 TRACK (companion -> FC)

Send one frame per processed camera frame, 10 to 30 Hz. The flight controller
treats the target as lost when no new `seq` arrives for
`FollowMeSettings.TrackTimeout` (default 300 ms), so keep sending frames with
the valid flag cleared while the tracker has no target.

| Offset | Type   | Field         | Meaning                                                       |
|--------|--------|---------------|---------------------------------------------------------------|
| 0      | uint8  | `seq`         | incremented for every frame                                   |
| 1      | uint8  | `flags`       | bit 0: target valid                                           |
| 2      | uint16 | `track_id`    | tracker id of the followed object                             |
| 4      | int16  | `x`           | bbox center, -10000 = left edge, 0 = center, 10000 = right edge |
| 6      | int16  | `y`           | bbox center, -10000 = top edge, 0 = center, 10000 = bottom edge |
| 8      | uint16 | `w`           | bbox width / image width * 10000                              |
| 10     | uint16 | `h`           | bbox height / image height * 10000                            |
| 12     | uint8  | `conf`        | detection confidence * 255                                    |
| 13     | uint8  | reserved      | 0                                                             |
| 14     | uint16 | `distance_cm` | distance to the object, 0 = unknown (not used yet)            |

Coordinates are in the undistorted image. The FC converts `x`/`y` to angles
with `FollowMeSettings.CameraFOV` (`Horizontal`/`Vertical`), so set those to the
real field of view of the camera.

Newer protocol revisions may append fields; the FC ignores extra bytes.

## 0x81 FC_STATUS (FC -> companion)

Sent at 5 Hz.

| Offset | Type  | Field            | Meaning                                       |
|--------|-------|------------------|-----------------------------------------------|
| 0      | uint8 | `armed`          | `FlightStatus.Armed` (0 disarmed, 1 arming, 2 armed) |
| 1      | uint8 | `flightmode`     | `FlightStatus.FlightMode` index               |
| 2      | uint8 | `followme_state` | `FollowMeStatus.State` (0 NoData, 1 Lost, 2 Tracking) |
| 3      | int16 | `roll`           | centidegrees                                  |
| 5      | int16 | `pitch`          | centidegrees                                  |
| 7      | int16 | `yaw`            | centidegrees                                  |

## Example (Python)

```python
import serial
import vt_protocol as vt

link = serial.Serial("/dev/ttyAMA0", 115200)
seq = 0
for det in tracker_results():          # your tracker loop
    if det is None:
        frame = vt.encode_track(seq, False, 0, 0, 0, 0, 0)
    else:
        cx, cy, bw, bh = det.bbox_center_and_size_px
        frame = vt.encode_track(seq, True,
                                x=(cx - W / 2) / (W / 2), y=(cy - H / 2) / (H / 2),
                                w=bw / W, h=bh / H, conf=det.conf, track_id=det.id)
    link.write(frame)
    seq = (seq + 1) & 0xFF
```
