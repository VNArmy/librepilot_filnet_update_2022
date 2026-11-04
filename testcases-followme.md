# Test case: flight mode FollowMe + VisionTracker

Nhánh `feature/visiontracker-followme`, viết theo code ở commit `1a70e21a5`. Tài liệu liên quan: `progress.md` mục 11, `flight/modules/VisionTracker/PROTOCOL.md`.

## 0. Cách dùng

- Chạy theo thứ tự nhóm: UT → BLD → CFG → UART → FM → INT → FLT. Chỉ sang nhóm sau khi mọi test P1 của nhóm trước đã pass. Nhóm REG chạy trước khi merge.
- Mức ưu tiên: **P1** bắt buộc (an toàn hoặc chức năng chính), **P2** nên làm, **P3** tùy chọn.
- Giá trị "Mong đợi" tính với setting mặc định (mục 1.4), theo đúng thuật toán trong `followmehandler.c` (bảng 1.6). Sai số chấp nhận: góc ±0.5°, yaw rate ±0.5 °/s, tốc độ leo ±0.01 m/s, thrust ±0.01.
- Ghi kết quả vào bảng ở mục 10. Test nào đổi setting thì cuối test phải trả setting về mặc định.

| Nhóm | Nội dung | Cần |
|---|---|---|
| UT | Unit test parser C, công cụ Python | PC (WSL) |
| BLD | Build firmware, GCS | PC (WSL) |
| CFG | Nâng cấp, cấu hình trên GCS, sanity check | Revolution + USB |
| UART | Bench test cổng UART (Mốc A) | + USB-UART 3.3 V |
| FM | Logic FollowMe trên bàn, không cánh (Mốc B) | + RC |
| INT | Tích hợp với companion thật | + companion, camera |
| FLT | Bay thử | drone hoàn chỉnh, dây giữ |
| REG | Hồi quy | |

## 1. Chuẩn bị chung

### 1.1 Phần cứng

- Revolution. Chỉ firmware Revolution có module VisionTracker.
- Adapter USB-UART mức logic 3.3 V (CP2102, FT232, CH340...). Không dùng adapter logic 5 V.
- Bộ phát RC và receiver đã cấu hình xong trong GCS (Input wizard).
- Các test có arm: ưu tiên **chỉ cấp nguồn USB, không cắm pin** để motor không quay. Nếu không arm được vì alarm pin thì cắm pin nhưng **tháo hết cánh**.

### 1.2 Phần mềm

- GCS và firmware build từ cùng một commit của nhánh này (UAVO hash phải khớp).
- Python 3 và pyserial: `pip install pyserial`. Mọi lệnh Python chạy trong thư mục `python/visiontracker/`.
- Cổng nối tiếp của adapter: Windows `COMx` (xem Device Manager), Linux `/dev/ttyUSB0`. Tài liệu này dùng `COM5`, thay bằng cổng thật.
- WSL2 không thấy thiết bị USB nếu chưa dùng usbipd (`progress.md` mục 7), nên chạy `vt_sim.py` trên Windows.

### 1.3 Đấu dây

- TX của adapter → RX của FC, RX của adapter → TX của FC, GND ↔ GND. Không nối VCC của adapter vào FC (FC đã có nguồn USB).
- **W-FLEXI / W-MAIN**: cổng 4 chân GND, VCC, TX, RX. Đối chiếu sơ đồ chân Revolution trước khi cắm.
- **W-RCVR**: UART của cổng Rcvr là USART6, TX = PC6, RX = PC7 (`board_hw_defs.c`). Tra sơ đồ chân FlexiIO để biết dây tương ứng. PPM vẫn ở chân PPM như cũ.
- Khi cần khởi động lại FC mà adapter vẫn cắm, dùng nút Reset ở trang Firmware của GCS thay vì rút USB: chân TX của adapter có thể cấp nguồn ngược vào FC qua chân RX.

### 1.4 Cấu hình nền (CFG-BASE)

Dùng cho nhóm UART và FM nếu test không ghi khác:

- Hardware: Flexi = `VisionTracker`, `HwSettings.VisionTrackerSpeed` = 115200. Save, reboot.
- Vehicle: multirotor (ví dụ Quad X).
- Input → Flight Mode Switch Settings: 3 vị trí.
  - Pos1 = Stabilized1, Thrust = Manual (dùng để arm).
  - Pos2 = Stabilized2, Thrust = AltitudeVario (dùng khi bay).
  - Pos3 = FollowMe, Assisted Control = None.
- `FollowMeSettings` mặc định: CameraFOV 80/60, YawPID Kp 1.0 / Kd 0.05, MaxYawRate 30, ElevationKp 0.05, ElevationDeadband 2, MaxClimbRate 0.3, AltitudeWindow Descent 1 / Climb 3, MinConfidence 0.4, TrackTimeout 300, LostTimeout 10000, LostAction Hover, LostDescentRate 0.3.
- `AltitudeHoldSettings`: ThrustExp 128, ThrustRate 5 (mặc định). Nếu khác thì cột Thrust ở bảng 1.6 sẽ khác.

Sửa setting bằng UAVObject Browser: đổi giá trị → Send (gửi xuống FC, mất khi reboot) → Save (lưu vào flash) nếu cần giữ.

### 1.5 Theo dõi trên GCS

- UAVObject Browser: `TrackingData`, `FollowMeStatus`, `StabilizationDesired`, `RateDesired`, `ActuatorDesired`, `FlightStatus`, `AttitudeState`, `ManualControlCommand`, `TaskInfo`, `SystemAlarms`, `SystemStats`.
- Scope gadget để vẽ đồ thị theo thời gian.
- `TrackingData` và `FollowMeStatus` được gửi về GCS tối đa 4 Hz (throttled 250 ms). Ở 20 Hz, mỗi lần GCS cập nhật `RxFrames` tăng khoảng 5: bình thường.
- `FollowMeStatus` chỉ được cập nhật khi đang ở FollowMe. Ngoài FollowMe nó giữ giá trị cũ, và `followme_state` trong FC_STATUS cũng vậy.
- Handler FollowMe chạy cả khi **chưa arm**, nên phần lớn nhóm FM đọc lệnh trên GCS mà không cần motor quay.

### 1.6 Bảng giá trị mong đợi

Setting mặc định, thân ngang (`AttitudeState.Pitch` ≈ 0), board đứng yên, mục tiêu đứng yên (`vt_sim.py --static X Y`).

- `ax = atan(X·tan40°)`, `ay = atan(Y·tan30°)`
- `YawError = ax`, `YawRateCmd = kẹp(1.0·YawError, ±30)`
- `ElevationError = Pitch − ay`; nếu |e| > 2 thì `ClimbRateCmd = kẹp(0.05·(|e| − 2)·dấu(e), ±0.3)`, ngược lại bằng 0
- Dấu: X > 0 là bên phải ảnh, YawRateCmd > 0 là quay phải. Y < 0 là phía trên ảnh, ClimbRateCmd > 0 là leo.

