/* Arduino IDE for ESP8266 only
      Z21 ESP UDP Interface for Arduino UNO / NANO / MEGA Zentral Station
      
      This Sketch communicate with the Z21 App via UDP on Port 21105
      It send all data over Serial out. It add to each serial packet a client identify code,
      so you are able to answer a request to the IP that ask.
      The UDP answer is generated by serial receive, the data always start with the identify code.
      The length of each packet is coded in the second byte, that is the first byte of the Z21 LAN protocoll!
      
      Example:
        Answer with HTerm to
        LAN X GET Version in HEX with: 0x00, 0x09, 0x00, 0x40, 0x00, 0x63, 0x21, 0x30, 0x12, 0x60  
                                     client | Z21 LAN protocoll        
      
      The Sketch provide a AJAX Webserver to configure the AP (after ESP reset) and client WiFi connection. 
        - Read more about AJAX: https://startingelectronics.org/tutorials/arduino/ethernet-shield-web-server-tutorial/web-server-read-switch-using-AJAX/
      
      Important:
      You can reset the stored data to default of the AP, 
      when you pull-down the GPIO 2 while starting the ESP8266!
*/      
#define VERSION 3.2
/*
      Copyright (c) 2021 Philipp Gahtow  All right reserved.
      digitalmoba@arcor.de

Changelog:
      02.06.2016 Add Auto Baudrate detection
      17.07.2016 Fix Network UDP answer port - send paket back to source port
      23.07.2017 accept also SSID length with only one char
      03.10.2018 change buffer declaration in line 506 from char to byte
      02.11.2018 change Serial read data and set default baud to 500000
      04.11.2018 add request config data with 'get' statement and change S88 setting data
      16.06.2019 add Basic OTA Update via Arduino IDE
      10.09.2019 Fix Z21 IP Subnet and Gateway to stay connected with new smartphones
      23.03.2020 Fix sending zero Packet with length = 1
      20.01.2021 add reporting client identificaion (IP-Hash) to master
                 add BuildInLED to show data communication
      21.01.2021 Fix empty/wrong client SSID and password when EEPROM is erased
*/ 

#define EnableOTA   //Programmierung über WLAN mit Arduino IDE
#define EnableZ21IP //Default IP is set to 192.168.0.111

#define SssidAP "Z21_ESP"   // Default Z21 AP (SSID)
#define SpassAP "12345678"  // Default Z21 network password
#define SkanalAP 6          // Default Kanal des AP

//*************************************************************
//*************************************************************
//For Debugging active flag and read out via serial:
#define DEBUG

#include <ESP8266WiFi.h>
#if defined(EnableOTA)
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#endif
#include <WiFiClient.h> 
#include <WiFiUDP.h>

#include <EEPROM.h>

WiFiServer server(80);

WiFiUDP Udp;

unsigned long serialbaud = 500000;   //Default Serial Baudrate
byte counterr = 0;
#define MaxError 10     //bei Übertragungsfehler - starte Neuerkennung Baudrate

//client ssid and pw stored in EEPROM!
String ssid = "";  
String pass = "";
 
//AP ssid and pw read out from EEPROM:
String ssidAP = "";
String passAP = "";
byte kanalAP = 6;  

#define maxIP 20                //Client IP that are stored
#define Z21_UDP_TX_MAX_SIZE 20  //max UDP packet size
#define localPort 21105

//Z21 WLAN Default config:
static IPAddress Ip(192, 168, 0, 111); //default IP
static IPAddress Gw(192, 168, 0, 111); //default Gateway
static IPAddress Sb(255, 255, 255, 0); //default Subnet

//EEPROM Konfiguration
#define EESize 512    //Größe des EEPROM
#define EEStringMaxSize 40    //Länge String im EEPROM
//Client:
#define EEssidLength 0       //Länge der SSID
#define EEssidBegin 1        //Start Wert
#define EEpassLength 32        //Länge des Passwort
#define EEpassBegin 33        //Start Wert
//AP:
#define EEssidAPLength 64       //Länge der SSID AP
#define EEssidAPBegin 65        //Start Wert
#define EEpassAPLength 98        //Länge des Passwort AP
#define EEpassAPBegin 99        //Start Wert
#define EEkanalAP 150          //Kanal AP

