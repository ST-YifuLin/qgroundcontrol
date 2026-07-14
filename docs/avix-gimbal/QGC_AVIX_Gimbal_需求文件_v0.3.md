# QGC × AVIX 雲台整合 — 軟體需求文件 (SRD)

2026-07-13　v0.3　供 Claude Code 開發使用

## 1. 範圍

雲台運作獨立於飛控。開發分兩個階段：Phase 1 做雲台移動、鏡頭縮放控制與影像串流顯示；Phase 2 補上畫質參數設定。GPS/姿態橋接、AI 追蹤、熱像控制、SBUS 不在本文件範圍內（見附錄 A）。MAVLink 相機/雲台協定橋接（middleware/shim）目前不實作，但控制介面設計上會預留擴充點（見第 5 節控制來源仲裁），避免日後要做時重構核心邏輯。

## 2. 系統介面

| 介面 | 傳輸 | 位址 | 本文件涵蓋 |
|---|---|---|---|
| 雲台控制協定 | TCP，Port 2000 | 192.168.144.200 | Phase 1 |
| 相機 CGI | HTTP，Port 80 | 192.168.144.108 | Phase 2 |
| 影像串流 | RTSP | rtsp://admin:53373957@192.168.144.108:554/eo | Phase 1 |

## 3. 功能需求 — Phase 1（優先）

| 編號 | 需求 | 依據 | 驗收標準 |
|---|---|---|---|
| FR-1 | 使用者可透過 QGC 介面即時觀看雲台影像串流 | RTSP，rtsp://admin:53373957@192.168.144.108:554/eo | Fly View 能正常顯示畫面，斷線後可重新連線 |
| FR-2 | 使用者可控制雲台 Yaw/Pitch 移動 | Port 2000，0x32（速度模式）或 0x33（角度模式），擇一實作 | 指令送出後雲台實際角度／速度變化，並在合理延遲內（<500ms）反應 |
| FR-3 | 使用者可控制可見光鏡頭縮放（Zoom In/Out/Stop） | Port 2000，0x3A，Mode=0 | 縮放指令生效，畫面焦段隨之改變 |

## 4. 功能需求 — Phase 2（後補）

| 編號 | 需求 | 依據 | 驗收標準 |
|---|---|---|---|
| FR-4 | 使用者可讀取/修改影像編碼參數：解析度、Bitrate、Framerate、編碼格式（h264/h265） | CGI，GetMultimedia.cgi（讀）/ SetMultimedia.cgi（寫，需 Admin 權限） | 設定頁開啟時顯示目前參數；修改後串流實際套用新參數 |

> FR-4 目前僅需支援 CH0（EO 主頻道）；CH 參數化設計，之後加 CH3（IR）不需重構。

## 5. 架構決策

| 項目 | 決策 |
|---|---|
| 命名 | QGC 原生已有 GimbalController（MAVLink Gimbal Protocol v2 用），本專案一律加前綴 Avix，避免撞名：AvixGimbalLink / AvixGimbalProtocol / AvixGimbalController / AvixGimbalCameraCGI，目錄 `src/AvixGimbal/` |
| 定位 | AvixGimbalController 是 `QGCTool`（透過 QGCToolbox 註冊的常駐服務），不是 `Vehicle`——雲台不講 MAVLink、沒有 System ID/Component ID，不出現在載具清單裡 |
| 影像串流 | 沿用 QGC 既有 Settings → General → Video（Source=RTSP Video Stream），不新增程式碼 |
| 雲台控制（FR-2/3） | AvixGimbalLink（TCP framing/checksum）+ AvixGimbalProtocol（0x32/0x33/0x3A encode/decode）+ AvixGimbalController（QGCTool，暴露 QML property/slot） |
| 畫質設定（FR-4） | AvixGimbalCameraCGI（QNetworkAccessManager 包裝 CGI），獨立面板，不影響 Phase 1 進度 |
| 控制來源仲裁 | AvixGimbalController 內建 ControlSource enum（Native / MavlinkBridge，MVP 僅實作 Native），下指令前檢查來源是否為目前 active source，非 active 來源的指令一律丟棄；切換來源時先送停止指令再交接；velocity 模式下加 watchdog（逾時未收新指令且上次非零速度則自動送停止）。目的是預留日後 MAVLink shim 的介面，避免屆時 native 面板與 shim 同時對雲台下指令造成競態（AVIX 協定本身無法仲裁，只能在 QGC 端處理）。 |
| Threading | 沿用 Qt event loop signal/slot，不另開執行緒 |

## 6. 待確認事項

| # | 問題 | 影響 | 建議處理 |
|---|---|---|---|
| 1 | TCP 是否穩定支援常駐連線、idle 斷線時間、keep-alive | FR-2/3 | 實機寫 Python TCP client 測試 |
| 2 | Sequence 欄位與 0x01 ACK 無明確配對機制 | FR-2/3 | 先假設同步、單一 pending-command 佇列 |
| 3 | Checksum 涵蓋範圍（是否含 0xAB 0xCD 兩個 sync byte）未定義 | FR-2/3 | 用已知封包反推，寫進 unit test |
| 4 | SetMultimedia.cgi 的 Admin 認證是否與 RTSP 帳密共用 | FR-4 | 實機呼叫一次確認 |
| 5 | /eo 路徑是否對應 CGI CH0 | FR-1/FR-4 | 向 AVIX 確認或實測比對 |

## 7. 開發順序

1. FR-1：在 QGC 既有介面直接測試 RTSP URL，確認畫面可顯示（不寫程式）
2. 用 Python TCP client 對實機驗證 0x32/0x33/0x3A 封包與 checksum，處理待確認事項 #1~3
3. AvixGimbalLink + AvixGimbalProtocol + unit test
4. FR-2/FR-3：AvixGimbalController（含 ControlSource 仲裁 + watchdog）+ QML 控制面板
5. FR-4：確認待確認事項 #4 後，實作 AvixGimbalCameraCGI + 設定面板

## 附錄 A：暫緩範圍

GPS/姿態橋接（0x60）與 UART MAVLink 橋接、AI 追蹤（SetTracking/GetDetection）、熱像控制（0x3C/SetThermal.cgi）、SBUS、CGI 其餘設定（GetAdjust/SetAdjust/GetNetwork/SetNetwork/GetTime/SetTime）。細節見 `Gimbal_QGC_Integration_Spec.md`。

MAVLink 相機/雲台協定橋接（middleware/shim，讓既有 QGC 相機面板、搖桿雲台控制、任務規劃 ROI 指令可以操作 AVIX 雲台）：目前不做，原因見架構討論——工程量遠大於原生方案、且此雲台目前無真實飛控 MAVLink 連線可掛載，語意對不上的風險高。若日後真的需要（例如要支援多家雲台廠牌、或要做任務自動化），可以把 shim 寫成 AvixGimbalController 之上的一層薄轉接，透過第 5 節已預留的 ControlSource 介面接入，不需重構核心邏輯。

## 附錄 B：參考文件

| 檔案 | 內容 |
|---|---|
| QGC_5.0_Windows_Build_SOP.md | QGC v5.0.8 Windows 建置 SOP |
| Gimbal_QGC_Integration_Spec.md | AVIX 雲台 ICD 濃縮技術規格（完整版） |
| gimbal_icd.txt | AVIX ICD V1.12 純文字擷取版 |
| Gimbal Interface Control Document V1_12_EN.pdf | 原始 75 頁 PDF |
| CLAUDE.md | Claude Code 專案規則（程式碼風格、模組邊界、控制來源仲裁設計） |
