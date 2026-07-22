# QGC AVIX Gimbal 面板 UI 設計預覽 SOP — 工作報告

日期：2026-07-22
範圍：`AvixGimbalControl.qml`（[src/AvixGimbal/AvixGimbalControl.qml](../../src/AvixGimbal/AvixGimbalControl.qml)）面板排版/樣式調整的標準作業流程。目的是讓其他人之後要調整這個面板（或用同樣手法調整別的 QGC QML 面板）時，有一套可以直接照做的步驟，不用重新踩一次這次過程中踩過的坑。

---

## 1. 為什麼不能直接在正式檔案上用視覺化編輯器改

QGC 的 QML 檔案是**編譯時**打包進 Qt Resource System（`qrc:/qml/...`），不是執行期直接讀磁碟檔案，這件事本身不影響視覺化編輯，真正的問題是**依賴**：

- `AvixGimbalControl.qml` 依賴兩個 **C++ singleton**：`ScreenTools`（[src/QmlControls/ScreenTools.qml](../../src/QmlControls/ScreenTools.qml)，`pragma Singleton`，內部再依賴 `ScreenToolsController` C++ 類別）跟 `AvixGimbalController`（[src/AvixGimbal/AvixGimbalController.h](../../src/AvixGimbal/AvixGimbalController.h)，`Q_APPLICATION_STATIC` 單例，靠 `QGCApplication::init()` 時呼叫 `registerQmlTypes()` 才能被 QML engine 認得）。
- 這兩個 singleton **只有在完整的 QGroundControl.exe 真的執行起來、跑過 App 的 C++ 初始化流程之後才存在**。
- Qt Design Studio／Qt Creator 的視覺化畫布用的是一個叫 `qmlpuppet` 的輕量渲染程序，**不會**跑 `QGCApplication::init()`，所以直接開正式檔案，畫布會直接報 `ScreenTools is not defined` / `AvixGimbalController is not defined`，或整個空白。
- 正式的 `QGCButton`/`QGCLabel`（[src/QmlControls/](../../src/QmlControls/)）本身又依賴 `QGroundControl.Palette` 的 `QGCPalette`（C++ 類別，走主題色），依賴鏈會一路往下拉，不是換兩個 singleton 就能解決。

**解法：另外做一份「假資料設計預覽」副本**，把上述依賴全部換成簡化的純 QML mock，讓視覺化編輯器能正常載入，同時完全不動、不影響正式檔案。以下 SOP 就是這份副本怎麼從零建起來。

---

## 2. 工具安裝

### 2.1 Qt Design Studio（視覺化 QML 編輯器）

- 本機路徑：`C:\Qt\Tools\QtDesignStudio-4.1.1-lts`（已安裝，本次直接沿用）
- 沒裝的話：透過 Qt Maintenance Tool（`MaintenanceTool.exe`，在 Qt 安裝目錄下）勾選安裝
- **跟 Qt Creator 是兩個不同的 App**，容易搞混：
  - **Qt Creator**：C++/QML 混合專案的完整 IDE，這個 repo 平常建置、跑 CDB 除錯都用它
  - **Qt Design Studio**：專注純 QML 的視覺化編輯器（畫布拖拉 + Properties 面板 + Navigator 元件樹），本 SOP 的假資料預覽主要用這個開

### 2.2 qml runtime（`qml.exe`，隨 Qt SDK 附贈，免安裝）

- 本機路徑：`C:\Qt\6.8.3\msvc2022_64\bin\qml.exe`（對齊本專案 CMake 用的 Qt 版本，見 [CMakeLists.txt:177-179](../../CMakeLists.txt)）
- 用法：`qml.exe <某個.qml檔案路徑>`，直接跳出一個真正在跑的視窗，不用開整個 Design Studio
- 用途：
  - 快速驗證改動有沒有語法錯誤（比開 Design Studio 快）
  - 真實滑鼠互動測試（按壓變色這類效果）
  - 獨立於 Design Studio 之外的「第二個引擎」，Design Studio 畫布顯示異常時可以拿來交叉驗證是不是畫布本身卡住（見第4節「已知陷阱」）

### 2.3 不建議的路徑：Qt Creator 的 QML Debug / Apply Changes on Save

Qt Creator 有 `QGC_DEBUG_QML`（[cmake/CustomOptions.cmake:25](../../cmake/CustomOptions.cmake)）這個 CMake 選項，理論上能讓存檔即時套用到執行中的 App。**實測這條路在這個專案上不值得走**：