byte outData[Z21_UDP_TX_MAX_SIZE];    //store received Serial to send via UDP
byte sendTOO = 0xFF;  //memIP to whom to send the data
byte outDcount = 0;     //length of data read in
char lastbyte = 0;      //last read Serial Byte

typedef struct		//Rückmeldung des Status der Programmierung
{
  IPAddress IP;
  byte time;  //aktive Zeit
  unsigned int port;    //source Port
} listofIP;
listofIP mem[maxIP];

byte countIP = 0;    //zähler für Eintragungen

#define ActTimeIP 20    //Aktivhaltung einer IP für (sec./2)
#define interval 2000   //interval in milliseconds for checking IP aktiv state

unsigned long IPpreviousMillis = 0;       // will store last time of IP decount updated

byte S88Module = 0xFE;  //Anzahl der abzufragenden S88 Module

#define ResetPin 2      //to Reset the Module at Startup!

byte count = 0;     //Zähler bis Data Flush

/**********************************************************************************/
void EEPROMwrite (String s, uint16_t laenge, uint16_t start) {
  byte len = s.length();
  EEPROM.write(laenge,len);
  for (int i = start; i < (start+len); i++) {
     EEPROM.write(i,s[i-start]);
  }
}

/**********************************************************************************/
String EEPROMread (uint16_t laenge, uint16_t start) {
  String s = "";
  byte len = EEPROM.read(laenge);
  if (len < EEStringMaxSize) {
    for (int i = start; i < (start+len); i++) {
      s += char(EEPROM.read(i));
    }
  }
  return s;
}

/**********************************************************************************/
boolean tryWifiClient() {
  // WiFi.scanNetworks will return the number of networks found
  byte n = 1; //WiFi.scanNetworks();
  if ((n > 0) && (ssid.length() > 0)) {
    //Try to connect to WiFi
     WiFi.begin(ssid.c_str(), pass.c_str());
     n = 0;
     while(WiFi.waitForConnectResult() != WL_CONNECTED){
//        WiFi.begin(ssid.c_str(), pass.c_str());
        delay(100);
        n++;
        if (n > 30) {
            WiFi.disconnect();
            return false;
        }
      }
      return true;
  }
  return false;
}

/**********************************************************************************/
void sendUDP () {
  for (byte s = 0; s < countIP; s++) {
    if (mem[s].time > 0) {
      if (sendTOO == 0x00) {  //Broadcast
        Udp.beginPacket(mem[s].IP, mem[s].port);
      }
      else Udp.beginPacket(mem[sendTOO-1].IP, mem[sendTOO-1].port);  //singel Answer
      Udp.write(outData, outData[0]);   //Daten, Länge
      Udp.endPacket();
      if (sendTOO != 0x00)  //wenn kein Broadcast -> ENDE!
        return;
    }
  }
}

/**********************************************************************************/
// Function finds a standard baudrate of either
// 1200,2400,4800,9600,14400,19200,28800,38400,57600,115200,500000
long detRate(byte recpin) {
  long baud, rate;
  pinMode(recpin, INPUT_PULLUP);      // make sure serial in is a input pin
  do {
     rate = 10000;
     for (int i = 0; i < 10; i++) {
         do {
          baud = pulseIn(recpin,LOW);   // measure the next zero bit width
         } while (baud == 0);
         rate = baud < rate ? baud : rate;
     }
     if (rate < 6)
     baud = 500000;
     else if (rate < 12)
     baud = 115200;
     else if (rate < 20)
     baud = 57600;
     else if (rate < 29)
     baud = 38400;
     else if (rate < 40)
     baud = 28800;
     else if (rate < 60)
     baud = 19200;
     else if (rate < 80)
     baud = 14400;
     else if (rate < 150)
     baud = 9600;
     else if (rate < 300)
     baud = 4800;
     else if (rate < 600)
     baud = 2400;
     else if (rate < 1200)
     baud = 1200;
     else
     baud = 0; 
  } while (baud == 0);
  return baud;
}

