#include "web_server.h"
#include "relay_controller.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "WEB";

// Mount SPIFFS
esp_err_t init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS: total: %d, used: %d", total, used);
    }

    ESP_LOGI(TAG, "SPIFFS initialized");
    return ESP_OK;
}

static const char* mode_to_str(relay_mode_t mode) {
    switch (mode) {
        case RELAY_MODE_MANUAL: return "manual";
        case RELAY_MODE_TIMED: return "timed";
        default: return "off";
    }
}

// API endpoint to get relay status (JSON)
static esp_err_t api_status_handler(httpd_req_t *req) {
    char json[512];
    int len = snprintf(json, sizeof(json), "{\"relays\":[");
    
    for (int i = 0; i < NUM_RELAYS; i++) {
        relay_mode_t mode = relay_get_mode(i);
        uint32_t remaining = (mode != RELAY_MODE_OFF) ? relay_get_remaining_time(i) : 0;
        len += snprintf(json + len, sizeof(json) - len,
            R"(%s{"id":%d,"state":"%s","mode":"%s","rem":%u})",
            (i > 0) ? "," : "",
            i,
            (mode != RELAY_MODE_OFF) ? "on" : "off",
            mode_to_str(mode),
            (unsigned int)remaining
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
            char duration_str[16] = {0};

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
                } else if (strcmp(action, "timed") == 0) {
                    uint32_t duration = 0;
                    if (httpd_query_key_value(query, "duration", duration_str, sizeof(duration_str)) == ESP_OK) {
                        duration = atoi(duration_str);
                    }
                    relay_on_with_timer(relay, duration);
                    ESP_LOGI(TAG, "API: Relay %d turned ON for %u seconds", relay, (unsigned int)duration);
                } else {
                    free(query);
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid action");
                    return ESP_FAIL;
                }

                // Return JSON response
                relay_mode_t mode = relay_get_mode(relay);
                uint32_t remaining = (mode != RELAY_MODE_OFF) ? relay_get_remaining_time(relay) : 0;
                snprintf(buf, sizeof(buf),
                    "{\"relay\":%d,\"state\":\"%s\",\"mode\":\"%s\",\"rem\":%u,\"success\":true}",
                    relay,
                    (mode != RELAY_MODE_OFF) ? "on" : "off",
                    mode_to_str(mode),
                    (unsigned int)remaining
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

// Helper function to serve files from SPIFFS
static esp_err_t serve_spiffs_file(httpd_req_t *req, const char *filepath, const char *content_type) {
    ESP_LOGI(TAG, "Serving file: %s", filepath);
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGE(TAG, "File not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "File opened successfully, size: %ld", st.st_size);

    httpd_resp_set_type(req, content_type);

    char buf[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, read_bytes) != ESP_OK) {
            fclose(f);
            ESP_LOGE(TAG, "File sending failed");
            httpd_resp_sendstr_chunk(req, NULL);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t api_routines_handler(httpd_req_t *req) {
    const char* filepath = "/spiffs/routines.json";
    if (req->method == HTTP_GET) {
        struct stat st;
        if (stat(filepath, &st) != 0) {
            // If file doesn't exist, return empty array
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "[]");
            return ESP_OK;
        }
        return serve_spiffs_file(req, filepath, "application/json");
    }

    if (req->method == HTTP_POST) {
        int total_len = req->content_len;
        int remaining = total_len;
        
        if (total_len <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content length required");
            return ESP_FAIL;
        }

        if (total_len > 4096) { // Safety limit
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
            return ESP_FAIL;
        }

        char *buf = malloc(total_len + 1);
        if (buf == NULL) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
            return ESP_FAIL;
        }

        int received = 0;
        while (remaining > 0) {
            int ret = httpd_req_recv(req, buf + received, remaining);
            if (ret <= 0) {
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
                free(buf);
                return ESP_FAIL;
            }
            received += ret;
            remaining -= ret;
        }
        buf[received] = '\0';

        FILE *f = fopen(filepath, "w");
        if (f == NULL) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file for writing");
            return ESP_FAIL;
        }
        fputs(buf, f);
        fclose(f);
        free(buf);
        
        httpd_resp_sendstr(req, "{\"success\":true}");
        return ESP_OK;
    }
    return ESP_FAIL;
}

// API endpoint for OTA updates
static esp_err_t api_ota_handler(httpd_req_t *req) {
    char buf[1024];
    int remaining = req->content_len;
    int total_len = remaining;
    
    if (total_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content length required");
        return ESP_FAIL;
    }

    // Check if it's firmware or spiffs
    bool is_spiffs = false;
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char type[16];
        if (httpd_query_key_value(query, "type", type, sizeof(type)) == ESP_OK) {
            if (strcmp(type, "spiffs") == 0) {
                is_spiffs = true;
            }
        }
    }

    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;
    
    if (is_spiffs) {
        update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "storage");
        if (!update_partition) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SPIFFS partition not found");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Starting SPIFFS OTA update...");
        esp_partition_erase_range(update_partition, 0, update_partition->size);
    } else {
        update_partition = esp_ota_get_next_update_partition(NULL);
        if (!update_partition) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA partition not found");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Starting Firmware OTA update to partition: %s", update_partition->label);
        esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_begin failed");
            return ESP_FAIL;
        }
    }

    int received = 0;
    while (remaining > 0) {
        int ret = httpd_req_recv(req, buf, (remaining < sizeof(buf)) ? remaining : sizeof(buf));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            if (!is_spiffs && update_handle) esp_ota_abort(update_handle);
            return ESP_FAIL;
        }
        
        if (is_spiffs) {
            esp_partition_write(update_partition, received, buf, ret);
        } else {
            esp_ota_write(update_handle, buf, ret);
        }
        
        received += ret;
        remaining -= ret;
        
        if (received % (100 * 1024) < ret) {
             ESP_LOGI(TAG, "OTA Progress: %d%% (%d/%d)", (int)((float)received/total_len * 100), received, total_len);
        }
    }

    if (is_spiffs) {
        ESP_LOGI(TAG, "SPIFFS OTA finished, received %d bytes", received);
    } else {
        esp_err_t err = esp_ota_end(update_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_end failed");
            return ESP_FAIL;
        }
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_set_boot_partition failed");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Firmware OTA finished, received %d bytes. Rebooting...", received);
    }

    httpd_resp_sendstr(req, "Update successful. Rebooting...");
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t update_handler(httpd_req_t *req) {
    return serve_spiffs_file(req, "/spiffs/update.min.html", "text/html");
}

