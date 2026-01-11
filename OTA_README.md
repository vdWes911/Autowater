# OTA Update Guide for Autowater ESP32 Project

This project supports Over-The-Air (OTA) updates for both the firmware (app) and the filesystem (SPIFFS). This allows you to update the device wirelessly without needing a USB connection.

## Prerequisites

- The device must be connected to the same Wi-Fi network as your computer.
- You must have performed the **initial serial flash** after the OTA partition table was added. This is because the partition table itself cannot be updated via OTA.

## How to Update

1.  **Build the assets**: Run `pio run` to compile the firmware and minify the web files.
2.  **Access the Update Page**: Open your browser and go to `http://<esp32-ip>/update`.
3.  **Select Update Type**:
    - **Firmware**: To update the C code logic. Use the `.bin` file found in `.pio/build/esp32-s3-devkitc-1/firmware.bin`.
    - **Filesystem**: To update the web interface (HTML/CSS/JS). Use the `spiffs.bin` file (see below on how to generate it).
4.  **Upload**: Select the file and click "Start Update". The progress bar will show the upload status.
5.  **Reboot**: Once the upload is complete, the device will automatically reboot.

## Generating Update Binaries

### Firmware Binary
The firmware binary is automatically generated whenever you run `pio run`.
Path: `.pio/build/esp32-s3-devkitc-1/firmware.bin`

### SPIFFS Binary
To generate the SPIFFS image binary without uploading it via USB:
```bash
pio run -t buildfs
```
Path: `.pio/build/esp32-s3-devkitc-1/spiffs.bin`

## Technical Details

### Partition Table
The project uses a custom partition table with two OTA app partitions and one SPIFFS partition:
- `ota_0`: App partition 1 (4MB)
- `ota_1`: App partition 2 (4MB)
- `storage`: SPIFFS partition (7MB)
- `otadata`: Stores information about which app partition to boot from.

### Backend Implementation
The OTA update is handled by the `/api/ota` endpoint in `src/web_server.c`.
- For **Firmware**: It uses the native `esp_ota_ops` component to flash the next available OTA partition and sets it as the boot partition.
- For **Filesystem**: It directly writes the binary to the `storage` partition using `esp_partition` APIs.

## Troubleshooting

- **Update Fails**: Ensure the file you are uploading matches the selected type. Uploading a firmware binary as a filesystem update will corrupt the SPIFFS partition.
- **Device doesn't reboot**: Check the Serial Monitor if possible to see if there were any errors during the flash process.
- **UI doesn't load after SPIFFS update**: You might have uploaded an invalid SPIFFS image. Re-flash via USB using `pio run -t uploadfs`.
