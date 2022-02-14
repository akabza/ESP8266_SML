
/*
   (c) Alexander Kabza, 2022-01-16
   Code for ESP8266 ESP01 to read serial SmartMeterLanguage from Smart Meters via D0 optical interface
     
*/

#define OTA

#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>
#include <SoftwareSerial.h>

#define MYPORT_TX 5  // D1
#define MYPORT_RX 4  // D2

SoftwareSerial myPort;

#ifdef OTA
#include <ArduinoOTA.h>
#endif

const String DEVICENAME = "ESP8266_SML_D0front";
const String CODEVERSION = "published version from 2022-01-16";

String macStr;
uint32_t prevOneSecMillis = 0;
uint32_t wifiConnectCounter = 0;
uint32_t prevWIFIMillis = 0;                // WIFI check trigger
uint32_t prevNTPMillis = 0;                 // NTP update trigger
uint32_t prevPostData = 0;
uint32_t prev_smlMillis = 0;
uint32_t startEpochTime = 0;
uint32_t WIFIConnectCounter = 0;
String formattedStartDateTime;
float runTimeHours = 0.0;

const String smlBegin         = "1b1b1b1b01010101";
const String smlEnd           = "1b1b1b1b1a";
const String searchStr_E      = "77070100010800ff";
const String searchStr_E_feed = "77070100020800ff";

const String search1 = "620062007263070177010b09014553591103b0a6cb080100620affff007262016501cc"; // unbekannt, danach 2 Byte! Das ist ein Zähler!!! i++ bei jeder SML-Botschaft

String smlTemp = ""; 
String smlMsg = "";

float energy_in = 0.0;
float energy_feed = 0.0;

int64_t unknown1 = 0;


//------------------------------------------
//WIFI
const char* ssid = "SSID";
const char* password = "TopSecret";

IPAddress ip(192, 168, X, Y); // Static IP
IPAddress gateway(192, 168, X, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, X, 1);

//------------------------------------------
//HTTP
ESP8266WebServer server(80);

// ****************************************************************

String toHEX(unsigned char* bytearray, uint8_t arraysize) {
  String str = "";
  for (uint8_t i = 0; i < arraysize; i++) {
    if (i > 0) str += ":";
    if (bytearray[i] < 16) str += String(0, HEX);
    str += String(bytearray[i], HEX);
  }
  return str;
}  // end toHEX

// ****************************************************************
// same like toHEX, but without ":" in between

String toHEX2(unsigned char* bytearray, uint16_t arraysize) {
  String str = "";
  for (uint16_t i = 0; i < arraysize; i++) {
    if (bytearray[i] < 16) str += String(0, HEX);
    str += String(bytearray[i], HEX);
  }
  return str;
}  // end toHEX

// ****************************************************************

String bytetoHEX(byte onebyte) {
  String str = "";
  if (onebyte < 16) str += String(0, HEX);
  str += String(onebyte, HEX);
  return str;
}  // end bytetoHEX

// ****************************************************************

//https://stackoverflow.com/questions/58768018/what-is-the-difference-between-crc-16-ccitt-false-and-crc-16-x-25
uint16_t crc16x25(String hexStr) {
    uint16_t wCrc = 0xffff;
    char* pData;
    
    char byteArray[hexStr.length() / 2];
    
    uint16_t j = 0;
    for (uint16_t i = 0; i < hexStr.length(); i+=2) {
      String hexByte = String(hexStr[i]) + String(hexStr[i+1]);
      //Serial.print(hexByte);
      uint8_t b = strtol(hexByte.c_str(), NULL, 16);
      byteArray[j] = (char)b;
      j++;
    }
    pData = byteArray;
    uint16_t i = sizeof(byteArray);
    while (i--) {
        wCrc ^= *(unsigned char *)pData++ << 0;
        for (j=0; j < 8; j++)
            wCrc = wCrc & 0x0001 ? (wCrc >> 1) ^ 0x8408 : wCrc >> 1;
    }
    return wCrc ^ 0xffff;
}   // end crc16x25

// ****************************************************************