| `--static X Y` | YawError (°) | YawRateCmd (°/s) | ElevationError (°) | ClimbRateCmd (m/s) | `StabilizationDesired.Thrust` |
|---|---|---|---|---|---|
| `0 0` | 0 | 0 | 0 | 0 | 0.500 |
| `0.25 0` | +11.85 | +11.85 | 0 | 0 | 0.500 |
| `0.5 0` | +22.76 | +22.76 | 0 | 0 | 0.500 |
| `-0.5 0` | −22.76 | −22.76 | 0 | 0 | 0.500 |
| `1 0` | +40.00 | +30.00 (kẹp) | 0 | 0 | 0.500 |
| `-1 0` | −40.00 | −30.00 (kẹp) | 0 | 0 | 0.500 |
| `0 0.05` | 0 | 0 | −1.65 | 0 (deadband) | 0.500 |
| `0 0.1` | 0 | 0 | −3.30 | −0.065 | 0.389 |
| `0 -0.1` | 0 | 0 | +3.30 | +0.065 | 0.611 |
| `0 0.5` | 0 | 0 | −16.10 | −0.300 (kẹp) | 0.352 |
| `0 -0.5` | 0 | 0 | +16.10 | +0.300 (kẹp) | 0.648 |

Nếu Pitch của board không bằng 0 thì cộng `AttitudeState.Pitch` vào ElevationError rồi tính lại ClimbRateCmd.

---

## 2. UT: unit test và công cụ Python

### UT-01 Unit test parser C · P1

Điều kiện: WSL Ubuntu 24.04, đã chạy `make gtest_install`.

Bước:
1. Trong WSL, `cd` vào thư mục repo.
2. `make ut_visiontracker_run`

Mong đợi:
- Chạy 11 test, kết thúc bằng `[  PASSED  ] 11 tests.`, không có `FAILED`.
- Có đủ: `DecodesPythonFrame`, `EncoderMatchesPython`, `RejectsCorruptedFrameAndRecovers`, `SkipsGarbageBetweenFrames`, `RejectsOversizedLength`, `RecoversAfterTruncatedFrame`, `ParsesBackToBackFrames`, `AcceptsEmptyAndUnknownMessages`, `TrackPayloadLength`, `EncodeFrameChecksSizes`, `FcStatusRoundTrip`.

### UT-02 Encoder Python khớp vector của C · P1

Bước:
```
python -c "import vt_protocol as vt; print(vt.encode_track(7, True, 0.25, -0.5, 0.2, 0.4, 0.9, track_id=3, distance_m=2.5).hex(' '))"
```

Mong đợi: `a5 5a 01 10 07 01 03 00 c4 09 78 ec d0 07 a0 0f e6 00 fa 00 3b`, giống mảng `pythonTrackFrame` trong `flight/tests/visiontracker/unittest.cpp`.

### UT-03 Python kẹp giá trị ngoài dải · P2

Bước:
```
python -c "import vt_protocol as vt; print(vt.decode_track(vt.encode_track(1, True, 1.5, -2, 1.2, -0.1, 1.5)[4:-1]))"
```

Mong đợi: `'x': 1.0, 'y': -1.0, 'w': 1.0, 'h': 0.0, 'conf': 1.0`.

### UT-04 Parser Python bỏ rác và frame sai CRC, decode FC_STATUS · P2

Bước: chạy đoạn sau trong `python/visiontracker/` (bash/Git Bash/WSL; trên PowerShell thì lưu thành file `.py` rồi chạy):
```bash
python3 - <<'EOF'
import vt_protocol as vt
good = vt.encode_track(1, True, 0.1, 0.2, 0.3, 0.4, 0.9)
bad = bytearray(vt.encode_track(2, True, 0, 0, 0, 0, 0.5)); bad[-1] ^= 0xFF
p = vt.Parser()
frames = p.feed(b"\x00\xA5\x13\x37" + good + bytes(bad) + vt.encode_track(3, False, 0, 0, 0, 0, 0))
print([(hex(m), vt.decode_track(pl)["seq"]) for m, pl in frames], "crc_errors =", p.crc_errors)
st = vt.encode_frame(vt.MSG_FC_STATUS, bytes([2, 19, 2]) + (-1500).to_bytes(2, "little", signed=True)
                     + (250).to_bytes(2, "little", signed=True) + (-17999).to_bytes(2, "little", signed=True))
print(vt.decode_fc_status(vt.Parser().feed(st)[0][1]))
EOF
```

Mong đợi:
```
[('0x1', 1), ('0x1', 3)] crc_errors = 1
{'armed': 2, 'flightmode': 19, 'followme_state': 2, 'roll': -15.0, 'pitch': 2.5, 'yaw': -179.99}
```

---

## 3. BLD: build

### BLD-01 Firmware Revolution · P1

Bước (WSL):
1. `make fw_revolution_clean && make fw_revolution 2>&1 | tee build_revo.log`
2. `grep -i warning build_revo.log | grep -iE "visiontracker|vtparser|followme|sanitycheck|armhandler|manualcontrol|notification|pios_board"`
3. `tools/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-nm build/firmware/fw_revolution/fw_revolution.elf | grep -E "followMeHandler|visionTrackerTask|vt_parser_feed"`
4. `tools/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-size build/firmware/fw_revolution/fw_revolution.elf`

Mong đợi:
- Bước 1 build thành công, có `fw_revolution.opfw`.
- Bước 2 không in gì.
- Bước 3 in đủ 3 symbol.
- Bước 4: `text` khoảng 280 744 byte (`progress.md` 11.6). Ghi lại số đo.

### BLD-02 Các board khác · P2

Bước:
1. `make fw_revonano fw_sparky2 fw_discoveryf4bare fw_ccf3d fw_spracingf3evo fw_simposix`
2. `tools/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-nm build/firmware/fw_revonano/fw_revonano.elf | grep visionTrackerTask` (lặp lại với `fw_sparky2`)

Mong đợi: tất cả build thành công, không có warning mới. Bước 2 không in gì (không có module VisionTracker).

### BLD-03 GCS và độ khớp UAVO · P1

Bước:
1. Build GCS theo `progress.md` (mục 3–6), chạy GCS từ terminal để xem log.
2. Flash firmware của BLD-01 (CFG-01), kết nối USB.

Mong đợi:
- GCS kết nối, không cảnh báo lệch phiên bản firmware/GCS.
- UAVObject Browser có `TrackingData`, `FollowMeSettings`, `FollowMeStatus`. `FlightStatus.FlightMode` có option `FollowMe`.
- Log terminal không có dòng `uav.js: flightModeName() do not match`.

---

## 4. CFG: nâng cấp và cấu hình

### CFG-01 Nâng cấp firmware, giữ settings · P1

Bước:
1. Dùng GCS cũ với firmware cũ: File → Export UAV Settings → lưu `before.uav`.
2. Dùng GCS mới: trang Firmware → flash `fw_revolution` mới (Rescue nếu cần).
3. File → Import UAV Settings → chọn `before.uav` → lưu vào board.
4. Reset board, kết nối lại.

Mong đợi:
- Vehicle, Input (kênh, flight mode), Output, Stabilization và chức năng của cổng Main/Flexi/Rcvr giống trước khi nâng cấp.
- Không có alarm mới.
- Object nào import lỗi hoặc bị bỏ qua thì ghi lại và cấu hình lại bằng tay.

### CFG-02 Có tùy chọn VisionTracker trên trang Hardware · P1

Bước: Configuration → Hardware, mở lần lượt ô Main, Flexi, Receiver.

Mong đợi:
- Main có `VisionTracker`, Flexi có `VisionTracker`, Receiver có `PPM+VisionTracker` và `VisionTracker`.
- Khi chọn VisionTracker cho một cổng, ô Speed của cổng đó bị ẩn.

### CFG-03 Mỗi lúc chỉ một cổng VisionTracker · P1

