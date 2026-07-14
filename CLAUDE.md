# AVIX Gimbal Integration — 專案規則（QGroundControl v5.0.8）

本檔案供 Claude Code 每次在此 repo 工作時自動載入。開發 AVIX 雲台整合功能時，請遵守以下規則。

## 0. 背景文件

開發前請先讀（放在 `docs/avix-gimbal/`，若實際路徑不同請告知）：

- `QGC_AVIX_Gimbal_需求文件_v0.3.md` — 功能需求（FR-1~FR-4）、架構決策、待確認事項、開發順序（以此份 .md 為準，.docx 版本僅供人閱讀）
- `Gimbal_QGC_Integration_Spec.md` — AVIX ICD 完整技術規格
- `gimbal_icd.txt` — 原廠 ICD 純文字擷取版
- `QGC_5.0_Windows_Build_SOP.md` — 本機建置環境設定

## 1. 程式碼風格 — 必須遵守既有專案規範

- 開始寫任何 C++/QML 之前，先讀 repo 根目錄的 `CODING_STYLE.md` 與 `CodingStyle.h`（若專案內路徑不同，先用 Glob/Grep 找到實際位置，不要用記憶裡的舊路徑）。
- 命名慣例、大括號風格、header guard、include 順序等一律照既有規範，不要套用自己預設的風格。
- 提交前跑 `clang-format`（repo 內應有 `.clang-format`）與 `pre-commit`（若有設定），確保格式跟既有程式碼一致。
- QML 部分比照既有檔案的屬性宣告順序、signal/slot 命名慣例（可參考 `src/Camera/` 或 `src/FlightDisplay/` 底下現有元件）。

## 2. 模組邊界 — 新功能全部關在獨立目錄

- 新程式碼一律放 `src/AvixGimbal/`，不要散落到其他既有目錄。
- 類別命名一律加 `Avix` 前綴：`AvixGimbalLink`、`AvixGimbalProtocol`、`AvixGimbalController`、`AvixGimbalCameraCGI`。
  **原因：** QGC 原生已有一個 `GimbalController` 類別（MAVLink Gimbal Protocol v2 用），本專案的雲台走完全獨立的 TCP 二進位協定，兩者邏輯上必須保持獨立，命名絕對不能撞在一起。
- QML 面板獨立新增（例如 `AvixGimbalControl.qml`、`AvixGimbalCameraSettings.qml`），直接 bind 對應 Controller 的 property，不要塞進既有 MAVLink camera 元件的 class 階層。

### 2.1 控制來源仲裁（現在就要實作，即使 MAVLink shim 還沒做）

`AvixGimbalController` 是唯一持有 TCP 連線、真正對雲台下指令的角色。就算現在只有 native QML 面板這一個輸入來源，也要從一開始就內建「控制來源」的狀態機制，不要等以後真的要做 MAVLink shim（見附錄 A）才回頭改：

- 加一個 `ControlSource` enum（`Native` / `MavlinkBridge`），存成 `AvixGimbalSettings` 底下的 Fact，可持久化、可從 Settings 或 Fly View 切換。MVP 階段只實作 `Native` 一種值。
- 所有下指令的入口（`setVelocity()`／`setAngle()`／`setZoom()`）都要先檢查呼叫來源是否等於目前的 `activeControlSource`，不是的話直接丟棄該指令並記 log，不送到雲台。UI 同步處理：非 active 來源的面板控制項要灰掉。
- 切換來源時要「乾淨交接」：先送停止/中性指令（速度模式下 Yaw/Pitch 速度歸零）給雲台，確認後才把 `activeControlSource` 切過去，避免舊來源殘留的非零速度指令繼續生效。
- 加 watchdog：velocity 模式下，若目前 active source 超過一個逾時（建議 300~500ms）沒有送新指令，且上一次是非零速度，自動送一次停止指令，避免控制端斷線/當掉時雲台持續轉動。

這樣設計是為了防止「native 面板」跟「未來可能的 MAVLink shim」同時對雲台下指令造成的競態問題（AVIX 協定的 Sequence/ACK 配對本來就不明確，不能靠協定本身仲裁，只能在 QGC 端做）。現在把介面留好，之後如果真的要做 shim，只需要照這個介面接進來，不需要重構核心邏輯。

## 3. 修改既有檔案 — 能不動就不動

只有在下列情況才允許修改既有檔案，且改動範圍盡量降到最低（單行註冊/掛載為原則，不要順手重構或格式化整個檔案）：

- `QGCToolbox.h` / `QGCToolbox.cc`：註冊新的 `AvixGimbalController` QGCTool
- `CMakeLists.txt`（對應層級）：加入 `src/AvixGimbal/` 的原始檔與新 QML 檔案
- Fly View 對應的 QML（若要把新面板掛進既有畫面）：只加掛載新元件的那幾行，不要動既有元件邏輯
- `SettingsManager` 相關檔案：註冊新的 `AvixGimbalSettings` group

除此之外的既有檔案（尤其 `src/Camera/`、既有 `GimbalController`、`src/Comms/`、`src/GPS/` 等）一律不要修改。如果發現「好像需要改」，先停下來說明原因，不要直接動手。

## 4. 開發順序（照 SRD 第 7 節）

1. 在 QGC 既有 Settings → General → Video 直接測試 RTSP URL（`rtsp://admin:53373957@192.168.144.108:554/eo`），確認畫面可顯示（FR-1，不寫程式）
2. 寫一個獨立的 Python TCP client（不進 QGC），對實機驗證 0x32/0x33/0x3A 封包格式與 checksum，同時處理下方「待確認事項」#1~3
3. `AvixGimbalLink` + `AvixGimbalProtocol` + unit test（先讓底層收發/編解碼正確，再往上蓋）
4. FR-2/FR-3：`AvixGimbalController` + QML 控制面板（移動＋縮放）
5. FR-4（後補）：確認待確認事項 #4 後，實作 `AvixGimbalCameraCGI` + 畫質設定面板

## 5. 待確認事項（開發中遇到請對照，不要自行假設）

| # | 問題 | 影響 |
|---|---|---|
| 1 | TCP 是否穩定支援常駐連線、idle 斷線時間、keep-alive | AvixGimbalLink 重連邏輯 |
| 2 | Sequence 欄位與 0x01 ACK 無明確配對機制 | 先假設同步、單一 pending-command 佇列 |
| 3 | Checksum 涵蓋範圍（是否含 0xAB 0xCD 兩個 sync byte）未定義 | 用已知封包反推，寫進 unit test |
| 4 | SetMultimedia.cgi 的 Admin 認證是否與 RTSP 帳密共用 | FR-4 開發前需實機確認 |
| 5 | `/eo` 路徑是否對應 CGI CH0 | FR-1/FR-4 |

## 6. 單元測試

Checksum、封包 encode/decode 這類「錯一個 byte offset 就整包壞掉」的邏輯，一律搭配 unit test，不要只靠肉眼看 log 驗證。測試框架比照 repo 內既有慣例（先確認 QGC 用的是哪套，例如 Qt Test）。
