# QGC ↔ AVIX 雲台（Gimbal）整合開發交接文件

給 Claude Code 使用的技術規格摘要，來源為 AVIX Technology《Gimbal Control Module Interface Control Document V1.12》(2026-02-25)。此檔案已將 75 頁原文濃縮為實作所需的關鍵規格，避免每次都要重新解析整份 PDF。原始 PDF 一併放在同一資料夾（`Gimbal Interface Control Document V1_12_EN.pdf`）供需要時查證細節或截圖範例（1.1.1、4.2 節的封包範例圖未文字化，若需要請回查原檔）。

編譯/執行環境設定另見同資料夾 `QGC_5.0_Windows_Build_SOP.md`（Qt 6.8.3 msvc2022_64、VS Build Tools 2022、GStreamer 1.22.12、GPS driver GIT_TAG 修正等）。

---

## 0. 系統拓樸總覽

裝置分屬三個獨立通訊介面，**互相獨立、不可混用**：

| # | 介面 | 傳輸方式 | 位址 | 用途 |
|---|------|----------|------|------|
| 1 | 雲台二進位協定 | TCP/UDP，Port 2000 | `192.168.144.200` | 雲台角度/速度控制、相機快門/縮放/錄影、測距、狀態回報 |
| 2 | 相機 CGI/HTTP | HTTP，Port 80 | `192.168.144.108` | 影像品質、多媒體(解析度/碼率)、網路設定、AI 追蹤、回放檔案列表 |
| 3 | 飛控協定 | MAVLink 2 over UART | 115200bps 8N1 | 飛控 → 雲台：姿態/GPS 資訊（雲台端會 parse，不是給 QGC 消費，但 QGC 若要模擬/橋接需支援同一封裝） |
| 4 | SBUS（選用） | UART 100000bps 9E2 | — | RC 直接控制雲台，QGC 通常不需實作，除非要做地面站模擬搖桿 |

QGC 主要需要對接 #1（雲台控制/狀態）與 #2（相機參數/影像）。#3、#4 是雲台本身與飛控/遙控器之間的協定，QGC 若只是「透過 MAVLink 提供飛機姿態/GPS 給雲台」才會用到 #3 的封包格式；否則可視為不在 QGC 開發範圍。

---

## 1. 雲台二進位協定（Port 2000）

### 1.1 封包格式（Little-Endian）

| Offset | 欄位 | 型別 | 說明 |
|---|---|---|---|
| 0~1 | Header | uint8[2] | 固定 `0xAB 0xCD` |
| 2 | Protocol Version | uint8 | |
| 3 | From Device ID | uint8 | 見 1.2 |
| 4 | To Device ID | uint8 | 見 1.2 |
| 5 | Sequence | uint8 | |
| 6 | Length | uint8 | Data 長度 |
| 7 | Message ID | uint8 | |
| 8~(8+Length-1) | Data | byte[Length] | 依 Message ID 定義 |
| 8+Length | Checksum | uint8 | 見 1.4 |

**Device ID**：Gimbal = `0x01`，PC = `0xA0`

**Checksum**：對 Header 到 Data 結尾（不含 checksum 本身）逐 byte 累加取低 8 位（simple sum，非 CRC）：
```cpp
unsigned char crc = 0;
for (unsigned char i = 0; i < length; i++) crc += buffer[i];
```

### 1.2 Message ID 一覽