- Windows 上 CDB（C++ 除錯器）跟 QML debugger 常常搶啟動時機，一起失敗，錯誤訊息是「Launching QML Debugger failed」+「Launching CDB Debugger failed」同時出現
- 就算連線成功，「Apply Changes on Save」在這種 C++/QML 混合大型專案上不可靠，測試時存檔沒有真的即時反映
- 純視覺調整工作**不需要中斷點除錯**，改走本 SOP 的假資料預覽 + `qml.exe`/Design Studio Live Preview 這條路，快非常多

---

## 3. 建置假資料設計預覽環境 SOP

### 3.1 目錄結構

放在跟 repo **無關**的目錄（scratchpad 或任意本機資料夾，**不要**進版控）：

```
avix_gimbal_design_preview/
├── AvixGimbalPreview.qmlproject   Qt Design Studio 專案檔（用 Open Project 開這個）
├── main.qml                       進入點：模擬 Fly View 深色背景 + 全螢幕掛載面板
├── AvixGimbalControl_Preview.qml  面板本體，複製自正式檔案，只拿掉 import
├── qmldir                         宣告兩個假 singleton（缺這個會找不到 singleton）
├── ScreenTools.qml                假 singleton
├── AvixGimbalController.qml       假 singleton
├── QGCButton.qml                  簡化替身按鈕
└── QGCLabel.qml                   簡化替身文字
```

### 3.2 各檔案內容重點

**`qmldir`**（同目錄下的隱含 import，缺這個 `ScreenTools.xxx`/`AvixGimbalController.xxx` 這種 dot-access 會抓到 `undefined`）：
```
singleton ScreenTools 1.0 ScreenTools.qml
singleton AvixGimbalController 1.0 AvixGimbalController.qml
```

**`ScreenTools.qml`**（只給排版計算會用到的欄位，數值抓合理估計值即可）：
```qml
pragma Singleton
import QtQuick

QtObject {
    readonly property real defaultFontPixelHeight: 16
    readonly property real defaultFontPixelWidth:  9
    readonly property real defaultFontPointSize:   11
}
```

**`AvixGimbalController.qml`**（複製正式 C++ 介面的函式簽名，全部指令只印 log）：
```qml
pragma Singleton
import QtQuick

QtObject {
    // 對應 C++ enum class ControlSource。⚠️ 見第4節：屬性名稱不能大寫開頭，
    // 且 "native" 這個字本身是 QML/JS 保留字，兩個坑疊在一起，要用 controlSourceNative 這種前綴繞開。
    readonly property int controlSourceNative:        0
    readonly property int controlSourceMavlinkBridge: 1

    property int activeControlSource: controlSourceNative

    function requestControlSource(source) { activeControlSource = source }
    function setVelocity(source, yawRateDegS, pitchRateDegS) { console.log("[mock] setVelocity", source, yawRateDegS, pitchRateDegS) }
    function setAngle(source, yawDeg, pitchDeg)               { console.log("[mock] setAngle", source, yawDeg, pitchDeg) }
    function setZoom(source, direction)                       { console.log("[mock] setZoom", source, direction) }
    function emergencyStop(source)                            { console.log("[mock] emergencyStop", source) }
}
```

**`QGCButton.qml`**（簡化替身，只做出「大小/可點擊/文字置中」，可覆寫屬性隨改動需求逐步擴充）：
```qml
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: control

    property string text:         ""
    property real   pointSize:    ScreenTools.defaultFontPointSize
    property color  textColor:    "#202020"
    property real   fontWeight:   Font.Normal
    property string fontFamily:   ""            // 空字串 = 用系統預設字型
    property color  pressedColor: "#B0B0B0"     // 按下去的顏色，預設維持灰色，個別按鈕可覆寫

    signal clicked()
    signal pressed()
    signal released()

    implicitWidth:  Math.max(ScreenTools.defaultFontPixelHeight * 2.4,
                              label.implicitWidth + ScreenTools.defaultFontPixelWidth * 2)
    implicitHeight: ScreenTools.defaultFontPixelHeight * 1.8

    radius:       4
    color:        mouseArea.pressed ? control.pressedColor : "#D8D8D8"
    border.width: 1
    border.color: "#909090"

    Text {
        id:               label
        anchors.centerIn: parent
        text:             control.text
        font.pointSize:   control.pointSize
        font.weight:      control.fontWeight
        font.family:      control.fontFamily.length > 0 ? control.fontFamily : Qt.application.font.family
        color:            control.textColor
    }

    MouseArea {
        id:           mouseArea
        anchors.fill: parent
        onClicked:    control.clicked()
        onPressed:    control.pressed()
        onReleased:   control.released()
    }
}
```

