# SPIFFS Implementation Summary

## ✅ Completed Changes

### 1. Partition Table Created
**File**: `partitions.csv`
- Allocated 1MB for SPIFFS partition
- Factory app: 2MB
- SPIFFS storage: 1MB

### 2. PlatformIO Configuration Updated
**File**: `platformio.ini`
- Added `board_build.partitions = partitions.csv`
- Added `post:upload_spiffs.py` to extra_scripts

### 3. CMakeLists.txt Updated
**File**: `src/CMakeLists.txt`
- Removed `EMBED_FILES` directive (no longer embedding files)
- Now using standard component registration

### 4. Web Server Updated
**File**: `src/web_server.c`
- Removed embedded file declarations
- Added `#include "esp_spiffs.h"` and `#include <sys/stat.h>`
- Created `init_spiffs()` function to mount SPIFFS
- Created `serve_spiffs_file()` helper function
- Updated handlers to read from SPIFFS:
  - `index_handler()` → reads `/spiffs/index.min.html`
  - `style_handler()` → reads `/spiffs/style.min.css`
  - `app_js_handler()` → reads `/spiffs/app.min.js`

### 5. Web Server Header Updated
**File**: `src/web_server.h`
- Added `esp_err_t init_spiffs(void);` declaration
- Added `#include "esp_err.h"`

### 6. Main Application Updated
**File**: `src/main.c`
- Added SPIFFS initialization before starting web server
- Calls `init_spiffs()` with error checking

### 7. SPIFFS Upload Script Created
**File**: `upload_spiffs.py`
- Parses `partitions.csv` to find SPIFFS offset and size
- Uses `mkspiffs` tool to create SPIFFS image from `data/` folder
- Uses `esptool.py` to upload SPIFFS image to ESP32
- Registered as custom PlatformIO target: `uploadfs`

### 8. Documentation Created
**Files**: 
- `SPIFFS_README.md` - Complete guide for using SPIFFS
- Includes troubleshooting, workflow, and quick reference

## 📁 File Structure

```
Autowater/
├── data/                          # Web files (source)
│   ├── index.html                # Original HTML
│   ├── index.min.html            # Minified → uploaded to SPIFFS
│   ├── style.css                 # Original CSS
│   ├── style.min.css             # Minified → uploaded to SPIFFS
│   ├── app.js                    # Original JS
│   └── app.min.js                # Minified → uploaded to SPIFFS
├── partitions.csv                # ✨ NEW: Flash partition table
├── platformio.ini                # ✅ UPDATED: Added SPIFFS config
├── build_minify.py               # ✅ EXISTING: Minifies web files
├── upload_spiffs.py              # ✨ NEW: Uploads SPIFFS image
├── minify_web.js                 # ✅ EXISTING: Minification logic
├── SPIFFS_README.md              # ✨ NEW: Documentation
└── src/
    ├── CMakeLists.txt            # ✅ UPDATED: Removed EMBED_FILES
    ├── main.c                    # ✅ UPDATED: Calls init_spiffs()
    ├── web_server.c              # ✅ UPDATED: Reads from SPIFFS
    └── web_server.h              # ✅ UPDATED: Added init_spiffs()
```

## 🚀 How to Use

### First Time Setup
```bash
# 1. Install npm dependencies (if not already done)
npm install

# 2. Build firmware (auto-minifies web files)
pio run

# 3. Upload firmware
pio run -t upload

# 4. Upload SPIFFS (web files)
pio run -t uploadfs
```

### Update Web Files Only
```bash
# Build SPIFFS and upload
pio run -t uploadfs
```

### Update Firmware Only
```bash
# Build and upload firmware
pio run -t upload
```

### Update Both
```bash
# Upload firmware
pio run -t upload

# Upload SPIFFS
pio run -t uploadfs
```

## 🔍 How It Works

### Build Process
1. **Pre-build**: `build_minify.py` runs `npm run minify`
   - Minifies `index.html` → `index.min.html`
   - Minifies `style.css` → `style.min.css`
   - Minifies `app.js` → `app.min.js`