Bước: trên trang Hardware, lần lượt làm từng dòng (mỗi dòng bắt đầu từ trạng thái trước của dòng đó).

| # | Trạng thái trước | Thao tác | Mong đợi |
|---|---|---|---|
| 1 | Main = VisionTracker | Flexi → VisionTracker | Main tự về Disabled |
| 2 | Flexi = VisionTracker | Main → VisionTracker | Flexi tự về Disabled |
| 3 | Main = VisionTracker | Receiver → VisionTracker | Main tự về Disabled |
| 4 | Receiver = PPM+VisionTracker | Flexi → VisionTracker | Receiver tự về PPM |
| 5 | Receiver = VisionTracker | Main → VisionTracker | Receiver tự về Disabled |
| 6 | Flexi = VisionTracker | Receiver → PPM+VisionTracker | Flexi tự về Disabled |

Sau dòng 6: Save, reset, mở lại trang Hardware.

Mong đợi: cấu hình được giữ, trong `HwSettings` chỉ một cổng có VisionTracker.

### CFG-04 Đặt tốc độ baud · P2

Bước:
1. UAVObject Browser → `HwSettings.VisionTrackerSpeed`.
2. Đổi sang 230400 → Send → Save → reset board.

Mong đợi: có đúng 3 option `57600, 115200, 230400`, mặc định 115200. Sau khi reset giá trị vẫn là 230400. Trả về 115200 (UART-06 kiểm tra chức năng).

### CFG-05 Module chỉ chạy khi có cổng VisionTracker · P1

Bước:
1. Không cổng nào là VisionTracker, reset. Đọc `TaskInfo.Running.VisionTracker`, `TrackingData.RxFrames`, `SystemStats.CPULoad`.
2. Flexi = VisionTracker, Save, reset. Đọc lại các giá trị trên và `TaskInfo.StackRemaining.VisionTracker`.

Mong đợi:
- Bước 1: Running = False, RxFrames = 0 và không đổi.
- Bước 2: Running = True. Ghi lại StackRemaining (báo lỗi nếu < 100 byte). CPULoad chênh không quá 2% so với bước 1.

### CFG-06 Gán FollowMe cho switch · P1

Bước:
1. Input → Flight Mode Switch Settings: Pos3 = FollowMe, Assisted Control của Pos3 = None. Save.
2. Gạt switch qua Pos1, Pos2, Pos3.

Mong đợi:
- `SystemAlarms.Alarm.SystemConfiguration` = OK (ô Config trong System Health không đỏ).
- Ở Pos3, `FlightStatus.FlightMode` = FollowMe.

### CFG-07 Sanity check: FollowMe với GPSAssist · P1

Bước:
1. Assisted Control của Pos3 = GPSAssist, Save.
2. Ở Pos1, thử arm.
3. Trả Assisted Control của Pos3 về None, Save.

Mong đợi:
- Sau bước 1: `SystemAlarms.Alarm.SystemConfiguration` = Critical, `ExtendedAlarmStatus.SystemConfiguration` = FlightMode, `ExtendedAlarmSubStatus.SystemConfiguration` = 2 (Pos3, đếm từ 0).
- Bước 2 không arm được.
- Sau bước 3 alarm hết.

### CFG-08 Sanity check: khung không phải multirotor · P2

Bước:
1. Export settings để khôi phục sau.
2. Vehicle → đổi sang Fixed Wing, Save (Pos3 vẫn là FollowMe).
3. Đổi Pos3 sang Stabilized3, Save.
4. Import lại settings ở bước 1.

Mong đợi:
- Sau bước 2: SystemConfiguration = Critical, ExtendedAlarmStatus = FlightMode.
- Sau bước 3: alarm do FollowMe không còn (có thể còn alarm khác do cấu hình fixed wing, ghi lại).

### CFG-09 Sanity check: board không có module VisionTracker · P2

Điều kiện: RevoNano hoặc Sparky2 chạy firmware của nhánh này.

Bước:
1. Input → gán FollowMe cho một vị trí switch, Save.
2. Thử arm ở vị trí khác.
3. RevoNano: mở trang Hardware.

Mong đợi:
- Bước 1: SystemConfiguration = Critical, ExtendedAlarmStatus = FlightMode, SubStatus = chỉ số vị trí vừa gán.
- Bước 2 không arm được.
- Bước 3: không có tùy chọn VisionTracker.

### CFG-10 Lưu FollowMeSettings · P2

Bước:
1. UAVObject Browser → `FollowMeSettings`: so với giá trị mặc định ở mục 1.4.
2. Đổi `CameraFOV.Horizontal` = 70, `LostAction` = Descend → Send → Save → reset board.
3. Trả về mặc định, Save.

Mong đợi: bước 1 khớp mặc định. Sau bước 2 giá trị mới được giữ sau reset.

---

## 5. UART: bench test cổng UART (Mốc A)

Điều kiện chung: CFG-BASE, W-FLEXI, FC nối GCS qua USB, không arm, switch ở Pos1.

### UART-01 Nhận frame TRACK · P1

Bước:
1. `python vt_sim.py COM5 --static 0 0`
2. Mở `TrackingData`, ghi `RxFrames`, chờ đúng 10 s, ghi lại.

Mong đợi:
- vt_sim in `sending to COM5 @ 115200 baud, 20.0 Hz` và mỗi giây một dòng `FC: armed=0 mode=1 ...`.
- `Valid` = True, `X` = 0, `Y` = 0, `Width` = 0.2, `Height` = 0.4, `Confidence` ≈ 0.902, `TrackId` = 1, `Distance` = 0, `Seq` thay đổi liên tục.
- `RxFrames` tăng 200 ± 5 trong 10 s. `RxErrors` = 0.

### UART-02 Mục tiêu chuyển động · P1

Bước:
1. `python vt_sim.py COM5` (hình sin: biên độ X 0.5, Y 0.3, chu kỳ 8 s).
2. Scope: vẽ `TrackingData.X` và `TrackingData.Y` trong 30 s.
3. `python vt_sim.py COM5 --amp-x 1.5`

Mong đợi:
- Bước 2: X dao động −0.5…+0.5 với chu kỳ 8 s, Y dao động −0.3…+0.3 với chu kỳ 4 s, không có điểm nhảy bất thường. `RxErrors` = 0.
- Bước 3: X bị kẹp ở ±1.0.

### UART-03 Frame FC_STATUS · P1

Bước:
1. Chạy `python vt_sim.py COM5 --static 0 0`.
2. Nghiêng board sang phải khoảng 30°, rồi ngẩng mũi lên khoảng 30°, rồi xoay yaw. So từng dòng `FC:` với `AttitudeState`.
3. Gạt switch Pos1 → Pos2 → Pos3.
4. Dừng vt_sim (Ctrl+C), đo tần số FC_STATUS bằng đoạn sau trong `python/visiontracker/`:
   ```python
   import serial, time, vt_protocol as vt
   s = serial.Serial("COM5", 115200, timeout=0.05); p = vt.Parser(); n = 0; t0 = time.time()
   while time.time() - t0 < 10:
       n += sum(1 for m, _ in p.feed(s.read(64)) if m == vt.MSG_FC_STATUS)
   print("FC_STATUS/s:", n / 10, "crc_errors:", p.crc_errors)
   ```

