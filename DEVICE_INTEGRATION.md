# Device integration checklist

อ้างอิง `DEVICE_SETUP.md` และ `worker/index.js` จาก Application repository.

- [ ] ยืนยันว่าบอร์ดจริงเป็น ESP32-S3 DevKitC-1 ตามโปรไฟล์ใน PlatformIO
- [ ] ถ่ายรูป/อ่านฉลากบอร์ดและ Servo
- [ ] ยืนยันว่า Servo เป็นรุ่น continuous rotation หากต้องหมุนแท่นต่อเนื่อง
- [ ] ยืนยัน GPIO, แหล่งจ่ายไฟ และวงจรขับ
- [ ] ยืนยันว่าเซนเซอร์เป็น PIR หรือ HC-SR04 (เอกสารเดิมระบุไม่ตรงกัน)
- [ ] กำหนด `SERVO_PIN` และปรับเทียบ STOP/ROTATE pulse กับเวลา
- [ ] เตรียม `secrets.h` ในเครื่องเท่านั้น
- [ ] ทดสอบ `IDENTIFY`, `STATUS`, `STOP` ผ่าน Serial
- [ ] ทดสอบ heartbeat และ polling โดยยังปิดเอาต์พุตมอเตอร์
- [ ] ทดสอบการหยุดเมื่อ Wi-Fi หลุดและเมื่อ timeout
- [ ] เปิดมอเตอร์/remote control หลังผ่านการตรวจความปลอดภัยเท่านั้น

## Protocol

Heartbeat JSON:
`{"motorReady":true,"pirDetected":false,"firmware":"0.1.0"}`

Next command response:
`{"command":{"id":"...","command":"rotate_360_once","issuedAt":0}}`

Result JSON:
`{"commandId":"...","status":"completed","message":"..."}`

`completed` เป็นผลการทำงานตาม timed profile ไม่ใช่การวัดองศาด้วย encoder.
