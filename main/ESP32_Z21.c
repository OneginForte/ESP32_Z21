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
#include <freertos/queue.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_int_wdt.h"
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
#include "XBusInterface.h"
#include "XpressNet.h"



/* @brief tag used for ESP serial console messages */
static const char TAG[] = "Z21";
static const char *Z21_TASK_TAG = "Z21_UDP_RECIEVER";
static const char *Z21_SENDER_TAG = "Z21_UDP_SENDER";

static const int XNET_RX_BUF_SIZE = 128;

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

#define MY_ADDRESS 29

const char z21_nvs_namespace[] = "esp_z21";

TaskHandle_t xHandle1;
TaskHandle_t xHandle2;

void (**cb_z21_ptr_arr)(void *) = NULL;

BaseType_t Z21_send_message(cb_z21_message_t code, void *param)
{
    z21_message msg;
    msg.code = code;
    msg.param = param;
    return xQueueSend(z21_queue, &msg, portMAX_DELAY);
}

void Z21_set_callback(cb_z21_message_t message_code, void (*func_ptr)(void *))
{

    if (cb_z21_ptr_arr && message_code < MESSAGE_Z21_CODE_COUNT)
    {
        cb_z21_ptr_arr[message_code] = func_ptr;
    }
}

void cb_z21sender(void *pvParameters)
{
    //char addr_str[128];
    //uint8_t *addr_str = (uint8_t *)malloc(16);
    //ESP_LOGI(Z21_SENDER_TAG, "UDP sender started");
    //memset(&addr_str, 0x00, sizeof(addr_str));
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    int opt = 1;
    setsockopt(global_sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

        //while (1) {

            //while (txSendFlag)
            //{
            //}
                ESP_LOGI(Z21_SENDER_TAG, "New message to UDP sender");

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
                    //ip4addr_ntoa_r((const ip4_addr_t *)&(((struct sockaddr_in *)&dest_addr)->sin_addr), addr_str, sizeof(addr_str) - 1);
                    //ESP_LOGI(Z21_SENDER_TAG, "Hurrah! New message to %s, %d:", addr_str, htons (dest_addr.sin_port));
                    }
                
                txBflag=0;

                //ESP_LOG_BUFFER_HEXDUMP(Z21_SENDER_TAG, (uint8_t *)&Z21txBuffer, txBlen, ESP_LOG_INFO);

                int err = sendto(global_sock, (uint8_t *)&Z21txBuffer, txBlen, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                
                if (err < 0)
                    {
                    ESP_LOGE(Z21_SENDER_TAG, "Error occurred during sending: errno %d", errno);
                    txSendFlag = 0;
                    vTaskDelay(5 / portTICK_PERIOD_MS);
                    return;
                    }
                    
                //ESP_LOGI(Z21_SENDER_TAG, "%d bytes send.", txBlen);
                    //memset(&Z21txBuffer,0,Z21_UDP_TX_MAX_SIZE);
                bzero(&Z21txBuffer, Z21_UDP_TX_MAX_SIZE);
                txSendFlag = 0;
                vTaskDelay(5 / portTICK_PERIOD_MS);
                return;
            //}
            //vTaskDelay(10 / portTICK_PERIOD_MS);
        //}
        

    //free(addr_str);
    //vTaskDelete(NULL);

}

static void udp_server_task(void *pvParameters)
{
    uint8_t rx_buffer[128];
    char addr_str[16];
    //uint8_t * rxBuffer= (uint8_t *)malloc(Z21_UDP_RX_MAX_SIZE);
    //memset(&addr_str, 0x00, sizeof(addr_str));
    //uint8_t *Z21_rx_buffer = (uint8_t *)malloc(Z21_UDP_RX_MAX_SIZE);
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    //struct sockaddr_in dest_addr;
    static struct sockaddr_in dest_addr;
    static unsigned int socklen;
    socklen = sizeof(dest_addr);

    while (1)
        {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
        z21rcvFlag = false;
        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        global_sock = sock;
        
        if (sock < 0)
            {
            ESP_LOGE(Z21_TASK_TAG, "Unable to create socket: errno %d", errno);
            break;
            } 
        else 
            {
            ESP_LOGI(Z21_TASK_TAG, "Socket created: %d", sock);

            int err = bind(sock, (struct sockaddr *)&dest_addr, socklen);
            if (err < 0)
                {
                ESP_LOGE(Z21_TASK_TAG, "Socket unable to bind: errno %d", errno);
                break;
                }
            listen(sock, 1);
            //xTaskCreate(udp_sender_task, "udp_client", 4096, NULL, 2, &xHandle2);
            ESP_LOGI(Z21_TASK_TAG, "Socket bound, port %d", PORT);
            ESP_LOGI(Z21_TASK_TAG, "Waiting for data");
            ESP_LOGI(Z21_TASK_TAG, "UDP server task started.");
            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklenr = sizeof(source_addr);
            dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
            while (1)
                {
                    
                    int Len = recvfrom(sock, rx_buffer, Z21_UDP_RX_MAX_SIZE, 0, (struct sockaddr *)&source_addr, &socklenr);
                    // Error occurred during receiving
                    if (Len < 0)
                    {
                        ESP_LOGE(Z21_TASK_TAG, "Recvfrom failed: errno %d", errno);
                        break;
                    }
                    // Data received
                    else
                    {
                        ip4addr_ntoa_r((const ip4_addr_t*)&(((struct sockaddr_in *)&source_addr)->sin_addr), addr_str, sizeof(addr_str) - 1);
                        uint16_t from_port = (((struct sockaddr_in *)&source_addr)->sin_port);
                        z21client = Z21addIP(ip4_addr1((const ip4_addr_t *)&(((struct sockaddr_in *)&source_addr)->sin_addr)), ip4_addr2((const ip4_addr_t *)&(((struct sockaddr_in *)&source_addr)->sin_addr)), ip4_addr3((const ip4_addr_t *)&(((struct sockaddr_in *)&source_addr)->sin_addr)), ip4_addr4((const ip4_addr_t *)&(((struct sockaddr_in *)&source_addr)->sin_addr)), from_port);
                        ESP_LOGI(Z21_TASK_TAG, "Recieve from UDP");
                        ESP_LOG_BUFFER_HEXDUMP(Z21_TASK_TAG, (uint8_t *)&rx_buffer, Len, ESP_LOG_INFO);
                        while (z21rcvFlag)
                        {
                        //   vTaskDelay(5 / portTICK_PERIOD_MS);
                        }
                       memcpy(Z21rxBuffer, rx_buffer, Len);
                       z21RcvLen = Len;
                       z21rcvFlag = true;
                       bzero(rx_buffer, Z21_UDP_RX_MAX_SIZE);
                       Len = 0;
                       //Z21_send_message(MESSAGE_Z21_SERVER, NULL);
                       if (cb_z21_ptr_arr[MESSAGE_Z21_SERVER])
                           (*cb_z21_ptr_arr[MESSAGE_Z21_SERVER])(NULL);

                       // receive(client, Z21rxBuffer, len);
                       vTaskDelay(5 / portTICK_PERIOD_MS);
                    }
                    //vTaskDelay(10 / portTICK_PERIOD_MS);
                }
                if (sock != -1)
                    {
                    ESP_LOGE(Z21_TASK_TAG, "Shutting down socket and restarting...");
                    shutdown(sock, SHUT_RDWR);
                    close(sock);
                }
            }
           
        }
    //free(addr_str);
    vTaskDelete(NULL);
}

int XnetSendData(const uint8_t *data, uint8_t len)
{
    const uint16_t txBytes = uart_write_bytes(UART_NUM_1, &data, len);
    
    return txBytes;
}

int XnetSendString(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes_with_break(UART_NUM_1, data, len, 100);
    ESP_LOGI(logName, "Wrote to Xnet %d bytes", txBytes);
    return txBytes;
}

void cb_z21_parse(void *pvParameter)
{
    static const char *Z21_PARSER_TAG = "Z21_PARSER";
    esp_log_level_set(Z21_PARSER_TAG, ESP_LOG_INFO);
    //ESP_LOGI(Z21_PARSER_TAG, "Z21 Parser task start.");
    uint8_t rxlen;
    uint8_t client;
    uint8_t data[16];

       //if (z21rcvFlag)
        //{
            
            rxlen = z21RcvLen;
            client = z21client;
            addIPToSlot(client, 0);
            // send a reply, to the IP address and port that sent us the packet we received
            int header = (Z21rxBuffer[3] << 8) + Z21rxBuffer[2];
             // z21 send storage
            ESP_LOGI(Z21_PARSER_TAG, "Hello in Z21 parser!");
            //#if defined(ESP32)
            // portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
            //#endif

            switch (header)
            {
            case LAN_GET_SERIAL_NUMBER:
                ESP_LOGI(Z21_PARSER_TAG, "GET_SERIAL_NUMBER");

                // ESP_LOGI(Z21_PARSER_TAG, "Socket created");

                // data[0] = FSTORAGE.read(CONFz21SnLSB);
                // data[1] = FSTORAGE.read(CONFz21SnMSB);
                data[0] = z21SnLSB;
                data[1] = z21SnMSB;
                data[2] = 0x00;
                data[3] = 0x00;
                EthSend(client, 0x08, LAN_GET_SERIAL_NUMBER, data, false, Z21bcNone); // Seriennummer 32 Bit (little endian)
                break;
            case LAN_GET_HWINFO:
                ESP_LOGI(Z21_PARSER_TAG, "GET_HWINFO");

                data[0] = z21HWTypeLSB; // HwType 32 Bit
                data[1] = z21HWTypeMSB;
                data[2] = 0x00;
                data[3] = 0x00;
                data[4] = z21FWVersionLSB; // FW Version 32 Bit
                data[5] = z21FWVersionMSB;
                data[6] = 0x00;
                data[7] = 0x00;
                EthSend(client, 0x0C, LAN_GET_HWINFO, data, false, Z21bcNone);
                break;
            case LAN_LOGOFF:
                ESP_LOGI(Z21_PARSER_TAG, "LOGOFF");

                clearIPSlot(client);
                // Antwort von Z21: keine
                break;
            case LAN_GET_CODE: // SW Feature-Umfang der Z21
                /*#define Z21_NO_LOCK        0x00  // keine Features gesperrt
                    #define z21_START_LOCKED   0x01  // �z21 start�: Fahren und Schalten per LAN gesperrt
                    #define z21_START_UNLOCKED 0x02  // �z21 start�: alle Feature-Sperren aufgehoben */
                data[0] = 0x00; // keine Features gesperrt
                EthSend(client, 0x05, LAN_GET_CODE, data, false, Z21bcNone);
                break;
            case (LAN_X_Header):
                switch (Z21rxBuffer[4])
                { // X-Header
                case LAN_X_GET_SETTING:
                    switch (Z21rxBuffer[5])
                    { // DB0
                    case 0x21:
                        ESP_LOGI(Z21_PARSER_TAG, "X_GET_VERSION");

                        data[0] = LAN_X_GET_VERSION; // X-Header: 0x63
                        data[1] = 0x21;              // DB0
                        data[2] = 0x30;              // X-Bus Version
                        data[3] = 0x12;              // ID der Zentrale
                        EthSend(client, 0x09, LAN_X_Header, data, true, Z21bcNone);
                        break;
                    case 0x24:
                        data[0] = LAN_X_STATUS_CHANGED; // X-Header: 0x62
                        data[1] = 0x22;                 // DB0
                        data[2] = Railpower;            // DB1: Status
                        // ZDebug.print("X_GET_STATUS ");
                        // csEmergencyStop  0x01 // Der Nothalt ist eingeschaltet
                        // csTrackVoltageOff  0x02 // Die Gleisspannung ist abgeschaltet
                        // csShortCircuit  0x04 // Kurzschluss
                        // csProgrammingModeActive 0x20 // Der Programmiermodus ist aktiv
                        EthSend(client, 0x08, LAN_X_Header, data, true, Z21bcNone);
                        break;
                    case 0x80:
                        ESP_LOGI(Z21_PARSER_TAG, "X_SET_TRACK_POWER_OFF");

                        if (notifyz21RailPower)
                            notifyz21RailPower(csTrackVoltageOff);
                        break;
                    case 0x81:
                        ESP_LOGI(Z21_PARSER_TAG, "X_SET_TRACK_POWER_ON");

                        data[0] = LAN_X_BC_TRACK_POWER;
                        data[1] = 0x01;
                        EthSend(client, 0x07, LAN_X_Header, data, true, Z21bcNone);

                        if (notifyz21RailPower)
                            notifyz21RailPower(csNormal);
                        break;
                    }
                    break; // ENDE DB0
                case LAN_X_CV_READ:
                    if (Z21rxBuffer[5] == 0x11)
                    { // DB0
                        ESP_LOGI(Z21_PARSER_TAG, "X_CV_READ");

                        if (notifyz21CVREAD)
                            notifyz21CVREAD(Z21rxBuffer[6], Z21rxBuffer[7]); // CV_MSB, CV_LSB
                    }
                    break;
                case LAN_X_CV_WRITE:
                    if (Z21rxBuffer[5] == 0x12)
                    { // DB0
                        ESP_LOGI(Z21_PARSER_TAG, "X_CV_WRITE");

                        if (notifyz21CVWRITE)
                            notifyz21CVWRITE(Z21rxBuffer[6], Z21rxBuffer[7], Z21rxBuffer[8]); // CV_MSB, CV_LSB, value
                    }
                    break;
                case LAN_X_CV_POM:
                    if (Z21rxBuffer[5] == 0x30)
                    { // DB0
                        uint8_t Adr = ((Z21rxBuffer[6] & 0x3F) << 8) + Z21rxBuffer[7];
                        uint8_t CVAdr = ((Z21rxBuffer[8] & 0b11000000) << 8) + Z21rxBuffer[9];
                        uint8_t value = Z21rxBuffer[10];
                        if ((Z21rxBuffer[8] & 0xFC) == 0xEC)
                        {
                            ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_WRITE_BYTE");

                            if (notifyz21CVPOMWRITEBYTE)
                                notifyz21CVPOMWRITEBYTE(Adr, CVAdr, value); // set decoder
                        }
                        else if ((Z21rxBuffer[8] & 0xFC) == 0xE8)
                        {
                            ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_WRITE_BIT");
                        }
                        else
                        {
                            ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_READ_BIYTE");

                            if (notifyz21CVPOMREADBYTE)
                                notifyz21CVPOMREADBYTE(Adr, CVAdr); // set decoder
                        }
                    }
                    else if (Z21rxBuffer[5] == 0x31)
                    { // DB0
                        ESP_LOGI(Z21_PARSER_TAG, "LAN_X_CV_POM_ACCESSORY");
                    }
                    break;
                case LAN_X_GET_TURNOUT_INFO:
                {
                    ESP_LOGI(Z21_PARSER_TAG, "X_GET_TURNOUT_INFO ");

                    if (notifyz21AccessoryInfo)
                    {
                        data[0] = 0x43;                                                             // X-HEADER
                        data[1] = Z21rxBuffer[5];                                                   // High
                        data[2] = Z21rxBuffer[6];                                                   // Low
                        if (notifyz21AccessoryInfo((Z21rxBuffer[5] << 8) + Z21rxBuffer[6]) == true) // setCANDetector(uint16_t NID, uint16_t Adr, uint8_t port, uint8_t typ, uint16_t v1, uint16_t v2);
                            data[3] = 0x02;                                                         // active
                        else
                            data[3] = 0x01;                                         // inactive
                        EthSend(client, 0x09, LAN_X_Header, data, true, Z21bcNone); // BC new 23.04. !!! (old = 0)
                    }
                    break;
                }
                case LAN_X_SET_TURNOUT:
                {
                    ESP_LOGI(Z21_PARSER_TAG, "X_SET_TURNOUT");

                    // bool TurnOnOff = bitRead(Z21rxBuffer[7],3);  //Spule EIN/AUS
                    if (notifyz21Accessory)
                    {
                        notifyz21Accessory((Z21rxBuffer[5] << 8) + Z21rxBuffer[6], bitRead(Z21rxBuffer[7], 0), bitRead(Z21rxBuffer[7], 3));
                    } //	Addresse					Links/Rechts			Spule EIN/AUS
                    break;
                }
                case LAN_X_SET_EXT_ACCESSORY:
                {
                    ESP_LOGI(Z21_PARSER_TAG, "X_SET_EXT_ACCESSORY RAdr");
                    // setExtACCInfo((Z21rxBuffer[5] << 8) + Z21rxBuffer[6], Z21rxBuffer[7]);
                    break;
                }
                case LAN_X_GET_EXT_ACCESSORY_INFO:
                {
                    ESP_LOGI(Z21_PARSER_TAG, "X_EXT_ACCESSORY_INFO RArd.");
                    // setExtACCInfo((packet[5] << 8) + packet[6], packet[7]);
                    break;
                }
                case LAN_X_SET_STOP:
                    ESP_LOGI(Z21_PARSER_TAG, "X_SET_STOP");

                    if (notifyz21RailPower)
                        notifyz21RailPower(csEmergencyStop);
                    break;
                case LAN_X_GET_LOCO_INFO:
                    ESP_LOGI(Z21_PARSER_TAG, "LAN_X_GET_LOCO_INFO");
                    ESP_LOG_BUFFER_HEXDUMP(Z21_PARSER_TAG, Z21rxBuffer, rxlen, ESP_LOG_INFO);
                    if (Z21rxBuffer[5] == 0xF0)
                    { // DB0
                        // ZDebug.print("X_GET_LOCO_INFO: ");
                        // Antwort: LAN_X_LOCO_INFO  Adr_MSB - Adr_LSB
                        // if (notifyz21getLocoState)
                        // notifyz21getLocoState(((packet[6] & 0x3F) << 8) + packet[7], false);
                        getLocoInfo(Word(Z21rxBuffer[6] & 0x3F, Z21rxBuffer[7]));
                        // uint16_t WORD = (((uint16_t)packet[6] & 0x3F) << 8) | ((uint16_t)packet[7]);
                        ESP_LOGI(Z21_PARSER_TAG, "Adr:  %d", Word(Z21rxBuffer[6] & 0x3F, Z21rxBuffer[7]));
                        returnLocoStateFull(client, Word(Z21rxBuffer[6] & 0x3F, Z21rxBuffer[7]), false);
                        // Antwort via "setLocoStateFull"!
                    }
                    break;
                case LAN_X_SET_LOCO:
                    // setLocoBusy:
                    ESP_LOGI(Z21_PARSER_TAG, "LAN_X_SET_LOCO aka setLocoBusy: %d", Word(Z21rxBuffer[6] & 0x3F, Z21rxBuffer[7]));
                    ESP_LOG_BUFFER_HEXDUMP(Z21_PARSER_TAG, Z21rxBuffer, rxlen, ESP_LOG_INFO);
                    // uint16_t WORD = (((uint16_t)Z21rxBuffer[6] & 0x3F) << 8) | ((uint16_t)Z21rxBuffer[7]);

                    addBusySlot(client, Word(Z21rxBuffer[6] & 0x3F, Z21rxBuffer[7]));

                    if (Z21rxBuffer[5] == LAN_X_SET_LOCO_FUNCTION)
                    { // DB0
                        // LAN_X_SET_LOCO_FUNCTION  Adr_MSB        Adr_LSB            Type (00=AUS/01=EIN/10=UM)      Funktion
                        // word(packet[6] & 0x3F, packet[7]), packet[8] >> 6, packet[8] & 0b00111111

                        if (notifyz21LocoFkt)
                            notifyz21LocoFkt(Word(Z21rxBuffer[6] & 0x3F, Z21rxBuffer[7]), Z21rxBuffer[8] >> 6, Z21rxBuffer[8] & 0b00111111);
                        // uint16_t Adr, uint8_t type, uint8_t fkt
                    }
                    else
                    { // DB0

                        /*
                        0x1SAnzahl der Fahrstufen,abhängig vomeingestellten Schienenformat
                        S=0: DCC14 Fahrstufenbzw. MMI mit 14 Fahrstufen und F0
                        S=2:DCC28 Fahrstufenbzw. MMII mit 14 realenFahrstufen und F0-F4
                        S=3:DCC 128 Fahrstufen(alias„126 Fahrstufen“ ohne die Stops),bzw. MMII mit 28 realenFahrstufen (Licht-Trit) und F0-F4
                        */
                        // ZDebug.print("X_SET_LOCO_DRIVE ");
                        uint8_t steps = DCC128;
                        if ((Z21rxBuffer[5] & 0x03) == DCC128)
                            steps = DCC128;
                        else if ((Z21rxBuffer[5] & 0x03) == DCC28)
                            steps = DCC28;
                        else if ((Z21rxBuffer[5] & 0x03) == DCC14)
                            steps = DCC14;

                        if (notifyz21LocoSpeed)
                            notifyz21LocoSpeed(Word(Z21rxBuffer[6] & 0x3F, Z21rxBuffer[7]), Z21rxBuffer[8], steps);
                    }
                    returnLocoStateFull(client, Word(Z21rxBuffer[6] & 0x3F, Z21rxBuffer[7]), true);
                    break;
                case LAN_X_GET_FIRMWARE_VERSION:
                    ESP_LOGI(Z21_PARSER_TAG, "X_GET_FIRMWARE_VERSION");

                    data[0] = 0xF3;            // identify Firmware (not change)
                    data[1] = 0x0A;            // identify Firmware (not change)
                    data[2] = z21FWVersionMSB; // V_MSB
                    data[3] = z21FWVersionLSB; // V_LSB
                    EthSend(client, 0x09, LAN_X_Header, data, true, Z21bcNone);
                    break;
                case 0x73:
                    // LAN_X_??? WLANmaus periodische Abfrage:
                    // 0x09 0x00 0x40 0x00 0x73 0x00 0xFF 0xFF 0x00
                    // length X-Header	XNet-Msg			  speed?
                    ESP_LOGI(Z21_PARSER_TAG, "LAN-X_WLANmaus");

                    // set Broadcastflags for WLANmaus:
                    if (addIPToSlot(client, 0x00) == 0)
                        addIPToSlot(client, Z21bcAll);
                    break;
                default:

                    ESP_LOGI(Z21_PARSER_TAG, "UNKNOWN_LAN-X_COMMAND");

                    data[0] = 0x61;
                    data[1] = 0x82;
                    EthSend(client, 0x07, LAN_X_Header, data, true, Z21bcNone);
                }
                break;
                //---------------------- LAN X-Header ENDE ---------------------------
            case (LAN_SET_BROADCASTFLAGS):
            {
                unsigned long bcflag = Z21rxBuffer[7];
                bcflag = Z21rxBuffer[6] | (bcflag << 8);
                bcflag = Z21rxBuffer[5] | (bcflag << 8);
                bcflag = Z21rxBuffer[4] | (bcflag << 8);
                addIPToSlot(client, getLocalBcFlag(bcflag));
                // no inside of the protokoll, but good to have:
                if (notifyz21RailPower)
                    notifyz21RailPower(Railpower); // Zustand Gleisspannung Antworten
                ESP_LOGI(Z21_PARSER_TAG, "SET_BROADCASTFLAGS: ");
                int *ptr = (int *)&bcflag;
                ESP_LOGI(Z21_PARSER_TAG, "%p", ptr);
                // ESP_LOG_BUFFER_CHAR(Z21_PARSER_TAG, &bcflag, 4);
                //  1=BC Power, Loco INFO, Trnt INFO; 2=BC �nderungen der R�ckmelder am R-Bus

                break;
            }
            case (LAN_GET_BROADCASTFLAGS):
            {
                uint16_t flag = getz21BcFlag(addIPToSlot(client, 0x00));
                data[0] = flag;
                data[1] = flag >> 8;
                data[2] = flag >> 16;
                data[3] = flag >> 24;
                EthSend(client, 0x08, LAN_GET_BROADCASTFLAGS, data, false, Z21bcNone);
                ESP_LOGI(Z21_PARSER_TAG, "GET_BROADCASTFLAGS: ");

                break;
            }
            case (LAN_GET_LOCOMODE):
                data[0] = Z21rxBuffer[4];
                data[1] = Z21rxBuffer[5];
                data[2] = 0; // 0=DCC Format; 1=MM Format
                EthSend(client, 0x07, LAN_GET_LOCOMODE, data, false, Z21bcNone);
                break;
            case (LAN_SET_LOCOMODE):
                break;
            case (LAN_GET_TURNOUTMODE):
                data[0] = Z21rxBuffer[4];
                data[1] = Z21rxBuffer[5];
                data[2] = 0; // 0=DCC Format; 1=MM Format
                EthSend(client, 0x07, LAN_GET_LOCOMODE, data, false, Z21bcNone);
                break;
            case (LAN_SET_TURNOUTMODE):
                break;
            case (LAN_RMBUS_GETDATA):
                if (notifyz21S88Data)
                {
                    ESP_LOGI(Z21_PARSER_TAG, "RMBUS_GETDATA");

                    // ask for group state 'Gruppenindex'
                    notifyz21S88Data(Z21rxBuffer[4]); // normal Antwort hier nur an den anfragenden Client! (Antwort geht hier an alle!)
                }
                break;
            case (LAN_RMBUS_PROGRAMMODULE):
                break;
            case (LAN_SYSTEMSTATE_GETDATA):
            { // System state
                ESP_LOGI(Z21_PARSER_TAG, "LAN_SYS-State");

                if (notifyz21getSystemInfo)
                    notifyz21getSystemInfo(client);
                break;
            }
            case (LAN_RAILCOM_GETDATA):
            {
                uint16_t Adr = 0;
                if (Z21rxBuffer[4] == 0x01)
                {
                    // RailCom-Daten f�r die gegebene Lokadresse anfordern
                    // Adr = word(packet[6], packet[5]);
                    Adr = (((uint16_t)Z21rxBuffer[6]) << 8) | ((uint16_t)Z21rxBuffer[5]);
                }
                if (notifyz21Railcom)
                {
                    Adr = notifyz21Railcom(); // return global Railcom Adr
                }

                data[0] = Adr >> 8;   // LocoAddress
                data[1] = Adr & 0xFF; // LocoAddress
                data[2] = 0x00;       // UINT32 ReceiveCounter Empfangsz�hler in Z21
                data[3] = 0x00;
                data[4] = 0x00;
                data[5] = 0x00;
                data[6] = 0x00; // UINT32 ErrorCounter Empfangsfehlerz�hler in Z21
                data[7] = 0x00;
                data[8] = 0x00;
                data[9] = 0x00;
                /*
              data[10] = 0x00;	//UINT8 Reserved1 experimentell, siehe Anmerkung
              data[11] = 0x00;	//UINT8 Reserved2 experimentell, siehe Anmerkung
              data[12] = 0x00;	//UINT8 Reserved3 experimentell, siehe Anmerkung
              */
                EthSend(client, 0x0E, LAN_RAILCOM_DATACHANGED, data, false, Z21bcNone);
                break;
            }
            case (LAN_LOCONET_FROM_LAN):
            {
                ESP_LOGI(Z21_PARSER_TAG, "LOCONET_FROM_LAN");

                if (notifyz21LNSendPacket)
                {
                    uint8_t LNdata[Z21rxBuffer[0] - 0x04]; // n Bytes
                    for (uint8_t i = 0; i < (Z21rxBuffer[0] - 0x04); i++)
                        LNdata[i] = Z21rxBuffer[0x04 + i];
                    notifyz21LNSendPacket(LNdata, Z21rxBuffer[0] - 0x04);
                    // Melden an andere LAN-Client das Meldung auf LocoNet-Bus geschrieben wurde
                    EthSend(client, Z21rxBuffer[0], LAN_LOCONET_FROM_LAN, Z21rxBuffer, false, Z21bcLocoNet_s); // LAN_LOCONET_FROM_LAN
                }
                break;
            }
            case (LAN_LOCONET_DISPATCH_ADDR):
            {
                if (notifyz21LNdispatch)
                {
                    data[0] = Z21rxBuffer[4];
                    data[1] = Z21rxBuffer[5];
                    data[2] = notifyz21LNdispatch(Z21rxBuffer[5], Z21rxBuffer[4]); // dispatchSlot
                    ESP_LOGI(Z21_PARSER_TAG, "LOCONET_DISPATCH_ADDR ");

                    EthSend(client, 0x07, LAN_LOCONET_DISPATCH_ADDR, data, false, Z21bcNone);
                }
                break;
            }
            case (LAN_LOCONET_DETECTOR):
                if (notifyz21LNdetector)
                {
                    ESP_LOGI(Z21_PARSER_TAG, "LOCONET_DETECTOR Abfrage");
                    uint16_t ADDR = (((uint16_t)Z21rxBuffer[6]) << 8) | ((uint16_t)Z21rxBuffer[5]);
                    notifyz21LNdetector(client, Z21rxBuffer[4], ADDR); // Anforderung Typ & Reportadresse
                }
                break;
            case (LAN_CAN_DETECTOR):
                if (notifyz21CANdetector)
                {
                    ESP_LOGI(Z21_PARSER_TAG, "CAN_DETECTOR Abfrage");
                    uint16_t ADDR = (((uint16_t)Z21rxBuffer[6]) << 8) | ((uint16_t)Z21rxBuffer[5]);
                    notifyz21CANdetector(client, Z21rxBuffer[4], ADDR); // Anforderung Typ & CAN-ID
                }
                break;
            case (0x12): // configuration read
                // <-- 04 00 12 00
                // 0e 00 12 00 01 00 01 03 01 00 03 00 00 00
                data[0] = 0x0e;
                data[1] = 0x00;
                data[2] = 0x12;
                data[3] = 0x00;
                data[4] = 0x01;
                data[5] = 0x00;
                data[6] = 0x01;
                data[7] = 0x03;
                data[8] = 0x01;
                data[9] = 0x00;
                data[10] = 0x03;
                data[11] = 0x00;
                data[12] = 0x00;
                // for (uint8_t i = 0; i < 10; i++)
                //{
                // data[i] = FSTORAGE.read(CONF1STORE + i);
                // }
                EthSend(client, 0x0e, 0x12, data, false, Z21bcNone);
                ESP_LOGI(Z21_PARSER_TAG, "Z21 Eins(read) ");

                break;
            case (0x13):
            { // configuration write
                //<-- 0e 00 13 00 01 00 01 03 01 00 03 00 00 00
                // 0x0e = Length; 0x12 = Header
                /* Daten:
                    (0x01) RailCom: 0=aus/off, 1=ein/on
                    (0x00)
                    (0x01) Power-Button: 0=Gleisspannung aus, 1=Nothalt
                    (0x03) Auslese-Modus: 0=Nichts, 1=Bit, 2=Byte, 3=Beides
                    */
                ESP_LOGI(Z21_PARSER_TAG, "Z21 Eins(write) ");
                /*


                packet[4] = 0x0e;
                packet[5] = 0x00;
                packet[6] = 0x13;
                packet[7] = 0x00;
                packet[8] = 0x01;
                packet[9] = 0x00;
                packet[10] = 0x01;
                packet[11] = 0x03;
                packet[12] = 0x01;
                packet[13] = 0x00;
                packet[14] = 0x03;
                packet[15] = 0x00;
                packet[16] = 0x00;
                packet[17] = 0x00;

                //for (uint8_t i = 0; i < 10; i++)
                //{
                //	FSTORAGE.FSTORAGEMODE(CONF1STORE + i, packet[4 + i]);
                //}
                */
                // Request DCC to change
                // if (notifyz21UpdateConf)
                //	notifyz21UpdateConf();
                break;
            }
            case (0x16): // configuration read
            {            //<-- 04 00 16 00
                // 14 00 16 00 19 06 07 01 05 14 88 13 10 27 32 00 50 46 20 4e
                data[0] = 0x14;
                data[1] = 0x00;
                data[2] = 0x16;
                data[3] = 0x00;
                data[4] = 0x19;
                data[5] = 0x06;
                data[6] = 0x07;
                data[7] = 0x01;
                data[8] = 0x05;
                data[9] = 0x14;
                data[10] = 0x88;
                data[11] = 0x13;
                data[12] = 0x10;
                data[13] = 0x27;
                data[14] = 0x32;
                data[15] = 0x00;
                data[16] = 0x50;
                data[17] = 0x46;
                data[18] = 0x20;
                data[19] = 0x4e;
                // for (uint8_t i = 0; i < 16; i++)
                //{
                //	data[i] = FSTORAGE.read(CONF2STORE + i);
                //}
                EthSend(client, 0x14, 0x16, data, false, Z21bcNone);
                ESP_LOGI(Z21_PARSER_TAG, "Z21 Eins(read) ");

                break;
            }
            case (0x17):
            { // configuration write
                //<-- 14 00 17 00 19 06 07 01 05 14 88 13 10 27 32 00 50 46 20 4e
                // 0x14 = Length; 0x16 = Header(read), 0x17 = Header(write)
                /* Daten:
                    (0x19) Reset Packet (starten) (25-255)
                    (0x06) Reset Packet (fortsetzen) (6-64)
                    (0x07) Programmier-Packete (7-64)
                    (0x01) ?
                    (0x05) ?
                    (0x14) ?
                    (0x88) ?
                    (0x13) ?
                    (0x10) ?
                    (0x27) ?
                    (0x32) ?
                    (0x00) ?
                    (0x50) Hauptgleis (LSB) (12-22V)
                    (0x46) Hauptgleis (MSB)
                    (0x20) Programmiergleis (LSB) (12-22V): 20V=0x4e20, 21V=0x5208, 22V=0x55F0
                    (0x4e) Programmiergleis (MSB)
                    */
                ESP_LOGI(Z21_PARSER_TAG, "Z21 Eins(write) ");
                // for (uint8_t i = 0; i < 16; i++)
                //{
                //		FSTORAGE.FSTORAGEMODE(CONF2STORE + i, packet[4 + i]);
                // }
                Z21rxBuffer[4] = 0x14;
                Z21rxBuffer[5] = 0x00;
                Z21rxBuffer[6] = 0x17;
                Z21rxBuffer[7] = 0x00;
                Z21rxBuffer[8] = 0x19;
                Z21rxBuffer[9] = 0x06;
                Z21rxBuffer[10] = 0x07;
                Z21rxBuffer[11] = 0x01;
                Z21rxBuffer[12] = 0x05;
                Z21rxBuffer[13] = 0x14;
                Z21rxBuffer[14] = 0x88;
                Z21rxBuffer[15] = 0x13;
                Z21rxBuffer[16] = 0x10;
                Z21rxBuffer[17] = 0x27;
                Z21rxBuffer[18] = 0x32;
                Z21rxBuffer[19] = 0x00;
                Z21rxBuffer[20] = 0x50;
                Z21rxBuffer[21] = 0x46;
                Z21rxBuffer[22] = 0x20;
                Z21rxBuffer[23] = 0x4e;
                // Request DCC to change
                if (notifyz21UpdateConf)
                    notifyz21UpdateConf();
                break;
            }
            default:
                ESP_LOGI(Z21_PARSER_TAG, "UNKNOWN_COMMAND");

                ESP_LOG_BUFFER_HEXDUMP(Z21_PARSER_TAG, Z21rxBuffer, sizeof(Z21rxBuffer), ESP_LOG_INFO);

                data[0] = 0x61;
                data[1] = 0x82;
                EthSend(client, 0x07, LAN_X_Header, data, true, Z21bcNone);
            }
            //---------------------------------------------------------------------------------------
            // check if IP is still used:
            /*
            unsigned long currentMillis = (unsigned long)(esp_timer_get_time() / 1000);
            if ((currentMillis - z21IPpreviousMillis) > z21IPinterval)
            {
                z21IPpreviousMillis = currentMillis;
                for (uint8_t i = 0; i < z21clientMAX; i++)
                {
                    if (ActIP[i].time > 0)
                    {
                        ActIP[i].time--; //Zeit herrunterrechnen
                    }
                    else
                    {
                        clearIP(i); //clear IP DATA
                                                //send MESSAGE clear Client
                    }
                }
            }
            */
            z21rcvFlag = false;
        //}

        //vTaskDelay(100 / portTICK_PERIOD_MS);

}

static void xnet_rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "XNET_RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t *data = (uint8_t *)malloc(XNET_RX_BUF_SIZE);
    uint8_t pos_p;
    uint16_t rxb;
    ESP_LOGI(RX_TASK_TAG, "Xnet RX task start.");
    while (1) 
    {
        const uint16_t rxBytes = uart_read_bytes(UART_NUM_1, data, XNET_RX_BUF_SIZE, 100 / portTICK_RATE_MS);
        if (rxBytes > 0) { 
        pos_p=0;
        rxb=rxBytes;
        if (data[0] <= rxb){
            
            //ESP_LOGI(RX_TASK_TAG, "New Xnet data!");

            memcpy(XNetMsg, data + pos_p, data[pos_p]);
            if (XNetMsg[1] == 0xFF && XNetMsg[2] == 0xFA)
            {
                switch (XNetMsg[3]){
                case 0xA0: // XNet bridge is offline!
                ESP_LOGI(RX_TASK_TAG, "XnetRun false");
                XNetRun = false;
                break;
                case 0xA1: // Xnet bridge is online!
                ESP_LOGI(RX_TASK_TAG, "XnetRun true");
                XNetRun = true;
                break;
                case 0xA2: // Answer from Stop Xnet request

                break;
                case 0xA3: // XnetSlot addr request from Z21

                break;
                case 0xA4: // Set new XnetSlot addr

                break;
                default:

                break;
                }
            }
                else
                {
                    //memcpy(XNetMsg, data, rxBytes);
                    while (DataReady)
                    {
                        // vTaskDelay(pdMS_TO_TICKS(1));
                        //vTaskDelay(10 / portTICK_PERIOD_MS);
                    }
                    ESP_LOGI(RX_TASK_TAG, "Read from XNET %d bytes:", data[pos_p]);
                    ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data + pos_p, data[pos_p], ESP_LOG_INFO);
                    DataReady = true;
                    //Z21_send_message(MESSAGE_XNET_SERVER, NULL);
                    if (cb_z21_ptr_arr[MESSAGE_XNET_SERVER])
                        (*cb_z21_ptr_arr[MESSAGE_XNET_SERVER])(NULL);
                    // xnetreceive();
                    // data[pos_p] = 0;
                }

                rxb = rxb - data[pos_p];
                pos_p = pos_p + data[pos_p];
                

            }

        }

        //vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    free(data);
    vTaskDelete(NULL);
}