Mong đợi:
- `armed=0`. `rpy` cùng dấu và lệch không quá 1° so với `AttitudeState` Roll/Pitch/Yaw.
- `mode` = 1, 2, 19 lần lượt ở Pos1, Pos2, Pos3. Ở Pos3 có `state=2` (Tracking).
- Bước 4: `FC_STATUS/s` = 5.0 ± 0.2, `crc_errors` = 0. FC vẫn gửi FC_STATUS khi không nhận được gì.

### UART-04 Byte rác và frame hỏng · P1

Bước:
1. Ghi `SystemStats.FlightTime`.
2. `python vt_sim.py COM5 --static 0.3 -0.2 --garbage 0.2`, chạy 60 s, theo dõi `TrackingData`.

Mong đợi:
- `RxErrors` tăng dần. `RxFrames` vẫn tăng, ít nhất 14 frame/s (70% của UART-01).
- `X` luôn bằng 0.3 và `Y` luôn bằng −0.2: không frame hỏng nào lọt qua.
- FC không reset (`FlightTime` tăng liên tục). vt_sim vẫn in dòng `FC:`.

### UART-05 Sai baud · P1

Bước:
1. FC ở 115200. Ghi `RxFrames`, `RxErrors`.
2. `python vt_sim.py COM5 --baud 57600 --static 0 0`, chạy 20 s.
3. Lặp lại với `--baud 230400`.

Mong đợi:
- `RxFrames` không tăng (bắt buộc). `RxErrors` thường tăng: ghi lại.
- vt_sim không in dòng `FC:`.
- `TrackingData` giữ nguyên giá trị cũ.

### UART-06 Đổi baud · P2

Bước: lần lượt với tốc độ 57600 và 230400:
1. `HwSettings.VisionTrackerSpeed` = tốc độ đó → Send → Save → reset.
2. `python vt_sim.py COM5 --baud <tốc độ> --static 0 0`
3. Làm lại các kiểm tra của UART-01.

Mong đợi: như UART-01. Cuối test trả về 115200.

### UART-07 Cổng Main · P1

Bước:
1. Hardware: Main = VisionTracker (Flexi tự về Disabled). Save, reset.
2. Đấu W-MAIN, làm lại UART-01 và UART-03 (bước 1–2).

Mong đợi: như UART-01 và UART-03.

### UART-08 Cổng Receiver = VisionTracker · P2

Điều kiện: receiver RC chuyển sang cổng khác (S.Bus/DSM), hoặc test không dùng RC.

Bước:
1. Hardware: Receiver = VisionTracker. Save, reset.
2. Đấu W-RCVR, làm lại UART-01 và UART-03 (bước 1–2).

Mong đợi: như UART-01 và UART-03.

### UART-09 Cổng Receiver = PPM+VisionTracker · P1 nếu dùng cấu hình này

Bước:
1. Hardware: Receiver = PPM+VisionTracker. Save, reset.
2. Receiver PPM cắm chân PPM, adapter cắm chân USART6 (W-RCVR).
3. Chạy `python vt_sim.py COM5 --static 0 0`, đồng thời gạt từng stick hết hành trình.

Mong đợi:
- `TrackingData` như UART-01.
- `ManualControlCommand` Roll/Pitch/Yaw/Throttle chạy theo stick như trước. Alarm Receiver = OK.

### UART-10 Mất kết nối và thứ tự khởi động · P1

Bước:
1. vt_sim đang chạy. Rút dây TX của adapter 5 s rồi cắm lại.
2. vt_sim vẫn chạy. Reset FC bằng nút Reset trên trang Firmware.
3. Dừng vt_sim, reset FC, đợi FC khởi động xong rồi mới chạy vt_sim.
4. Ctrl+C rồi chạy lại vt_sim (Seq bắt đầu lại từ 0).

Mong đợi:
- Bước 1: `RxFrames` dừng tăng trong lúc rút dây, rồi tăng lại, không cần reset FC.
- Bước 2–4: `TrackingData` cập nhật lại trong vòng 1 s. `RxErrors` có thể tăng 1 do frame bị cắt. FC không kiểm tra Seq nên vẫn nhận.

### UART-11 Tần số frame và tải CPU · P2

Bước: với lần lượt `R` = 10, 30, 100, chạy `python vt_sim.py COM5 --rate R --static 0 0` trong 60 s. Ghi `RxFrames/s`, `RxErrors`, `SystemStats.CPULoad`, `TaskInfo.StackRemaining.VisionTracker`.

Mong đợi:
- `RxFrames/s` ≈ R (±5%), `RxErrors` = 0.
- CPULoad tăng không quá 3% so với CFG-05 bước 2. StackRemaining không giảm so với CFG-05. Không có alarm.
- P3: thử `--rate 300` (khoảng 6.3 KB/s ở 115200 baud). FC không treo, không reset. Ghi lại `RxErrors`.

---

## 6. FM: logic FollowMe trên bàn (Mốc B)

Điều kiện chung: CFG-BASE, W-FLEXI, bộ phát RC đang bật, FC nối GCS. **Không arm** trừ khi test ghi khác. Board đặt nằm ngang và đứng yên.

Theo dõi: `FollowMeStatus` (State, YawError, ElevationError, YawRateCmd, ClimbRateCmd, AltitudeOffset), `StabilizationDesired` (Roll, Pitch, Yaw, Thrust, StabilizationMode), `FlightStatus`.

### FM-01 Không arm được khi đang ở FollowMe · P1

Bước:
1. Chạy `vt_sim --static 0 0`, gạt Pos3.
2. Thực hiện thao tác arm (stick hoặc switch theo `FlightModeSettings.Arming`), giữ 3 s.
3. Gạt về Pos1, arm, rồi disarm (đối chứng).

Mong đợi: bước 2 `FlightStatus.Armed` giữ Disarmed, không qua Arming. Bước 3 arm và disarm bình thường.

### FM-02 Vào FollowMe khi không có dữ liệu · P1

Bước:
1. Reset FC, không chạy vt_sim.
2. Gạt Pos3, chờ 2 s.

Mong đợi:
- `FlightStatus.FlightMode` = FollowMe. `FollowMeStatus.State` = NoData (300 ms đầu có thể thấy Lost).
- YawRateCmd = 0, ClimbRateCmd = 0.
- `StabilizationDesired`: Roll = 0, Pitch = 0, Yaw = 0, Thrust = 0.5. StabilizationMode = Attitude / Attitude / Rate / AltitudeVario.
- PFD hiện `FOLLOW ME` màu cyan.

### FM-03 Mục tiêu ở giữa ảnh · P1

Bước: `vt_sim --static 0 0`, gạt Pos3, quan sát 10 s.

Mong đợi:
- State = Tracking. YawError ≈ 0, YawRateCmd ≈ 0.
- ElevationError ≈ `AttitudeState.Pitch`. ClimbRateCmd = 0 nếu |Pitch| < 2°. Thrust = 0.5.
- vt_sim in `mode=19 state=2`.

### FM-04 Chiều và độ lớn lệnh yaw · P1

Bước: giữ Pos3, lần lượt chạy `vt_sim --static 0.25 0`, `--static 0.5 0`, `--static -0.5 0` (Ctrl+C rồi chạy lại cho từng giá trị).

Mong đợi:
- YawError và YawRateCmd lần lượt bằng +11.85, +22.76, −22.76 (bảng 1.6).
- `StabilizationDesired.Yaw` = YawRateCmd, `RateDesired.Yaw` = YawRateCmd.
- `ActuatorDesired.Yaw` cùng dấu với YawRateCmd (board đứng yên nên vòng rate ra lệnh quay theo hướng đó).