/**********************************************************************************/
void receiveEvent() {
  char serialIn = Serial.read();

  if ((lastbyte == '\r') && (serialIn == 0x0A)) {   //"CR+LF" 
    Serial.print("OK");  //get data
    clearReadBuffer(); //reset read buffer
    return;
  }
  
  if (sendTOO == 0xFF)
    sendTOO = serialIn;
  else {    //DataIn:
    outData[outDcount] = serialIn;
    outDcount++;
    //Receive Data is ready?
    if ((outData[0] == outDcount) && (sendTOO <= countIP) && (outData[0] > 3)) {
      sendUDP();  //send Data out!
      //clear Buffer
      clearReadBuffer(); //reset read buffer
      counterr = 0; //Fehler rücksetzen!
      return; 
    }

    //ERROR:
    if ((outDcount >= Z21_UDP_TX_MAX_SIZE) || (outData[0] < outDcount)) {  //keine valied Data!
      Serial.print("EE");  //Fail
      clearReadBuffer(); //reset read buffer
      counterr++; //Fehler zählen
      if (counterr > MaxError) {
        Serial.end();
        serialbaud = detRate(3);  // detect Baud rate again!
        Serial.begin(serialbaud);
        Serial.flush();
        counterr = 0;
      }
      return; 
    } //End Error

    //set S88 Module:
      if (sendTOO == 0xFE && outData[0] == 'S' && outData[1] == '8' && outData[2] == '8' && outDcount > 3) {
         if (outData[3] > 0)
           S88Module = outData[3];
           Serial.print("OK");  //get data
           clearReadBuffer(); //reset read buffer
           return;
       }
       
       //request WiFi config data:
       if (sendTOO == 0xFE && outData[0] == 'g' && outData[1] == 'e' && outData[2] == 't' && outDcount > 3) {
         if (outData[3] == 0x00)
           Serial.print(WiFi.softAPIP());
         if (outData[3] == 0x01)
           Serial.print(ssidAP);
         if (outData[3] == 0x02)
           Serial.print(passAP);
         if (outData[3] == 0x03)     
           Serial.print(kanalAP);
         if (outData[3] == 0x04)
           Serial.print(WiFi.localIP());  
         if (outData[3] == 0x05)
           Serial.print(ssid);
         if (outData[3] == 0x06)
           Serial.print(pass);
         if (outData[3] == 0x07) {
           if (S88Module != 0xFE)
             Serial.print(S88Module);
           else Serial.print("0");
         }
         if (outData[3] == 0x08) {
           Serial.print(VERSION);
         }
         if (outData[3] == 0x09) {
            //Answer all client IP-Hash:
           reportALLIPHash();
         }
         if (outData[3] > 0x09) {   //everything else Answer!
            Serial.print("OK");
         }
         clearReadBuffer(); //reset read buffer 
      } //End request WiFi Data
  } //End DataIn
  lastbyte = serialIn;  //store the last read Byte!
}

void clearReadBuffer () {
  for (byte i = 0; i < Z21_UDP_TX_MAX_SIZE; i++) {
    outData[i] = 0x00;
  }
  outDcount = 0;   //reset read buffer
  sendTOO = 0xFF;
  lastbyte = 0x00;
}

/**********************************************************************************/
//calculate the Hash of the named IP-Adr.
byte getIPHash (IPAddress ip) {
  return ip[0] ^ ip[1] ^ ip[2] ^ ip[3];
}

/**********************************************************************************/
//send all active IP-Hash
void reportALLIPHash (void) {
  for (byte i = 0; i < countIP; i++) {
    Serial.write(getIPHash(mem[i].IP));   //report Hash
  }
  Serial.write(0x00); //Ende
}

/**********************************************************************************/
void reportIPHash (byte client, IPAddress ip) {
  //send client IP-Hash to the master to the Master can find the last config for this client!
  Serial.write(client);  //send and save client Identity
  Serial.write(0x02);    //send Hash Ident
  Serial.write(getIPHash(ip));   //report Hash
}

