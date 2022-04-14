#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

#include "driver/gpio.h"


#define ESP_WIFI_SSID "examplewifi" //exchange to your wifi name
#define ESP_WIFI_PASS "examplepassword" //exchange to your wifi password
#define TAG "name"
bool wifi_established = false;

#define LED GPIO_NUM_5



//WIFI INIT
static void wifi_event_handler(void* arg , esp_event_base_t event_base , int32_t event_id , void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        ESP_LOGI(TAG , "connect_to_the_AP");
        wifi_established = false;
        esp_wifi_connect ();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        ESP_LOGI(TAG , "retry_to_connect_to_the_AP");
        esp_wifi_connect ();
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG , "got_ip:%s", ip4addr_ntoa ( &event ->ip_info.ip)); 
        wifi_established = true;
    } 
    else 
    {
        ESP_LOGI(TAG , "unhandled_event_(%s)_with_ID_%d!", event_base , event_id);
    }

}


void wifi()
{
    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default ());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT ();
    cfg.nvs_enable = false;
    ESP_ERROR_CHECK(esp_wifi_init (&cfg));


    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT ,ESP_EVENT_ANY_ID , &wifi_event_handler , NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT ,IP_EVENT_STA_GOT_IP , &wifi_event_handler , NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT ,IP_EVENT_STA_LOST_IP , &wifi_event_handler , NULL));

    wifi_config_t wifi_config = {
        .sta = {
          .ssid = ESP_WIFI_SSID,
        .password = ESP_WIFI_PASS,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA , &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start ());
}

//HTTP
esp_err_t http_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                printf("%.*s", evt->data_len, (char*)evt->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

static void https()
{
    esp_http_client_config_t config = {
        .url = "http://worldclockapi.com/api/jsonp/cet/now?callback=mycallback",
        .event_handler = http_event_handle ,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    }
    esp_http_client_cleanup(client);
}


//WEBSERVER
esp_err_t ledOFF_handler(httpd_req_t *req)
{
	esp_err_t error;
	ESP_LOGI(TAG, "LED Turned OFF");
	gpio_set_level(LED, 1);
	const char *response = (const char *) req->user_ctx;
	error = httpd_resp_send(req, response, strlen(response));
	if (error != ESP_OK)
	{
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	}
	else ESP_LOGI(TAG, "Response sent Successfully");
	return error;
}

httpd_uri_t ledoff = {
    .uri       = "/ledoff",
    .method    = HTTP_GET,
    .handler   = ledOFF_handler,
    .user_ctx  = "<!DOCTYPE html>\
<html>\
<head>\
<style>\
.button {\
  border: none;\
  color: white;\
  padding: 15px 32px;\
  text-align: center;\
  text-decoration: none;\
  display: inline-block;\
  font-size: 16px;\
  margin: 4px 2px;\
  cursor: pointer;\
}\
\
.button1 {background-color: #19a1e6;}\
</style>\
</head>\
<body>\
\
<h1>ESP32 WEBSERVER</h1>\
<h3> Led Status : OFF </h3>\
\
<button class=\"button button1\" onclick= \"window.location.href='/ledon'\">LED ON</button>\
\
</body>\
</html>"
};


esp_err_t ledON_handler(httpd_req_t *req)
{
	esp_err_t error;
	ESP_LOGI(TAG, "LED Turned ON");
	gpio_set_level(LED, 0);
	const char *response = (const char *) req->user_ctx;
	error = httpd_resp_send(req, response, strlen(response));
	if (error != ESP_OK)
	{
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	}
	else ESP_LOGI(TAG, "Response sent Successfully");
	return error;
}

httpd_uri_t ledon = {
    .uri       = "/ledon",
    .method    = HTTP_GET,
    .handler   = ledON_handler,
    .user_ctx  = "<!DOCTYPE html>\
<html>\
<head>\
<style>\
.button {\
  border: none;\
  color: white;\
  padding: 15px 32px;\
  text-align: center;\
  text-decoration: none;\
  display: inline-block;\
  font-size: 16px;\
  margin: 4px 2px;\
  cursor: pointer;\
}\
\
.button1 {background-color: #e6197f;}\
</style>\
</head>\
<body>\
\
<h1>ESP32 WEBSERVER</h1>\
<h3> Led Status : ON </h3>\
\
<button class=\"button button1\" onclick= \"window.location.href='/ledoff'\">LED OFF</button>\
\
</body>\
</html>"
};

httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = ledOFF_handler,
    .user_ctx  = "<!DOCTYPE html>\
<html>\
<head>\
<style>\
.button {\
  border: none;\
  color: white;\
  padding: 15px 32px;\
  text-align: center;\
  text-decoration: none;\
  display: inline-block;\
  font-size: 16px;\
  margin: 4px 2px;\
  cursor: pointer;\
}\
\
.button1 {background-color: #19a1e6;}\
</style>\
</head>\
<body>\
\
<h1>ESP32 WEBSERVER</h1>\
<h3> Led Status : OFF </h3>\
\
<button class=\"button button1\" onclick= \"window.location.href='/ledon'\">LED ON</button>\
\
</body>\
</html>"
};


httpd_handle_t start_webserver(void) 
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config ) == ESP_OK) 
    {
        ESP_LOGI(TAG, "Registering Uri handlers");
        httpd_register_uri_handler(server, &ledoff);  
        httpd_register_uri_handler(server, &ledon);
        httpd_register_uri_handler(server, &root); 
        return server;
    }
    //return server;
    ESP_LOGI(TAG, "Error starting server");
    return NULL;
}

//LED CONF
void conf_led (void)
{
	gpio_reset_pin (LED);
	gpio_set_direction (LED, GPIO_MODE_OUTPUT);
}

void app_main()
{
    conf_led();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi();
    https();
    start_webserver();

    while(1){
        while(!wifi_established){
            vTaskDelay(100);
            printf("not connected\n");
        } 
         printf("connected\n");
         vTaskDelay(100);
    }
 
}