### FM-05 Kẹp MaxYawRate · P1

Bước:
1. `vt_sim --static 1 0`, rồi `--static -1 0`.
2. `MaxYawRate` = 15 (Send), chạy lại `--static 1 0`.
3. Trả `MaxYawRate` về 30.

Mong đợi:
- Bước 1: YawError = ±40.00, YawRateCmd = ±30.00.
- Bước 2: YawRateCmd = +15.00.

### FM-06 Lệnh leo, hạ và ga ảo · P1

Bước:
1. Lần lượt `vt_sim --static 0 -0.5`, `0 0.5`, `0 -0.1`, `0 0.1`.
2. `MaxClimbRate` = 0.1 (Send), chạy lại `--static 0 -0.5`.
3. Trả `MaxClimbRate` về 0.3.

Mong đợi:
- Bước 1, (ClimbRateCmd, Thrust) lần lượt: (+0.300, 0.648), (−0.300, 0.352), (+0.065, 0.611), (−0.065, 0.389).
- Bước 2: ClimbRateCmd = +0.100, Thrust = 0.616.
- `AltitudeOffset` phải nằm trong (−1, +3). Nếu baro trôi ra ngoài khoảng đó thì gạt ra rồi vào lại FollowMe để đặt lại.

### FM-07 Deadband độ cao · P2

Bước:
1. `vt_sim --static 0 0.05`.
2. `ElevationDeadband` = 0 (Send).
3. Trả `ElevationDeadband` về 2.

Mong đợi:
- Bước 1: ElevationError = −1.65, ClimbRateCmd = 0, Thrust = 0.5.
- Bước 2: ClimbRateCmd = −0.083, Thrust ≈ 0.387.

### FM-08 Bù góc pitch của thân · P1

Bước: `vt_sim --static 0 0`, gạt Pos3. Nghiêng board, đọc `AttitudeState.Pitch`:
1. Ngẩng mũi lên khoảng 5°.
2. Chúc mũi xuống khoảng 5°.
3. Ngẩng mũi lên trên 10°.

Mong đợi:
- ElevationError ≈ Pitch ở mọi bước.
- Bước 1: ClimbRateCmd ≈ +0.15, Thrust ≈ 0.624. Bước 2: ≈ −0.15, Thrust ≈ 0.376. Bước 3: +0.300 (kẹp).
- Tổng quát, với 2° < |Pitch| < 8°: ClimbRateCmd = 0.05·(|Pitch| − 2), cùng dấu với Pitch.

### FM-09 Bù yaw giữa hai frame · P2

Bước:
1. `vt_sim --static 0.5 0 --rate 4` (250 ms mỗi frame, dưới TrackTimeout 300 ms). Gạt Pos3.
2. Cầm board, xoay đều sang phải (`AttitudeState.Yaw` tăng) khoảng 30 °/s trong 5 s.
3. Xoay sang trái với tốc độ tương tự.

Mong đợi:
- State luôn là Tracking.
- Đứng yên: YawError = 22.76.
- Xoay phải: YawError nằm trong khoảng 15…22.8 (không vượt 22.8).
- Xoay trái: YawError nằm trong khoảng 22.8…30 (không dưới 22.8).
- Nếu ngược lại thì phần bù yaw bị sai dấu.

### FM-10 Thành phần D của yaw · P2

Bước:
1. `YawPID.Kd` = 0 (Send). Chạy `vt_sim` (hình sin mặc định), Scope vẽ `YawError` và `YawRateCmd` trong 30 s.
2. `YawPID.Kd` = 0.5 (Send), quan sát lại.
3. Trả `Kd` về 0.05.

Mong đợi:
- Bước 1: YawRateCmd trùng YawError, biên độ ≈ 22.8.
- Bước 2: biên độ YawRateCmd ≈ 24.8, đỉnh đến sớm hơn đỉnh YawError khoảng 0.5 s. Không có gai lớn.

### FM-11 Ảnh hưởng của CameraFOV · P2

Bước:
1. `CameraFOV.Horizontal` = 60 (Send), chạy `vt_sim --static 0.5 0`.
2. `CameraFOV.Vertical` = 40 (Send), chạy `vt_sim --static 0 -0.5`.
3. Trả về 80/60.

Mong đợi: bước 1 YawError = 16.10. Bước 2 ElevationError = +10.31.

### FM-12 MinConfidence · P1

Bước:
1. `vt_sim --static 0.5 0 --conf 0.35`.
2. `vt_sim --static 0.5 0 --conf 0.45`.

Mong đợi:
- Bước 1: `TrackingData.Valid` = True, `Confidence` ≈ 0.349. State = Lost, YawRateCmd = 0, Thrust = 0.5.
- Bước 2: State = Tracking, YawRateCmd = 22.76.

### FM-13 Companion báo mất mục tiêu (valid = 0) · P1

Bước: `vt_sim --static 0.5 0 --drop 10:3`, gạt Pos3, quan sát 30 s.

Mong đợi:
- 3 s đầu của mỗi chu kỳ 10 s: `TrackingData.Valid` = False, State = Lost, YawRateCmd = 0, ClimbRateCmd = 0.
- 7 s còn lại: State = Tracking, YawRateCmd = 22.76.
- Không bao giờ là NoData (frame vẫn tới đều).

### FM-14 Mất frame (TrackTimeout) · P1

Bước:
1. `vt_sim --static 0.5 0`, gạt Pos3, chờ Tracking.
2. Ctrl+C vt_sim.
3. Chạy lại vt_sim.
4. Lặp lại bước 1–3 nhưng rút dây TX của adapter thay vì Ctrl+C.

Mong đợi:
- Trong vòng 0.5 s sau bước 2 (300 ms timeout cộng 250 ms telemetry): State = NoData, YawRateCmd = 0, Thrust = 0.5.
- Trong vòng 0.5 s sau bước 3: State = Tracking, YawRateCmd = 22.76.
- Bước 4 cho kết quả giống hệt.

### FM-15 Tần số frame quanh TrackTimeout · P2

Bước:
1. `vt_sim --static 0.5 0 --rate 5` (200 ms/frame).
2. `vt_sim --static 0.5 0 --rate 3` (333 ms/frame, trên TrackTimeout), quan sát 30 s.
3. `TrackTimeout` = 500 (Send), lặp lại bước 2. Sau đó trả về 300.

Mong đợi:
- Bước 1: Tracking ổn định.
- Bước 2: thỉnh thoảng thấy NoData và YawRateCmd = 0 xen giữa các lần Tracking.
- Bước 3: Tracking ổn định.
- Kết luận cho companion: phải gửi nhanh hơn 1000/TrackTimeout Hz, khuyến nghị 10–30 Hz.

### FM-16 LostAction = Hover · P1

Bước: `LostAction` = Hover (mặc định). Gạt Pos3 với `vt_sim --static 0.5 0`, rồi Ctrl+C vt_sim, chờ 15 s (vượt LostTimeout 10 s).

Mong đợi: ClimbRateCmd = 0 và Thrust = 0.5 trong suốt 15 s.

### FM-17 LostAction = Descend · P1

Bước:
1. `LostAction` = Descend, `LostTimeout` = 3000 (Send).
2. `vt_sim --static 0 0`, gạt Pos3, chờ Tracking ít nhất 2 s.
3. Ctrl+C vt_sim, bấm đồng hồ.
4. Chạy lại vt_sim.
5. Lặp lại bước 2–4 nhưng gây Lost bằng `vt_sim --static 0 0 --conf 0.1` thay vì dừng vt_sim.
6. `LostDescentRate` = 0.5, lặp lại bước 2–3.
7. Dừng vt_sim, gạt Pos1, rồi gạt Pos3 (vào mode khi chưa từng có mục tiêu), bấm đồng hồ.
8. Trả về Hover, 10000, 0.3.

