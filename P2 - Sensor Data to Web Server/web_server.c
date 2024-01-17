#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <time.h>

#define EXAMPLE_ESP_WIFI_SSID      "IOT_MAX"
#define EXAMPLE_ESP_WIFI_PASS      "IOT_12345678"
#define EXAMPLE_MAX_STA_CONN       10

#define LED_GPIO 22
uint8_t ledstate = 0;

static const char *TAG = "softAP_WebServer";

char cad[20];


#include "driver/spi_master.h"
#define CHIP_SELECT 15  
#define SCLK_PIN    2   
#define MISO_PIN    4 
#define MOSI_PIN    5   

#define MS5611_CMD_RESET 0x1E
#define MS5611_CMD_ADC_READ 0x00
#define MS5611_CMD_TEMP_CONV 0x58 //TEMP OSR = 4096
#define MS5611_CMD_PRESS_CONV 0x48 

float TEMP_FINAL=0;
float temps[4096] = {0};
int historialTemps =10;

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
        .clock_speed_hz = 1000000,  
        .mode = 0,                  
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

    spi_device_transmit(spi2, &trans); 
    delayMs(5); 
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
    spi_device_transmit(spi2, &trans); 
    delayMs(1000);
    trans.length = 24;
    trans.tx_buffer = NULL;
    trans.rx_buffer = buffer;
    trans.user = NULL;
    trans.cmd = MS5611_CMD_ADC_READ;
    spi_device_transmit(spi2, &trans); 
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

/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

/* ----- UTILIDADES SOLO DE PRUEBA ----- */
float generarNumeroAleatorioFloat(float min, float max) {
    float num = (float)rand() / RAND_MAX; // Genera un número entre 0 y 1
    num = num * (max - min) + min; // Escala y desplaza el número al rango deseado
    return num;
}

int generarNumeroAleatorioInt(int min, int max) {
    int num = rand() % (max - min + 1) + min; // Genera un número entero dentro del rango deseado
    return num;
}
/* ----- UTILIDADES SOLO DE PRUEBA ----- */

static esp_err_t historial_handler(httpd_req_t *req)
{
    char buffer1[25];
    snprintf(buffer1, sizeof(buffer1), "%f", temps[historialTemps-1]);
    const char* sensor1_str = buffer1;

    char buffer2[25];
    snprintf(buffer2, sizeof(buffer2), "%f", temps[historialTemps-2]);
    const char* sensor2_str = buffer2;
    
    char buffer3[25];
    snprintf(buffer3, sizeof(buffer3), "%f", temps[historialTemps-3]);
    const char* sensor3_str = buffer3;

    char buffer4[25];
    snprintf(buffer4, sizeof(buffer4), "%f", temps[historialTemps-4]);
    const char* sensor4_str = buffer4;

    char buffer5[25];
    snprintf(buffer5, sizeof(buffer5), "%f", temps[historialTemps-5]);
    const char* sensor5_str = buffer5;

    char buffer6[25];
    snprintf(buffer6, sizeof(buffer6), "%f", temps[historialTemps-6]);
    const char* sensor6_str = buffer6;

    char buffer7[25];
    snprintf(buffer7, sizeof(buffer7), "%f", temps[historialTemps-7]);
    const char* sensor7_str = buffer7;

    char buffer8[25];
    snprintf(buffer8, sizeof(buffer8), "%f", temps[historialTemps-8]);
    const char* sensor8_str = buffer8;

    char buffer9[25];
    snprintf(buffer9, sizeof(buffer9), "%f", temps[historialTemps-9]);
    const char* sensor9_str = buffer9;

    char buffer10[25];
    snprintf(buffer10, sizeof(buffer10), "%f", temps[historialTemps-10]);
    const char* sensor10_str = buffer10;
    

    
    ESP_LOGI("HISTORIAL", "<-");
    const char* html_template = "<html>\n"
                                "<style>\n"
                                " body{ \n"
                                " margin-top: 35rem;\n"
                                " background-color:black;\n"
                                " color: white; \n"
                                " text-align: center; }\n"
                                " h1 {\n"
                                " font-family: sans-serif;\n"
                                " color:white;\n"
                                " }\n"
                                " h3 {\n"
                                " font-family: sans-serif;\n"
                                " color:white;\n"
                                " }\n"
                                "button {\n"
                                "    color: black; \n"
                                "    border: 2px solid white; \n"
                                "    padding: 10px 20px; \n"
                                "    cursor: pointer;\n"
                                "}\n"
                                "</style>\n"
                                "<script>\n"
                                "        \n"
                                "        function updateValues() {\n"
                                "            location.reload(); \n"
                                "        }\n"
                                "    </script>\n"
                                "<h1>HISTORIAL</h1>\n"
                                "<h3>%s °C</h3><h3>%s °C</h3><h3>%s °C</h3><h3>%s °C</h3><h3>%s °C</h3><h3>%s °C</h3><h3>%s °C</h3><h3>%s °C</h3><h3>%s °C</h3><h3>%s °C</h3><a href=\"/historial\"><button>HISTORIAL</button></a><a href=\"/temp\"><button>LECTURA ACTUAL</button</a>";

    // Calcular el tamaño necesario para la cadena de respuesta
    size_t html_size = snprintf(NULL, 0, html_template, sensor1_str, sensor2_str, sensor3_str, sensor4_str, sensor5_str, sensor6_str, sensor7_str, sensor8_str, sensor9_str, sensor10_str) + 1;
    
    // Crear un búfer para la respuesta HTML
    char* html_response = malloc(html_size);
    
    // Combinar la plantilla HTML y los datos de los sensores en la respuesta final
    snprintf(html_response, html_size, html_template, sensor1_str, sensor2_str, sensor3_str, sensor4_str, sensor5_str, sensor6_str, sensor7_str, sensor8_str, sensor9_str, sensor10_str);
    
    // Enviar la respuesta HTTP al cliente
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html_response, HTTPD_RESP_USE_STRLEN);
    
    // Liberar la memoria asignada para la respuesta HTML
    free(html_response);
    return ESP_OK;
}

