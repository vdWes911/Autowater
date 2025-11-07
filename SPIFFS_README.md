# SPIFFS Setup Guide for Autowater ESP32 Project

This project uses SPIFFS (SPI Flash File System) to store and serve web interface files (HTML, CSS, JavaScript) from the ESP32's flash memory.

## What Changed

### Previous Approach (Embedded Files)
- Web files were embedded directly into the firmware binary
- Files were compiled into the `.c` code
- Increased firmware size
- Required recompilation to update web files

### Current Approach (SPIFFS)
- Web files are stored in a separate flash partition
- Can be uploaded independently from firmware
- Smaller firmware size
- Easier to update web interface without recompiling

## Project Structure

```
Autowater/
├── data/                      # Web files (uploaded to SPIFFS)
│   ├── index.html            # Original HTML
│   ├── index.min.html        # Minified HTML (uploaded to ESP32)
│   ├── style.css             # Original CSS
│   ├── style.min.css         # Minified CSS (uploaded to ESP32)
│   ├── app.js                # Original JavaScript
│   └── app.min.js            # Minified JavaScript (uploaded to ESP32)
├── partitions.csv            # Partition table with SPIFFS
├── build_minify.py           # Pre-build script (minifies web files)
├── upload_spiffs.py          # SPIFFS upload script
├── minify_web.js             # Node.js minification script
└── src/
    ├── main.c                # Initializes SPIFFS
    ├── web_server.c          # Reads files from SPIFFS
    └── web_server.h          # SPIFFS initialization header
```

## Partition Table

The `partitions.csv` file defines the flash memory layout:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x200000,  # 2MB for firmware
storage,  data, spiffs,  ,        0x100000,  # 1MB for SPIFFS
```

- **NVS**: Non-volatile storage for Wi-Fi credentials
- **factory**: Main firmware partition (2MB)
- **storage**: SPIFFS partition for web files (1MB)

## Build and Upload Process

### Step 1: Build Firmware
```bash
pio run
```

This will:
1. Run `build_minify.py` (minifies HTML/CSS/JS)
2. Compile the firmware
3. Create the binary

### Step 2: Upload Firmware
```bash
pio run -t upload
```

This uploads the firmware to the ESP32.

### Step 3: Upload SPIFFS
```bash
pio run -t uploadfs
```

This will:
1. Create a SPIFFS image from the `data/` folder
2. Upload the SPIFFS image to the storage partition

**Important**: You must upload SPIFFS at least once after building the firmware!

## How SPIFFS Works in the Code

### Initialization (main.c)
```c
void app_main(void) {
    // ... other initialization ...
    
    // Initialize SPIFFS before starting web server
    if (init_spiffs() != ESP_OK) {
        ESP_LOGE("APP", "Failed to initialize SPIFFS");
    }
    
    // Start HTTP server
    web_server_start();
}
```

### Mounting SPIFFS (web_server.c)
```c
esp_err_t init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",         // Mount point
        .partition_label = NULL,         // Use default partition
        .max_files = 5,                  // Max open files
        .format_if_mount_failed = true   // Auto-format on first use
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    // ... error checking ...
}
```

### Serving Files (web_server.c)
```c
static esp_err_t serve_spiffs_file(httpd_req_t *req, const char *filepath, const char *content_type) {
    FILE *f = fopen(filepath, "r");  // Open from /spiffs/...
    // ... read and send file ...
}

static esp_err_t index_handler(httpd_req_t *req) {
    return serve_spiffs_file(req, "/spiffs/index.min.html", "text/html");
}
```

## Development Workflow

### Updating Web Files Only

If you only changed HTML/CSS/JS:

```bash
npm run minify          # Minify the files (optional, build does this)
pio run -t uploadfs     # Upload SPIFFS only
```

No need to rebuild/upload firmware!

### Updating Firmware Only

If you only changed C code:

```bash
pio run -t upload       # Build and upload firmware
```

SPIFFS partition remains unchanged.

### Full Update

If you changed both:

```bash
pio run -t upload       # Upload firmware
pio run -t uploadfs     # Upload SPIFFS
```

## Troubleshooting

### "Failed to find SPIFFS partition"
- Check that `partitions.csv` exists
- Verify `board_build.partitions = partitions.csv` is in `platformio.ini`
- Re-upload firmware with `pio run -t upload`

### "Failed to open file"
- Make sure you ran `pio run -t uploadfs`
- Check that minified files exist in `data/` folder
- Verify files are named correctly (index.min.html, style.min.css, app.min.js)

### "SPIFFS partition information" shows 0 bytes used
- You haven't uploaded SPIFFS yet: run `pio run -t uploadfs`

### Web page shows 404
- Confirm SPIFFS is initialized in main.c
- Check ESP32 serial output for SPIFFS mount errors
- Verify file paths in web_server.c match actual files

## File Size Optimization

The minification process typically reduces file sizes by 30-50%:

Example output:
```
📄 Processing index.html...
  ✓ index.min.html created
  📊 5234 → 2891 bytes (saved 2343 bytes, 44.8%)

📄 Processing style.css...
  ✓ style.min.css created
  📊 8764 → 4123 bytes (saved 4641 bytes, 53.0%)

📄 Processing app.js...
  ✓ app.min.js created
  📊 3421 → 1876 bytes (saved 1545 bytes, 45.2%)
```

## Quick Reference

| Command | Purpose |
|---------|---------|
| `pio run` | Build firmware (auto-minifies web files) |
| `pio run -t upload` | Upload firmware to ESP32 |
| `pio run -t uploadfs` | Upload SPIFFS (web files) to ESP32 |
| `npm run minify` | Manually minify web files |
| `pio device monitor` | View serial output |

## First Time Setup Checklist

- [ ] Install npm dependencies: `npm install`
- [ ] Build firmware: `pio run`
- [ ] Upload firmware: `pio run -t upload`
- [ ] Upload SPIFFS: `pio run -t uploadfs`
- [ ] Connect to ESP32's Wi-Fi or check serial for IP
- [ ] Access web interface at `http://<esp32-ip>/`

## Notes

- The SPIFFS partition is 1MB, plenty for small web files
- Minified files are automatically created during build
- Original files (index.html, style.css, app.js) are kept for development
- Only minified files (.min.*) are uploaded to ESP32
- SPIFFS is mounted at `/spiffs` in the ESP32 filesystem