Mong đợi:
- Bước 3: 0–3 s ClimbRateCmd = 0. Sau khoảng 3 s ClimbRateCmd = −0.300, Thrust = 0.352, giữ nguyên kể cả khi AltitudeOffset < −1 (khi hạ cánh không bị giới hạn bởi AltitudeWindow).
- Bước 4: Tracking, ClimbRateCmd về 0 ngay.
- Bước 5: giống bước 3.
- Bước 6: ClimbRateCmd = −0.500, Thrust ≈ 0.323.
- Bước 7: khoảng 3 s sau khi vào mode thì bắt đầu hạ (LostTimeout tính từ lúc vào mode). Ghi nhớ điều này khi bay.

### FM-18 Cửa sổ độ cao · P2

Điều kiện: cáp USB dài hoặc telemetry không dây, để nâng hạ board được khoảng 1 m.

Bước:
1. `AltitudeWindow.Climb` = 0.5, `Descent` = 0.5 (Send).
2. Board trên bàn, gạt Pos3 (AltitudeOffset ≈ 0). `vt_sim --static 0 -0.5`. ClimbRateCmd = +0.3.
3. Nâng board cao hơn bàn khoảng 1 m, rồi hạ về bàn.
4. `vt_sim --static 0 0.5` (muốn hạ). Hạ board xuống sàn, thấp hơn bàn khoảng 1 m.
5. Trả AltitudeWindow về 1/3.

Mong đợi:
- Bước 3: khi AltitudeOffset ≥ 0.5 thì ClimbRateCmd = 0, Thrust = 0.5. Hạ về bàn thì trở lại +0.3.
- Bước 4: khi AltitudeOffset ≤ −0.5 thì ClimbRateCmd = 0.
- Nhiễu baro khoảng ±0.3 m nên giá trị có thể nhấp nháy quanh biên.

### FM-19 Bỏ qua stick · P1

Bước: Pos3, `vt_sim --static 0.5 0`. Gạt lần lượt từng stick hết hành trình (roll, pitch, yaw, ga min và max).

Mong đợi:
- `StabilizationDesired` không đổi: Roll = 0, Pitch = 0, Yaw = 22.76, Thrust = 0.5.
- `ManualControlCommand` vẫn chạy theo stick (chứng tỏ RC hoạt động).

### FM-20 Vào lại mode thì khởi tạo lại · P2

Bước:
1. Pos3 với `vt_sim --static 0.5 0`, chờ Tracking. Ctrl+C vt_sim, chờ NoData.
2. Gạt Pos1 rồi Pos3 (vt_sim vẫn tắt).
3. Ở Pos1, nâng board lên khoảng 1 m, rồi gạt Pos3.

Mong đợi:
- Bước 2: không quay lại Tracking (frame nhận trước khi vào mode không được dùng). Lost trong ≤ 0.3 s rồi NoData. AltitudeOffset ≈ 0.
- Bước 3: AltitudeOffset ≈ 0 tại độ cao mới.

### FM-21 FollowMe khi chưa cấu hình cổng · P2

Bước:
1. Hardware: không cổng nào là VisionTracker, Save, reset.
2. Pos3 vẫn là FollowMe. Gạt Pos3, chờ 15 s.

Mong đợi:
- Không có alarm cấu hình (sanity check chỉ kiểm tra firmware có module, không kiểm tra cổng).
- State = NoData mãi. YawRateCmd = 0, Thrust = 0.5 (hoặc hạ sau LostTimeout nếu LostAction = Descend).
- Ghi nhận: nếu lỡ bay với cấu hình này, drone chỉ giữ heading và độ cao.

### FM-22 Không tự disarm khi ga thấp ở FollowMe · P1

Điều kiện: có arm (xem 1.1: chỉ USB hoặc tháo cánh). `FlightModeSettings.ArmedTimeout` = 30000 (mặc định).

Bước:
1. `vt_sim --static 0 0`. Arm ở Pos1, ngay sau đó gạt Pos3, ga để min.
2. Chờ 60 s.
3. Gạt về Pos1, ga vẫn min, chờ 40 s.

Mong đợi:
- Bước 2: vẫn Armed sau 60 s.
- Bước 3: tự disarm sau khoảng 30 s (hành vi cũ ở Stabilized không đổi).

### FM-23 Disarm bằng tay khi đang ở FollowMe · P1

Bước: arm rồi gạt Pos3 như FM-22. Ga min, thực hiện thao tác disarm và giữ lâu hơn `DisarmingSequenceTime` (1 s).

Mong đợi: Disarmed sau khoảng 1 s ngay trong FollowMe, không cần gạt ra. Đây là cách tắt motor sau khi LostAction = Descend đã hạ cánh (`progress.md` 11.7 hiện ghi phải gạt ra khỏi FollowMe trước, cập nhật lại nếu test này pass).

### FM-24 Mất RC khi đang ở FollowMe · P1

Điều kiện: arm rồi gạt Pos3 như FM-22, `vt_sim --static 0.5 0`, State = Tracking.

Bước:
1. `ManualControlSettings.FailsafeFlightModeSwitchPosition` = −1. Tắt bộ phát RC 60 s, rồi bật lại.
2. `FailsafeFlightModeSwitchPosition` = 0 (Pos1). Tắt bộ phát 40 s, rồi bật lại.
3. Đặt `FailsafeFlightModeSwitchPosition` về giá trị sẽ dùng khi bay, lặp lại bước 2.

Mong đợi:
- Bước 1: alarm Receiver. FlightMode vẫn là FollowMe, vẫn Armed sau 60 s, YawRateCmd vẫn 22.76 (drone sẽ tiếp tục bám). Bật lại bộ phát thì switch có tác dụng trở lại.
- Bước 2: FlightMode chuyển sang Stabilized1, `ManualControlCommand.Throttle` = `FailsafeChannel.Throttle` (mặc định −1), motor về mức thấp nhất, tự disarm sau ArmedTimeout. **Khi bay thật cấu hình này làm drone rơi.**
- Bước 3: ghi lại hành vi. Chỉ chấp nhận vị trí failsafe mà ở đó `StabilizationDesired.Thrust` không làm drone rơi hay hạ quá nhanh.

### FM-25 Hiển thị chế độ · P2

Bước: gạt Pos1 ↔ Pos3, khi disarm và khi arm (như FM-22).

Mong đợi:
- PFD: `STAB 1` màu xanh lá ở Pos1, `FOLLOW ME` màu cyan ở Pos3.
- LED trên board ở FollowMe nháy giống PathPlanner/POI. Nếu có LED WS281x (Notify) thì hiện chuỗi của chế độ tự động.
- Nếu có cổng MAVLink nối QGroundControl/Mission Planner: chế độ hiện GUIDED.
- Nếu dùng HoTT telemetry: chữ `FollowMe`.
- FC_STATUS: `mode=19`.

---

## 7. INT: tích hợp companion thật

Điều kiện chung:
- Companion (Raspberry Pi, Jetson...) nối UART 3.3 V vào cổng đã chọn (TX ↔ RX, GND chung). Code tracker dùng `vt_protocol.encode_track`.
- Camera gắn cố định lên khung, nhìn về trước.
- `CameraFOV` đặt theo camera thật (sau khi undistort).
- FC không arm.

