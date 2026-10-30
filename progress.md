# LibrePilot GCS trên Ubuntu 24.04 / WSL2: nhật ký build và sửa lỗi

Nhánh: `ubuntu24-qt515-osgearth`, tách từ `master` @ `35e84b70` ("update OSG and OSGEarth", 2022-08-18).

## Trạng thái hiện tại

GCS build được trên Ubuntu 24.04 với Qt 5.15.13 của hệ thống, GCC 13.3, OSG 3.5.5 và osgEarth 2.8. Đã chạy "tạm ổn" trên Ubuntu (máy thật, VirtualBox) và WSL2.

Nhánh này chỉ chứa thay đổi mã nguồn và tài liệu. Bản nhị phân không được commit vì không nên nằm trong lịch sử git (bản đầu khoảng 103 MB, vượt giới hạn 100 MB/file của GitHub; bản build 2026-10-27 là 82,7 MB). Nếu cần phát hành, đưa file `.tar.gz` lên GitHub Releases.

Build lại ngày 2026-10-27 trên WSL2 Ubuntu 24.04.5 từ commit `25e8c0a5`, theo đúng mục 2 đến 6: không có lỗi biên dịch, bản giải nén qua bước kiểm tra ở mục 6 (chạy offscreen). Đóng gói giờ dùng `make gcs_bundle_osg` (mục 6).

Còn mở:

- Model View / Earth View: bản sửa cuối (dùng chung GL context với Qt Quick, mục 4.3) là bản đang chạy "tạm ổn". Trước bản sửa đó hai view này hiện màu đen hoặc trống. Cần xác nhận lại trên máy thật rằng mô hình 3D và địa cầu render đúng, không chỉ là không crash.
- USB trên WSL2 chưa dùng được (mục 7).
- Danh sách hạn chế khác ở mục 9.

Nhánh `feature/visiontracker-followme` thêm flight mode FollowMe nhận dữ liệu tracking qua UART (mục 11). Đã build và unit test, chưa thử trên phần cứng.

## 1. Nguồn mã

| Nguồn | Kết quả |
|---|---|
| `bitbucket.org/librepilot/librepilot`, nhánh `next` | Không truy cập được từ sandbox build (bị chặn bởi allowlist mạng). |
| Mirror `github.com/librepilot/LibrePilot`, nhánh `next` | Cũ: HEAD `8c101adc`, tháng 2/2019. Build được nhưng thiếu 120 commit. |
| Fork `github.com/VNArmy/librepilot_filnet_update_2022`, nhánh `master` | Dùng bản này. HEAD `35e84b70` chứa `8c101adc` và `70d5fd3` trong lịch sử. |

So với mirror 2019, fork này đã có sẵn các fix cho GCC 13 / Qt 5.15 (thiếu `#include <qpainterpath.h>` trong qwt, lỗi lấy địa chỉ rvalue trong `flightlogmanager.cpp`). Những fix đó không cần làm lại.

Các host `download.qt.io` và `download.librepilot.org` cũng bị chặn, nên không dùng được script tải Qt SDK và OSG/osgEarth của Makefile (`make qt_sdk_install`, `make osg_install`, `make osgearth_install`).

## 2. Môi trường build

- Ubuntu 24.04 x86_64, GCC 13.3.0, Qt 5.15.13 từ apt (không dùng Qt SDK 5.9 của dự án; Makefile tự rơi về `qmake` hệ thống khi không có `tools/qt-*`).
- Python 2 không còn trên 24.04 nhưng build gọi cứng `python2`, nên cần shim:

```bash
sudo ln -sf /usr/bin/python3 /usr/local/bin/python2
```

- Gói apt cho build:

```bash
sudo apt-get install -y build-essential git curl gdb wget debhelper p7zip-full \
  unzip flex bison libsdl1.2-dev libudev-dev libusb-1.0-0-dev mesa-common-dev \
  libgl1-mesa-dev libglu1-mesa-dev qtbase5-dev qtbase5-dev-tools qtdeclarative5-dev \
  qtquickcontrols2-5-dev qtmultimedia5-dev qtscript5-dev qttools5-dev \
  qttools5-dev-tools libqt5svg5-dev libqt5serialport5-dev libqt5sql5-sqlite \
  libqt5opengl5-dev qml-module-qtquick2 qml-module-qtquick-controls \
  qml-module-qtquick-controls2 qml-module-qtquick-window2 \
  qml-module-qtquick-layouts qml-module-qtqml-models2 patchelf cmake \
  libgdal-dev libgeos-dev libcurl4-openssl-dev libsqlite3-dev libpng-dev zlib1g-dev
```