**`QGCLabel.qml`**：
```qml
import QtQuick

Text {
    color:          "#202020"
    font.pointSize: ScreenTools.defaultFontPointSize
}
```

**`AvixGimbalControl_Preview.qml`**：直接複製正式檔案 [AvixGimbalControl.qml](../../src/AvixGimbal/AvixGimbalControl.qml) 全部內容，只做兩個修改：
1. 刪掉 `import QGroundControl` / `QGroundControl.Controls` / `QGroundControl.ScreenTools` / `QGroundControl.Palette` / `AvixGimbal` 這幾行（同目錄的假 singleton/替身元件會自動被隱含 import 解析，不需要 import）
2. `AvixGimbalController.Native` 這種寫法全部改成 `AvixGimbalController.controlSourceNative`（原因同上，QML property 不能大寫開頭）

**`main.qml`**（進入點，模擬實際掛載情境：[FlyView.qml:130-133](../../src/FlightDisplay/FlyView.qml) 是 `anchors.fill: parent` 掛在深色背景上）：
```qml
import QtQuick
import QtQuick.Window

Window {
    width: 1280; height: 720; visible: true
    Rectangle {
        anchors.fill: parent
        color: "#202830"   // 模擬 Fly View 深色地圖/影像底圖
        AvixGimbalControl_Preview { anchors.fill: parent }
    }
}
```

**`AvixGimbalPreview.qmlproject`**（讓 Qt Design Studio 當「專案」開，而不是單一散檔案）：
```qml
import QmlProject

Project {
    mainFile:   "main.qml"
    mainUiFile: "main.qml"
    qt6Project: true
    QmlFiles       { directory: "." }
    JavaScriptFiles { directory: "." }
    ImageFiles     { directory: "." }
    Environment { QT_QUICK_CONTROLS_STYLE: "Basic" }
    qdsVersion:   "4.8"     // 對齊實際安裝的 Qt Design Studio 版本
    quickVersion: "6.5"     // 對齊 Kit 顯示的 Qt 版本
}
```

### 3.3 開啟方式

**開法1（最快，改一次看一次）**：
```
C:\Qt\6.8.3\msvc2022_64\bin\qml.exe "<路徑>\main.qml"
```
存檔後重跑這行指令即可看到最新效果，不用建置、不用管 QML debug 連線。

**開法2（要用滑鼠拖拉/Properties面板視覺化編輯）**：
Qt Design Studio 歡迎頁點 **Open Project**，選 `AvixGimbalPreview.qmlproject`（不要用 Open File 開單一檔案——那樣只有純文字編輯模式，沒有畫布，見第4節）。載入後左側 Projects 面板點開 `AvixGimbalControl_Preview.qml`（不是 `main.qml`，`main.qml` 裡的面板是封裝好的元件實例，Navigator 展不開內部結構），才能在 Navigator/畫布上選到面板內部的每個元件。開著後點工具列 **Live Preview**，存檔會自動即時反映。

---

## 4. 已知陷阱（Troubleshooting 記錄）