### INT-01 Chiều trục ảnh · P1

Bước:
1. Đặt vật hoặc người ở giữa khung hình.
2. Lần lượt dời sang phải, sang trái, lên trên, xuống dưới (phải/trái theo hướng nhìn của camera), rồi ra sát mép phải.
3. Đưa vật lại gần camera.

Mong đợi:
- Bước 1: X ≈ 0, Y ≈ 0.
- Bước 2: phải → X > 0, trái → X < 0, lên → Y < 0, xuống → Y > 0, mép phải → X ≈ +1.
- Bước 3: Width và Height tăng.
- **Không bay nếu test này fail.** Sai dấu X thì drone quay ra xa mục tiêu, sai dấu Y thì leo/hạ ngược. Kiểm tra cả thiết lập lật/gương ảnh của camera.

### INT-02 Tần số và độ ổn định của link · P1

Bước: tracker chạy liên tục 10 phút, có mục tiêu trong khung hình.

Mong đợi:
- `RxFrames/s` ≈ FPS của tracker (10–30). `RxErrors` = 0 hoặc gần như không tăng (ghi lại).
- `Confidence` ổn định khi có mục tiêu.
- Companion nhận FC_STATUS 5 Hz, armed/mode/rpy đúng với GCS.

### INT-03 Kiểm tra CameraFOV bằng góc đã biết · P2

Bước:
1. Gạt Pos3 (disarm), drone đứng yên.
2. Đặt vật cách camera 2 m, lệch sang phải 1 m (góc thật atan(1/2) = 26.6°), cùng độ cao với camera.
3. Đặt vật ngay trước camera, cách 2 m, cao hơn camera 0.5 m (góc thật atan(0.5/2) = 14.0°).

Mong đợi:
- Bước 2: YawError ≈ 26.6° ±2°.
- Bước 3: ElevationError ≈ +14.0° ±2° (thân ngang).
- Nếu lệch nhiều hơn thì chỉnh `CameraFOV.Horizontal`/`Vertical`.

### INT-04 Vòng kín bằng tay · P1

Bước:
1. Gạt Pos3 (disarm). Cầm drone, vật đứng yên lệch phải khoảng 20°.
2. Xoay drone theo dấu của YawRateCmd cho tới khi vật vào giữa ảnh, rồi xoay quá một chút.
3. Vật ở giữa ảnh. Lần lượt ngẩng mũi và chúc mũi drone khoảng 10°, mỗi tư thế giữ 2 s.

Mong đợi:
- Bước 1: YawRateCmd > 0 (quay phải).
- Bước 2: YawError giảm dần về 0, xoay quá thì đổi dấu.
- Bước 3: ElevationError thay đổi ít (dưới 3°) vì bù pitch triệt tiêu chuyển động của vật trong ảnh. Nếu ElevationError thay đổi khoảng gấp đôi góc nghiêng thì dấu pitch bị sai.

### INT-05 Mất mục tiêu với tracker thật · P1

Bước:
1. Gạt Pos3, có mục tiêu (Tracking).
2. Che camera 5 s, rồi bỏ che.
3. Tắt process tracker.
4. Chạy lại tracker, rồi rút dây UART.
5. Hai người cùng trong khung hình, một người đi ngang qua người kia.

Mong đợi:
- Bước 2: Lost (tracker gửi valid = 0), bỏ che thì Tracking lại.
- Bước 3 và 4: NoData trong vòng 0.5 s.
- Bước 5: ghi lại `TrackId` có giữ ổn định không. FC không kiểm tra TrackId, giữ đúng người là việc của companion.

### INT-06 Thứ tự khởi động · P2

Bước:
1. Bật companion trước FC.
2. Bật FC trước companion.
3. Reset FC khi companion đang chạy.
4. Khởi động lại companion khi FC đang chạy.

Mong đợi: mọi trường hợp FC nhận lại TRACK tự động trong vòng 1 s sau khi cả hai chạy. Companion không treo khi chưa có FC_STATUS.

### INT-07 Độ trễ camera + tracker · P3

Bước: trên companion, ghi timestamp lúc chụp ảnh và lúc ghi frame ra UART, chạy 1 phút.

Mong đợi: ghi lại độ trễ trung bình và lớn nhất. Trên 100 ms thì giữ Kp/Kd thấp khi bay (độ trễ chưa được bù).

---

## 8. FLT: bay thử

### Điều kiện an toàn (kiểm tra trước mỗi buổi bay)

- Đã pass mọi test P1 của UT, CFG, UART, FM, INT, nhất là INT-01, INT-04, FM-23, FM-24.
- Bay trong nhà rộng (tối thiểu 6 × 6 m), có dây giữ ngắn hơn khoảng cách tới vật cản. Người làm mục tiêu đứng ngoài tầm dây giữ.
- `AltitudeWindow.Climb` ≤ chiều cao trần − độ cao lúc vào mode − 1 m. Ví dụ trần 3 m, vào mode ở 1.5 m thì Climb ≤ 0.5. Mặc định 3 m là quá cao khi bay trong nhà.
- Stabilized và AltitudeVario đã tinh chỉnh (FLT-01 pass).
- Arm switch hoạt động. Phi công an toàn cầm bộ phát suốt buổi.
- `FailsafeFlightModeSwitchPosition` đã chọn và kiểm tra ở FM-24.
- Các lần bay đầu dùng `LostAction` = Hover.
- Pin đầy. Bật ghi log telemetry trên GCS (file `.opl`).
- **Quy tắc thoát FollowMe:** luôn thoát về Pos2 (AltitudeVario) với stick ga ở giữa. Thoát về Pos1 thì ga theo stick ngay lập tức, nếu stick đang ở thấp thì drone rơi.

### FLT-01 Hover cơ sở · P1

Bước:
1. Arm ở Pos1, cất cánh, hover 1–1.5 m.
2. Đưa ga về giữa, gạt Pos2, giữ 30 s.
3. Hạ cánh.

Mong đợi: ở Pos2 độ cao giữ trong khoảng ±0.3 m. Nếu không đạt thì tinh chỉnh `AltitudeHoldSettings` trước khi làm tiếp.

### FLT-02 Vào và ra FollowMe khi không có mục tiêu · P1

Bước:
1. Companion chạy, camera nhìn vào tường trống (valid = 0).
2. Hover ở Pos2 độ cao 1.5 m, ga ở giữa.
3. Gạt Pos3, giữ 10 s.
4. Ga ở giữa, gạt về Pos2.

Mong đợi:
- Bước 3: State = Lost. Không giật lên hay xuống (độ cao giữ ±0.3 m), heading giữ gần như cố định, thân giữ ngang. Trôi ngang theo gió hoặc trim là bình thường.
- Bước 4: chuyển mode mượt.

### FLT-03 Mục tiêu đứng yên ở giữa · P1

Bước: người đứng trước drone 3–4 m, ở giữa ảnh. Hover ở Pos2 rồi gạt Pos3, giữ 30 s.

Mong đợi: Tracking. YawError dao động dưới ±5°, không có dao động yaw rõ rệt. ClimbRateCmd gần 0, độ cao ổn định.

### FLT-04 Mục tiêu đi ngang · P1

Bước:
1. Người đi chậm (khoảng 0.5 m/s) sang trái rồi sang phải trên cung khoảng 60°, rồi đứng lại.
2. Lặp lại, đi nhanh hơn (khoảng 1.5 m/s).