/**********************************************************************************/
byte addIP (IPAddress ip, unsigned int port) {
  //suche ob IP schon vorhanden?
  for (byte i = 0; i < countIP; i++) {
    if (mem[i].IP == ip && mem[i].port == port) {
      mem[i].time = ActTimeIP; //setzte Zeit
      return i+1;      //Rückgabe der Speicherzelle
    }
  }
  //nicht vorhanden!
  if (countIP >= maxIP) {
    for (byte i = 0; i < countIP; i++) {
      if (mem[i].time == 0) { //Abgelaufende IP, dort eintragen!
        mem[i].IP = ip;
        mem[i].port = port;
        mem[i].time = ActTimeIP; //setzte Zeit
        reportIPHash(i, ip);
        return i+1;
      }
    }
    Serial.print("EE");  //Fail
    return 0;           //Fehler, keine freien Speicherzellen!
  }
  mem[countIP].IP = ip;  //eintragen
  mem[countIP].port = port;
  mem[countIP].time = ActTimeIP; //setzte Zeit
  reportIPHash(countIP, ip);    //inform Master over IP
  countIP++;            //Zähler erhöhen
  return countIP;       //Rückgabe
}

/**********************************************************************************/
void webconfig() {
  WiFiClient client = server.available();
  if (!client)
    return;
    
  String HTTP_req;            // stores the HTTP request 

  if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                HTTP_req += c;  // save the HTTP request 1 char at a time
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: keep-alive");
                    client.println();      //don't forget this!!!
                    // AJAX request for switch state
                    if (HTTP_req.indexOf("/ajax_switch") > -1) {
                        // read switch state and send appropriate paragraph text
                        ssid = HTTP_req.substring(HTTP_req.indexOf("&s=")+3,HTTP_req.indexOf("&p="));
                        pass = HTTP_req.substring(HTTP_req.indexOf("&p=")+3,HTTP_req.indexOf("&As="));
                        ssidAP = HTTP_req.substring(HTTP_req.indexOf("&As=")+4,HTTP_req.indexOf("&Ap="));
                        passAP = HTTP_req.substring(HTTP_req.indexOf("&Ap=")+4,HTTP_req.indexOf("&Ak="));
                        kanalAP = HTTP_req.substring(HTTP_req.indexOf("&Ak=")+4,HTTP_req.indexOf("&S8=")).toInt();
                        if (S88Module != 0xFE) {
                          S88Module = HTTP_req.substring(HTTP_req.indexOf("&S8=")+4,HTTP_req.indexOf("&nocache")).toInt();
                          Serial.write(0xFE);
                          Serial.print("S88");
                          Serial.write(S88Module);    //sende S88 Einstellungen an Zentrale!
                        }
                        
                        if ((kanalAP < 1) || (kanalAP > 13)) {
                          kanalAP = SkanalAP;
                          client.print("Ka. error! ");
                        }
                        if (passAP.length() < 8) {
                          passAP = SpassAP;
                          client.print("Code length error (min. 8)! ");
                        }
                        
                        // write eeprom
                        EEPROMwrite (ssid, EEssidLength, EEssidBegin);
                        EEPROMwrite (pass, EEpassLength, EEpassBegin);
                        
                        EEPROMwrite (ssidAP, EEssidAPLength, EEssidAPBegin);
                        EEPROMwrite (passAP, EEpassAPLength, EEpassAPBegin);
                        EEPROM.write(EEkanalAP, kanalAP);
                        EEPROM.commit(); 

                        WiFi.disconnect();
                        tryWifiClient();
                        
                        Udp.begin(localPort);

                        client.println("saved");   //OK!
                    }
                    else {  // HTTP request for web page
                        // send web page - contains JavaScript with AJAX calls
                        client.println("<!DOCTYPE html>");
                        client.println("<html><head><title>Z21</title>");
                        client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>");
                        client.println("<script>");
                        client.println("function SetState() {");
                        client.println("document.getElementById(\"state\").innerHTML = \"wait\";");
                        client.println("nocache = \"&s=\" + document.getElementById(\"ssid\").value;");
                        client.println("nocache += \"&p=\" + document.getElementById(\"pass\").value;");
                        client.println("nocache += \"&As=\" + document.getElementById(\"ssidAP\").value;");
                        client.println("nocache += \"&Ap=\" + document.getElementById(\"passAP\").value;");
                        client.println("nocache += \"&Ak=\" + document.getElementById(\"kanalAP\").value;");
                        client.println("nocache += \"&S8=\" + document.getElementById(\"S88\").value;");
                        client.println("nocache += \"&nocache=\" + Math.random() * 1000000;");
                        client.println("var request = new XMLHttpRequest();");
                        client.println("request.onreadystatechange = function() {");
                        client.println("if (this.readyState == 4){");
//                        client.println("if (this.status == 200){");
//                        client.println("if (this.responseText != null) {");
                        client.println("document.getElementById(\"state\").innerHTML = this.responseText;");
                        client.println("top.window.location.reload(true);");
                        client.println("}}");
                        client.println("request.open(\"GET\", \"ajax_switch\" + nocache, true);");
                        client.println("request.send(null);");
                        //client.println("setTimeout('SetState()', 1000);");
                        client.println("}");
                        client.println("</script>");
                        client.println("</head>");
                        client.println("<body><h1>Z21 Net-config</h1><hr>");
                        client.print("<h2>WiFi Direct AP</h2>");
                        client.print("<dl><dd>IP: ");
                        client.print(WiFi.softAPIP());
                        client.print("</dd><dd>Connected Clients: ");
                        client.print(WiFi.softAPgetStationNum());
                        client.print(" of 4</dd><dd>SSID: <input type=\"text\" id=\"ssidAP\" value=\"");
                        client.print(ssidAP);
                        client.print("\"></dd><dd>code: <input type=\"text\" id=\"passAP\" value=\"");
                        client.print(passAP);
                        client.print("\"></dd><dd>Ka.: <input type=\"number\" min=\"1\" max=\"13\" id=\"kanalAP\" value=\"");
                        client.print(kanalAP);
                        client.println("\"></dd></dl>");
                        
                        client.print("<h2>WiFi client</h2>");
                        client.print("<dl><dd>IP: ");
                        if (WiFi.status() == WL_CONNECTED)
                          client.print(WiFi.localIP());
                        else client.print("none");
                        client.print("</dd><dd>SSID: <input type=text id=\"ssid\" value=\"");  
                        client.print(ssid.c_str());
                        client.print("\"></dd><dd>code: <input type=text id=\"pass\" value=\"");
                        client.print(pass.c_str());
                        client.println("\"></dd></dl>");

                        client.println("<h2>S88 Module</h2>");
                        client.print("<dl><dd>8x Anzahl: <input type=number min=\"0\" max=\"62\" id=\"S88\" value=\"");
                        if (S88Module != 0xFE) {                          
                          client.print(S88Module);
                          client.print("\"");
                        }
                        else client.print("0\" disabled");
                        client.println("></dd></dl><br>");
                        
                        client.println("<input type=submit onclick=\"SetState()\">"); 
                        client.println("<p id=\"state\"></p>");
                        client.print("<hr><p>Z21_ESPArduinoUDP_v");
                        client.print(VERSION);
                        client.print("<br>Run at Baudrate: ");
                        client.print(serialbaud);
                        client.println("<br>Copyright (c) 2021 Philipp Gahtow<br>digitalmoba@arcor.de</p>");
                        client.println("</body>");
                        client.print("</html>");
                    }
                    // display received HTTP request on serial port
                    //Serial.print(HTTP_req);
                    HTTP_req = "";            // finished with request, empty string
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}