void handleRoot() {

  String msg = "";

  msg += "<MyHome>\n";
  msg += "<data name=\"Device\" value=\"" + DEVICENAME + "\" valueunit=\"text\"/>\n";
  msg += "<data name=\"MAC\" value=\"" + macStr + "\" valueunit=\"AA:BB:CC:DD:EE:FF\"/>\n";
  msg += "<data name=\"Version\" value=\"" + CODEVERSION + "\" valueunit=\"text\"/>\n";
  msg += "<data name=\"SSID\" value=\"" + WiFi.SSID() + "\" valueunit=\"text\"/>\n";
  msg += "<data name=\"IP\" value=\"" + WiFi.localIP().toString() + "\" valueunit=\"xxx.xxx.xxx.xxx\"/>\n";
  msg += "<data name=\"wifiConnectCounter\" value=\"" + String(wifiConnectCounter) + "\" valueunit=\"\"/>\n";
  msg += "<data name=\"help\" value=\"/SMLMsg to show complete SML Message additionally\" valueunit=\"text\"/>\n";
  msg += "<data name=\"Energy_in\" value=\"" + String(energy_in, 1) + "\" valueunit=\"Wh\"/>\n";
  msg += "<data name=\"Energy_feed\" value=\"" + String(energy_feed, 1) + "\" valueunit=\"Wh\"/>\n";
  
  msg += "</MyHome>";

  server.send(200, "text/plain", msg);       //Response to the HTTP request
}  // end handleRoot()

// ****************************************************************

void handleSMLMsg() {

  String msg = "";
  
  msg += "<MyHome>\n";
  msg += "<data name=\"Device\" value=\"" + DEVICENAME + "\" valueunit=\"text\"/>\n";
  msg += "<data name=\"MAC\" value=\"" + macStr + "\" valueunit=\"AA:BB:CC:DD:EE:FF\"/>\n";
  msg += "<data name=\"Version\" value=\"" + CODEVERSION + "\" valueunit=\"text\"/>\n";
  msg += "<data name=\"SSID\" value=\"" + WiFi.SSID() + "\" valueunit=\"text\"/>\n";
  msg += "<data name=\"IP\" value=\"" + WiFi.localIP().toString() + "\" valueunit=\"xxx.xxx.xxx.xxx\"/>\n";
  msg += "<data name=\"wifiConnectCounter\" value=\"" + String(wifiConnectCounter) + "\" valueunit=\"\"/>\n";
  msg += "<data name=\"Energy_in\" value=\"" + String(energy_in, 1) + "\" valueunit=\"Wh\"/>\n";
  msg += "<data name=\"Energy_feed\" value=\"" + String(energy_feed, 1) + "\" valueunit=\"Wh\"/>\n";
  msg += "<data name=\"smlMsg\" value=\"" + smlMsg + "\" valueunit=\"text\"/>\n";

  uint16_t i = smlMsg.length();
  smlMsg = smlMsg.substring(0, i-4);   // cut last 4 chars (2 last bytes for CRC15X25)
  i = crc16x25(smlMsg);
  msg += "<data name=\"CRC\" value=\"" + String(i, HEX) + "\" valueunit=\"text\"/>\n";

  msg += "<data name=\"unknown1\" value=\"" + String(unknown1) + "\" valueunit=\"text\"/>\n";
  
  msg += "</MyHome>";

  server.send(200, "text/plain", msg);       //Response to the HTTP request
}  // end handleSMLMsg()

// ****************************************************************

void handleNotFound() {
  String msg = "File Not Found\n\n";
  msg += "URI: ";
  msg += server.uri();
  msg += "\nMethod: ";
  msg += (server.method() == HTTP_GET) ? "GET" : "POST";
  msg += "\nArguments: ";
  msg += server.args();
  msg += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    msg += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html", msg);
}  // end handleNotFound

// ****************************************************************

void wificonnect() {
  byte mac[6];
  long counter = 0;
  long StartMillis = millis();
  WiFi.setAutoConnect (true);
  WiFi.setAutoReconnect (true);

  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet, dns);
  WiFi.begin(ssid, password); //Connect to the WiFi network

  Serial.println();
  WiFi.macAddress(mac);
  macStr = toHEX(mac, sizeof(mac));
  Serial.print("MAC = " + macStr);

  //Wait for WIFI connection
  while ((WiFi.status() != WL_CONNECTED) && (counter < 1000)) { //Wait for connection
    delay(100);
    counter++;
  }
  delay(1000);
  if (WiFi.status() != WL_CONNECTED) {
    ESP.reset();
  }

  Serial.println("");
  Serial.print("WIFI connected to: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  wifiConnectCounter++;
}  // end wificonnect

//***********************************************************************

