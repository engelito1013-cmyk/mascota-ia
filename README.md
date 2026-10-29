# Mascota IA — firmware

Archivos para compilar el firmware de la mascota (Waveshare ESP32-S3-LCD-2 + INMP441 + MAX98357A) sobre el proyecto abierto [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32).

- `board/mascota-esp32-s3-lcd-2/config.h` → pines. Si cambias un cable de sitio, cambia aquí el número de GPIO.
- `board/mascota-esp32-s3-lcd-2/mascota_board.cc` → comportamiento de la placa (botones, reacción al agitarla, batería).
- `.github/workflows/compilar-mascota.yml` → compila en GitHub: pestaña **Actions → Compilar firmware de la mascota → Run workflow**.
- Resultado: artefacto `firmware-mascota` con `merged-binary.bin`, que se graba en la dirección `0x0`.
