[English](README.md) · **繁體中文**

# PCDeskCYD
<img width="3024" height="2081" alt="image" src="https://github.com/user-attachments/assets/47d75915-9cf5-43b6-af75-a281262daafc" />

運行於 Cheap Yellow Display (ESP32-2432S028) 上的 3 頁式觸控螢幕 Home Assistant 控制面板，透過原生 WebSocket API 與 HA 通訊，並具備 REST 備援機制以取得初始狀態。採用 PlatformIO + Arduino + LVGL 9 + TFT_eSPI 建置。

## 功能

- **3 個房間分頁**（書房／次臥／廚房）搭配觸控導覽，各分頁將空調、燈光與風扇實體顯示為即時更新的圖示磚
- **即時同步**：透過限定範圍的 `subscribe_trigger` WebSocket 訂閱，HA 中的實體狀態變更會在約 250ms 內反映至螢幕上
- **觸控控制**：直接從房間分頁開關燈光；輕觸空調或風扇圖示可開啟完整詳細資訊頁面（溫度、HVAC 模式、風速、擺頭、預設模式）
- **樂觀 UI（Optimistic UI）**：輕觸操作立即重繪畫面，若呼叫失敗則會依下一次確認的 HA 狀態自我校正
- **支援 CJK 標籤**：透過產生的 Noto Sans TC 字型子集，搭配 Segoe Fluent Icons 字形以呈現燈光／空調圖示
- **雙核心運作**：所有 WiFi／WebSocket／HTTP I/O 皆在綁定至 core 0 的專屬 FreeRTOS 任務上執行；LVGL／顯示器則專門保留在 core 1 上

## 硬體

ESP32-2432S028 ("Cheap Yellow Display", CYD) -- 2.8" 320x240 ILI9341 TFT、XPT2046 電阻式觸控、無 PSRAM。在點亮並調試此開發板時所發現的特定驅動程式特性、WebSocket 訊框大小限制以及字型處理流程注意事項，請參閱 [HARDWARE_NOTES.md](HARDWARE_NOTES.md) — 在修改顯示／觸控／字型程式碼前非常值得一讀。

## 設定

1. 複製 `include/secrets.h.example` 為 `include/secrets.h`，並填入您的 WiFi 連線憑證與 Home Assistant 長期存取權杖（**Profile -> Security -> Long-Lived Access Tokens** in HA）。
2. 編輯 `include/config.h`，填入您的 HA 主機名稱／連接埠。
3. 編輯 `src/ha_client.cpp` 中的實體列表（`g_entities[]`）與房間分頁檔案（`src/ui_page_*.cpp`），以符合您自己的 HA 實體 — 本儲存庫的預設值是針對特定住家的空調／燈光／風扇配置所設定，換到另一套 HA 就沒有意義。
4. 使用 [PlatformIO](https://platformio.org/) 建置並燒錄：
   ```
   pio run -t upload
   ```

## 專案結構

- [`PLAN.md`](PLAN.md) -- 原始實作計畫，逐一記錄各個里程碑（從 M1 螢幕點亮到 M7 詳細資訊頁面），並隨著實際實作與初始設計有所差異時持續更新
- [`HARDWARE_NOTES.md`](HARDWARE_NOTES.md) -- 開發板專屬的經驗與發現：顯示驅動程式選擇、WebSocket 函式庫的 15KB 訊框上限與為何 `get_states` 在此技術架構下無法運作、CJK／圖示字型流程注意事項、畫面撕裂限制
- `src/ha_client.cpp` -- WiFi/WebSocket/REST、實體狀態、`call_service`
- `src/ui_manager.cpp` + `src/ui_page_*.cpp` -- 房間分頁、導覽、即時狀態 -> 圖示綁定
- `src/ui_detail_climate.cpp` / `src/ui_detail_fan.cpp` -- 全螢幕控制疊加層

## 授權條款

MIT -- 請參閱 [LICENSE](LICENSE)。