void init_XNET(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, XNET_RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreatePinnedToCore(xnet_rx_task, "xnet_rx_task", 2048 * 2, NULL, 5, NULL, 0);
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
        vTaskDelay(pdMS_TO_TICKS(10));
        
        unsigned long currentMillis = esp_timer_get_time();

        if (currentMillis - SlotTime > SlotInterval)
        {
            SlotTime = currentMillis;
            UpdateBusySlot(); // Server Update - Anfrage nach Status�nderungen
        }

        if (currentMillis - z21IPpreviousMillis > z21IPinterval)
        {
            z21IPpreviousMillis = currentMillis;
            for (int i = 0; i < z21clientMAX; i++)
            {
                if (ActIP[i].ip3 != 0)
                { //Slot nicht leer?
                    if (ActIP[i].time > 0)
                    {
                        ActIP[i].time--; //Zeit herrunterrechnen
                    }
                    else
                    {
                        clearIPSlot(i); //clear IP DATA
                    }
                }
            }
        }
        
    }
    vTaskDelete(NULL);
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

    xTaskCreatePinnedToCore(udp_server_task, "udp_server", 4096, (void *)AF_INET, 4, &xHandle1, 1);
    //xTaskCreate(z21_parse_task, "z21_parse_task", 4096, NULL, 6, NULL);
}
void cb_connection_off(void *pvParameter)
{
    //ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;
    vTaskDelete(xHandle2);
    vTaskDelete(xHandle1);
    ESP_LOGI(TAG, "I do not have connection. Stop UDP!");
}