| ID | 名稱 | 方向 | 頻率 | 重點欄位 |
|---|---|---|---|---|
| 0x01 | Message Response | Gimbal→PC | 按需 | Data[0]=被回應的 Message ID |
| 0x31 | Follow Random Head | PC→Gimbal | 按需 | Data[0]: 0=不跟隨機頭 1=跟隨機頭 |
| 0x32 | Control Gimbal Velocity | PC→Gimbal | 按需 | float Yaw速度[-100~100°/s], float Pitch速度, uint8 Flags(bit0=跟隨機頭) |
| 0x33 | Transfer to Specify Angle | PC→Gimbal | 按需 | float Yaw角[-180~180], float Pitch角[-90~90], uint8 Flags(bit0=啟用Yaw,bit1=啟用Pitch,bit2=跟隨機頭) |
| 0x3A | Camera Control(可見光) | PC→Gimbal | 按需 | uint8 Mode(0=Zoom/1=Focus/2=錄影), uint8 ZoomCtrl(0停/1進/2出), uint8 FocusCtrl(0停/1遠/2近/3單次AF), uint8 RecSwitch(0停/1錄) |
| 0x3B | Camera Capture | PC→Gimbal | 按需 | 無資料欄位，觸發單張拍照 |
| 0x3C | Thermal Control | PC→Gimbal | 按需 | uint8 Mode(0=Zoom/1=Focus/2=調色盤), uint8 ZoomCtrl, uint8 FocusCtrl, uint8 Palette(1白熱..5熔岩,見原文對照) |
| 0x50 | Distance Measurement | PC→Gimbal | 按需 | uint8 Switch(0關/1開) |
| 0x60 | UAV Status Return | PC→Gimbal | **連續 10Hz** | float 經度(1e-7°), float 緯度(1e-7°), float 海拔(m), float 航向(0~360°), float 水平速度(m/s), int32 距home距離(m), float 電池電壓(V), uint8 衛星數, uint8 保留(預設1)。**僅在 UART 飛控連線不可用時用此通道補送姿態/位置資訊** |
| 0xC0 | Get Control IP | PC→PTZ | 按需 | UDP broadcast 255.255.255.255:2000 查詢控制器 IP |
| 0xC1 | Return/Change Control IP | PC→PTZ | 按需 | char[4] IP, char[4] Netmask, char[4] Gateway |
| 0xC2 | Change Control IP Response | PTZ→PC | 按需 | 確認回覆 |
| 0xF0 | Gimbal Status Return | PTZ→PC | **連續 10Hz** | float Roll[-90~90°], float Pitch[-90~90°], float Yaw[-180~180°], int16 可見光變焦(0.1x), int16 熱像變焦(0.1x), float 目標距離(m), float 目標海拔(m), int32 目標經度(1e-7°), int32 目標緯度(1e-7°), uint8 資料接收頻率(Hz), uint8 DataFlag(bit0 IMU校正 bit1 Pitch正常 bit2 Roll正常 bit3 Yaw正常 bit4 PTZ模式0自由1跟機頭 bit5 測距成功) |
| 0xF1 | Gimbal Version Return | PTZ→PC | 按需 | uint8 韌體版本(高4bit主/低4bit次), int32 韌體日期(YYMMDD格式如251001), char[16] 雲台型號, uint16 雲台SN |
| 0xF2 | Get Gimbal Version | PC→PTZ | 按需 | 觸發 0xF1 回應 |

> 注意：Data 欄位為多 byte 數值時皆為 little-endian，float 為 IEEE754 32-bit。原文件對 0x01、0xC0/C1/C2、0xF2 部份欄位表格留空（僅有標題列），實作時建議先以封包截圖範例（1.1.1節）或實機抓包驗證再定案。

---

## 2. 相機 CGI/HTTP 協定（Port 80, 192.168.144.108）

明文查詢字串，`http://192.168.144.108/<Endpoint>.cgi?<params>`，成功回應 HTTP 200（body 常為 `200 is OK` 或 `key=val<br>` 純文字列表）。無需驗證，除非啟用才走 Basic Auth。

