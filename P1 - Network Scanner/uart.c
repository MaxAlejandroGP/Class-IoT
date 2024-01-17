#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "uart.h"

/**
 * @brief Configure and install the default UART, then, connect it to the
 * console UART.
 */

void delayMs(uint16_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void uartInit(uart_port_t uart_num, uint8_t txPin, uint8_t rxPin)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(uart_num, READ_BUF_SIZE, READ_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void uartClrScr(uart_port_t uart_num)
{
    // Uso "const" para sugerir que el contenido del arreglo lo coloque en Flash y no en RAM
    const char caClearScr[] = "\e[2J";
    uart_write_bytes(uart_num, caClearScr, sizeof(caClearScr));
}

uint16_t myAtoi(char *str){
    int flag=0;
    uint16_t result=0;
    int i=0;
    while(str[i] >= '0' && str[i] <= '9'){
        if (flag==1){
            result=0;
            result ='0';
            flag=0;
        }else{
        result = result*10+(str[i]-'0');
        }
        i++;
        if (result == 65535){
            flag=1;
        }
    }
    return result;
}

void uartGoto11(uart_port_t uart_num)
{
    // Limpie un poco el arreglo de caracteres, los siguientes tres son equivalentes:
    //"\e[1;1H" == "\x1B[1;1H" == {27,'[','1',';','1','H'}
    const char caGoto11[] = "\e[1;1H";
    uart_write_bytes(uart_num, caGoto11, sizeof(caGoto11));
}

void uartGotoxy(uart_port_t uart_num, uint8_t x, uint8_t y)
{
    int i=0;
    int j=2;
    char tmpX[20]={'\0'};
    char tmpY[20]={'\0'};
    myItoa(x,tmpX,10);
    myItoa(y,tmpY,10);

    char tmp1[20]={'\0'};
    tmp1[0] = '\e';
    tmp1[1] = '[';
    while(tmpX[i] !='\0'){
        tmp1[j]=tmpX[i];
        j++;
        i++;
    }
    tmp1[j] = ';';
    j++;
    i=0;
    while(tmpY[i] !='\0'){
        tmp1[j]=tmpY[i];
        j++;
        i++;
    }
    tmp1[j] = 'H';
    uart_write_bytes(uart_num, tmp1, sizeof(tmp1));
}

bool uartKbhit(uart_port_t uart_num)
{
    uint8_t length;
    uart_get_buffered_data_len(uart_num, (size_t*)&length);
    return (length > 0);
}

void uartGets(uart_port_t com, char *str){
    int limit=0;
    int i=0;
    int tmp=0;
    char x;
    while( tmp==0){    //0x0A es enter
  
        x = uartGetchar(com);
        
        if(x == 8){
            if (limit==0){
               //uartPuts(com,"\r");
                //uartPutchar(com,'X');
                //str[i]='X';
            }else{
                limit--;
                x = 32;
                uartPutchar(com,8);
                uartPutchar(com,32);
                uartPutchar(com,8);
                str[i]='\0';
                i--;
            }
        }else{

            if (x == 13){
                tmp=1;
            }else{
                uartPutchar(com,x);
                str[i]=x;
                i++;
                limit++;
            }
        }
    }

    while(i < sizes-1){  //limpio el resto de la cadena
        str[i]='\0';
        i++;
    }
}

void uartPuts(uart_port_t com, char *str){
    int i=0;
    while(str[i] != '\0'){
        uartPutchar(com,str[i]);
        i++;
    }
}

void uartPutchar(uart_port_t uart_num, char c)
{
    uart_write_bytes(uart_num, &c, sizeof(c));
}

void myItoa(uint16_t number, char* str, uint8_t base){
    uint16_t tmpModule = 0;
    int i=0;
    int j=0;
    char strTmp[20]={'\0'};
    if (number == 0){
        str[i] = '0';
        j++;
    }else{
        while (number!=0) {
            tmpModule=(number%base);
            number/=base;
            if (tmpModule>9){
                strTmp[i] = tmpModule + 55;
            }else{
                strTmp[i] = tmpModule + '0';
            }
            i++;
            }
            i--;
            while(i != -1){
                str[j]=strTmp[i];
                i--;
                j++;
            }
    }
    while(j < 19){
        str[j]='\0';
        j++;
    }
}

char uartGetchar(uart_port_t uart_num)
{
    char c;
    // Wait for a received byte
    while(!uartKbhit(uart_num))
    {
        delayMs(10);
    }
    // read byte, no wait
    uart_read_bytes(uart_num, &c, sizeof(c), 0);

    return c;
}

void uartSetColor(uart_port_t uart_num, uint8_t color){

    int i=0;
    int j=2;
    char tmpColor[20]={'\0'};
    myItoa(color,tmpColor,10);

    char setColor[20]={'\0'};
    setColor[0] = '\e';
    setColor[1] = '[';
    while(tmpColor[i] !='\0'){
        setColor[j]=tmpColor[i];
        j++;
        i++;
    }
    setColor[j] = 'm';
    uart_write_bytes(uart_num, setColor, sizeof(setColor));
}