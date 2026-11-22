# Jewery Gem Stone — Firmware (ESP32)

Firmware repository สำหรับส่วนฮาร์ดแวร์ของโครงงาน Smart Gemstone Showcase
ออกแบบให้ทำงานร่วมกับ Application repository ของกลุ่ม:

- Application: https://github.com/chonlachattee2548-crypto/-
- Deployment: https://chonlachattee2548-crypto.github.io/-/
- Cloudflare Worker: `https://jewelry-gemstone-control.chonlachat-tee2548.workers.dev`

## สถานะและข้อควรระวัง

แบบวงจรระบุโมดูล **ESP32-S3-WROOM-1 (N16R8)** บน PCB ที่ออกแบบเอง
(16 MB Flash และ 8 MB Octal PSRAM). PlatformIO ใช้ board profile เฉพาะของโครงงาน
ใน `boards/esp32-s3-wroom-1-n16r8.json` ไม่ใช่ ESP32-S3 DevKitC-1.
ตรวจสอบฉลากโมดูลจริงก่อนอัปโหลดลงบอร์ด เพราะ BOM ระบุชื่อรุ่นหลักโดยไม่แสดง suffix.
Firmware ใช้ Arduino framework และ HTTPS polling ไปยัง Cloudflare Worker.
ทั้ง `REMOTE_CONTROL_ENABLED` และ `MOTOR_OUTPUT_ENABLED` ปิดไว้โดยค่าเริ่มต้น
และ `SERVO_PIN` ยังไม่กำหนด (-1) เพื่อป้องกันการขยับมอเตอร์ก่อนยืนยันอุปกรณ์จริง.

อย่าเปิดสวิตช์เหล่านี้ก่อนยืนยันโมดูลจริง, GPIO, รุ่น Servo, แหล่งจ่ายไฟ,
การหยุดฉุกเฉิน และทดสอบกับแท่นจริงอย่างปลอดภัย. เวลา 1 วินาทีในตัวอย่าง
ไม่ใช่การรับประกันว่าหมุนครบ 360 องศา.

## โครงสร้าง

- `src/main.cpp` — firmware หลัก: Serial identify/status/stop, Wi-Fi,
  heartbeat, polling คำสั่ง และส่งผลกลับ
- `platformio.ini` — PlatformIO สำหรับ PCB ที่ใช้ ESP32-S3-WROOM-1-N16R8
- `boards/esp32-s3-wroom-1-n16r8.json` — board profile: 16 MB Flash, 8 MB Octal PSRAM
- `secrets.example.h` — แม่แบบการตั้งค่า (ห้ามใส่ค่าจริงลง GitHub)
- `.gitignore` — กันไฟล์ลับและไฟล์ build

## การสื่อสารกับ Worker

บอร์ดเป็นฝ่ายเชื่อมต่อออกไปผ่าน HTTPS โดยใช้ `Authorization: Bearer DEVICE_TOKEN`.

| Method | Endpoint | หน้าที่ |
|---|---|---|
| POST | `/device/heartbeat` | รายงาน `motorReady`, `pirDetected`, `firmware` ทุก ~5 วินาที; `pirDetected` เป็น `null` จนกว่าจะต่อและอ่านเซนเซอร์จริง |
| GET | `/device/next` | ตรวจคำสั่งทุก ~2 วินาที |
| POST | `/device/result` | ส่ง `commandId`, `status`, `message` กลับ |

คำสั่งที่รองรับคือ `rotate_360_once` เท่านั้น. Worker เป็นผู้จัดการสถานะ,
ป้องกันคำสั่งซ้อน และจำกัดความถี่. Firmware ต้องไม่รายงาน `completed`
จนกว่าจะทำงานตามโปรไฟล์ที่ตั้งไว้เสร็จ.

## เริ่มต้น

1. ติดตั้ง VS Code และ PlatformIO IDE.
2. เปิดโฟลเดอร์นี้ใน VS Code.
3. Build โดยยังไม่ต้องเสียบบอร์ด: `pio run` (ใช้โปรไฟล์ N16R8 ตามแบบวงจร).
4. ตรวจรุ่นโมดูลที่ติดตั้งจริงและการตั้งค่า Flash/PSRAM ก่อนอัปโหลด.
5. เมื่อยืนยันอุปกรณ์และการต่อจริงแล้ว ให้คัดลอก `secrets.example.h` เป็น `secrets.h`
   และกรอก Wi-Fi, Worker URL, DEVICE_TOKEN และ Root CA ที่ถูกต้อง.
6. ตั้งค่า GPIO และ profile servo หลังตรวจ wiring/แหล่งจ่ายไฟจริงเท่านั้น.
7. ทดสอบ Serial ด้วย `IDENTIFY`, `STATUS`, `STOP` ที่ 115200 baud.

## ตรวจสอบอัตโนมัติ

GitHub Actions จะ build ด้วย PlatformIO ทุกครั้งที่ push หรือเปิด Pull Request
โดยไม่ต้องใช้บอร์ดจริงหรือใส่ secrets ใน workflow. ตรวจผลได้จากแท็บ **Actions**
ของ GitHub repository.

## ความปลอดภัย

- ห้าม commit `secrets.h`, Wi-Fi password, `DEVICE_TOKEN`,
  Cloudflare API token หรือ Turnstile secret.
- ห้ามปิด TLS certificate verification.
- ห้ามเปิด remote control/motor output ก่อนผ่านการทดสอบฮาร์ดแวร์.
- ต้องมีวิธีตัดไฟมอเตอร์ฉุกเฉิน; network timeout ไม่รับประกันว่าจะหยุดมอเตอร์ได้.

## หมายเหตุเรื่องเซนเซอร์

Application repository ระบุ `pirDetected` ใน heartbeat และเอกสารโครงงานเดิม
มีทั้งการกล่าวถึง PIR และ HC-SR04. BOM ที่ตรวจสอบระบุชิ้นส่วนบน PCB
แต่ยังไม่ระบุรุ่น Servo หรือเซนเซอร์ภายนอก. โค้ดจึงยังไม่อ่านเซนเซอร์จริง
และส่ง `pirDetected: null` จนกว่าจะยืนยันชนิดเซนเซอร์และขา GPIO ที่ต่อจริง.
