# Device integration checklist

อ้างอิง `DEVICE_SETUP.md` และ `worker/index.js` จาก Application repository.

- [x] แบบวงจรระบุ ESP32-S3-WROOM-1 (N16R8); BOM ระบุ ESP32-S3-WROOM-1
- [ ] ตรวจฉลากโมดูลที่ติดตั้งจริงและยืนยัน suffix N16R8 กับ BOM
- [x] เพิ่ม PlatformIO board profile สำหรับ PCB: 16 MB Flash, 8 MB Octal PSRAM
- [ ] Build และอัปโหลดทดสอบกับ PCB จริง; การ build ไม่ยืนยันการทำงานของวงจร
- [ ] ระบุรุ่น Servo และยืนยันว่าเป็นรุ่น continuous rotation หากต้องหมุนแท่นต่อเนื่อง
- [ ] ระบุรุ่นและชนิดเซนเซอร์จริง (PIR หรือ HC-SR04); BOM ที่ตรวจยังไม่ระบุอุปกรณ์ภายนอก
- [ ] ยืนยันหน้าที่และทิศทางขาของแต่ละ connector, GPIO, แหล่งจ่ายไฟ และวงจรขับ
- [ ] ตรวจ pin mapping ตาม schematic: U2 มี GPIO1/3.3V/GND; U8 มี GPIO17/GPIO18/GND
- [ ] ยืนยันการต่อ USB ที่ GPIO19/GPIO20 และไม่ใช้ขาเหล่านี้ซ้ำ
- [ ] กำหนด `SERVO_PIN` และปรับเทียบ STOP/ROTATE pulse กับเวลา
- [ ] เตรียม `secrets.h` ในเครื่องเท่านั้น
- [ ] ทดสอบ `IDENTIFY`, `STATUS`, `STOP` ผ่าน Serial
- [ ] ทดสอบ heartbeat และ polling โดยยังปิดเอาต์พุตมอเตอร์
- [ ] ทดสอบการหยุดเมื่อ Wi-Fi หลุดและเมื่อ timeout
- [ ] เปิดมอเตอร์/remote control หลังผ่านการตรวจความปลอดภัยเท่านั้น

## Protocol

Heartbeat JSON:
`{"motorReady":false,"pirDetected":null,"firmware":"0.1.0"}`

Next command response:
`{"command":{"id":"...","command":"rotate_360_once","issuedAt":0}}`

Result JSON:
`{"commandId":"...","status":"completed","message":"..."}`

`completed` เป็นผลการทำงานตาม timed profile ไม่ใช่การวัดองศาด้วย encoder.
ห้ามเปิด `MOTOR_OUTPUT_ENABLED` หรือ `REMOTE_CONTROL_ENABLED` จนกว่าจะยืนยัน
รุ่น Servo, pin mapping, แหล่งจ่ายไฟ และการหยุดฉุกเฉิน พร้อมทดสอบอย่างปลอดภัย.
