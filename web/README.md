# Web Interface Files

This directory contains the web interface files for the Autowater ESP32 project.

## Files

- **index.html** - The main HTML page for the web interface
- **style.css** - CSS styles for the UI with a modern dark theme
- **app.js** - JavaScript for controlling relays and updating status

## How It Works

These files are minified into the `data/` directory and then uploaded to the ESP32 SPIFFS partition using PlatformIO's `uploadfs` target. This link is defined in `platformio.ini` by the `data_dir = data` setting in the `[platformio]` section and integrated into the build via `spiffs_create_partition_image` in `CMakeLists.txt`. The web server then serves these minified files from the `/spiffs` mount point.

## Making Changes

You can edit these files directly with proper syntax highlighting and formatting. After making changes:

1.  **Minify and Build**: `pio run` (automatically runs `npm run minify`)
2.  **Upload Firmware**: `pio run -t upload`
3.  **Upload Web Files**: `pio run -t uploadfs` (This is required whenever web files are updated)

The `uploadfs` command will create a SPIFFS image from the `data/` directory and upload it to the ESP32.

## API Endpoints

The web interface communicates with these REST API endpoints:

- `GET /api/status` - Get the status of all relays
- `GET /api/relay?id=<relay_id>&action=<on|off|toggle>` - Control a specific relay

## Development

For development, you can use any text editor or IDE with HTML/CSS/JavaScript support. The files will have proper syntax highlighting and formatting, unlike when they were embedded directly in C code.