2. **Build**: PlatformIO compiles firmware
   - No web files embedded in binary
   - Firmware size is smaller

3. **Upload**: Standard firmware upload via serial

4. **Upload FS**: `upload_spiffs.py` creates and uploads SPIFFS
   - Reads `partitions.csv` for SPIFFS offset/size
   - Uses `mkspiffs` to create filesystem image from `data/`
   - Uses `esptool.py` to flash image to storage partition

### Runtime Process
1. **Boot**: ESP32 starts, runs `app_main()`

2. **Init SPIFFS**: `init_spiffs()` mounts SPIFFS partition
   ```
   /spiffs/
   ├── index.min.html
   ├── style.min.css
   └── app.min.js
   ```

3. **Web Server**: HTTP handlers serve files from `/spiffs/`
   - GET `/` → reads `/spiffs/index.min.html`
   - GET `/style.css` → reads `/spiffs/style.min.css`
   - GET `/app.js` → reads `/spiffs/app.min.js`

4. **API**: REST endpoints work as before
   - GET `/api/status` → returns relay states
   - GET `/api/relay?id=X&action=Y` → controls relays

## ⚠️ Important Notes

1. **Must upload SPIFFS**: After uploading firmware, you MUST run `pio run -t uploadfs` at least once, or the web interface won't load.

2. **File names matter**: The code expects:
   - `/spiffs/index.min.html`
   - `/spiffs/style.min.css`
   - `/spiffs/app.min.js`

3. **Partition erased on firmware upload**: Some upload methods may erase SPIFFS partition. If web interface stops working after firmware update, re-run `pio run -t uploadfs`.

4. **SPIFFS auto-formats**: If SPIFFS partition is corrupted or empty, it will auto-format on first mount (see `format_if_mount_failed = true`).

## ✨ Benefits

### Before (Embedded Files)
- ❌ Web files compiled into firmware
- ❌ Larger firmware binary
- ❌ Must recompile to update web UI
- ❌ HTML/CSS/JS mixed with C code

### After (SPIFFS)
- ✅ Web files separate from firmware
- ✅ Smaller firmware binary
- ✅ Update web UI without recompiling
- ✅ Clean separation of concerns
- ✅ Proper syntax highlighting for web files
- ✅ Independent upload of web assets

## 🧪 Testing

After uploading both firmware and SPIFFS:

1. **Check Serial Monitor**:
   ```
   I (1234) WEB: Initializing SPIFFS
   I (1256) WEB: SPIFFS: total: 1048576, used: 12345
   I (1267) WEB: Web server started with API endpoints
   ```

2. **Access Web Interface**: `http://<esp32-ip>/`
   - Should see the web UI
   - Relay controls should work
   - Status should update

3. **Check SPIFFS Files**:
   - All three handlers should work:
     - `/` (index.html)
     - `/style.css`
     - `/app.js`

## 📊 Expected Output

### Build Output
```
Running web asset minification...
📄 Processing index.html...
  ✓ index.min.html created
  📊 5234 → 2891 bytes (saved 2343 bytes, 44.8%)
✅ Minification complete!

Building...
[SUCCESS] Firmware built
```

### Upload FS Output
```
Building and uploading SPIFFS image...
SPIFFS partition offset: 0x310000
SPIFFS partition size: 0x100000 (1048576 bytes)

Creating SPIFFS image from data/...
SPIFFS image created: .pio/build/esp32-c6-devkitm-1/spiffs.bin

Uploading SPIFFS image to 0x310000...
SPIFFS upload completed successfully!
```

### Serial Monitor on Boot
```
I (345) WEB: Initializing SPIFFS
I (367) WEB: SPIFFS: total: 1048576, used: 8543
I (378) WEB: Web server started with API endpoints
I (389) APP: Relay web server started!
```

## 🎉 You're Done!

SPIFFS is now fully configured and ready to use. Your web interface files are stored in flash memory and can be updated independently of your firmware.