| # | 症狀 | 原因 | 解法 |
|---|---|---|---|
| 1 | `Unable to assign [undefined] to double` | 假 singleton 有 `pragma Singleton` 但同目錄隱含 import 沒有 `qmldir` 宣告，dot-access 抓不到 | 補 `qmldir`，用 `singleton TypeName 1.0 File.qml` 語法宣告 |
| 2 | Parse error：`Property names cannot begin with an upper case letter` | 想在純 QML 裡宣告 `property int Native: 0` 這種大寫開頭屬性（正式 C++ `Q_ENUM` 可以大寫，純 QML 不行） | 改小寫命名 |
| 3 | Parse error：`Expected token 'identifier'`，指到一個看似正常的屬性名稱 | `native` 這個字本身在 QML/JS 底層是保留字，即使小寫也會 parse error | 換名字繞開（例：`controlSourceNative`） |
| 4 | 畫布上拖拉調整某元件大小後，程式碼裡對應的 `width:` 被改成寫死數字（例：`width: 42`），但 `height: width` 這種綁定其他屬性的行沒被動 | Design Studio 拖拉縮放手把只會覆寫「被拖的那個軸」的 property，其他還綁定它的 property 不受影響，但原本的公式綁定（例如 `_root._dpadButtonSize`）已經被換成字面值 | 拖完務必切到 Code 檢視核對每個被動過的元件；不確定就整個關閉分頁選 **Don't Save**，重新從硬碟開啟即可還原（前提是硬碟上的檔案本身是對的，拖拉當下若未存檔，硬碟版本不會被污染） |
| 5 | 畫布上拖拉調整某元件**位置**後，anchors 相對關係跑掉 | 同上，拖拉會把 `anchors.bottom: stopButton.top` 這種相對定位換成絕對 `x`/`y`，脫離跟其他元件的關聯 | 同上，`d-pad` 這種靠 anchors 串起來對稱定位的元件，**不要**用拖拉調，只能改共用的基準變數（見第5節矩陣） |
| 6 | 程式碼核對起來完全正確，但 2D 畫布顯示的排列還是錯的（例：十字形 d-pad 排成不規則形狀） | Design Studio 的畫布渲染程序（qmlpuppet）偶爾會卡在舊狀態，沒有正確反映最新存檔內容 | 用 `qml.exe` 獨立驗證同一份檔案（截圖比對），如果 `qml.exe` 顯示正常、只有 Design Studio 畫布異常，代表是工具本身卡住：先關閉分頁重開，還是不行就整個重啟 Design Studio |
| 7 | Qt Creator 用 F5（Debug 模式）啟動，跳出「Launching QML Debugger failed」+「Launching CDB Debugger failed」 | 啟用 `QGC_DEBUG_QML` 後，CDB（C++ 除錯器）跟 QML debugger 在 Windows 上常搶啟動時機，一起失敗 | 純視覺調整工作改用一般 **Run（Ctrl+R）**，不需要中斷點除錯，避開這個衝突 |
| 8 | 一般 Run 也失敗，跳出「程序啟動失敗：應用程式控制原則已封鎖此檔案」 | Windows 端點防護軟體（本機案例：McAfee 消費版套件 + Windows Smart App Control 都在 Evaluation 模式）封鎖新編譯出來、未簽章的 exe | 檢查 McAfee 安全記錄／Windows 保護記錄，加白名單或排除清單。**這是系統安全性設定，不建議請 Claude 代為執行，需自行到對應 UI 操作** |
| 9 | Mock 上直接可用的 `border.color`／`background` 覆寫，搬到正式檔案上寫法行不通 | 正式的 `QGCButton`（[QGCButton.qml](../../src/QmlControls/QGCButton.qml)）繼承 `Button`，背景是內部獨立的 `background: Rectangle { id: backRect; ... }`，沒有 `border` 群組屬性可以直接從外面覆寫，也沒有 alias 曝露邊框顏色；跟 mock（本身就是一個 `Rectangle`）行為不同 | 見第7節「正式檔案樣式客製化的兩條路線」 |

---

## 5. 面板元件位置/大小關係矩陣

以下以正式檔案 [AvixGimbalControl.qml](../../src/AvixGimbal/AvixGimbalControl.qml) 目前的結構為準（行號對應現況）。

### 5.1 共用基準變數（第 29-31 行）——大部分尺寸的源頭

| 變數 | 公式 | 影響範圍 |
|---|---|---|
| `_dpadButtonSize` | `ScreenTools.defaultFontPixelHeight * 1.8` | 上下左右 4 個方向鍵的邊長 |
| `_stopButtonSize` | `ScreenTools.defaultFontPixelHeight * 2.6` | 中間緊急停止鈕的邊長（刻意比方向鍵大，避免誤觸辨識） |
| `_dpadSpacing` | `ScreenTools.defaultFontPixelHeight / 4` | 方向鍵與停止鈕之間的間距 |

這三個是**唯一建議拿來調整 d-pad 區塊大小的地方**——它們同時驅動按鈕本身尺寸跟 `dpad` 容器的 `implicitWidth`/`implicitHeight`（見下），兩者天生同步，不會跑版。

### 5.2 各元件位置/大小控制表

