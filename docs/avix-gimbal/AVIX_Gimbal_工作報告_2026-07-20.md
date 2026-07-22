# AVIX 雲台整合 — 工作報告

日期：2026-07-20
範圍：QGroundControl v5.0.8，AVIX 雲台 TCP 控制協定整合（FR-1~FR-3）

---

## 1. 軟體拓樸

新增的 `src/AvixGimbal/` 模組是一條獨立於既有 MAVLink 體系的路徑：

- `AvixGimbalController`（`Q_APPLICATION_STATIC` singleton，App 生命週期，不掛在任何 `Vehicle` 底下）在 `QGCApplication::init()` 註冊給 QML 使用
- `AvixGimbalControl.qml` 面板掛在既有 `FlyViewVideo.qml`，透過 `Q_INVOKABLE` 呼叫 `AvixGimbalController`
- `AvixGimbalController` 持有 `AvixGimbalLink`（`QTcpSocket`，接 `192.168.144.200:2000`），`AvixGimbalLink` 用 `AvixGimbalProtocol`（純 encode/decode，不做 I/O）組封包
- 目前的 `ControlSource` 設定（`Native`/`MavlinkBridge`）存成 `AvixGimbalSettings` 底下的 Fact，走既有 `SettingsManager` 機制持久化
- 相機影像/CGI（`192.168.144.108`，RTSP + Port 80）沿用 QGC 既有功能，這次完全沒有動到

**關鍵設計決策：**

| 項目 | 決策 | 原因 |
|---|---|---|
| 常駐機制 | `Q_APPLICATION_STATIC` singleton | 這版 QGC（v5.0.8）沒有 `QGCToolbox`，改用這個 repo 目前的既有慣例（比照 `SettingsManager`） |
| 定位 | 不是 `Vehicle` 的一部分 | 雲台走獨立 TCP 協定、不講 MAVLink，跟任何飛控連線狀態無關 |
| 控制來源仲裁 | `ControlSource` enum（Native / MavlinkBridge） | 預留未來 MAVLink shim 介面，MVP 只實作 Native |
| Watchdog | Velocity 模式 400ms 逾時自動送停止 | 防止控制端斷線/當掉時雲台持續轉動 |

**深度整合 vs. 獨立模組（Plugin）方式比較：**

這次整個 AVIX 雲台邏輯關在 `src/AvixGimbal/` 一個目錄裡，跟既有程式碼的接觸點壓到最低（只有幾行註冊/掛載），性質上比較接近「外掛」而非「深度整合」。兩種路線的取捨：

| 面向 | 深度整合（併入既有 Vehicle/FactSystem/Joystick） | 獨立模組（目前採用的 Plugin 方式） |
|---|---|---|
| 開發風險 | 高——直接改動既有 `GimbalController`/`Camera`/`Joystick` 等核心程式碼，容易引入回歸問題 | 低——新程式碼關在 `src/AvixGimbal/`，既有邏輯完全不動 |
| 與上游 QGC 同步 | 難——改動點分散在多個既有檔案，之後跟上游版本 merge 容易衝突 | 易——只有少數幾行單行註冊，衝突面很小 |
| 單元測試 | 難——邏輯跟 `Vehicle`/MAVLink 生命週期綁在一起，測試需要建構完整或模擬的 Vehicle 環境 | 易——Protocol/Link/Controller 三層可各自獨立測試，不需要真的接 Vehicle |
| 即時狀態呈現 | 有優勢——可直接沿用 `FactSystem`/`Vehicle` 既有的即時資料顯示框架 | 目前沒有——`0xF0` 狀態資料收到後沒有現成管道顯示，需要另外搭一套小型呈現機制 |
| 搖桿/任務規劃整合 | 有優勢——原生銜接既有 `Joystick`、`MissionManager` ROI 指令等既有功能 | 做不到——要接這些就必須跨出目前的模組邊界，屆時「隔離」會被打破 |
| UI 一致性/使用者感受 | 較一致——可以直接融入既有相機/雲台面板的操作邏輯與外觀 | 目前是獨立浮動面板，跟既有 UI 是兩套視覺語言 |
| 未來 MAVLink shim 擴充性 | 較差——邏輯若跟既有 MAVLink 體系深度綁定，之後要抽出獨立協定反而更難 | 較好——`ControlSource` 介面已經預留，之後加 shim 不需重構核心邏輯 |

---

## 2. 原版 QGC 改了什麼

**新增檔案（全部關在獨立目錄，未動既有邏輯）：**

```
src/AvixGimbal/                       ← 全新目錄
  AvixGimbalProtocol.h/.cc            封包 encode/decode + checksum（純函式，已有 unit test）
  AvixGimbalLink.h/.cc                TCP 連線（含重連 timer）
  AvixGimbalController.h/.cc          singleton controller，ControlSource 仲裁 + watchdog
  AvixGimbalControl.qml               QML 控制面板
  CMakeLists.txt

src/Settings/
  AvixGimbalSettings.h/.cc            新增（比照既有 GimbalControllerSettings 掛法）
  AvixGimbal.SettingsGroup.json       新增

test/AvixGimbal/
  AvixGimbalProtocolTest.h/.cc        新增（8 個 test case）
  CMakeLists.txt
```