#ifdef OTA
void setupOTA() {
  ArduinoOTA.setHostname(DEVICENAME.c_str());
  ArduinoOTA.setPasswordHash("b0äöüe7f8be");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
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
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready");

}
#endif

//***********************************************************************

void setup() {
  Serial.begin(115200);  
  Serial.println("");
  Serial.println("Setup");

  myPort.begin(9600, SWSERIAL_8N1, MYPORT_RX, MYPORT_TX, false);
  if (myPort) { // If the object did not initialize, then its configuration is invalid
    Serial.println("SoftwareSerial port initialized.");
  } else {
    Serial.println("Invalid SoftwareSerial pin configuration, check config!"); 
  }
  
  wificonnect();

  server.on("/", handleRoot);
  server.on("/SMLMsg", handleSMLMsg);
  server.onNotFound( handleNotFound );

  server.begin();                                       //Start the server
  Serial.println("Server is now listening ...");
  Serial.println();

  #ifdef OTA
  setupOTA();
  #endif
  
}  // end setup

// ****************************************************************

void parse_smlMsg() {
  int64_t value = 0;
  String hexStr = "";
  
  // search energy in (Wh)
  String searchStr = searchStr_E;
  uint16_t pos = smlMsg.indexOf(searchStr);
  if (pos > 0) {
    pos = pos + searchStr.length() + 20;   // skip additional 10 Bytes = 20 Char!
    hexStr = smlMsg.substring(pos,  pos + 16);   // hexStr is 8 Byte = 16 Char
    value = strtoull(hexStr.c_str(), NULL, 16);
    energy_in = (float)value;
  } else {
    energy_in = 0.0;
  }

  // search energy feed (Wh)
  searchStr = searchStr_E_feed;
  pos = smlMsg.indexOf(searchStr);
  if (pos > 0) {
    pos = pos + searchStr.length() + 20;
    hexStr = smlMsg.substring(pos,  pos + 16);
    value = strtoull(hexStr.c_str(), NULL, 16);
    energy_feed = (float)value;
  } else {
    energy_feed = 0.0;
  }

  // Unbekannt 2 Byte
  searchStr = search1;
  pos = smlMsg.indexOf(searchStr);
  if (pos > 0) {
    pos = pos + searchStr.length();
    hexStr = smlMsg.substring(pos,  pos + 4);
    value = strtoul(hexStr.c_str(), NULL, 16);
    unknown1 = value;
  } else {
    unknown1 = 0;
  }

}

// ****************************************************************

void loop() {
  uint32_t currentMillis = millis();
  uint8_t inByte;
    
  server.handleClient();    // Handling of incoming requests
  #ifdef OTA
  ArduinoOTA.handle();
  #endif
 
  while (myPort.available()) {
    inByte = myPort.read(); // read serial buffer into array
    smlTemp += bytetoHEX(inByte);
    //Serial.print(bytetoHEX(inByte));
    int i = smlTemp.indexOf(smlBegin);
    if (i > 0) {
      //Serial.println(smlBegin + " gefunden!");
      smlTemp = smlTemp.substring(i, i + smlBegin.length());   // start to record a new temporary SML message, starting with smlBegin
    }
    i = smlTemp.indexOf(smlEnd);
    if (i > 0) {                     // end of temporary SML message reached and complete now
      i = 0;
      while (myPort.available() && (i <= 5)) {          // read some more byte for CRC
        inByte = myPort.read(); // read serial buffer into array
        smlTemp += bytetoHEX(inByte);
        i++;
      }
      smlMsg = smlTemp;
      //Serial.println(smlMsg);
      smlTemp = "";                  // start with empty temporary SML message
      Serial.println(currentMillis - prev_smlMillis);
      prev_smlMillis = currentMillis;
      parse_smlMsg();
      Serial.println("Energy_in = " + String(energy_in, 1) + " Wh");
      Serial.println("Energy_feed = " + String(energy_feed, 1) + " Wh");
      Serial.println("");
    }
  }

  if (currentMillis - prevOneSecMillis >= 1000) {
    //Serial.println(currentpTotal);
    //Serial.println(String(currentconsumptionkWh, 3));
    prevOneSecMillis = currentMillis;
  }
  
  if (currentMillis - prevWIFIMillis >= 3600000) {    // check wifi every hour
    if (WiFi.status() != WL_CONNECTED) {
      wificonnect();
      delay(200);
    }
    prevWIFIMillis = currentMillis;
  }

  delay(10);
}  // end loop