| Endpoint | 方法/權限 | 用途 | 關鍵參數 |
|---|---|---|---|
| SetPTZ.cgi | PC→Camera / None | 可見光變焦/對焦 | Zoom_Ratio(1~42), PTZSpeed(1~9), AF_Mode(0/1), FocusMode(0自動/1手動/2按壓AF), Dir(ZoomIn/ZoomOut/ZoomStop/FocusFar/FocusNear/FocusStop) |
| SetThermal.cgi | / None | 熱像變焦/調色盤 | colorpalette(1彩虹..5熔岩), zoom_ratio(1~16) |
| GetAdjust.cgi | / None | 讀取影像品質 | 回傳 Brightness/Contrast/Hue/Saturation/Sharpness/AntiFog/i_BLC/Denoise_3D/Denoise_2D/DayNight_Day/dis |
| SetAdjust.cgi | / Admin | 設定影像品質 | 同上（無Sharpness，多AGC 6~96），例：`?Brightness=18&Hue=14` |
| GetNetwork.cgi | / None | 讀取網路設定 | Ipaddr/Netmask/Gateway/Dns1/Dns2 |
| SetNetwork.cgi | / Admin | 設定網路 | 同上，例：`?Ipaddr=192.168.2.60&Gateway=192.168.2.1` |
| GetMultimedia.cgi | / None | 讀取串流參數 | `?CH={0~3}` → Resolution/Fps/Bitrate/Mode(h264/h265) |
| SetMultimedia.cgi | / Admin | 設定串流參數 | CH(0=EO主/1=EO副1/2=EO副2/3=IR主), Resolution, Fps(5~30), Bitrate(256~4000), Mode |
| GetImage.cgi | / Admin | 取單張快照 | `?CH=&Resolution=`，回傳影像本體（不存SD卡） |
| GetPlayBack.cgi | / Admin | 列出錄影/照片檔案 | `?Time=YYYYMMDD` 或 `?List=YYYYMMDD_HHMMSSr` |
| GetSRTList.cgi | / Admin | 列出SRT字幕(含GPS/PTZ metadata) | `?List=YYYYMMDD_HHMMSSr` |
| GetTime.cgi / SetTime.cgi | / None,Admin | 讀寫相機時間 | TimeZone(分鐘×-1, GMT+8=-480), UseNtp, Ntp_Updata, NtpIP, Localtime(unix timestamp) |
| SetAI.cgi | / None | 開關AI辨識 | Trigger(0/1)（部份型號支援） |
| SetTracking.cgi | / None | 設定追蹤目標 | ID(-1停止/1~65535), 或手動框選 target_x1/y1/x2/y2(0~8191), enable=0停止 |
| GetDetection.cgi | / None | 取得目前追蹤框位置(~10Hz) | 回傳 `ID,Type,x1,y1,x2,y2` 列表，Type見AI物件對照表 |

AI 物件類別（Type）：0=Person, 1=Bicycle, 2=Car, 3=Motorcycle, 7=Truck, 99=Tracking

影像品質參數（Brightness/Contrast/Hue/Saturation 等）在 GetAdjust/SetAdjust 為 0~31 整數值，但另有一份「檔位對照表」(2.3節)把每個整數映射到 -4~+4 相對檔位，UI 若要做成滑桿建議直接用 0~31 raw range，不必重建對照表。

---

## 3. 飛控協定（UART, MAVLink 2, 115200 8N1）

標準 MAVLink 2 封裝，Header=`0xFD`。System ID 預設 1，Component ID 預設 154。僅用到 3 個訊息：

| Msg ID | 名稱 | 方向 | 頻率 | CRC_EXTRA |
|---|---|---|---|---|
| 0 | HEARTBEAT | Gimbal→FC | 1Hz | 50 |
| 30 | ATTITUDE | FC→Gimbal | 20Hz | 39 |
| 33 | GLOBAL_POSITION_INT | FC→Gimbal | 10Hz | 104 |

這 3 個訊息與標準 MAVLink common.xml 定義完全相同（HEARTBEAT/ATTITUDE/GLOBAL_POSITION_INT），因此**若 QGC 端要產生這些封包，直接複用 QGC 內建的 MAVLink library 即可，不需要另外實作**——只需確保 CRC_EXTRA 常數與上表一致（QGC 內建 mavlink headers 本來就會算，可直接比對驗證）。Checksum 為標準 CRC-16/MCRF4XX (X.25)，計算範圍為 Header 之後所有 byte + CRC_EXTRA。