| 元件 | 行號 | 位置怎麼決定 | 大小怎麼決定 |
|---|---|---|---|
| `panel`（外框 Rectangle） | 58-70 | `anchors.left/bottom: parent.left/bottom` + `anchors.leftMargin`（`defaultFontPixelWidth`）/ `anchors.bottomMargin`（`defaultFontPixelHeight*3`，刻意避開 QGC 地圖縮放鈕） | **公式算出來，非獨立設定**：`width/height = panelLayout.implicitWidth/Height + defaultFontPixelWidth * 1` |
| `panelLayout`（ColumnLayout） | 72-75 | `anchors.centerIn: parent`（置中於 panel） | implicit，＝底下 4 個區塊疊加 + `spacing`（`defaultFontPixelHeight/2`，區塊間垂直間距，**全域共用同一個值**） |
| 標題文字 `QGCLabel` | 80-83 | RowLayout 內靠左 | `Layout.fillWidth: true`，吃滿剩餘寬度——**注意：面板整體寬度其實是被這段文字撐開的，不是被 d-pad 撐開**，改按鈕大小不會讓面板變窄，要改標題文字長度 |
| 縮小鈕 `QGCButton "–"` | 85-90 | RowLayout 內最右 | `Layout.preferredWidth/Height = defaultFontPixelHeight * 1.2`，固定正方形 |
| `dpad`（Item 容器） | 97-101 | `Layout.alignment: Qt.AlignHCenter`（水平置中於 panelLayout） | `implicitWidth = _stopButtonSize + (_dpadButtonSize+_dpadSpacing)*2`，`implicitHeight` = 同寬（正方形）。⚠️ 這個容器**不裁切子元件**（`clip` 預設 false），本身大小只是「跟外層 ColumnLayout 報告要留多少空間」的宣告，不是子元件的邊界 |
| `stopButton` | 103-111 | `anchors.centerIn: parent`（置中於 `dpad`） | `width/height = _stopButtonSize`（正方形） |
| ▲ 上 | 112-121 | `anchors.horizontalCenter: stopButton.horizontalCenter` + `anchors.bottom: stopButton.top` + `bottomMargin: _dpadSpacing` | `width/height = _dpadButtonSize` |
| ▼ 下 | 122-131 | 同上邏輯，改 `anchors.top: stopButton.bottom` + `topMargin` | 同 ▲ |
| ◀ 左 | 132-141 | `anchors.verticalCenter: stopButton.verticalCenter` + `anchors.right: stopButton.left` + `rightMargin: _dpadSpacing` | 同 ▲ |
| ▶ 右 | 142-151 | 同左，改 `anchors.left: stopButton.right` + `leftMargin` | 同 ▲ |
| `Center` 按鈕 | 155-160 | `Layout.alignment: Qt.AlignHCenter`，排在 dpad 之後 | `Layout.fillWidth: true`；高度沒設，吃 `QGCButton` 預設 implicit 高度（唯一沒有集中控制數值的地方之一） |
| Zoom `RowLayout` | 163-177 | `Layout.alignment: Qt.AlignHCenter`，排在最後 | implicit，`spacing = defaultFontPixelWidth/2` |
| `Zoom -`／`Zoom +` | 167-176 | RowLayout 內左右並排 | 沒設定 width/height，吃 `QGCButton` 預設 implicit 大小（另一個沒有集中控制的地方） |
| `restoreButton`（縮小後替代鈕） | 181-192 | 跟 `panel` **同一組** `leftMargin`/`bottomMargin`，縮小/展開切換時位置不會跳動 | `width/height = defaultFontPixelHeight * 2.2`（固定正方形） |

### 5.3 anchors 與 width/height 的角色分工

- **`anchors`** 決定「這個元件在哪裡」（相對另一個元件的邊/中心線 + margin），**`width`/`height`** 決定「這個元件多大」，兩者互相獨立、互不影響。
- 4 個方向鍵的位置**全部只相對 `stopButton`** 用 anchors 串起來，不是相對 `dpad` 這個容器——所以只要 `stopButton` 位置/大小對，其他 4 個自動對稱跟上；反過來，單獨改 `dpad` 容器本身的 width/height（不透過三個基準變數）會讓 `stopButton`（`anchors.centerIn: parent`）重新置中到不成比例的新框正中間，導致排版跑掉，**這是本次過程中實際踩過的坑**（見第4節#5）。

### 5.4 量測「改動前後差多少」的技巧

視覺上看不準確切差了多少 px 時，不用用猜的，寫一個獨立的量測用 `.qml` 檔（跟正式檔案無關，不用碰任何正式檔案），用 `Timer` + `console.log` 讀取實際算出來的 `height`/`width`，配合 `qml.exe` 執行、`Qt.quit()` 自動結束：
```qml
Timer {
    interval: 150; running: true
    onTriggered: { console.log("HEIGHT=" + panel.height); Qt.quit() }
}
```
⚠️ 注意 QML 的 `console.log` 預設走 **stderr**（`qDebug()`），不是 stdout，擷取輸出時要抓錯的管道。

