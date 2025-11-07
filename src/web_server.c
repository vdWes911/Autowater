#include "web_server.h"
#include "relay_controller.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "WEB";

// Static buffer in SRAM (allocated at compile time, not on stack)
static char html_buffer[8192];

// Relay name mappings
static const char *relay_names[NUM_RELAYS] = {
    "Plants",
    "Front Lawn",
    "Grass",
    "Patio Grass"
};

static esp_err_t relay_handler(httpd_req_t *req) {
    char buf[128];
    int ret;
    
    // Validate content length
    if (req->content_len >= sizeof(buf)) {
        ESP_LOGW(TAG, "Request too large: %d bytes", req->content_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }
    
    ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        } else {
            httpd_resp_send_500(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Relay request: %s", buf);
    
    int relay = -1;
    char action[8] = {0};

    if (sscanf(buf, "relay=%d&action=%7s", &relay, action) != 2) {
        ESP_LOGW(TAG, "Invalid request format");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid format");
        return ESP_FAIL;
    }
    
    if (relay < 0 || relay >= NUM_RELAYS) {
        ESP_LOGW(TAG, "Invalid relay number: %d", relay);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid relay number");
        return ESP_FAIL;
    }
    
    if (strcmp(action, "on") == 0) {
        ESP_LOGI(TAG, "Relay %d turned ON", relay + 1);
        relay_on(relay);
    } else if (strcmp(action, "off") == 0) {
        ESP_LOGI(TAG, "Relay %d turned OFF", relay + 1);
        relay_off(relay);
    } else {
        ESP_LOGW(TAG, "Invalid action: %s", action);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid action");
        return ESP_FAIL;
    }

    // Redirect back to main page
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// API endpoint to get relay status (JSON)
static esp_err_t api_status_handler(httpd_req_t *req) {
    char json[512];
    int len = snprintf(json, sizeof(json), "{\"relays\":[");
    
    for (int i = 0; i < NUM_RELAYS; i++) {
        bool state = relay_get_state(i);
        len += snprintf(json + len, sizeof(json) - len,
            R"(%s{"id":%d,"state":"%s"})",
            (i > 0) ? "," : "",
            i,
            state ? "on" : "off"
        );
    }
    
    snprintf(json + len, sizeof(json) - len, "]}");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// API endpoint to control relay (REST API)
static esp_err_t api_relay_handler(httpd_req_t *req) {
    char buf[128];
    
    // Parse query string from URI (e.g., /api/relay?id=0&action=on)
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0) {
        char *query = malloc(query_len + 1);
        if (httpd_req_get_url_query_str(req, query, query_len + 1) == ESP_OK) {
            char action[8] = {0};
            char relay_str[8] = {0};
            
            // Parse parameters
            if (httpd_query_key_value(query, "id", relay_str, sizeof(relay_str)) == ESP_OK &&
                httpd_query_key_value(query, "action", action, sizeof(action)) == ESP_OK) {
                
                int relay = atoi(relay_str);
                
                if (relay < 0 || relay >= NUM_RELAYS) {
                    free(query);
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid relay ID");
                    return ESP_FAIL;
                }
                
                if (strcmp(action, "on") == 0) {
                    relay_on(relay);
                    ESP_LOGI(TAG, "API: Relay %d turned ON", relay);
                } else if (strcmp(action, "off") == 0) {
                    relay_off(relay);
                    ESP_LOGI(TAG, "API: Relay %d turned OFF", relay);
                } else if (strcmp(action, "toggle") == 0) {
                    relay_toggle(relay);
                    ESP_LOGI(TAG, "API: Relay %d toggled", relay);
                } else {
                    free(query);
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid action");
                    return ESP_FAIL;
                }
                
                // Return JSON response
                snprintf(buf, sizeof(buf), 
                    "{\"relay\":%d,\"state\":\"%s\",\"success\":true}",
                    relay,
                    relay_get_state(relay) ? "on" : "off"
                );
                
                httpd_resp_set_type(req, "application/json");
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
                free(query);
                return ESP_OK;
            }
            free(query);
        }
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing parameters");
    return ESP_FAIL;
}

static esp_err_t index_handler(httpd_req_t *req) {
    char chunk[512];
    
    // Send header and CSS
    const char *header = 
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>Relay Control</title>"
        "<style>"
        "* { margin: 0; padding: 0; box-sizing: border-box; }"
        "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif; "
        "background: linear-gradient(135deg, #1e3c72 0%, #2a5298 50%, #7e22ce 100%); "
        "min-height: 100vh; padding: 20px; }"
        ".container { max-width: 600px; margin: 0 auto; }"
        "h1 { color: #fff; text-align: center; margin-bottom: 35px; font-size: 32px; font-weight: 600; "
        "text-shadow: 0 2px 10px rgba(0,0,0,0.3); letter-spacing: -0.5px; }"
        ".card { background: rgba(255, 255, 255, 0.15); backdrop-filter: blur(10px); "
        "border: 1px solid rgba(255, 255, 255, 0.25); border-radius: 16px; padding: 24px; "
        "margin-bottom: 16px; box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37); "
        "transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); }"
        ".card:hover { transform: translateY(-4px); "
        "box-shadow: 0 12px 40px 0 rgba(0, 0, 0, 0.45); "
        "background: rgba(255, 255, 255, 0.20); }"
        ".relay-header { display: flex; justify-content: space-between; align-items: center; "
        "margin-bottom: 18px; }"
        ".relay-name { font-size: 20px; font-weight: 600; color: rgba(255, 255, 255, 0.95); "
        "letter-spacing: -0.3px; }"
        ".status { display: inline-flex; align-items: center; padding: 6px 14px; border-radius: 12px; "
        "font-size: 12px; font-weight: 600; letter-spacing: 0.5px; transition: all 0.3s; "
        "text-transform: uppercase; }"
        ".status::before { content: ''; width: 6px; height: 6px; border-radius: 50%; "
        "margin-right: 6px; }"
        ".status.on { background: rgba(74, 222, 128, 0.25); color: #86efac; "
        "border: 1px solid rgba(74, 222, 128, 0.4); }"
        ".status.on::before { background: #86efac; box-shadow: 0 0 6px rgba(134, 239, 172, 0.6); }"
        ".status.off { background: rgba(148, 163, 184, 0.25); color: #cbd5e1; "
        "border: 1px solid rgba(148, 163, 184, 0.4); }"
        ".status.off::before { background: #94a3b8; }"
        ".btn-group { display: flex; gap: 12px; }"
        "button { flex: 1; padding: 14px 20px; border: none; border-radius: 12px; "
        "font-size: 14px; font-weight: 600; cursor: pointer; "
        "transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); "
        "text-transform: uppercase; letter-spacing: 0.5px; position: relative; overflow: hidden; "
        "backdrop-filter: blur(10px); }"
        "button::before { content: ''; position: absolute; top: 50%; left: 50%; "
        "width: 0; height: 0; border-radius: 50%; background: rgba(255,255,255,0.2); "
        "transition: width 0.6s, height 0.6s, top 0.6s, left 0.6s; }"
        "button:active::before { width: 300px; height: 300px; top: -150px; left: -150px; }"
        "button:disabled { opacity: 0.5; cursor: not-allowed; }"
        "button:active:not(:disabled) { transform: scale(0.97); }"
        ".btn-on { background: rgba(74, 222, 128, 0.22); color: #a7f3d0; "
        "border: 1px solid rgba(74, 222, 128, 0.4); "
        "box-shadow: 0 4px 16px rgba(74, 222, 128, 0.25); }"
        ".btn-on:hover:not(:disabled) { background: rgba(74, 222, 128, 0.32); "
        "box-shadow: 0 6px 24px rgba(74, 222, 128, 0.35); transform: translateY(-2px); "
        "border-color: rgba(74, 222, 128, 0.6); }"
        ".btn-off { background: rgba(252, 165, 165, 0.22); color: #fecaca; "
        "border: 1px solid rgba(252, 165, 165, 0.4); "
        "box-shadow: 0 4px 16px rgba(252, 165, 165, 0.25); }"
        ".btn-off:hover:not(:disabled) { background: rgba(252, 165, 165, 0.32); "
        "box-shadow: 0 6px 24px rgba(252, 165, 165, 0.35); transform: translateY(-2px); "
        "border-color: rgba(252, 165, 165, 0.6); }"
        ".loading { pointer-events: none; }"
        ".loading::after { content: ''; position: absolute; right: 12px; top: 50%; "
        "width: 12px; height: 12px; margin-top: -6px; border: 2px solid currentColor; "
        "border-right-color: transparent; border-radius: 50%; "
        "animation: spinner 0.8s linear infinite; }"
        "@keyframes spinner { to { transform: rotate(360deg); } }"
        "@media (max-width: 480px) { h1 { font-size: 26px; } .relay-name { font-size: 18px; } "
        "button { font-size: 13px; padding: 12px 16px; } }"
        "</style>"
        "</head>"
        "<body>"
        "<div class='container'>"
        "<h1>💧 Watering Control</h1>";
    
    httpd_resp_send_chunk(req, header, strlen(header));

    ESP_LOGI(TAG, "Index request");

    // Send each relay card as a chunk
    for (int i = 0; i < NUM_RELAYS; i++) {
        bool is_on = relay_get_state(i);
        const char *state = is_on ? "ON" : "OFF";
        const char *status_class = is_on ? "on" : "off";
        
        int len = snprintf(chunk, sizeof(chunk),
            "<div class='card' id='relay-%d'>"
            "<div class='relay-header'>"
            "<span class='relay-name'>%s</span>"
            "<span class='status %s' id='status-%d'>%s</span>"
            "</div>"
            "<div class='btn-group'>"
            "<button class='btn-on' onclick='toggleRelay(%d, \"on\")'>Turn On</button>"
            "<button class='btn-off' onclick='toggleRelay(%d, \"off\")'>Turn Off</button>"
            "</div>"
            "</div>",
            i, relay_names[i], status_class, i, state, i, i
        );
        
        httpd_resp_send_chunk(req, chunk, len);
    }

    // Send JavaScript and footer
    const char *footer = 
        "</div>"
        "<script>"
        "async function toggleRelay(id, action) {"
        "  const card = document.getElementById('relay-' + id);"
        "  const status = document.getElementById('status-' + id);"
        "  const buttons = card.querySelectorAll('button');"
        "  "
        "  buttons.forEach(btn => { btn.disabled = true; btn.classList.add('loading'); });"
        "  "
        "  try {"
        "    const response = await fetch(`/api/relay?id=${id}&action=${action}`);"
        "    const data = await response.json();"
        "    "
        "    if (data.success) {"
        "      const isOn = data.state === 'on';"
        "      status.textContent = isOn ? 'ON' : 'OFF';"
        "      status.className = isOn ? 'status on' : 'status off';"
        "    }"
        "  } catch (error) {"
        "    console.error('Error:', error);"
        "    alert('Failed to control relay. Please try again.');"
        "  } finally {"
        "    buttons.forEach(btn => { btn.disabled = false; btn.classList.remove('loading'); });"
        "  }"
        "}"
        ""
        "async function updateStatus() {"
        "  try {"
        "    const response = await fetch('/api/status');"
        "    const data = await response.json();"
        "    "
        "    data.relays.forEach(relay => {"
        "      const status = document.getElementById('status-' + relay.id);"
        "      if (status) {"
        "        const isOn = relay.state === 'on';"
        "        status.textContent = isOn ? 'ON' : 'OFF';"
        "        status.className = isOn ? 'status on' : 'status off';"
        "      }"
        "    });"
        "  } catch (error) {"
        "    console.error('Status update error:', error);"
        "  }"
        "}"
        ""
        "setInterval(updateStatus, 5000);"
        "</script>"
        "</body>"
        "</html>";
    
    httpd_resp_send_chunk(req, footer, strlen(footer));
    
    // End chunked response
    httpd_resp_send_chunk(req, NULL, 0);
    
    return ESP_OK;
}

void web_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Web UI endpoints
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t relay_uri = {
            .uri = "/relay",
            .method = HTTP_POST,
            .handler = relay_handler
        };
        httpd_register_uri_handler(server, &relay_uri);
        
        // API endpoints
        httpd_uri_t api_status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = api_status_handler
        };
        httpd_register_uri_handler(server, &api_status_uri);
        
        httpd_uri_t api_relay_uri = {
            .uri = "/api/relay",
            .method = HTTP_GET,
            .handler = api_relay_handler
        };
        httpd_register_uri_handler(server, &api_relay_uri);
        
        ESP_LOGI(TAG, "Web server started with API endpoints");
    }
}
