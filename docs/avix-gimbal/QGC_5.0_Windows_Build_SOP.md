## QGroundControl 5.0（v5.0.8）Windows 原始碼建置報告與 SOP

**建置環境：** Windows 11
**QGC 版本：** v5.0.8（正式 tag）
**文件性質：** 整合實際安裝除錯歷程,作為之後重裝/交接/troubleshooting 的參考

---

### 一、專案背景

目標是在本機從原始碼建置 QGroundControl 5.0,並具備後續修改、編譯、除錯程式碼的能力。過程中依序排除了以下幾類問題：開發工具鏈缺失、Qt Creator Kit 設定不完整、Qt 模組缺漏、第三方相依套件（GStreamer）版本與路徑衝突、上游相依套件（PX4-GPSDrivers）版本漂移導致的編譯失敗,以及最後執行期的 DLL 載入問題。

---

### 二、最終驗證可用的環境配置

| 項目 | 設定值 |
|---|---|
| Git | Git for Windows(含 Git Bash） |
| 原始碼路徑 | `~/qgroundcontrol`（對應 `C:\Users\<user>\qgroundcontrol`） |
| 原始碼版本 | tag `v5.0.8`（非 master 分支） |
| Qt 版本 | 6.8.3,元件 `msvc2022_64` |
| Qt 附加模組 | Additional Libraries **全部勾選**（含 Core5Compat、TextToSpeech 等) |
| 編譯器 | Visual Studio **Build Tools 2022** 17.14.37411.7（amd64） |
| CMake | VS2022 BuildTools 內附版本 3.31.6，路徑：`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` |
| Generator | Ninja |
| Qt Creator Kit 名稱 | `QCP (Default)` |
| Build 目錄 | `build/QCP_Debug` |
| GStreamer 版本 | **1.22.12**（版本需完全一致，非「至少」） |
| GStreamer 安裝路徑 | `C:\QGC_gstreamer\1.0\msvc_x86_64`（QCP 專屬，與其他 GCS 隔離） |
| GStreamer 建置期環境變數 | Kit → Build Environment → `GSTREAMER_1_0_ROOT_MSVC_X86_64` |
| GStreamer 執行期環境變數 | Run Settings → Run Environment → PATH 已含 `C:\QGC_gstreamer\1.0\msvc_x86_64\bin`（建置流程已自動帶入，非崩潰主因） |
| 已知上游 bug 修正 | `src/GPS/CMakeLists.txt` 的 `GIT_TAG main` 改為固定 commit `caf5158061bd10e79c9f042abb62c86bc6f3e7a7` |
| 執行狀態 | ✅ 已成功啟動主畫面（Debug 模式驗證通過，正常顯示首次啟動的 Measurement Units 設定畫面） |

---

### 三、標準作業程序（SOP）

#### 步驟 1：安裝 Git