/**********************************************************************************/
void setup()
{
  EEPROM.begin(EESize);  //init EEPROM

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(ResetPin, INPUT);  //init Reset Pin

  // read eeprom for ssid and pass
  //--------------WIFI CLIENT---------------
  ssid = EEPROMread(EEssidLength, EEssidBegin);
  pass = EEPROMread(EEpassLength, EEpassBegin);
  //--------------ACCESS POINT------------- 
  ssidAP = EEPROMread(EEssidAPLength, EEssidAPBegin);
  passAP = EEPROMread(EEpassAPLength, EEpassAPBegin);
  if ((ssidAP.length() > 32) || (ssidAP.length() == 0) || (passAP.length() > 32) || (passAP.length() == 0) || (digitalRead(ResetPin) == LOW)) { //request is OK?
    ssidAP = SssidAP;
    passAP = SpassAP;
    kanalAP = SkanalAP;
          EEPROMwrite (ssidAP, EEssidAPLength, EEssidAPBegin);
          EEPROMwrite (passAP, EEpassAPLength, EEpassAPBegin);
          EEPROM.write(EEkanalAP, kanalAP);
          if (!EEPROM.commit())
            Serial.println("overwrite fail");
  }
  else {    
      kanalAP = EEPROM.read(EEkanalAP);
      if ((kanalAP < 1) || (kanalAP > 13))
        kanalAP = SkanalAP;
  }

  WiFi.mode(WIFI_AP_STA);  //AP & client
  
  #if defined(EnableZ21IP) 
  WiFi.softAPConfig(Ip, Gw, Sb);  //set the IP for Z21
  #endif
  WiFi.softAP(ssidAP.c_str(), passAP.c_str(), kanalAP, false);    //Start AcessPoint
  //don't hide SSID and max simultaneous connected stations set to eight!

  //---wait for serial to get ready!---
  Serial.begin(serialbaud);  //UDP to Serial Kommunikation
  Serial.flush();   //make serial empty and start!

  tryWifiClient();  //Try to connect to WiFi

  #if defined(EnableOTA)
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Z21-ESP-UDP");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    //Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
  /*Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    } */
  });
  ArduinoOTA.begin();
  #endif
 
  //Start the HTTP server
  server.begin();
  
  Udp.begin(localPort);   //open Z21 port

  #if defined(DEBUG)
  Serial.print("AP: ");
  Serial.println(ssidAP);
  Serial.print("PW: ");
  Serial.println(passAP);
  Serial.print("Cannel: ");
  Serial.println(kanalAP);
  
  #endif
}

