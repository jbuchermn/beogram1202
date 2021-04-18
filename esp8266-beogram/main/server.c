#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include <esp_http_server.h>

static char wifi_ssid[32] = CONFIG_BG_WIFI_SSID;
static char wifi_passwd[32] = CONFIG_BG_WIFI_PASSWORD;

static struct {
    /*
     * 0: (none)
     * 1: initialized
     * 2: Trying to connect
     * 3: Connected
     */
    int state;
    ip4_addr_t addr;
    httpd_handle_t webserver;

    bool (*is_running)();
    void (*relay)();
} server;

void server_init(){
    server.state = 0;
    server.webserver = NULL;

    if(esp_netif_init()) return;
    if(esp_event_loop_create_default()) return;

    server.state = 1;
}

esp_err_t post_handler(httpd_req_t *req){
    char buf[100];
    if(req->content_len > 100) return ESP_FAIL;

    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if(buf[0] == 'l' && buf[1] == 'i' && buf[2] == 'f' && buf[3] == 't'){
        if(server.relay){
            server.relay();
            const char resp[] = "OK";
            httpd_resp_send(req, resp, -1);
        }else{
            const char resp[] = "ERROR";
            httpd_resp_send(req, resp, -1);
        }
    }else if(buf[0] == 'r' && buf[1] == 'u' && buf[2] == 'n' && buf[3] == '?'){
        if(server.is_running){
            if(server.is_running()){
                const char resp[] = "Y";
                httpd_resp_send(req, resp, -1);
            }else{
                const char resp[] = "N";
                httpd_resp_send(req, resp, -1);
            }
        }else{
            const char resp[] = "ERROR";
            httpd_resp_send(req, resp, -1);
        }
    }

    return ESP_OK;
}

httpd_uri_t handler = {
    .uri       = "/endpoint",
    .method    = HTTP_POST,
    .handler   = post_handler,
    .user_ctx  = NULL
};

httpd_handle_t server_start(){
    httpd_handle_t http_server = 0;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_BG_HTTP_PORT;

    if(httpd_start(&http_server, &config)) return 0;
    httpd_register_uri_handler(http_server, &handler);
    return http_server;
}

void server_stop(httpd_handle_t server){
    httpd_stop(server);
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data){

    if(server.webserver){
        server_stop(server.webserver);
        server.webserver = NULL;
    }

    system_event_sta_disconnected_t *event = (system_event_sta_disconnected_t *)event_data;
    if (event->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
        /*Switch to 802.11 bgn mode */
        esp_wifi_set_protocol(
                ESP_IF_WIFI_STA,
                WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    }

    esp_wifi_connect();
    server.state = 2;
}


static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data){

    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    memcpy(&server.addr, &event->ip_info.ip, sizeof(server.addr));

    if(!server.webserver){
        server.webserver = server_start();
    }

    server.state = 3;
}

int server_connect(bool (*is_running)(), void (*relay)()){
    if(server.state == 0) server_init();
    if(server.state != 1) return -1;

    server.is_running = is_running;
    server.relay = relay;
        
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&cfg)) return -2;

    if(esp_event_handler_register(
                WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                &on_wifi_disconnect, NULL)) return -3;
    if(esp_event_handler_register(
                IP_EVENT, IP_EVENT_STA_GOT_IP,
                &on_got_ip, NULL)) return -4;

    if(esp_wifi_set_storage(WIFI_STORAGE_RAM)) return -5;
    wifi_config_t wifi_config = { 0 };

    strncpy((char *)&wifi_config.sta.ssid, wifi_ssid, 32);
    strncpy((char *)&wifi_config.sta.password, wifi_passwd, 32);

    if(esp_wifi_set_mode(WIFI_MODE_STA)) return -6;
    if(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config)) return -7;
    if(esp_wifi_start()) return -8;
    if(esp_wifi_connect()) return -9;

    server.state = 2;

    return 0;
}
