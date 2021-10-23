/*
Copyright (c) 2017-2019 Tony Pottier
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
@file main.c
@author Tony Pottier
@brief Entry point for the ESP32 application.
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include  "esp_int_wdt.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "wifi_manager.h"

#include "nvs.h"
#include "nvs_sync.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "z21header.h"
#include "z21.h"

//#include "protocol_examples_common.h"

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "Z21";
static const char *Z21_TASK_TAG = "Z21_UDP_RECIEVER";
static const char *Z21_SENDER_TAG = "Z21_UDP_SENDER";
static const int P50X_RX_BUF_SIZE = 128;

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

const char z21_nvs_namespace[] = "esp_z21";

TaskHandle_t xHandle1;
TaskHandle_t xHandle2;

static void udp_sender_task(void *pvParameters)
{
    char addr_str[128];
    //ESP_LOGI(Z21_SENDER_TAG, "Sender task started");
    memset(&addr_str, 0x00, sizeof(addr_str));
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    int opt = 1;
    setsockopt(global_sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    while (1)
    {
        while (1)
        {
            if (txSendFlag)
            {
                if (txBflag)
                {
                    dest_addr.sin_port = htons(PORT);
                    dest_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // txAddr.addr; //(HOST_IP_ADDR);htonl(INADDR_ANY);                ip4addr_ntoa_r((const ip4_addr_t *)&(((struct sockaddr_in *)&dest_addr)->sin_addr), addr_str, sizeof(addr_str) - 1);
                    //ESP_LOGI(Z21_SENDER_TAG, "Hurrah! New broadcast!");
                }
                else
                {
                    dest_addr.sin_addr.s_addr = txAddr.addr; //(HOST_IP_ADDR);htonl(INADDR_ANY);
                    dest_addr.sin_port=txport;
                    ip4addr_ntoa_r((const ip4_addr_t *)&(((struct sockaddr_in *)&dest_addr)->sin_addr), addr_str, sizeof(addr_str) - 1);
                    //ESP_LOGI(Z21_SENDER_TAG, "Hurrah! New message to %s, %d:", addr_str, htons (dest_addr.sin_port));
                }
                txBflag=0;

                //ESP_LOG_BUFFER_HEXDUMP(Z21_SENDER_TAG, (uint8_t *)&txBuffer, txBlen, ESP_LOG_INFO);

                int err = sendto(global_sock, (uint8_t *)&txBuffer, txBlen, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                //int err = send(txBsock, (uint8_t *)&txBuffer, txBlen, 0);
                if (err < 0)
                {
                    ESP_LOGE(Z21_SENDER_TAG, "Error occurred during sending: errno %d", errno);
                    txSendFlag = 0;
                    break;
                }
                //ESP_LOGI(Z21_SENDER_TAG, "%d bytes send.", txBlen);
                txSendFlag = 0;
                break;
            }
            //vTaskDelay(1 / portTICK_PERIOD_MS);
        }

    }
    free(addr_str);
    vTaskDelete(NULL);

}

static void udp_server_task(void *pvParameters)
{
    uint8_t rx_buffer[128];
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in dest_addr;
    ESP_LOGI(Z21_TASK_TAG, "Init server task started.");
    uint8_t *rx_buffer = (uint8_t *)malloc(rx_buffer);
    while (1)
        {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        global_sock = sock;
        if (sock < 0)
        {
            ESP_LOGE(Z21_TASK_TAG, "Unable to create socket: errno %d", errno);
            break;
        } else {
        //ESP_LOGI(Z21_TASK_TAG, "Socket created: %d", sock);

       int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0)
        {
            ESP_LOGE(Z21_TASK_TAG, "Socket unable to bind: errno %d", errno);
            break;
        }
        listen(sock, 1);

        //ESP_LOGI(Z21_TASK_TAG, "Socket bound, port %d", PORT);
        //ESP_LOGI(Z21_TASK_TAG, "Waiting for data");
        while (1)
        {
            if (!txSendFlag)
            {
            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0)
            {
                ESP_LOGE(Z21_TASK_TAG, "Recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else
            {
                ip4addr_ntoa_r((const ip4_addr_t*)&(((struct sockaddr_in *)&source_addr)->sin_addr), addr_str, sizeof(addr_str) - 1);
                int from_port = (((struct sockaddr_in *)&source_addr)->sin_port);
                uint8_t client = Z21addIP(ip4_addr1((const ip4_addr_t *)&(((struct sockaddr_in *)&source_addr)->sin_addr)), ip4_addr2((const ip4_addr_t *)&(((struct sockaddr_in *)&source_addr)->sin_addr)), ip4_addr3((const ip4_addr_t *)&(((struct sockaddr_in *)&source_addr)->sin_addr)), ip4_addr4((const ip4_addr_t *)&(((struct sockaddr_in *)&source_addr)->sin_addr)), from_port);
                receive(client, rx_buffer);
            }
            }
            
        }

        if (sock != -1)
        {
            ESP_LOGE(Z21_TASK_TAG, "Shutting down socket and restarting...");
            shutdown(sock, SHUT_RDWR);
            close(sock);
        }
        }
    }
    free(rx_buffer);
    vTaskDelete(NULL);
}


 
int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "P50X_TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        sendData(TX_TASK_TAG, "Hello world");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "P50X_RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t *data = (uint8_t *)malloc(P50X_RX_BUF_SIZE);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, P50X_RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            //data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read from P50x %d bytes:", rxBytes);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }
    free(data);
}

void init_p50x(void) {
    const uart_config_t uart_config = {
        .baud_rate = 19200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, P50X_RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
}
/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can integrate your code with wifi-manager
 */