- WSL2: build trong filesystem ext4 của distro (ví dụ `~/librepilot`), không build trên `/mnt/<ổ>`. File `ext4.vhdx` của distro mặc định nằm trên C:. Lần build 2026-10-27 (gói apt, OSG, osgEarth, GCS, đóng gói) làm distro dùng 5,4 GB và vhdx lớn tới 6,2 GB. Ổ chứa vhdx hết chỗ thì ext4 bên trong hỏng (mục 8 dòng 19).
- Máy có ít bộ nhớ ảo trống (RAM 16 GB, VM WSL nhận khoảng 7,8 GB, C: gần đầy) phải giảm `-j`: `-j6` làm VM WSL bị tắt ngang (mục 8 dòng 20), còn `-j3` build xong cả osgEarth lẫn GCS.

## 3. Thay đổi mã nguồn trong nhánh này

| File | Vấn đề | Sửa |
|---|---|---|
| `make/scripts/version-info.py` | Script Python 2: `print x`, output `subprocess` là `bytes`, duyệt `bytes` ra số nguyên. | Port sang Python 3: `print()`, `universal_newlines=True` trong `_exec`, `hex(c)` thay `hex(ord(c))`, decode/encode UTF-8 khi điền template. |
| `ground/gcs/src/plugins/coreplugin/authorsdialog.py` | Mở template ở chế độ nhị phân rồi gọi `str.replace`. | Mở file ở chế độ text (`"r"`, `"w"`). |
| `ground/gcs/src/libs/osgearth/osgQtQuick/OSGCamera.cpp` | osgEarth 2.8 chuyển `LogarithmicDepthBuffer` sang module Util. | `#include <osgEarthUtil/LogarithmicDepthBuffer>` |
| `ground/gcs/src/libs/osgearth/osgQtQuick/OSGSkyNode.cpp` | `SkyNode` nằm trong `osgEarth::Util`, `create()` cần `MapNode`. | Đổi include, namespace và gọi `osgEarth::Util::SkyNode::create(mapNode)`. |
| `ground/gcs/src/libs/osgearth/osgQtQuick/ga/OSGEarthManipulator.cpp` | `EarthManipulator` nằm trong osgEarthUtil. | `#include <osgEarthUtil/EarthManipulator>` |
| `ground/gcs/src/libs/osgearth/osgearth_dependencies.pri` | Thiếu link `osgEarthUtil`, lỗi `undefined symbol LogarithmicDepthBuffer::uninstall` khi load plugin. | Thêm `-losgEarthUtil` (Linux/macOS và nhánh win32). |
| `ground/gcs/src/plugins/pfdqml/pfdqmlgadgetwidget.cpp` | `saveState()`/`restoreState()` dùng `m_pfdQmlContext` khi QML chưa load được, gây segfault trong `PfdQmlContext::modelFile()` lúc bấm "Save GCS default settings". | Kiểm tra null trước khi gọi. |
| `ground/gcs/src/libs/osgearth/utils/qtwindowingsystem.cpp` | Xem mục 4.3. | Dùng chung context Qt Quick khi có; `makeCurrentImplementation` làm context current lại bằng surface của chính nó. |
| `ground/gcs/src/libs/osgearth/osgQtQuick/OSGViewport.cpp` | Xem mục 4.3. | Lưu và khôi phục context Qt Quick quanh các traversal của OSG; `createFramebufferObject` tự làm context current nếu cần. |
| `ground/gcs/src/app/main.cpp` | OSG không tìm thấy plugin (`osgPlugins-3.5.5/`, plugin osgEarth) khi bản cài nằm ở thư mục bất kỳ: "file not handled" với `.3ds`, `.png`, `.earth`. | Đặt `OSG_LIBRARY_PATH=<bin>/../lib/librepilot-gcs` lúc khởi động (giữ giá trị cũ nếu người dùng đã đặt). |

Các log chẩn đoán `[DIAG]`/`[DIAG2]` dùng trong lúc debug đã được bỏ khỏi nhánh này.

## 4. OpenSceneGraph và osgEarth

### 4.1 OSG 3.5.5