static esp_err_t temp_handler(httpd_req_t *req)
{
    /* ----- Obtención del promedio de sensado -----*/
    char buffer1[100];
    float promedio = 0;
    for(int l=0; l<5; l++){
        sensor();
        promedio = promedio + TEMP_FINAL;
    }
    promedio = promedio / 5;
    temps[historialTemps] = promedio;
    historialTemps++;
    /* ------ Obtención del promedio de sensado -----*/

    snprintf(buffer1, sizeof(buffer1), "%.2f", TEMP_FINAL);
    const char* sensor1_str = buffer1;
    ESP_LOGI("TEMP", "<-");
    const char* html_template = "<html>\n"
                                "<style>\n"
                                " body{ \n"
                                " margin-top: 35rem;\n"
                                " background-color:black;\n"
                                " color: white; \n"
                                " text-align: center; }\n"
                                " h1 {\n"
                                " font-family: sans-serif;\n"
                                " color:white;\n"
                                " }\n"
                                " h3 {\n"
                                " font-family: sans-serif;\n"
                                " color:white;\n"
                                " }\n"
                                "button {\n"
                                "    color: black; \n"
                                "    border: 2px solid white; \n"
                                "    padding: 10px 20px; \n"
                                "    cursor: pointer;\n"
                                "}\n"
                                "</style>\n"
                                "<script>\n"
                                "        setInterval(updateValues, 30000);\n"
                                "        function updateValues() {\n"
                                "            location.reload(); \n"
                                "        }\n"
                                "    </script>\n"
                                "<h1>TEMPERATURA</h1><h3>%s °C</h3><a href=\"/historial\"><button>HISTORIAL</button></a><a href=\"/temp\"><button>LECTURA ACTUAL</button</a>";

    // Calcular el tamaño necesario para la cadena de respuesta
    size_t html_size = snprintf(NULL, 0, html_template, sensor1_str) + 1;
    
    // Crear un búfer para la respuesta HTML
    char* html_response = malloc(html_size);
    
    // Combinar la plantilla HTML y los datos de los sensores en la respuesta final
    snprintf(html_response, html_size, html_template, sensor1_str);
    
    // Enviar la respuesta HTTP al cliente
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html_response, HTTPD_RESP_USE_STRLEN);
    
    // Liberar la memoria asignada para la respuesta HTML
    free(html_response);
    return ESP_OK;
}

static const httpd_uri_t historial = {
    .uri      = "/historial",
    .method   = HTTP_GET,
    .handler  = historial_handler,
    .user_ctx = NULL
};


static const httpd_uri_t temp = {
    .uri      = "/temp",
    .method   = HTTP_GET,
    .handler  = temp_handler,
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Iniciar el servidor httpd 
    ESP_LOGI(TAG, "Iniciando el servidor en el puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Manejadores de URI
        ESP_LOGI(TAG, "Registrando manejadores de URI");
        httpd_register_uri_handler(server, &historial);
        httpd_register_uri_handler(server, &temp);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "estacion "MACSTR" se unio, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "estacion "MACSTR" se desconecto, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

esp_err_t wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Inicializacion de softAP terminada. SSID: %s password: %s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    return ESP_OK;
}


void app_main(void)
{
    httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    esp_netif_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "init softAP");
    ESP_ERROR_CHECK(wifi_init_softap());

    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    spi_init();
    server = start_webserver();

}