# Web Interface Files

This directory contains the web interface files for the Autowater ESP32 project.

## Files

- **index.html** - The main HTML page for the web interface
- **style.css** - CSS styles for the UI with a modern dark theme
- **app.js** - JavaScript for controlling relays and updating status

## How It Works

These files are embedded into the ESP32 firmware at compile time using the ESP-IDF build system. The CMakeLists.txt file in the `src/` directory specifies these files as `EMBED_FILES`, which converts them into binary data that can be served by the web server.

## Making Changes

You can edit these files directly with proper syntax highlighting and formatting. After making changes:

1. Clean the build if needed: `pio run -t clean`
2. Build the project: `pio run`
3. Upload to the ESP32: `pio run -t upload`

The build system will automatically embed the updated files into the firmware.

## API Endpoints

The web interface communicates with these REST API endpoints:

- `GET /api/status` - Get the status of all relays
- `GET /api/relay?id=<relay_id>&action=<on|off|toggle>` - Control a specific relay

## Development

For development, you can use any text editor or IDE with HTML/CSS/JavaScript support. The files will have proper syntax highlighting and formatting, unlike when they were embedded directly in C code.

