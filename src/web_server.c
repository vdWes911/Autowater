#include "web_server.h"
#include "relay_controller.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "WEB";

static esp_err_t relay_handler(httpd_req_t *req) {
    char buf[64];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    printf("Relay request: %s\n", buf);
    int relay = -1;
    char action[8] = {0};

    if (sscanf(buf, "relay=%d&action=%7s", &relay, action) == 2) {
        if (relay >= 0 && relay < NUM_RELAYS) {
            if (strcmp(action, "on") == 0) {
                printf("Relay %d turned ON", relay + 1);
                ESP_LOGI(TAG, "Relay %d turned ON", relay + 1);
                relay_on(relay);

            } else if (strcmp(action, "off") == 0) {

                printf("Relay %d turned OFF", relay + 1);
                ESP_LOGI(TAG, "Relay %d turned OFF", relay + 1);relay_off(relay);
            }
        }
    } else {
        httpd_resp_send_500(req);
        printf("Invalid request format\n");
        return ESP_FAIL;
    }

    // Redirect back to main page
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req) {
    char html[1024];
    int len = snprintf(html, sizeof(html),
        "<html><body>"
        "<h1>Relay Control</h1>"
    );

    printf("Index request\n");

    for (int i = 0; i < NUM_RELAYS; i++) {
        const char *state = relay_get_state(i) ? "ON" : "OFF";
        len += snprintf(html + len, sizeof(html) - len,
            "<p>Relay %d: <b>%s</b></p>"
            "<form method='POST' action='/relay'>"
            "<input type='hidden' name='relay' value='%d'>"
            "<button type='submit' name='action' value='on'>ON</button>"
            "<button type='submit' name='action' value='off'>OFF</button>"
            "</form>",
            i + 1, state, i
        );
    }

    snprintf(html + len, sizeof(html) - len, "</body></html>");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


void web_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
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
    }

    ESP_LOGI(TAG, "Web server started, visit your ESP32 IP in a browser");
}