static esp_err_t update_js_handler(httpd_req_t *req) {
    return serve_spiffs_file(req, "/spiffs/update.min.js", "application/javascript");
}

static esp_err_t index_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Index request");
    return serve_spiffs_file(req, "/spiffs/index.min.html", "text/html");
}

static esp_err_t routine_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Routine request");
    return serve_spiffs_file(req, "/spiffs/routine.min.html", "text/html");
}

static esp_err_t style_handler(httpd_req_t *req) {
    return serve_spiffs_file(req, "/spiffs/style.min.css", "text/css");
}

static esp_err_t app_js_handler(httpd_req_t *req) {
    return serve_spiffs_file(req, "/spiffs/app.min.js", "application/javascript");
}

static esp_err_t config_js_handler(httpd_req_t *req) {
    return serve_spiffs_file(req, "/spiffs/config.min.js", "application/javascript");
}

static esp_err_t routine_js_handler(httpd_req_t *req) {
    return serve_spiffs_file(req, "/spiffs/routine.min.js", "application/javascript");
}

void web_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Web UI endpoints
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t update_page_uri = {
            .uri = "/update",
            .method = HTTP_GET,
            .handler = update_handler
        };
        httpd_register_uri_handler(server, &update_page_uri);

        httpd_uri_t update_min_page_uri = {
            .uri = "/update.min.html",
            .method = HTTP_GET,
            .handler = update_handler
        };
        httpd_register_uri_handler(server, &update_min_page_uri);

        httpd_uri_t update_js_uri = {
            .uri = "/update.js",
            .method = HTTP_GET,
            .handler = update_js_handler
        };
        httpd_register_uri_handler(server, &update_js_uri);

        httpd_uri_t update_min_js_uri = {
            .uri = "/update.min.js",
            .method = HTTP_GET,
            .handler = update_js_handler
        };
        httpd_register_uri_handler(server, &update_min_js_uri);

        httpd_uri_t routine_page_uri = {
            .uri = "/routine",
            .method = HTTP_GET,
            .handler = routine_handler
        };
        httpd_register_uri_handler(server, &routine_page_uri);

        httpd_uri_t style_min_uri = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = style_handler
        };
        httpd_register_uri_handler(server, &style_min_uri);

        httpd_uri_t ap_min_js_uri = {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = app_js_handler
        };
        httpd_register_uri_handler(server, &ap_min_js_uri);

        httpd_uri_t routine_min_js_uri = {
            .uri = "/routine.js",
            .method = HTTP_GET,
            .handler = routine_js_handler
        };
        httpd_register_uri_handler(server, &routine_min_js_uri);

        httpd_uri_t config_js_uri = {
            .uri = "/config.js",
            .method = HTTP_GET,
            .handler = config_js_handler
        };
        httpd_register_uri_handler(server, &config_js_uri);

        httpd_uri_t config_min_js_uri = {
            .uri = "/config.min.js",
            .method = HTTP_GET,
            .handler = config_js_handler
        };
        httpd_register_uri_handler(server, &config_min_js_uri);

        httpd_uri_t style_uri = {
            .uri = "/style.min.css",
            .method = HTTP_GET,
            .handler = style_handler
        };
        httpd_register_uri_handler(server, &style_uri);

        httpd_uri_t app_js_uri = {
            .uri = "/app.min.js",
            .method = HTTP_GET,
            .handler = app_js_handler
        };
        httpd_register_uri_handler(server, &app_js_uri);

        httpd_uri_t routine_js_uri = {
            .uri = "/routine.min.js",
            .method = HTTP_GET,
            .handler = routine_js_handler
        };
        httpd_register_uri_handler(server, &routine_js_uri);

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

        httpd_uri_t api_routines_uri = {
            .uri = "/api/routines",
            .method = HTTP_GET,
            .handler = api_routines_handler
        };
        httpd_register_uri_handler(server, &api_routines_uri);

        httpd_uri_t api_routines_post_uri = {
            .uri = "/api/routines",
            .method = HTTP_POST,
            .handler = api_routines_handler
        };
        httpd_register_uri_handler(server, &api_routines_post_uri);
        
        httpd_uri_t api_ota_uri = {
            .uri = "/api/ota",
            .method = HTTP_POST,
            .handler = api_ota_handler
        };
        httpd_register_uri_handler(server, &api_ota_uri);
        
        ESP_LOGI(TAG, "Web server started with API endpoints");
    }
}