Lấy bản build sẵn của LibrePilot từ repo `librepilot/librepilot.github.io` (file `tools/osg-3.5.5-linux-x64.tar.gz`, khoảng 13 MB). Bản này chạy được trên Ubuntu 24.04 không cần build lại.

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/librepilot/librepilot.github.io lpio
cd lpio && git sparse-checkout set --no-cone /tools/osg-3.5.5-linux-x64.tar.gz
tar -xzf tools/osg-3.5.5-linux-x64.tar.gz -C <repo>/tools/
# tạo <repo>/tools/osg-3.5.5-linux-x64/{bin,include,lib64}
```

Tên thư mục phải đúng `tools/osg-3.5.5-linux-x64` vì `make/tools.mk` tìm `OSG_SDK_DIR` ở đó.

### 4.2 osgEarth 2.8

Bản build sẵn `osgearth-2.8-linux-x64` không dùng được: link với `libgdal.so.1` và cần symbol `CURL_OPENSSL_3` của libcurl cũ. Build lại từ source với GDAL 3.8.4 và GEOS 3.12.1 của Ubuntu 24.04, áp hai patch có sẵn trong repo:

```bash
git clone --branch osgearth-2.8 --depth 1 https://github.com/gwaldron/osgearth osgearth-src
cd osgearth-src
patch -p1 < <repo>/make/3rdparty/osgearth/osgearth-remove-deprecated-call.patch
patch -p1 < <repo>/make/3rdparty/osgearth/osgearth-geos-3_6_1-support.patch
mkdir build && cd build
export LD_LIBRARY_PATH=<repo>/tools/osg-3.5.5-linux-x64/lib64
cmake .. -DCMAKE_BUILD_TYPE=Release -DOSGEARTH_USE_QT=OFF \
  -DOSG_DIR=<repo>/tools/osg-3.5.5-linux-x64 -DOSGEARTH_INSTALL_SHADERS=ON \
  -DDYNAMIC_OSGEARTH=ON -DDYNAMIC_OPENSCENEGRAPH=ON \
  -DCMAKE_INSTALL_PREFIX=<repo>/tools/osgearth-2.8-linux-x64