---

## 6. 改動搬回正式檔案的流程

1. 假資料副本存檔完畢後，**用 `diff` 比對副本跟正式檔案**，不要整份用工具 merge——因為副本結構上本來就跟正式檔案不同（少了 import、部分中文註解被拿掉、`AvixGimbalController.Native` 被改名成 `controlSourceNative`），這些屬於「本來就該不一樣」的雜訊，直接 merge 會把正式檔案的 import 砍掉。
2. 從 diff 結果裡**手動挑出真正的排版/樣式數值改動**（例如 `_dpadButtonSize` 的倍數、新加的 `textColor`/`fontWeight` 這類屬性行），逐行貼回正式檔案對應位置。
3. 貼之前留意正式檔案本身**有沒有在其他 session 已經被單獨改過**（例如這次過程中 `panel` 的 padding 係數在正式檔案已經被改成 `* 1`，但副本還停留在原始的 `* 2`）——diff 出來的衝突要跟人確認要保留哪一邊，不要照抄搬過去蓋掉既有的正確狀態。
4. 搬完之後**這次改動沒辦法在假資料預覽裡驗證的部分**（例如面板位置要跟 RTSP PIP 縮小圖示對齊，`PipView` 這個元件根本不存在於預覽環境），必須直接在正式 App 上實機驗證，不能只看預覽就當作完工。

---

## 7. 正式檔案樣式客製化的兩條路線（顏色/字重/字型/按壓效果）

正式的 `QGCButton` 繼承 `Button`，背景在內部獨立的 `background: Rectangle`，**沒有** `border`/highlight 顏色的 alias 曝露出來，跟 mock（純 `Rectangle`）不一樣，不能無腦照搬 mock 的寫法。想客製化某一顆按鈕（例如緊急停止鈕）的外框色/按壓變色，兩條路線：

**路線 A（推薦，不動共用檔案）**：在該按鈕實例上整個覆寫 `background:`
```qml
QGCButton {
    id: stopButton
    background: Rectangle {
        radius:       backRadius
        color:        primary ? qgcPal.primaryButton : qgcPal.button
        border.width: 1
        border.color: "#D32F2F"
    }
}
```
缺點：會蓋掉原本 `backRect` 內建的 hover/按下高亮疊加效果（[QGCButton.qml:51-56](../../src/QmlControls/QGCButton.qml)），這顆按鈕會失去跟其他 QGC 按鈕一致的按壓視覺回饋。

**路線 B**：修改共用的 `src/QmlControls/QGCButton.qml`，加一個**有預設值**的可覆寫屬性（例：`property color highlightColor: qgcPal.buttonHighlight`，預設值＝目前所有按鈕在用的顏色），內部 binding 改成引用這個屬性。**只要預設值設成目前的既有顏色，全 QGC 專案裡其他所有沒有明確指定這個新屬性的 `QGCButton` 實例行為完全不變**，只有明確覆寫的那顆按鈕會變。這是 QML 元件設計的標準安全模式（`textColor`/`backgroundColor` 已經是同樣的模式）。**這是全 QGC 共用元件，不在 `src/AvixGimbal/` 範圍內，照專案規則「發現好像需要改，先停下來說明原因，不要直接動手」，改之前需要跟人確認。**

---

## 8. 已知限制

- 假資料的字級數值（`defaultFontPixelHeight = 16` 等）是概略估計值，不是真實螢幕上的精確值；用第5.4節技巧量測出來的**百分比差異**可信，但**絕對 px 數字**只供參考，實際效果仍需在真的執行的 App 上確認。
- 跟畫面上其他既有元件（例如 RTSP `PipView` 縮小圖示）的對齊需求，因為那些元件不存在於預覽環境，沒辦法在預覽裡驗證位置關係，只能查證原始碼算出理論數值（見 [PipView.qml](../../src/QmlControls/PipView.qml)、[FlyView.qml:112-116](../../src/FlightDisplay/FlyView.qml)），實際效果必須實機跑一次確認。
- 本次假資料副本存放在系統暫存目錄（`%TEMP%`），不是持久化位置，重開機或暫存目錄被清理就會消失；如果要長期沿用這套 SOP，建議把 `avix_gimbal_design_preview/` 整個資料夾複製到一個穩定的本機位置（不要進 repo）。