//--------------------------------------------------------------------------------------------
// calculate the parity bit in the call byte for this guy
uint8_t callByteParity(uint8_t me)
{
    uint8_t parity = (1 == 0);
    uint8_t vv;
    me &= 0x7f;
    vv = me;

    while (vv)
    {
        parity = !parity;
        vv &= (vv - 1);
    }
    if (parity)
        me |= 0x80;
    return me;
}


void app_main()
{
  	//nvs_handle handle;
	//esp_err_t esp_err;
    z21_message z21msg;
    BaseType_t z21Status;
    cb_z21_ptr_arr = malloc(sizeof(void (*)(void *)) * MESSAGE_Z21_CODE_COUNT);
    for (int i = 0; i < MESSAGE_Z21_CODE_COUNT; i++)
    {
        cb_z21_ptr_arr[i] = NULL;
    }
    z21_queue = xQueueCreate(3, sizeof(z21msg));
    Z21_send_message(Z21_NONE, NULL);
    // size_t sz;
    //bool change = false;
    txBlen=0;
    txBflag=0;
    rxFlag=0;
    z21rcvFlag = false;
    //rxlen = 0;
    rxclient = 0;
    storedIP = 0;
    slotFullNext=0;
    DCCdefaultSteps = DCC128;
    TrntFormat = SwitchFormat;

        // initialize this instance's variables
    Railpower = 0xFF; //Ausgangs undef.

    XNetRun = false; //XNet ist inactive;
    LokStsclear();  //l�schen aktiver Loks in Slotserver
    ReqLocoAdr = 0;
    ReqLocoAgain = 0;
    ReqFktAdr = 0;
    SlotLast = 0;

    previousMillis = 0; //Reset Time Count
    myRequestAck = callByteParity(MY_ADDRESS | 0x00) | 0x100;
    myCallByteInquiry = callByteParity(MY_ADDRESS | 0x40) | 0x100;
    myDirectedOps = callByteParity(MY_ADDRESS | 0x60) | 0x100;

    for (uint8_t s = 0; s < 32; s++)
    {                           //clear busy slots
        SlotLokUse[s] = 0xFFFF; //slot is inactiv
    }

    uint8_t *Z21txBuffer = (uint8_t *)malloc(Z21_UDP_TX_MAX_SIZE);
    //uint8_t *Z21rxBuffer = (uint8_t *)malloc(Z21_UDP_RX_MAX_SIZE);
    
    // txSendFlag=0;
/*
    if (nvs_sync_lock(portMAX_DELAY))
        {
        // read value from flash 
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
*/
    init_XNET();
    

    //vTaskDelay(pdMS_TO_TICKS(100));
	
    //unsigned char getLoco[] = {0xE3, 0x00, 0, 3, 0x00};
	//getXOR(getLoco, 5);
	//XNettransmit (getLoco, 5);

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
    Z21_set_callback(MESSAGE_Z21_SERVER, &cb_z21_parse);
    Z21_set_callback(MESSAGE_XNET_SERVER, &cb_xnet_parse);
    Z21_set_callback(MESSAGE_Z21_SENDER, &cb_z21sender);

    /* your code should go here. Here we simply create a task on core 2 that monitors free heap memory */
    xTaskCreate(&monitoring_task, "monitoring_task", 1024*4, NULL, 6, NULL);
    
    //ReqLocoAdr = 3;
    //uint16_t rndAddr;
    //rndAddr = 3;
    //uint8_t rndSpeed;

   //rndAddr = (rand() % 1000);
    //rndSpeed = rand() % 127;
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
    //vTaskDelay(500 / portTICK_PERIOD_MS);
    //getLocoInfo(rndAddr);

    //ReqLocoAdr = rndAddr;
    //setLocoDrive(rndAddr, DCC128, rndSpeed);


    while (1) 
    {
    //if (DataReady==1){
       //xnetreceive();
    vTaskDelay(100 / portTICK_PERIOD_MS);
        
    //}
    z21Status = xQueueReceive(z21_queue, &z21msg, portMAX_DELAY);

    if (z21Status == pdPASS)
    {
        switch (z21msg.code)
        {

        case MESSAGE_XNET_SERVER:
        {
            //cb_xnet_parse(z21msg.param); // if(cb_z21_ptr_arr[z21msg.code]) (*cb_z21_ptr_arr[z21msg.code])(NULL);
        }
        break;
        case MESSAGE_Z21_SERVER:
        {
            //cb_z21_parse(z21msg.param); // if (cb_z21_ptr_arr[z21msg.code]) (*cb_z21_ptr_arr[z21msg.code])(NULL);
        }
        break;
        case MESSAGE_Z21_SENDER:
        {
            //cb_z21sender(z21msg.param); // if (cb_z21_ptr_arr[z21msg.code]) (*cb_z21_ptr_arr[z21msg.code])(NULL);
        }
        break;

        default:
            break;


        }
            // z21receive();
            // vTaskDelay(10 / portTICK_PERIOD_MS);
            // xnetreceive();
            // vTaskDelay(10 / portTICK_PERIOD_MS);

            // vTaskDelay(500 / portTICK_PERIOD_MS);

            // unsigned char setLoco[] = {0xE4, 0x13, highByte(rndAddr), lowByte(rndAddr), rand() % 127, 0x00};
            // getXOR(setLoco, 6);
            // XNettransmit(setLoco, 6);
            // ReqLocoAdr = rndAddr;
            // unsigned char getLoco[] = {0xE3, 0x00, highByte(rndAddr), lowByte(rndAddr), 0x00};
            // getXOR(getLoco, 5);
            // XNettransmit(getLoco, 5);
        }
    }
            free(Z21txBuffer);
            //free(Z21rxBuffer);
    }