---

## 4. SBUS（選用，QGC 通常不需實作）

UART 100000bps，9-bit even parity，2 stop bits，100Hz。25-byte frame：Header(0x0F) + 16ch×11bit packed(22 bytes) + Flag(bit3 Failsafe/bit2 FrameLost/bit1 Ch17/bit0 Ch18) + Footer(0x00)。僅在需要用地面站模擬RC訊號直控雲台時才需要，一般整合可略過。

---

## 5. QGC 端建議整合架構

以下based on QGC 5.0 既有架構（`LinkManager`/`LinkInterface`、`MultiVehicleManager`、`FirmwarePlugin`/`QGCCorePlugin` plugin 體系），實際檔案路徑需 Claude Code 在本機 repo 內確認，這裡只給方向：

1. **雲台二進位協定（Port 2000）**：建議實作為獨立的 `GimbalController`/`GimbalLink` 類別，內部用 `QTcpSocket`/`QUdpSocket` 直連，不必硬塞進 MAVLink LinkInterface 體系（因為封包格式完全不同）。可放在新目錄如 `src/Gimbal/`，仿照現有 `src/Camera/` 或 `src/Sensors/` 的 QGCTool 註冊方式掛進 `QGCToolbox`，並用 QML 暴露屬性給 Fly View 的 UI（角度/縮放/錄影按鈕）。0xF0/0x60 這種 10Hz 週期訊息建議用 `QTimer` 或收到封包後直接 emit signal 更新 QML binding。

2. **相機 CGI（Port 80）**：`QNetworkAccessManager` 發 HTTP GET 即可，回應是純文字 `key=value<br>` 格式，parse 很單純（split on `<br>` 再 split on `=`）。可與上面的 `GimbalController` 共用一個 QGCTool，或拆成 `GimbalCameraController`。

3. **UI**：QGC 5.0 的 Fly View 已有 Video/Camera 相關 QML（如 `PhotoVideoControl.qml`、`CameraPage.qml` 這類，實際檔名以 repo 為準），優先評估能否擴充既有元件而非整個重寫，尤其變焦/對焦/錄影這些操作邏輯上跟既有相機控制頁面高度重疊。

4. **飛控 MAVLink 橋接（若需要）**：如果 QGC 要「主動生成」HEARTBEAT/ATTITUDE/GLOBAL_POSITION_INT 從飛機轉送到雲台 UART，可掛在既有 `MAVLinkProtocol`/`Vehicle` 收到對應訊息時，另開一個 serial port 轉發，不需重新實作 MAVLink 序列化。

5. **設定/持久化**：雲台 IP/Port 若要可設定，比照既有 `SettingsManager` + `Fact` 系統加一組 QGCGimbalSettings，會比硬編碼更符合 QGC 慣例。

---

## 6. 建議開發順序

1. 先用 Python/簡單 CLI 腳本（不進 QGC）對真實雲台驗證封包格式與 checksum，尤其 0x60/0xF0 這種週期性封包，確保理解無誤後再寫進 C++。
2. `GimbalController` TCP/UDP 收發 + checksum + Message Response(0x01) ACK 處理。
3. 角度/速度控制（0x32/0x33）+ 相機控制（0x3A/0x3B/0x3C）— 這是最基本可用的手動控制功能。
4. 狀態回報解析（0xF0 10Hz）→ 綁定 QML 顯示。
5. CGI 相機參數（GetAdjust/SetAdjust/GetMultimedia/SetMultimedia）。
6. UAV Status Return（0x60）— 若飛控 UART 直連雲台已經在跑，這步在 QGC 端可能不必做（是雲台自己接飛控，不經過 QGC）。
7. AI 追蹤/偵測（SetTracking/GetDetection）留最後，屬於進階功能且部份雲台不支援。

每步都建議搭配單元測試（尤其 checksum/CRC 計算），因為封包格式錯一個 byte offset 就整包壞掉，肉眼從 log 很難抓。