**修改既有檔案（都是單行註冊/掛載，沒有重構或改動旁邊邏輯）：**

| 檔案 | 改動內容 |
|---|---|
| `src/CMakeLists.txt` | 加 `add_subdirectory(AvixGimbal)`、`target_link_libraries` 加 `AvixGimbalModule` |
| `src/FlightDisplay/FlyViewVideo.qml` | 加一行 `AvixGimbalControl { anchors.fill: parent; z: 1 }`，緊接既有 `OnScreenGimbalController` 之後 |
| `src/Settings/SettingsManager.h`/`.cc` | 掛 `AvixGimbalSettings`（forward-declare + `Q_MOC_INCLUDE` + `Q_PROPERTY` + `init()` 內 `new`） |
| `src/Settings/CMakeLists.txt` | 加 `AvixGimbalSettings.cc`/`.h` 到 `target_sources` |
| `src/QGCApplication.cc` | 加一行 `AvixGimbalController::registerQmlTypes();`（QML 端要能用到這個 singleton 唯一的機制） |
| `qgroundcontrol.qrc` | 加一行掛 `AvixGimbal.SettingsGroup.json` 資源 |
| `test/CMakeLists.txt`、`test/UnitTestList.cc` | 註冊 `AvixGimbalProtocolTest` |

**沒有動過的地方**：既有 `src/Gimbal/GimbalController`（MAVLink Gimbal Protocol v2）、`src/Camera/`、`src/Comms/`、`src/GPS/` 完全沒有修改，兩套雲台邏輯（AVIX TCP vs MAVLink）互不相干。

---

## 3. 文件缺漏：Protocol Version

原廠 ICD（`gimbal_icd.txt`）對封包第 2 個 byte「Protocol Version」只有欄位名稱、沒有定義數值（原文封包範例圖是截圖，未轉成文字）。一開始假設 `0x00`，導致雲台完全靜默忽略所有 FR-2/FR-3 指令，連線與 checksum 都正常但雲台毫無反應。

用 Wireshark 抓雲台自己送出的 `0xF0` 狀態封包逐 byte 反推，確認實際值是 `0x01`，改掉後雲台立即正常回應。**這個值是從雲台單方面封包觀察反推，非原廠書面確認**，韌體更新後可能需要重新驗證。同一輪也順便確認了 checksum 涵蓋範圍（含 sync byte）與我們的實作一致。

---

## 4. ICD 文件有提到、但目前尚未實作的功能

| Message / CGI | 功能 | 狀態 |
|---|---|---|
| `0x31` Follow Random Head | 雲台跟隨機頭開關 | 未實作，**已列入代辦**（見第5節，動工前需先確認姿態橋接缺口） |
| `0x33` Transfer to Specify Angle | 角度模式移動 | Controller/Protocol 有實作，UI 目前只有 `Center`（Yaw/Pitch=0）用到，沒有開放任意角度輸入 |
| `0x3A` Camera Control - Focus (Mode=1) | 對焦（自動/手動/遠近） | 未實作，目前只送 Mode=0（Zoom） |
| `0x3A` Camera Control - Rec (Mode=2) | 錄影開關（存在雲台自己的 SD 卡） | 未實作 |
| `0x3B` Camera Capture | 拍照（存在雲台自己的 SD 卡） | 未實作 |
| `0x50` Distance Measurement | 雷射測距開關 | 未實作 |
| `0xF0` Gimbal Status Return | 雲台狀態回報（10Hz，含 Roll/Pitch/Yaw/Zoom/DataFlag 等） | **只有收、沒有用**——封包有正確 decode，但欄位內容目前全部丟棄，UI 看不到雲台目前實際狀態 |
| `0xF1`/`0xF2` Gimbal Version | 查詢韌體版本/型號/SN | 未實作 |
| `0xC0`/`0xC1`/`0xC2` Control IP | 查詢/變更雲台控制 IP | 未實作，且 ICD 原文這幾個訊息的欄位表格本身也是空的 |
| CGI 全部（Port 80，FR-4/Phase 2） | 畫質設定、網路設定、AI 追蹤等 | 整個都還沒開始，需先解待確認事項 #4（Admin 認證） |

**明確排除、非缺口**（SRD 附錄A，刻意不做）：`0x60` GPS/姿態橋接、AI 追蹤（SetTracking/GetDetection）、熱像控制（0x3C/SetThermal.cgi）、SBUS。

---

## 5. 代辦事項

1. 確認 gimbal 待補功能（第4節列出的缺口，逐項確認優先順序，尤其跟隨機頭與 `0xF0` 狀態資料利用）
2. QGC 架構變更：`custom/` 客製化建置架構
3. 換品牌圖示
4. Android 版本（AirLink 3）
5. 用 Qt Designer 將操控介面優化
