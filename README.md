# netradio — port for LCDWiki ES3C40P

Internet Radio / MP3 Player / Alarm Clock บนบอร์ด **LCDWiki ES3C40P**
(ESP32-S3 N16R8, จอ ST7796S 4.0" 320x480 + ทัช FT6336U + เสียง ES8311)

- ต้นฉบับ: `/home/pi/Documents/ESP/es3c28p/netradio` (บอร์ด ES3C28P 2.8" ILI9341)
- HW ref: https://www.lcdwiki.com/4.0inch_ESP32-S3_Display

## สิ่งที่เปลี่ยนจากต้นฉบับ (ES3C28P → ES3C40P)

พินเหมือนกันเกือบทั้งหมด ต่างแค่ 2 จุด:

| เรื่อง | ES3C28P (netradio) | ES3C40P (โปรเจกต์นี้) |
|---|---|---|
| จอ | ILI9341 240x320 (`ILI9341_DRIVER`, `TFT_WIDTH=240/320`) | ST7796S 320x480 (`ST7796_DRIVER`, `TFT_WIDTH=320/480`), landscape = 480x320 |
| LVGL | `hor_res 320 / ver_res 240`, `LV_HOR_RES_MAX 320` | `hor_res 480 / ver_res 320`, `LV_HOR_RES_MAX 480` (ดูข้อควรรู้) |
| ทัช | FT6336G 240x320 | FT6336U 320x480 (โปรโตคอล/แอดเดรส 0x38 เดียวกัน, ไดรเวอร์เดิมใช้ได้) |
| เสียง I2S data | `I2S_DOUT = GPIO8` | `I2S_DOUT = GPIO8` (สเปกบอก `DO=IO6` แต่เทสแล้วเงียบ สลับกลับเป็น 8) |
| ภาษา/WiFi เริ่มต้น | ฝรั่งเศส / ว่าง | อังกฤษ + `DRBOM`/`55555555` (`setDefaultConfig`, magic `NR03`) |
| เลย์เอาต์ UI | 320x240 | reskin เป็น 480x320 ครบทุกจอ (ผ่าน `TFT_W`/`TFT_H` ใน `config.h`) |
| อื่นๆ (SPI/Touch/I2C/SD/LED/BAT/ปุ่ม) | — | เหมือนเดิมทุกขา |

ไฟล์ที่แก้: `platformio.ini` (env `es3c40p`), `src/config.h`
(`TFT_W/H`, `TP_W/H`, `I2S_DOUT`+`I2S_DIN`), `src/touch_helper.h`,
`src/display_gui.h` (คอมเมนต์ invert), `src/main.cpp` (LVGL buffer/res,
HTTP time sync, defaults), `include/lv_conf.h` (`LV_HOR_RES_MAX/VER_RES_MAX`),
`src/ui_*.cpp` (reskin 480x320 + `ui_font()`), `src/ui_lang.*` (ภาษาไทย),
`src/thai_*.c` + `src/thai_fonts.h` (ฟอนต์ Noto Sans Thai 9 ไซส์ + ไอคอน),
`src/font_7seg_135.c` (นาฬิกา 150%)

## ภาษาไทย + ฟอนต์

- เมนู Language มี **ไทย** (แปล 61 สตริง: วัน เดือน ทุกหน้าจอ)
- ฟอนต์ `thai_10..32` (Noto Sans Thai + ASCII + ไอคอน FontAwesome 31 ตัว)
  gen ด้วย `lv_font_conv` + merge script ใน `/tmp/opencode/`
- ทุก label ผ่าน `ui_font(size)` (ดู `src/thai_fonts.h`) — อยู่ภาษาไหนก็แสดงถูก
- ข้อจำกัด: แป้นพิมพ์กรอก WiFi ยังเป็นอังกฤษ (พิมพ์รหัสไทยไม่ได้)

## พินเสียง (I2S + ES8311)

ไมค์บนบอร์ดต่อเข้า codec ES8311 แล้วส่งข้อมูลมา ESP32 ทาง `I2S_DIN`:

| สัญญาณ | GPIO | หมายเหตุ |
|---|---|---|
| `I2S_MCK` | 4 | master clock |
| `I2S_BCK` | 5 | bit clock |
| `I2S_DOUT` (ลำโพง) | **8** | สเปกเขียน `DO=IO6` แต่เทสแล้วเงียบ ใช้ 8 ถึงมีเสียง |
| `I2S_WS` | 7 | word select |
| `I2S_DIN` (ไมค์) | **6** | ไมค์ → ES8311 → ESP32 (netradio ไม่ได้ใช้ input, ยังไม่เทส) |
| `AMP_EN` | 1 | LOW = เปิดแอมป์ |

## Build / Flash

```bash
cd /home/pi/Documents/ESP/es3c3248
export PATH="$HOME/.platformio/penv/bin:$PATH"
pio run                    # build (ผ่านแล้ว: Flash ~2.5MB / 7MB)
pio run -t upload          # แฟลชผ่าน /dev/ttyACM0
pio device monitor         # ดู log 115200
```

## ข้อควรรู้ (ต้องเช็คบนเครื่องจริง)

1. ~~UI แสดงมุมซ้ายบน~~ แก้แล้ว — reskin ครบทุกจอเป็น 480x320
   (header/footer เต็มความกว้าง, ลิสต์ยืด, ปุ่มกระจาย)
2. ~~เสียงไม่ออก~~ สลับ `I2S_DOUT` กลับเป็น GPIO8 แล้ว — ลองฟังดู
   ถ้ายังเงียบ สาเหตุถัดไปคือ init ES8311/แอมป์ ต้องดู log `[RADIO]`
3. **สีเพี้ยน/กลับสี**: สลับ `tft.invertDisplay(true/false)` ใน
   `src/display_gui.h` (`setup_display()`)
4. **ST7796 init**: ใช้ init มาตรฐานของ TFT_eSPI ถ้าจอดำ/ลาย
   ให้นำ `ST7796_Init.h` จาก data package ของ LCDWiki มาทับ
5. ที่เหลือเหมือน netradio ทุกอย่าง: WiFi seed `DRBOM`, web manager
   `admin/cydgold`, SD SPI mode, RTC DS3231 optional, sleep switch ปิด
