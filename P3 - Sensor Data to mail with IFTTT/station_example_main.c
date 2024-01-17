/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"

#include "lwip/err.h"
#include "lwip/sys.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

/* Threshold */
#define limit_text "Temperatura mayor a 31°C"
#define limit_int 31

/* Station config */
#define EXAMPLE_ESP_WIFI_SSID      "iPhone"
#define EXAMPLE_ESP_WIFI_PASS      "max12345"
#define EXAMPLE_ESP_MAXIMUM_RETRY  10

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

/* ----- CONEXIÓN ----- */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
/* ----- CONEXIÓN ----- */

/* ----- SENSADO ----- */
#include "driver/spi_master.h"
#define CHIP_SELECT 15  
#define SCLK_PIN    2   
#define MISO_PIN    4 
#define MOSI_PIN    5   

#define MS5611_CMD_RESET 0x1E
#define MS5611_CMD_ADC_READ 0x00
#define MS5611_CMD_TEMP_CONV 0x58 //TEMPERATURA OSR = 4096
#define MS5611_CMD_PRESS_CONV 0x48 //PRESION OSR = 4096

float TEMP_FINAL=0;
float promedio = 0;
float temps[4096] = {0};
int historialTemps =10;
#define temperature_threshold 31

spi_device_handle_t spi2;
spi_transaction_t trans;
uint16_t coeficientes[8] = {0};

static void delayMs(uint32_t ms)
{
    vTaskDelay(ms/portTICK_PERIOD_MS);
}

static void spi_init() {
    esp_err_t ret;
    spi_bus_config_t buscfg={
        .miso_io_num = MISO_PIN,
        .mosi_io_num = MOSI_PIN,
        .sclk_io_num = SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1,
    };

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, 1);
    ESP_ERROR_CHECK(ret);
    spi_device_interface_config_t devcfg={
        .clock_speed_hz = 1000000,  // 1 MHz
        .mode = 0,                  //SPI mode 0
        .spics_io_num = CHIP_SELECT,     
        .queue_size = 7,
        .pre_cb = NULL,
        .post_cb = NULL,
        .flags = SPI_DEVICE_NO_DUMMY,
        .command_bits = 8,
        .address_bits = 0,
        .dummy_bits = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .input_delay_ns = 0,
        };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi2));
};

void read_prom(void)
{
    // FASE DE LECTURA
    uint8_t buffer_rom[2] = {0};
    memset(coeficientes, 0, sizeof(coeficientes));
    trans.tx_buffer = NULL;
    trans.rx_buffer = NULL;
    trans.user = NULL;
    trans.cmd = MS5611_CMD_RESET; //RESET

    spi_device_transmit(spi2, &trans); //Mando reset
    delayMs(5); //reset tarda 2.8 ms en recargar
    for(uint8_t i = 0 ; i < 8 ; i++)
    {
        trans.cmd = 0xA0 + (i * 2),
        trans.length = 16;
        trans.tx_buffer = NULL;
        trans.rx_buffer = buffer_rom;
        trans.user = NULL;
        spi_device_transmit(spi2, &trans);
        delayMs(100);
        coeficientes[i]|= buffer_rom[0] << 8 | buffer_rom[1];
        printf("Lectura de coeficiente %d = %d\n", i, coeficientes[i]);
    }
}

void sensor(){

    char cadToRec[20];
    uint32_t aux1, aux2, aux3, aux4;
    char *token;
    float dt2;
    float OFFSET;
    memset(cadToRec,0,20);
    read_prom();
    uint8_t buffer[1] = {0};
    trans.tx_buffer = NULL;
    trans.rx_buffer = NULL;
    trans.user = NULL;
    trans.cmd = MS5611_CMD_TEMP_CONV;
    spi_device_transmit(spi2, &trans); //se manda conversion D1
    delayMs(1000);
    trans.length = 24;
    trans.tx_buffer = NULL;
    trans.rx_buffer = buffer;
    trans.user = NULL;
    trans.cmd = MS5611_CMD_ADC_READ;
    spi_device_transmit(spi2, &trans); //Se manda lectura del adc
    aux1 = (uint32_t)coeficientes[5];
    aux2 = (uint32_t)coeficientes[6];
    aux3 = (uint32_t)coeficientes[2];
    aux4 = (uint32_t)coeficientes[4];
    uint32_t temp_raw = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
    uint32_t dt = temp_raw - ( aux1 * 256 );
    dt2 = (float)dt * (aux2 / 8388608.00);
    TEMP_FINAL =  2000.00 + dt2;
    OFFSET = (aux3 * 65536) + ((aux4 * dt)/ 128);
    TEMP_FINAL = TEMP_FINAL/100;
    delayMs(10);
    
}
/* ----- SENSADO ----- */

esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}

static void post_trigger_function()
{

    char buffer1[100];
    snprintf(buffer1, sizeof(buffer1), "%.2f", promedio);
    const char* value1 = buffer1;
    const char* url = "http://maker.ifttt.com/trigger/temperature_threshold/with/key/gscp8yvSTus6jiGSPTOLpnUfZIXDc36AP-WM-1gQ5YK?value1=%s";
    // Calcular el tamaño necesario para la cadena de respuesta
    size_t url_size = snprintf(NULL, 0, url, value1) + 1;
    // Crear un búfer para la respuesta URL
    char* url_buffer = malloc(url_size);
    snprintf(url_buffer,url_size,url,value1);

    ESP_LOGI(TAG, "%s",url_buffer);
    esp_http_client_config_t config_post = {
        .url = url_buffer,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = client_event_post_handler};
        
    esp_http_client_handle_t client = esp_http_client_init(&config_post);
    

    //myItoa(promedio,value1,10);
    //strcat(config_post.url,value1);
    
    /*
    char  *post_data = "test ...";
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    */
    esp_http_client_set_header(client, "Content-Type", "application/json");
    

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void start_measure(){
    /* ----- Obtención del promedio de sensado -----*/
    while(1){
        promedio = 0;
        for(int l=0; l<5; l++){
            sensor();
            promedio = promedio + TEMP_FINAL;
        }
        promedio = promedio / 5;
        temps[historialTemps] = promedio;
        historialTemps++;
        printf("TEMPERATURA: %.2f \n",promedio);
        if (promedio > temperature_threshold){
            post_trigger_function();
        }
        delayMs(30000);
    }
    /* ------ Obtención del promedio de sensado -----*/
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    spi_init();

    //Wifi init
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    //Sensor measure
    start_measure();
}