void monitoring_task(void *pvParameter)
{
    for (;;)
    {
        //ESP_LOGI(TAG, "free heap: %d", esp_get_free_heap_size());
        esp_get_free_heap_size();
            // vTaskDelay(pdMS_TO_TICKS(10000));

        unsigned long currentMillis = esp_timer_get_time() / 1000;

        if (currentMillis - z21IPpreviousMillis > z21IPinterval)
        {
            z21IPpreviousMillis = currentMillis;
            for (int i = 0; i < z21clientMAX; i++)
            {
                if (ActIP[i].ip3 != 0)
                { //Slot nicht leer?
                    if (ActIP[i].time > 0)
                        ActIP[i].time--; //Zeit herrunterrechnen
                    else
                    {
                        clearIPSlot(i); //clear IP DATA
                    }
                }
            }
        }
    }
}

/**
 * @brief this is an exemple of a callback that you can setup in your own app to get notified of wifi manager event.
 */
void cb_connection_ok(void *pvParameter)
{
    ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;
       
    /* transform IP to human readable string */
    char str_ip[16];
    
    esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);

    ESP_LOGI(TAG, "I have a connection and my IP is %s!", str_ip);

    //ESP_ERROR_CHECK(example_connect());
    
    xTaskCreate(udp_server_task, "udp_server", 4096, (void *)AF_INET, 5, &xHandle1);
    
    xTaskCreate(udp_sender_task, "udp_client", 4096, NULL, 5, &xHandle2);
    
    




}
void cb_connection_off(void *pvParameter)
{
    //ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;

    vTaskDelete(xHandle2);
    vTaskDelete(xHandle1);
    ESP_LOGI(TAG, "I do not have connection. Stop UDP!");
}

void app_main()
{
  	nvs_handle handle;
	esp_err_t esp_err;
	//size_t sz;
    bool change = false;
    txBlen=0;
    txBflag=0;
    rxFlag=0;
    rxlen = 0;
    rxclient = 0;
    storedIP = 0;
    slotFullNext=0;
    DCCdefaultSteps=128;
    uint8_t *txBuffer = (uint8_t *)malloc(Z21_UDP_TX_MAX_SIZE);
    uint8_t *rxBuffer = (uint8_t *)malloc(Z21_UDP_RX_MAX_SIZE);
        // txSendFlag=0;

    if (nvs_sync_lock(portMAX_DELAY))
    {
        /* read value from flash */
    	esp_err = nvs_open(z21_nvs_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK){
			nvs_sync_unlock();
			//return esp_err;
		}

		//sz = sizeof(txSendFlag);
		//esp_err = nvs_get_blob(handle, "Bflag", txSendFlag , &sz);
        esp_err = nvs_get_u8(handle, "Bflag", (uint8_t *)&txSendFlag);
        if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND)&&txSendFlag!=0){
			
			esp_err = nvs_set_u8(handle, "Bflag", 0);
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				//return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "Wrote nope Bflag %d",txSendFlag);
		}
        ESP_LOGI(TAG, "Read stored Bflag %d",txSendFlag);
        if(change){
			esp_err = nvs_commit(handle);
		}
        nvs_close(handle);
		nvs_sync_unlock();
    }

    /* start the wifi manager */
    wifi_manager_start();
/*
nvs_handle handle;

if(nvs_sync_lock( portMAX_DELAY )){  
    if(nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle) == ESP_OK){
        // do something with NVS 
	nvs_close(handle);
    }
    nvs_sync_unlock();
}
*/


    /* register a callback as an example to how you can integrate your code with the wifi manager */
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);
    wifi_manager_set_callback(WM_ORDER_DISCONNECT_STA, &cb_connection_off);


    /* your code should go here. Here we simply create a task on core 2 that monitors free heap memory */
    xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);
 
    //init_p50x();