//--------------------------------------------------------------------------------------------
// z21 library callback functions
//--------------------------------------------------------------------------------------------
void notifyz21RailPower(uint8_t State)
        {
            //#if defined(DEBUGSERIAL)
            // DEBUGSERIAL.print("Power: ");
            // DEBUGSERIAL.println(State, HEX);
            //#endif
            globalPower(State);
        }

//--------------------------------------------------------------------------------------------
void notifyz21EthSend(uint8_t client, uint8_t * data, uint8_t datalen)
        {
            
            // ESP_LOG_BUFFER_HEXDUMP(Z21_SENDER_TAG, data, datalen, ESP_LOG_INFO);
            ip4_addr_t Addr;
            while (txSendFlag)
            {
                //vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            ESP_LOGI(Z21_SENDER_TAG, "notifyz21EthSend. Client is %d, sending data:", client);
            if (client == 0)
            { // all stored
                for (uint8_t i = 0; i < storedIP; i++)
                {
                    //txAddr=0.0.0.0;
                    txBlen = datalen;
                    // txport = mem[0].port;
                    memcpy(Z21txBuffer, data, datalen);
                    // memset(data,0,datalen);
                    txBflag = 1;
                    txSendFlag = 1;
                    //Z21_send_message(MESSAGE_Z21_SENDER, NULL);
                    if (cb_z21_ptr_arr[MESSAGE_Z21_SENDER])
                        (*cb_z21_ptr_arr[MESSAGE_Z21_SENDER])(NULL);
                }
            }
            else
            {
                IP4_ADDR(&Addr, mem[client - 1].IP0, mem[client - 1].IP1, mem[client - 1].IP2, mem[client - 1].IP3);
                txAddr = Addr;
                txBlen = datalen;
                txBflag = 0;
                txport = mem[client - 1].port;
                memcpy(Z21txBuffer, data, datalen);
                txSendFlag = 1;
                //Z21_send_message(MESSAGE_Z21_SENDER, NULL);
                if (cb_z21_ptr_arr[MESSAGE_Z21_SENDER])
                    (*cb_z21_ptr_arr[MESSAGE_Z21_SENDER])(NULL);
            }
        }
//--------------------------------------------------------------------------------------------
void notifyz21LNdetector(uint8_t client, uint8_t typ, uint16_t ID)
        {
            if (typ == 0x80)
            { // SIC Abfrage
                uint8_t data[4];
                data[0] = 0x01; // Typ
                data[1] = ID & 0xFF;
                data[2] = ID >> 8;
                data[3] = 0x00001; // getBasicAccessoryInfo(Adr); //Zustand Rückmelder
                setLNDetector(client, data, 4);
            }
        }
//--------------------------------------------------------------------------------------------
void notifyz21getSystemInfo(uint8_t client)
        {
            uint8_t data[20];
            data[0] = 0x14;
            data[1] = 0x00;
            data[2] = 0x80;
            data[3] = 0x00;
            data[4] = 0x10;        // MainCurrent mA
            data[5] = 0x10;        // MainCurrent mA
            data[6] = 0x00;        // ProgCurrent mA
            data[7] = 0x00;        // ProgCurrent mA
            data[8] = 0x00;        // FilteredMainCurrent
            data[9] = 0x00;        // FilteredMainCurrent
            data[10] = 0x20;       // Temperature
            data[11] = 0x20;       // Temperature
            data[12] = 0x0F;       // SupplyVoltage
            data[13] = 0x0F;       // SupplyVoltage
            data[14] = 0x0F;       // VCCVoltage
            data[15] = 0x03;       // VCCVoltage
            data[16] = getPower(); // CentralState
            data[17] = 0x00;       // CentralStateEx
            data[18] = 0x00;       // reserved
            data[19] = 0x00;       // reserved
            notifyz21EthSend(client, data, 20);
        }
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
void notifyz21LocoSpeed(uint16_t Adr, uint8_t speed, uint8_t steps)
        {
            // setSpeed(Adr, steps, speed);
            setLocoDrive(Adr, steps, speed);
            reqLocoBusy(Adr); // Lok wird nicht von LokMaus gesteuert!
            switch (steps)
            {
            case DCC14:
                setSpeed14(Adr, speed);
                break;
            case DCC28:
                setSpeed28(Adr, speed);
                break;
            default:
                setSpeed128(Adr, speed);
            }
            getLocoStateFull(Adr, false);
        }

//--------------------------------------------------------------------------------------------
void notifyz21Accessory(uint16_t Adr, bool state, bool active)
        {
            setBasicAccessoryPos(Adr, state, active);
            setTrntPos(Adr, state, active);
        }

bool getBasicAccessoryInfo(uint16_t address)
        {
            switch (TrntFormat)
            {
            case IB:
                address = address + IB;
                break;
            }

            return bitRead(BasicAccessory[address / 8], address % 8); // Zustand aus Slot lesen
        }

//--------------------------------------------------------------------------------------------
uint8_t notifyz21AccessoryInfo(uint16_t Adr)
        // return state of the Address (left/right = true/false)
        {
            return getBasicAccessoryInfo(Adr);
        }

//--------------------------------------------------------------------------------------------
void notifyz21LocoFkt(uint16_t Adr, uint8_t state, uint8_t fkt)
        {
            setLocoFunc(Adr, state, fkt);

            if (fkt <= 4)
                setFunctions0to4(Adr, getFunktion0to4(Adr));
            else if (fkt >= 5 && fkt <= 8)
                setFunctions5to8(Adr, getFunktion5to8(Adr));
            else if (fkt >= 9 && fkt <= 12)
                setFunctions9to12(Adr, getFunktion9to12(Adr));
            else if (fkt >= 13 && fkt <= 20)
                setFunctions13to20(Adr, getFunktion13to20(Adr));
            else if (fkt >= 21 && fkt <= 28)
                setFunctions21to28(Adr, getFunktion21to28(Adr));
            reqLocoBusy(Adr); // Lok wird nicht von LokMaus gesteuert!
        }

        //--------------------------------------------------------------------------------------------
        void notifyCVInfo(uint8_t State)
        {
            if (State == 0x01 || State == 0x02)
            { // Busy or No Data
                // LAN_X_CV_NACK
                uint8_t data[2];
                data[0] = 0x61; // HEADER
                data[1] = 0x13; // DB0
                EthSend(0, 0x07, 0x40, data, true, 0x00);
            }
        }