/**********************************************************************************/
void loop() {
  //If there's UDP data from WLAN available, read a packet:
  byte packetSize = Udp.parsePacket();
  if (packetSize) {
    
    digitalWrite(LED_BUILTIN, HIGH);   // Arduino: turn the LED on (HIGH)
    
    IPAddress remoteIp = Udp.remoteIP();
    Serial.write(addIP(remoteIp, Udp.remotePort()));  //send and save client Identity
    
    // read the packet into packetBufffer
    byte packetBuffer[packetSize]; //buffer to hold incoming packet
    byte len = Udp.read(packetBuffer, packetSize);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    Serial.write(packetBuffer, packetSize); //send to Zentrale (trenne erst dort kombinierte Pakete!)

    digitalWrite(LED_BUILTIN, LOW);    // Arduino: turn the LED off (LOW)
  }

  //receive data
  if (Serial.available() > 0) {  //Empfang UDP Daten über Serial
    digitalWrite(LED_BUILTIN, HIGH);   // Arduino: turn the LED on (HIGH)
    receiveEvent();
    digitalWrite(LED_BUILTIN, LOW);    // Arduino: turn the LED off (LOW)
  }
      
  //Nutzungszeit IP's bestimmen
  unsigned long currentMillis = millis();
  if(currentMillis - IPpreviousMillis > interval) {
    IPpreviousMillis = currentMillis;   
    for (byte i = 0; i < countIP; i++) {
        if (mem[i].time > 0) 
          mem[i].time--;    //Zeit herrunterrechnen
    }
  }  
  
  webconfig();  //create Website

  //delay(1);   //to let the wifi work

  #if defined(EnableOTA)
  ArduinoOTA.handle();
  #endif
} 