前往 [git-scm.com/download/win](https://git-scm.com/download/win) 下載安裝，全程預設值即可（會附帶安裝 Git Bash）。

#### 步驟 2：取得原始碼並切換到正確版本

```bash
cd ~/Documents
mkdir dev && cd dev
git clone --recursive -j8 https://github.com/mavlink/qgroundcontrol.git
cd qgroundcontrol
git fetch --tags
git checkout v5.0.8
git submodule update --init --recursive --force
```

> **重要：** 預設 `git clone` 會停在 `master`（持續開發中的分支），Qt 版本需求會不斷變動。務必明確切到 `v5.0.8` 這個穩定 tag。

#### 步驟 3：安裝 Qt 6.8.3

用 [Qt Online Installer](https://www.qt.io/download-qt-installer-oss) 安裝：

- 選 Custom Installation
- 勾選 **Qt 6.8.3 → MSVC 2022 64-bit**
- **Additional Libraries 全部勾選**（避免之後一個一個補裝 Core5Compat、TextToSpeech 等模組）
- 全選 Qt Tools 底下的 CMake、Ninja

#### 步驟 4：安裝 Visual Studio Build Tools 2022

> **常見誤區：** VS Code 不等於 Visual Studio，不含 C++ 編譯器；若機器上裝的是更新的 Visual Studio（如 Insiders 預覽版），其工具鏈可能跟 Qt 官方預編譯的 MSVC2022 版本不相容。

前往 [visualstudio.microsoft.com/downloads](https://visualstudio.microsoft.com/downloads/) 下載 **Build Tools for Visual Studio 2022**（若官網 500 錯誤，改用直連 `https://aka.ms/vs/17/release/vs_buildtools.exe` 或 `winget install --id Microsoft.VisualStudio.2022.BuildTools -e`）。

安裝時勾選工作負載：

```
☑ 使用 C++ 的桌面開發 (Desktop development with C++)
```

裝完重新啟動電腦。

#### 步驟 5：Qt Creator 設定 Kit

1. 開啟專案 `CMakeLists.txt`
2. **Edit → Preferences → Kits**，確認/建立 Kit，四項需一致對應：
   - Compiler：Visual Studio Build Tools 2022（amd64，非其他版本的 VS）
   - Qt version：Qt 6.8.3 in PATH (msvc2022_64)
   - CMake Tool：需明確指定（新裝環境常見 `CMake Tool: None`，需去 CMake → Tools 手動 Add）
   - CMake generator：Ninja
3. 若 Configure 階段出現 `Qt packages are missing` 但實際檔案都存在，多半是 Qt Creator 內建的 `QT_CREATOR_ENABLE_MAINTENANCE_TOOL_PROVIDER` 檢查機制誤判，可在 Initial Configuration 裡把它設為 **OFF**，讓 CMake 用標準 `find_package` 直接判斷，會顯示更準確的錯誤訊息。

#### 步驟 6：補齊 Qt 附加模組（若步驟 3 沒有全選）

依序可能遇到：

```
Failed to find required Qt component "Core5Compat"
Failed to find required Qt component "TextToSpeech"
```

用 `C:\Qt\MaintenanceTool.exe` → Add or remove components → Qt 6.8.3 → Additional Libraries，**全部勾選**後安裝。

#### 步驟 7：安裝 GStreamer 1.22.12（版本需精準）

```
https://gstreamer.freedesktop.org/data/pkg/windows/1.22.12/msvc/gstreamer-1.0-msvc-x86_64-1.22.12.msi
https://gstreamer.freedesktop.org/data/pkg/windows/1.22.12/msvc/gstreamer-1.0-devel-msvc-x86_64-1.22.12.msi
```

安裝時選 **Custom**，指定專屬資料夾（避免跟其他 GCS 軟體的 GStreamer 衝突）：

```
C:\QGC_gstreamer\1.0\msvc_x86_64
```

**兩邊環境變數都要設：**

- 建置期：Kit → Edit Build Environment → `GSTREAMER_1_0_ROOT_MSVC_X86_64 = C:\QGC_gstreamer\1.0\msvc_x86_64`
- 執行期：Run Settings → Edit Run Environment → PATH 需含 `C:\QGC_gstreamer\1.0\msvc_x86_64\bin`（QGC 的 CMake install 流程會自動把此路徑加入 Run Environment 的 PATH，通常不需手動補，但建議仍檢查一次是否有重複項目）

> 若不需要視訊回傳功能，可改用 CMake 變數 `QGC_ENABLE_GST_VIDEOSTREAMING=OFF` 整個跳過此功能，不需安裝 GStreamer。

#### 步驟 8：修正 GPS 驅動程式版本漂移問題（已知 bug）

**現象：**

```
C2661: 'GPSDriverUBX::GPSDriverUBX': 多載函式不使用 5 引數
```

**原因：** `src/GPS/CMakeLists.txt` 用 `GIT_TAG main`（浮動分支，非固定版本）抓取 `PX4/PX4-GPSDrivers`，上游該分支持續開發，導致跟 v5.0.8 呼叫端程式碼簽名對不上。此為官方已知問題（[Issue #14368](https://github.com/mavlink/qgroundcontrol/issues/14368)，已排入 V5.1 修正）。

**修正：** 編輯 `src/GPS/CMakeLists.txt`：

```diff
 CPMAddPackage(
     NAME px4-gpsdrivers
     GITHUB_REPOSITORY PX4/PX4-GPSDrivers
-    GIT_TAG main
+    GIT_TAG caf5158061bd10e79c9f042abb62c86bc6f3e7a7
     SOURCE_SUBDIR src
 )
```

> **臨時替代方案：** 若不需要序列埠/USB 連線功能，可加入 CMake 變數 `QGC_NO_SERIAL_LINK=ON` 整個跳過此模組（含 GPS 驅動），但會連帶關閉所有序列埠連線能力。

#### 步驟 9：清乾淨建置與執行

每次改動 Kit、編譯器、或前述任何 Initial Configuration 變數後：

```bash
cd ~/qgroundcontrol
rm -rf build
```

Qt Creator 重開專案 → Configure Project → Build。

執行前可檢查一次 **Run Environment** 的 PATH 清單，確認沒有重複的 `GSTREAMER_1_0_ROOT_MSVC_X86_64` 或路徑項目（多次調整設定後容易殘留重複行，雖不一定致命但建議清理）。若一般執行（Ctrl+R）出現 `terminated abnormally`，改用除錯模式（F5）執行可取得實際崩潰的 Call Stack，比單純猜測環境變數更快定位問題。

---

### 四、疑難排解索引

| 錯誤訊息關鍵字 | 根本原因 | 對應章節 |
|---|---|---|
| `git` is not recognized | 未安裝 Git | 步驟 1 |
| Configure Project 頁面 Kit 清單為空 | Qt Creator 未偵測到 Qt/編譯器 | 步驟 5 |
| Compilers 分頁沒有 MSVC | 誤裝 VS Code、或 VS 版本與 Qt 預編譯版本不符 | 步驟 4 |
| `CMake Tool: None` | Kit 未指定 CMake 執行檔 | 步驟 5 |
| CMake generator 沒有 Ninja | Qt 安裝時未勾 Additional Libraries 裡的 Ninja | 步驟 3 |
| `Qt6 6.11.0+ not found` | 誤 clone 到 master 分支而非 v5.0.8 tag | 步驟 2 |
| `Qt packages are missing`（但檔案確實存在） | Qt Creator Maintenance Tool Provider 誤判，需關閉該機制看真實錯誤 | 步驟 5 |
| `Failed to find required Qt component "XXX"` | Qt 附加模組未安裝 | 步驟 6 |
| `Could not locate GStreamer` | 未安裝或路徑未指定 | 步驟 7 |
| `C2661: GPSDriverUBX ... 5 引數` | px4-gpsdrivers 用浮動 `GIT_TAG main`，上游 API 已變動 | 步驟 8 |
| 長時間播放視訊串流數小時後 `terminated abnormally`（Access Violation 0xc0000005） | GStreamer `qmlglsink` 上游已知記憶體洩漏，非本機設定問題 | 五、已知問題 |
| 同一設定反覆重試仍報錯 | `build/` 資料夾內 CMake 快取殘留舊狀態 | 步驟 9（`rm -rf build`） |

---

### 五、已知問題與待辦事項

- [x] **建置成功** — Debug 版於 2026/07 完整編譯通過（耗時約 11 分 34 秒），無錯誤。
- [x] **執行成功** — 以除錯模式（F5）啟動，成功顯示主畫面與首次啟動的 Measurement Units 設定畫面，確認整條建置鏈路（Git → Qt → 編譯器 → CMake → GStreamer → GPS 驅動修正）可運作。
- [x] **長時間視訊串流後崩潰（`terminated abnormally` / Access Violation 0xc0000005）— 根本原因已確認** — 持續播放視訊串流約 2 小時後突發崩潰，經除錯器攔截確認為記憶體存取違規，崩潰點位於 Qt QML 引擎（`Qt6Qmld.dll`）內部。查證為 **GStreamer `qmlglsink`（QML GL 視訊接收器）已知的上游記憶體洩漏問題**，非本機環境設定錯誤：
  - GStreamer 官方已列管此問題：[GitLab Issue #1614: qmlglsink Memory leak](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/1614)
  - 社群實測案例：連續播放 7 小時，記憶體從約 500MB 增長到 2.5GB（[GStreamer Discourse 討論串](https://discourse.gstreamer.org/t/qmlglsink-memory-leak/3808)）
  - QGC 自 [PR #8093](https://github.com/mavlink/qgroundcontrol/pull/8093) 起改用 `qmlglsink` 顯示視訊畫面，因此連帶受影響
  - **限制：** 新版 GStreamer 據回報洩漏情況較輕微，但 QGC v5.0.8 的 CMake 設定將版本精準釘死在 1.22.12（見步驟 7），無法簡單升級規避
  - **緩解方式（非根治）：** 長時間任務建議每 1～1.5 小時主動重啟 QGC；可用工作管理員觀察 `QGroundControl.exe` 記憶體是否持續攀升以估算安全時長；深入除錯可設定環境變數 `GST_TRACERS=leaks` 與 `GST_DEBUG=GST_TRACER:7`，結束時會列出所有未釋放物件
- [ ] Run Environment 清單中曾出現重複的 `GSTREAMER_1_0_ROOT_MSVC_X86_64` 項目（一筆結尾含反斜線、一筆不含），建議清理只保留一筆，避免長期累積混亂。
- [ ] 除錯時若 Call Stack 函式名稱看起來離奇（例如跨模組交錯出現、偏移量極大），先檢查是否已安裝 **Qt Debug Information Files**（Qt Maintenance Tool 內），未安裝時堆疊會顯示「最接近的已知符號」而非真實函式，容易誤判。
- [ ] 官方已預告會發布 v5.0.8.1 或後續修正版本 backport GIT_TAG 釘選，屆時可考慮改用官方修正版本，取代目前手動 patch 的方式。
- [ ] `src/AnalyzeView`、`src/Comms`、`src/Joystick`、`src/MAVLink/LibEvents` 底下也有類似的浮動 `GIT_TAG main`/`master` 寫法（見 Issue #14368 附註），目前尚未受影響，但存在同樣風險，未來若遇到類似的簽名不符錯誤可比照步驟 8 處理。

---

### 六、常用指令參考

```bash
# 確認目前所在版本
git describe --tags
git status

# 確認子模組同步狀態
git submodule status

# 完全重置建置環境
rm -rf build

# 檢查系統/使用者 PATH 是否含特定關鍵字（PowerShell）
[Environment]::GetEnvironmentVariable("Path","Machine") -split ';' | Select-String gstreamer
[Environment]::GetEnvironmentVariable("Path","User") -split ';' | Select-String gstreamer

# 手動驗證 Qt qmake 是否可執行
"C:\Qt\6.8.3\msvc2022_64\bin\qmake6.exe" -v
```