make -j$(nproc) && make install
```

Thư mục cài phải tên `tools/osgearth-2.8-linux-x64` (Makefile tìm `OSGEARTH_SDK_DIR` ở đó). Không cần vá thêm mã osgEarth.

### 4.3 Tích hợp OSG với Qt Quick (phần mất nhiều thời gian nhất)

`OSGViewport` là một `QQuickFramebufferObject`. Qt Quick bind sẵn FBO rồi gọi `render()`; thiết kế gốc để OSG vẽ thẳng vào FBO đó nên OSG phải dùng chung GL context với Qt Quick. Cửa sổ OSG là `GraphicsWindowQt` (windowing system "MyQt", file `qtwindowingsystem.cpp`).

Ba lỗi chồng lên nhau:

1. Mã gốc: khi mượn context của Qt (`_owned == false`), `makeCurrentImplementation()` không bao giờ gọi `makeCurrent()`. Sau khi OSG nhả context, lần sau nó không lấy lại được, log `context is not current`, viewport đen.
2. OSG nhả context sau traversal. Khi `synchronize()` / `render()` trả về, Qt Quick không còn context current:
   - `createFramebufferObject()` crash vì `shareGroup` null khi chuyển từ PFD sang Model View / Earth View.
   - `QSGRenderer::renderScene()` crash trong `QOpenGLContext::functions()`.
3. `winContext->makeCurrent(window)` trả về `false` với cửa sổ offscreen của `QQuickWidget`. Phải dùng `winContext->makeCurrent(winContext->surface())`.

Cách sửa trong nhánh:

- `realizeImplementation()`: có context current thì dùng chung (`_owned = false`, gán rõ để GC đã từng owned rồi realize lại không xoá nhầm context của Qt); không có thì tạo context + `QOffscreenSurface` riêng.
- `makeCurrentImplementation()`: nếu context chưa current thì `makeCurrent()` bằng `_surface` (owned) hoặc `_glContext->surface()` (dùng chung).
- `synchronize()` và `render()`: lưu context + surface của Qt Quick trước `advance/eventTraversal/updateTraversal` và `renderingTraversals`, khôi phục sau đó nếu bị đổi.
- `createFramebufferObject()`: nếu không có context current thì làm current context của `QQuickWindow` bằng surface của chính nó trước khi tạo FBO.

Một hướng thử bị bỏ: luôn tạo context riêng cho OSG. Hướng này hết "context is not current" nhưng OSG vẽ vào context khác, không phải FBO mà Qt Quick hiển thị, nên view trống.

## 5. Build GCS

```bash
git clone -b ubuntu24-qt515-osgearth https://github.com/VNArmy/librepilot_filnet_update_2022.git
cd librepilot_filnet_update_2022
# chuẩn bị tools/osg-3.5.5-linux-x64 và tools/osgearth-2.8-linux-x64 như mục 4
export LD_LIBRARY_PATH=$PWD/tools/osg-3.5.5-linux-x64/lib64:$PWD/tools/osgearth-2.8-linux-x64/lib64
make gcs_qmake
make gcs -j$(nproc)
# kết quả: build/librepilot-gcs_release/{bin,lib,share}
```

OSG/osgEarth được bật mặc định trên Linux. Muốn build không có 3D: `make GCS_WITH_OSG=0 GCS_WITH_OSGEARTH=0 gcs`.

Nếu đổi `osgearth_dependencies.pri`, xoá `libGCSOsgEarth.so*` cũ trong build tree để buộc link lại.

## 6. Đóng gói bản chạy độc lập

Makefile không tự copy OSG/osgEarth vào bản build, nên có target riêng (chỉ trên Linux):

```bash
make gcs_bundle_osg
# kết quả: build/librepilot-gcs_release/librepilot-gcs-with-osgearth-linux-x86_64.tar.gz
```

Target này chạy `make gcs` trước, rồi gọi `make/scripts/gcs-bundle-osg-linux.sh` với `OSG_SDK_DIR` / `OSGEARTH_SDK_DIR` của `make/tools.mk`. Script làm các bước trước đây phải chạy tay:

1. Copy thư viện lõi OSG và osgEarth (`lib64/*.so*`) vào `lib/librepilot-gcs`, cùng thư mục với `libGCSOsgEarth`, rồi `patchelf --set-rpath '$ORIGIN'`.
2. Copy plugin osgEarth (`osgdb_earth.so`, `osgdb_osgearth_*.so`, `osgdb_kml.so`, ...; 55 file) vào cùng thư mục đó, rpath `$ORIGIN`. osgEarth build từ source theo mục 4.2 cài các plugin này vào `lib64/osgPlugins-3.5.5/` chứ không phải `lib64/`, nên bản hướng dẫn tay trước đây (chỉ copy `lib64/*.so*`) bỏ sót chúng.
3. Copy 89 plugin đọc file của OSG (.3ds, .png, .osg, ...) vào `lib/librepilot-gcs/osgPlugins-3.5.5/`, rpath `$ORIGIN/..`.
4. Build lại `osgdb_png.so` từ source OSG 3.6.5, vì bản build sẵn cần libpng12 mà 24.04 không còn. Source được sparse clone vào `downloads/osg-3.6.5-png-src`, chỉ clone ở lần chạy đầu. Lệnh link có thêm `-Wl,-rpath-link` tới `lib64` của OSG để `ld` tìm được các phụ thuộc gián tiếp của `libosgDB` mà không cần `LD_LIBRARY_PATH`.
5. Copy shader/resource của osgEarth vào `share/librepilot-gcs/osgearth-resources`.
6. Nén `bin lib share` (thiếu `lib` thì bản chạy hỏng, xem mục 8 dòng 3).

Chạy lại target nhiều lần được. Vì target build `gcs` trước, nếu cây mã có thay đổi chưa commit thì `libVersionInfo` được build lại với nhãn `-dirty`; nên commit trước khi đóng gói bản phát hành. Ngày 2026-10-27, file nén do target tạo ra trùng từng byte với bản đóng gói tay đã kiểm tra (806 mục), chỉ khác `libVersionInfo` vì nhãn `-dirty`.

Kiểm tra trước khi phát hành: giải nén ra thư mục mới, `unset LD_LIBRARY_PATH OSG_LIBRARY_PATH`, dlopen từng plugin GCS và OSG (ví dụ `python3 -c "import ctypes; ctypes.CDLL('lib/librepilot-gcs/plugins/LibrePilot/libPfdQml.so')"`), rồi chạy `QT_QPA_PLATFORM=offscreen ./bin/librepilot-gcs` và grep log tìm `file not handled` / `context is not current`. Chỉ chạy `-version` thì chưa đủ để kết luận.

- Kết quả ngày 2026-10-27: dlopen 206 file OK, 5 lỗi, đúng là 5 plugin OSG lệch ABI ở mục 9 (`osgdb_curl`, `osgdb_gdal`, `osgdb_jp2`, `osgdb_ogr`, `osgdb_tiff`). GCS khởi động trong khoảng 2 giây, PFD load được, log không có `file not handled`, `context is not current` hay `cannot open shared object`. Bước này không mở Model View / Earth View, nên chưa nói được gì về việc chúng render đúng.
- GCS không thoát khi nhận SIGTERM. Dùng `timeout -k 5 40 ./librepilot-gcs` để tiến trình chắc chắn bị kill; `timeout 40` không có `-k` sẽ treo.

## 7. Cài đặt và chạy trên máy người dùng (Ubuntu 24.04 / WSL2)

```bash
sudo apt install -y libqt5core5t64 libqt5widgets5t64 libqt5qml5 libqt5quick5 \
  libqt5quickwidgets5 libqt5svg5 libqt5xml5t64 libqt5network5t64 libqt5multimedia5 \
  libqt5serialport5 libqt5sql5t64 libqt5printsupport5t64 libusb-1.0-0 libudev1 \
  libsdl1.2debian libgl1 libpulse0 \
  qml-module-qtquick2 qml-module-qtquick-controls qml-module-qtquick-controls2 \
  qml-module-qtquick-window2 qml-module-qtquick-layouts qml-module-qtqml-models2 \
  libgdal34t64
tar -xzf librepilot-gcs-with-osgearth-linux-x86_64.tar.gz
cd <thư mục giải nén>/bin && ./librepilot-gcs
```

- `qml-module-qtquick-controls` (QtQuick.Controls 1) là bắt buộc cho PFD: `TooltipArea.qml` dùng nó, thiếu sẽ báo `Type PfdView unavailable` và PFD trắng.
- `libgdal34t64`: osgEarth build trên 24.04 link với `libgdal.so.34`. Cài qua apt thay vì đóng gói kèm khoảng 102 thư viện phụ thuộc của GDAL.
- Thiếu mục PFD trong Options hoặc gadget hiện trống sau nhiều lần thử: file cấu hình người dùng có thể đã hỏng. Đổi tên để GCS tạo lại từ mặc định:

```bash
mv ~/.config/LibrePilot/"LibrePilot GCS.xml" ~/.config/LibrePilot/"LibrePilot GCS.xml.bak"
mv ~/.config/LibrePilot/"LibrePilot GCS.db" ~/.config/LibrePilot/"LibrePilot GCS.db.bak" 2>/dev/null
```

- Chạy đúng bản vừa giải nén. Có lần lỗi `libosgSim.so.145` vẫn xuất hiện sau khi đã sửa vì đang chạy bản cũ nằm trong `~/.local/share/Trash`.

### USB

Board dùng USB HID (plugin `opHID`, cần `/dev/hidraw*`). Cài udev rule có sẵn trong repo:

```bash
sudo cp package/linux/45-uav.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
# rút và cắm lại board
```

| Môi trường | Kết quả | Lý do |
|---|---|---|
| Ubuntu máy thật | Chạy | Kernel desktop có đủ driver HID. |
| VirtualBox Ubuntu | Chạy | VirtualBox giả lập USB host controller, kernel khách dùng driver HID bình thường. |
| WSL2 + usbipd | `lsusb` thấy board nhưng GCS không kết nối | Kernel WSL2 mặc định nhiều khả năng thiếu `CONFIG_HIDRAW`/`CONFIG_USB_HID`, nên không tạo `/dev/hidraw*`. Kiểm tra: sau `usbipd attach`, xem `dmesg | tail` và `ls /dev/hidraw*`. |
| Docker Desktop trên Windows | Như WSL2 | Container dùng chung kernel WSL2/Hyper-V. Docker bên trong VM VirtualBox thì dùng được với `--device=/dev/hidrawN`. |

Muốn dùng USB trên WSL2 phải build kernel WSL2 tuỳ chỉnh bật HIDRAW và USB HID, trỏ `.wslconfig` sang kernel đó. Cách ít tốn công hơn là dùng VirtualBox hoặc Ubuntu máy thật cho việc nối board.

## 8. Lịch sử debug (triệu chứng, nguyên nhân, cách sửa)

| # | Triệu chứng | Nguyên nhân | Sửa |
|---|---|---|---|
| 1 | Build dừng ở bước sinh version info | Script Python 2 | Port Python 3 + shim `python2` |
| 2 | File `.o` rỗng, link lỗi | Lệnh build bị ngắt giữa chừng do timeout của sandbox | `find build -name '*.o' -size 0 -delete`, build lại |
| 3 | `libUtils.so.1: cannot open shared object` | File nén chỉ có `bin/` | Nén đủ `bin lib share`, kiểm tra bằng cách giải nén ra thư mục mới |
| 4 | Không có 3D PFD | Build với `GCS_WITH_OSG=0` vì không có osgEarth | Thêm OSG 3.5.5 + build osgEarth 2.8 (mục 4) |
| 5 | Lỗi biên dịch osgQtQuick | osgEarth 2.8 chuyển class sang osgEarthUtil | Sửa include/namespace (mục 3) |
| 6 | `undefined symbol LogarithmicDepthBuffer::uninstall` | Thiếu `-losgEarthUtil` | Sửa `.pri`, xoá lib cũ để link lại |
| 7 | `libosgSim.so.145: cannot open` | Lib OSG không tìm thấy nhau | Copy vào `lib/librepilot-gcs`, `patchelf --set-rpath '$ORIGIN'` |
| 8 | `libgdal.so.34` thiếu | Máy người dùng chưa có GDAL runtime | `apt install libgdal34t64` |
| 9 | Options không có mục PFD | File `LibrePilot GCS.xml` hỏng | Đổi tên file cấu hình |
| 10 | Segfault `PfdQmlContext::modelFile()` khi lưu default settings | Context null khi QML lỗi | Kiểm tra null |
| 11 | PFD trắng, `Type PfdView unavailable` | Thiếu QtQuick.Controls 1 | `apt install qml-module-qtquick-controls` |
| 12 | `file not handled` với `.3ds`, `.png` | Thiếu `osgPlugins-3.5.5`, `osgdb_png` cần libpng12 | Đóng gói plugin + patchelf, build lại `osgdb_png` |
| 13 | `.earth` "file not handled" | Plugin osgEarth không tìm thấy lib lõi | `patchelf '$ORIGIN'` cho 53 plugin osgEarth |
| 14 | Plugin chỉ tìm thấy khi đặt `OSG_LIBRARY_PATH` tay | OSG tìm plugin theo `OSG_LIBRARY_PATH` | Đặt biến trong `main.cpp` |
| 15 | `context is not current`, Model/Earth View đen | `makeCurrentImplementation` không lấy lại context dùng chung | Mục 4.3 |
| 16 | Crash `createFramebufferObject` (shareGroup null) khi chuyển sang Model/Earth View | Không có context current sau traversal | Khôi phục context, fallback `makeCurrent(surface())` |
| 17 | Crash `QSGRenderer::renderScene` | OSG nhả context sau `renderingTraversals` | Lưu/khôi phục context trong `render()` |
| 18 | Hết crash nhưng view trống | Hướng thử "luôn tạo context riêng" làm OSG vẽ ngoài FBO | Quay lại dùng chung context (mục 4.3) |
| 19 | `Input/output error`, `getpwnam(root) failed` trong WSL giữa lúc `apt install` | Ổ C: chứa `ext4.vhdx` của distro hết chỗ | Chuyển distro sang ổ khác (`wsl --manage <distro> --move <thư mục>`), `wsl --shutdown`, `dpkg --configure -a`, cài lại gói bị cài dở |
| 20 | VM WSL tắt ngang khi build osgEarth, cuối file log toàn byte 0 | Build `-j6`, Windows hết bộ nhớ ảo ("Virtual Memory Minimum Too Low") vì C: gần đầy nên pagefile không lớn thêm được | Build `-j3`; xoá các file build ghi sau thời điểm VM tắt rồi build tiếp |
| 21 | Không có plugin osgEarth nào trong `lib64/` của bản cài osgEarth, bước 6.2 cũ không có gì để patch | osgEarth build từ source cài plugin vào `lib64/osgPlugins-3.5.5/` | `make gcs_bundle_osg` copy plugin từ thư mục đó (mục 6) |

Rút kinh nghiệm từ quá trình này: thêm log chẩn đoán trước khi đoán cách sửa, và chỉ kết luận "đã kiểm tra" sau khi dlopen/chạy thật từ bản giải nén mới.

## 9. Hạn chế còn lại

- Trang Welcome báo thiếu `QtQuick.XmlListModel`: chỉ ảnh hưởng hiển thị. Cài `qml-module-qtquick-xmllistmodel` để hết.
- Ảnh vệ tinh Google (`khms`) trả 404 vì API cũ. Các file `.earth` dùng readymap / ArcGIS cần Internet.
- Các plugin `osgPlugins-3.5.5` cho TIFF, JP2, GDAL, OGR và curl trong bản OSG build sẵn vẫn lệch ABI với 24.04, chưa build lại. Plugin osgEarth (kể cả driver GDAL của osgEarth) được build trên 24.04 nên không thuộc nhóm này.
- Trang Options của TelemetryMonitorGadget không mở được.
- Lỗi `TypeError` của QML khi thoát chương trình: vô hại.
- USB trên WSL2 (mục 7).
- Chưa có bản Windows. Hướng dẫn build Windows (MSYS2/MinGW) nằm ngoài nhánh này.

## 10. Việc nên làm tiếp

1. Xác nhận Model View và Earth View render đúng trên máy thật với bản build từ nhánh này.
2. Xong (2026-10-27): đóng gói bằng `make gcs_bundle_osg` (mục 6).
3. Build lại các plugin OSG bị lệch ABI (tiff, gdal, curl) từ source OSG 3.6.5 theo cách đã làm với `osgdb_png`, hoặc chuyển hẳn sang OSG 3.6.5 của apt.
4. Đưa bản nhị phân lên GitHub Releases.

## 11. Flight mode FollowMe với vision tracker qua UART

Nhánh `feature/visiontracker-followme`, tách từ `ubuntu24-qt515-osgearth` @ `70ba9764`.

### 11.1 Mục tiêu

Companion computer (camera + YOLO, project riêng) gửi vị trí bounding box của đối tượng qua UART. Trong flight mode **FollowMe**, FC tự giữ đối tượng ở giữa khung hình. Phi công không điều khiển gì: stick và ga đều bị bỏ qua, chỉ dùng switch để vào hoặc thoát mode.

Phạm vi: multirotor, bay trong nhà, không GPS, camera gắn cố định nhìn về trước. Chưa giữ khoảng cách tới đối tượng.

### 11.2 Luật điều khiển (`flight/modules/ManualControl/followmehandler.c`)

| Trục | Lệnh gửi vào Stabilization | Tính từ |
|---|---|---|
| Yaw | `RATE` | `Kp·e + Kd·ȧx`, kẹp bởi `MaxYawRate`. `ax = atan(X·tan(HFOV/2))`, `e = ax − (yaw hiện tại − yaw lúc nhận frame)`. |
| Thrust | `ALTITUDEVARIO` | Sai số dọc `pitch − atan(Y·tan(VFOV/2))`, trừ deadband, nhân `ElevationKp`, kẹp bởi `MaxClimbRate` và cửa sổ độ cao quanh lúc vào mode. Đổi tốc độ leo thành "ga ảo" bằng cách giải ngược đường cong vario của `altitudeloop.cpp`, ga ảo luôn ≥ 0,05 để không kích hoạt `CutThrustWhenZero`. |
| Roll / Pitch | `ATTITUDE` = 0 | Giữ thân ngang. |

- Góc chúc của camera bị triệt tiêu khi mục tiêu là giữ đối tượng ở giữa ảnh, vì vậy không có setting `CameraTilt`.
- Frame tới khoảng 20–30 Hz, còn handler chạy 50 Hz. Giữa hai frame, sai số yaw được bù bằng góc yaw đã quay kể từ frame cuối.
- Mất track (cờ valid = 0, confidence < `MinConfidence`, hoặc quá `TrackTimeout` không có frame mới): yaw rate = 0, giữ độ cao. Sau `LostTimeout`, nếu `LostAction = Descend` thì hạ với `LostDescentRate`, không giới hạn bởi cửa sổ độ cao (tức là hạ cánh).
- `FollowMeStatus` báo `State` (NoData / Lost / Tracking), các sai số, lệnh yaw rate, lệnh leo và độ cao so với lúc vào mode.

### 11.3 Giao thức UART

Đặc tả cho phía companion nằm ở `flight/modules/VisionTracker/PROTOCOL.md`. Tóm tắt: frame `A5 5A | id | len | payload | crc8` (CRC8 của UAVTalk). `0x01` TRACK (companion → FC), `0x81` FC_STATUS (FC → companion, 5 Hz).

Công cụ Python (không cần FC để thử parser):

- `python/visiontracker/vt_protocol.py`: encode/decode, dùng chung bảng CRC với FC.
- `python/visiontracker/vt_sim.py`: giả lập companion, gửi TRACK 20 Hz với mục tiêu dao động hình sin. Có `--static X Y`, `--drop EVERY:DURATION`, `--garbage PROB`, và in ra các FC_STATUS nhận được. Cần `pyserial`.

### 11.4 File thay đổi

| Nhóm | File |
|---|---|
| UAVO | `flightstatus.xml`, `flightmodesettings.xml` (thêm `FollowMe` vào cuối enum), `hwsettings.xml` (port `VisionTracker`, `PPM+VisionTracker`, `VisionTrackerSpeed`), `taskinfo.xml` (task `VisionTracker`), mới: `trackingdata.xml`, `followmesettings.xml`, `followmestatus.xml`; `UAVObjects.inc` của các board; `uavobjects.pro` của GCS. |
| Cổng UART | `pios_board_io.h/.c` (`PIOS_BOARD_IO_UART_VISIONTRACKER`), `revolution/firmware/pios_board.c` (Main, Flexi, Rcvr). |
| Module | `flight/modules/VisionTracker/` (`visiontracker.c`, `vtparser.c`, `inc/`), chỉ có trong `OPTMODULES` của Revolution. |
| Flight mode | `followmehandler.c`, `manualcontrol.c`, `inc/manualcontrol.h`. |
| An toàn | `armhandler.c`: không tự disarm do ga thấp khi đang ở FollowMe, vì ga bị bỏ qua. `sanitycheck.c`: FollowMe chỉ hợp lệ khi firmware có module VisionTracker, khung là multirotor và slot đó không bật GPSAssist. |
| Hiển thị | `Notify/inc/sequences.h`, `libraries/notification.c`, `UAVOMavlinkBridge.c` (`CUSTOM_MODE_GUIDED`), `uavohottbridge.c`, GCS `uav.js` (PFD hiện "FOLLOW ME"), `configrevohwwidget.cpp` (mỗi lúc chỉ một cổng được chọn VisionTracker). |
| Test | `flight/tests/visiontracker/` (11 test của parser, gồm cả vector tạo bằng Python). |
| Build | `make/scripts/version-info.py`, `flight/make/unittest.mk` (xem 11.6). |

### 11.5 Cấu hình và nâng cấp

ID của `FlightStatus`, `FlightModeSettings`, `HwSettings` và `TaskInfo` đổi vì enum có thêm phần tử. Firmware và GCS phải cùng bản build. Quy trình:

1. Bằng GCS cũ: File → Export UAV Settings.
2. Flash firmware mới bằng GCS mới.
3. Import lại settings rồi kiểm tra lại các trang cấu hình.

Cấu hình FollowMe:

1. Hardware: chọn `VisionTracker` cho Main, Flexi hoặc Rcvr (`PPM+VisionTracker` nếu vẫn dùng PPM trên cổng Rcvr), rồi reboot board. Tốc độ mặc định là 115200; đổi `HwSettings.VisionTrackerSpeed` trong UAVObject Browser nếu cần (trang Hardware chưa có ô chọn tốc độ cho chức năng này).
2. Input → Flight Mode Switch Settings: gán `FollowMe` cho một vị trí switch, không bật Assisted Control cho vị trí đó.
3. `FollowMeSettings` (UAVObject Browser): đặt `CameraFOV` đúng với camera thật; giữ gain mặc định cho lần bay đầu.
4. `AltitudeHoldSettings` phải đã được tinh chỉnh (FollowMe dùng vòng giữ độ cao của AltitudeVario).
5. Đặt `ManualControlSettings.FailsafeFlightModeSwitchPosition` về một vị trí an toàn. Nếu để -1 thì khi mất RC, drone vẫn ở FollowMe và tiếp tục bám theo, vì FollowMe không dùng stick.

Không arm được khi switch đang ở FollowMe. Quy trình bay: arm và cất cánh ở chế độ Stabilized (hoặc AltitudeVario), hover ổn định rồi mới gạt sang FollowMe.

### 11.6 Kiểm tra đã làm (2026-10-30, WSL2 Ubuntu 24.04)

- `make ut_visiontracker`: 11/11 test pass.
- Firmware build không lỗi, không có warning mới: `fw_revolution`, `fw_revonano`, `fw_sparky2`, `fw_discoveryf4bare`, `fw_ccf3d`, `fw_spracingf3evo`, `fw_simposix`. Chỉ `fw_revolution` có module VisionTracker; các board còn lại có handler FollowMe nhưng sanity check chặn chọn mode này.
- Kích thước `fw_revolution`: text 280 744 byte. Phần thêm vào khoảng 3 KB flash (`followMeHandler` 1,4 KB, task VisionTracker 0,5 KB, parser 0,4 KB, phần còn lại là code UAVO) và khoảng 110 byte RAM tĩnh. Trên heap còn có buffer UART 160 byte (chỉ khi cổng được chọn), stack 512 byte của task và dữ liệu của 3 UAVO mới.
- Để build được firmware trên Ubuntu 24.04 phải sửa hai chỗ không liên quan tới FollowMe:
  - `make/scripts/version-info.py` (bản port sang Python 3 ở mục 3): `sha1()` dùng sentinel `''` khi đọc file nhị phân nên lặp vô hạn ở bước FWINFO; `get_hash_of_dirs()` mở file bằng mode `'rU'` (bị bỏ từ Python 3.11), lỗi bị nuốt nên hash UAVO được tính trên tập rỗng. Bản GCS build ngày 2026-10-27 vì vậy mang hash UAVO rỗng; build lại GCS để hash khớp với firmware.
  - `flight/make/unittest.mk`: gtest 1.7.0 (`make gtest_install`) sinh warning mới với GCC 13 và hỏng vì `-Werror`; tắt `-Werror` riêng cho `gtest-all.o`.
- Toolchain: `make arm_sdk_install` (GNU Arm 10.3-2021.10, cài vào `tools/`, không cần sudo) và `make gtest_install`.

Chưa làm (cần phần cứng): Mốc A (bench test cổng UART với `vt_sim.py`), Mốc B (tháo cánh, kiểm tra chiều lệnh, không tự disarm, các tình huống mất track), tích hợp với companion thật và bay thử. Danh sách bước chi tiết nằm ở kế hoạch thực hiện (giai đoạn 3–7).

### 11.7 Hạn chế

- Không có giữ vị trí ngang (không GPS, chưa có optical flow), nên drone sẽ trôi theo gió và theo sai lệch trim. Bay trong không gian rộng, có dây giữ.
- `LostAction = Descend` hạ cánh rồi vẫn armed, motor quay ở `ThrustLimits.Min`. Phi công phải gạt ra khỏi FollowMe rồi disarm.
- Parser không quay lui: một frame bị cắt cụt làm mất thêm frame kế tiếp, frame sau đó mới nhận lại được.
- Độ trễ camera + YOLO chưa được bù (chỉ bù phần yaw quay giữa hai frame tới). Giữ `Kp`/`Kd` thấp nếu thấy dao động yaw.