Mong đợi:
- Bước 1: drone quay theo. Sau khi người đứng lại, vật về giữa ảnh trong vòng 2 s, vọt lố tối đa 1 lần, không lắc qua lại liên tục.
- Bước 2: YawRateCmd chạm 30 °/s, drone chậm hơn mục tiêu nhưng không mất ổn định.
- Nếu dao động: giảm `YawPID.Kp` từng bước 0.2 rồi bay lại. Ghi lại gain cuối cùng.

### FLT-05 Mục tiêu lên xuống · P2

Bước: mục tiêu (vật gắn trên sào, hoặc người lên xuống bậc thang) nâng lên khoảng 1 m, giữ, rồi hạ xuống.

Mong đợi:
- Drone leo/hạ với tốc độ tối đa 0.3 m/s, không dao động độ cao.
- Dừng ở biên AltitudeWindow (AltitudeOffset = +Climb hoặc −Descent).

### FLT-06 Mất mục tiêu khi bay · P1

Bước: đang Tracking, người núp sau vật che 5 s rồi bước ra.

Mong đợi: Lost, giữ heading và độ cao. Khi người xuất hiện lại: Tracking, drone quay về phía người mượt mà, không giật (thành phần D được reset).

### FLT-07 LostAction = Descend · P2

Bước:
1. `LostAction` = Descend, `LostTimeout` = 5000.
2. Tracking ở độ cao 1.5 m, người núp đi.
3. Sau khi drone chạm đất, disarm ngay trong FollowMe (FM-23).

Mong đợi:
- Giữ 5 s, rồi hạ với tốc độ 0.3 m/s, chạm đất nhẹ, không nảy.
- Sau khi chạm đất vẫn Armed, motor quay ở `ThrustLimits.Min`.
- Bước 3: Disarmed.

### FLT-08 Companion hoặc UART hỏng khi bay · P2

Bước: đang Tracking (LostAction = Hover), tắt process tracker (hoặc rút nguồn companion). Sau 10 s bật lại.

Mong đợi: NoData, drone giữ heading và độ cao. Khi tracker chạy lại thì Tracking trở lại.

### FLT-09 Mất RC khi bay · P3

Chỉ làm khi FM-24 bước 3 đã chứng minh vị trí failsafe an toàn, và phải có dây giữ.

Bước: đang ở FollowMe, tắt bộ phát 2 s rồi bật lại.

Mong đợi: hành vi đúng như FM-24 bước 3. Phi công lấy lại điều khiển ngay khi bật bộ phát.

### FLT-10 Phân tích log, tinh chỉnh gain · P2

Bước: sau mỗi chuyến bay, mở log `.opl`, vẽ YawError, YawRateCmd, ElevationError, ClimbRateCmd, AltitudeOffset, `AttitudeState.Yaw`.

Mong đợi:
- Mục tiêu đứng yên: YawError dưới ±5°, không có dao động kéo dài.
- Ghi lại bộ gain (Kp, Kd, ElevationKp) cho từng chuyến và lý do thay đổi.

---

## 9. REG: hồi quy

| ID | Ưu tiên | Bước | Mong đợi |
|---|---|---|---|
| REG-01 | P1 | Trong WSL: `make all_ut_run` | Mọi unit test pass |
| REG-02 | P1 | Arm và bay các chế độ đang dùng (Stabilized1–3, AltitudeVario, AltitudeHold; PositionHold/RTB nếu có GPS) | Hoạt động như trước. PFD hiện đúng tên chế độ |
| REG-03 | P1 | Sau CFG-01, kiểm tra các chức năng cổng khác: Telemetry, GPS, S.Bus, DSM, PPM, PWM | Hoạt động như trước |
| REG-04 | P2 | RevoNano, Sparky2 chạy firmware của nhánh | Khởi động, kết nối GCS, không có alarm mới |
| REG-05 | P1 | Arm ở Stabilized, ga min | Tự disarm sau ArmedTimeout như cũ (FM-22 bước 3) |
| REG-06 | P2 | So `SystemStats.CPULoad`, `HeapRemaining` trước và sau nâng cấp (có cổng VisionTracker) | CPU tăng dưới 3%. Ghi lại heap |

---

## 10. Bảng ghi kết quả

Firmware (commit): ________  GCS (commit): ________  Board: ________  Người test: ________

| ID | Ưu tiên | Ngày | Kết quả (Pass / Fail / Bỏ qua) | Ghi chú |
|---|---|---|---|---|
| UT-01 | P1 | | | |
| UT-02 | P1 | | | |
| UT-03 | P2 | | | |
| UT-04 | P2 | | | |
| BLD-01 | P1 | | | |
| BLD-02 | P2 | | | |
| BLD-03 | P1 | | | |
| CFG-01 | P1 | | | |
| CFG-02 | P1 | | | |
| CFG-03 | P1 | | | |
| CFG-04 | P2 | | | |
| CFG-05 | P1 | | | |
| CFG-06 | P1 | | | |
| CFG-07 | P1 | | | |
| CFG-08 | P2 | | | |
| CFG-09 | P2 | | | |
| CFG-10 | P2 | | | |
| UART-01 | P1 | | | |
| UART-02 | P1 | | | |
| UART-03 | P1 | | | |
| UART-04 | P1 | | | |
| UART-05 | P1 | | | |
| UART-06 | P2 | | | |
| UART-07 | P1 | | | |
| UART-08 | P2 | | | |
| UART-09 | P1* | | | *nếu dùng PPM+VisionTracker |
| UART-10 | P1 | | | |
| UART-11 | P2 | | | |
| FM-01 | P1 | | | |
| FM-02 | P1 | | | |
| FM-03 | P1 | | | |
| FM-04 | P1 | | | |
| FM-05 | P1 | | | |
| FM-06 | P1 | | | |
| FM-07 | P2 | | | |
| FM-08 | P1 | | | |
| FM-09 | P2 | | | |
| FM-10 | P2 | | | |
| FM-11 | P2 | | | |
| FM-12 | P1 | | | |
| FM-13 | P1 | | | |
| FM-14 | P1 | | | |
| FM-15 | P2 | | | |
| FM-16 | P1 | | | |
| FM-17 | P1 | | | |
| FM-18 | P2 | | | |
| FM-19 | P1 | | | |
| FM-20 | P2 | | | |
| FM-21 | P2 | | | |
| FM-22 | P1 | | | |
| FM-23 | P1 | | | |
| FM-24 | P1 | | | |
| FM-25 | P2 | | | |
| INT-01 | P1 | | | |
| INT-02 | P1 | | | |
| INT-03 | P2 | | | |
| INT-04 | P1 | | | |
| INT-05 | P1 | | | |
| INT-06 | P2 | | | |
| INT-07 | P3 | | | |
| FLT-01 | P1 | | | |
| FLT-02 | P1 | | | |
| FLT-03 | P1 | | | |
| FLT-04 | P1 | | | |
| FLT-05 | P2 | | | |
| FLT-06 | P1 | | | |
| FLT-07 | P2 | | | |
| FLT-08 | P2 | | | |
| FLT-09 | P3 | | | |
| FLT-10 | P2 | | | |
| REG-01 | P1 | | | |
| REG-02 | P1 | | | |
| REG-03 | P1 | | | |
| REG-04 | P2 | | | |
| REG-05 | P1 | | | |
| REG-06 | P2 | | | |