while (1) {

    if (rxFlag == 1) // && txSendFlag==0
    {
        ESP_LOGI(TAG, "Prepare parser");
        receive(rxclient, (uint8_t *)&rxBuffer);
        ESP_LOGI(TAG, "Parser done!");
        // memset((uint8_t *)&rxBuffer, 0, 128);
        rxlen = 0;
        rxclient = 0;
        rxFlag = 0;
        }
}
free(txBuffer);
free(rxBuffer);
}


//--------------------------------------------------------------------------------------------
// z21 library callback functions
//--------------------------------------------------------------------------------------------
void notifyz21RailPower(uint8_t State)
{
    //#if defined(DEBUGSERIAL)
    //DEBUGSERIAL.print("Power: ");
    //DEBUGSERIAL.println(State, HEX);
    //#endif
    setPower(State);
}

//--------------------------------------------------------------------------------------------
void notifyz21EthSend(uint8_t client, uint8_t *data, uint8_t datalen)
{
    //ESP_LOGI(Z21_SENDER_TAG, "Hello in notifyz21EthSend. Client is %d, sending data:", client);
    //ESP_LOG_BUFFER_HEXDUMP(Z21_SENDER_TAG, data, datalen, ESP_LOG_INFO);
    ip4_addr_t Addr;
    if (client == 0)
    { //all stored
        for (uint8_t i = 0; i < storedIP; i++)
        {
           // if (mem[i].port != 0)
            //{
                //IP4_ADDR(&Addr, mem[i].IP0, mem[i].IP1, mem[i].IP2, mem[i].IP3);
                while (txSendFlag)
                {
                    //yield();
                }
                //txAddr = Addr;
                txBlen = datalen;
                //txport = mem[0].port;
                memcpy((uint8_t *)&txBuffer, data, datalen);
                txBflag=1;
                txSendFlag = 1;
                //while (txSendFlag)
                //{
                //};
                //Udp.beginPacket(ip, mem[i].port); //Broadcast
                //Udp.write(data, data[0]);
                //Udp.endPacket();
           // }
        }
    }
    else
    {
        IP4_ADDR(&Addr, mem[client - 1].IP0, mem[client - 1].IP1, mem[client - 1].IP2, mem[client - 1].IP3);
        while (txSendFlag)
        {
        };
        txAddr = Addr;
        txBlen = datalen;
        txport = mem[client - 1].port;
        memcpy((uint8_t *)&txBuffer, data, datalen);
        txSendFlag = 1;
        //while (txSendFlag)
        //{
        //};
        //Udp.beginPacket(ip, mem[client - 1].port); //no Broadcast
        //Udp.write(data, data[0]);
        //Udp.endPacket();
    }
}
//--------------------------------------------------------------------------------------------
void notifyz21LNdetector(uint8_t client, uint8_t typ, uint16_t ID)
{
    if (typ == 0x80)
    { //SIC Abfrage
        uint8_t data[4];
        data[0] = 0x01; //Typ
        data[1] = ID & 0xFF;
        data[2] = ID >> 8;
        data[3] = 0x00001;//getBasicAccessoryInfo(Adr); //Zustand Rückmelder
        setLNDetector(client, data, 4);
    }
}


void notifyz21getSystemInfo(uint8_t client)
{
    uint8_t data[20];
    data[0] = 0x14;
    data[1] = 0x00;
    data[2] = 0x80;
    data[3] = 0x00;
    data[4] = 0x10;        //MainCurrent mA
    data[5] = 0x10;        //MainCurrent mA
    data[6] = 0x00;        //ProgCurrent mA
    data[7] = 0x00;        //ProgCurrent mA
    data[8] = 0x00;        //FilteredMainCurrent
    data[9] = 0x00;        //FilteredMainCurrent
    data[10] = 0x20;       //Temperature
    data[11] = 0x20;       //Temperature
    data[12] = 0x0F;       //SupplyVoltage
    data[13] = 0x0F;       //SupplyVoltage
    data[14] = 0x0F;       //VCCVoltage
    data[15] = 0x03;       //VCCVoltage
    data[16] = getPower(); //CentralState
    data[17] = 0x00;       //CentralStateEx
    data[18] = 0x00;       //reserved
    data[19] = 0x00;       //reserved
    notifyz21EthSend(client, data, 20);
}

//--------------------------------------------------------------------------------------------
