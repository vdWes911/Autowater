#include "web_server.h"
#include "relay_controller.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "WEB";

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

static esp_err_t index_handler(httpd_req_t *req) {
    char chunk[512];
    
    // Send header
    const char *header = 
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>"
        "body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; background: #f4f4f9; }"
        "h1 { color: #333; text-align: center; margin-bottom: 30px; }"
        ".relay-card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 15px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }"
        ".relay-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }"
        ".relay-name { font-size: 18px; font-weight: bold; color: #555; }"
        ".relay-status { padding: 5px 15px; border-radius: 20px; font-size: 14px; font-weight: bold; }"
        ".status-on { background: #4CAF50; color: white; }"
        ".status-off { background: #f44336; color: white; }"
        ".button-group { display: flex; gap: 10px; }"
        "button { flex: 1; padding: 12px 24px; font-size: 16px; font-weight: bold; border: none; border-radius: 5px; cursor: pointer; transition: all 0.3s; }"
        "button:active { transform: translateY(0); }"
        ".btn-on { background: #4CAF50; color: white; }"
        ".btn-on:hover { background: #45a049; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.2); }"
        ".btn-on:active { background: #3d8b40; }"
        ".btn-off { background: #f44336; color: white; }"
        ".btn-off:hover { background: #da190b; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.2); }"
        ".btn-off:active { background: #c41408; }"
        "</style>"
        "</head>"
        "<body>"
        "<h1>🔌 Relay Control</h1>";
    
    httpd_resp_send_chunk(req, header, strlen(header));

    printf("Index request\n");

    // Send each relay card
    for (int i = 0; i < NUM_RELAYS; i++) {
        bool is_on = relay_get_state(i);
        const char *state = is_on ? "ON" : "OFF";
        const char *status_class = is_on ? "status-on" : "status-off";
        
        int len = snprintf(chunk, sizeof(chunk),
            "<div class='relay-card'>"
            "<div class='relay-header'>"
            "<span class='relay-name'>Relay %d</span>"
            "<span class='relay-status %s'>%s</span>"
            "</div>"
            "<form method='POST' action='/relay'>"
            "<input type='hidden' name='relay' value='%d'>"
            "<div class='button-group'>"
            "<button type='submit' name='action' value='on' class='btn-on'>ON</button>"
            "<button type='submit' name='action' value='off' class='btn-off'>OFF</button>"
            "</div>"
            "</form>"
            "</div>",
            i + 1, status_class, state, i
        );
        
        httpd_resp_send_chunk(req, chunk, len);
    }

    // Send footer
    const char *footer = "</body></html>";
    httpd_resp_send_chunk(req, footer, strlen(footer));
    
    // End chunked response
    httpd_resp_send_chunk(req, NULL, 0);
    
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
        char *query = (char *)malloc(query_len + 1);
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
                    R"({"relay":%d,"state":"%s","success":true})",
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
