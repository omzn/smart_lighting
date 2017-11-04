/*
   Aqua-tan smart light manager

   API

   /on
      power=(int)
   /off
   /status
   /config
      enable=[true|false]
      on_h=(int)
      on_m=(int)
      off_h=(int)
      off_m=(int)
      dim=(int)
   /wifireset
   /reset
   /reboot
*/

#include "esp_lighting.h" // Configuration parameters

#include <pgmspace.h>
#include <Wire.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include <FS.h>
#include "ntp.h"
#include "ledlight.h"
#include "SF_s7s_hw.h"

const String boolstr[2] = {"false","true"};

const String website_name  = "esplight";
const char* apSSID         = "WIFI_LIGHT_TAN";
String sitename;
boolean settingMode;
String ssidList;

uint32_t timer_count = 0;

uint32_t p_millis;
int prev_m = 0;
int prev_s = 0;

DNSServer dnsServer;
MDNSResponder mdns;
const IPAddress apIP(192, 168, 1, 1);
ESP8266WebServer webServer(80);

RTC_Millis rtc;

NTP ntp("ntp.nict.jp");
ledLight light(PIN_LIGHT);
S7S s7s;

/* Setup and loop */

void setup() {
  pinMode(PIN_LIGHT, OUTPUT);
  digitalWrite(PIN_LIGHT, LOW);

  analogWriteRange(MAX_PWM_VALUE);
  analogWriteFreq(200);

  p_millis = millis();
#ifdef DEBUG
  Serial.begin(115200);
#else
  Serial.begin(9600);
#endif
  EEPROM.begin(512);
  delay(10);

//  s7s.begin(9600);
  s7s.clearDisplay();
  s7s.setBrightness(255);
  s7s.print("-HI-");
  
  SPIFFS.begin();
  rtc.begin(DateTime(2017, 1, 1, 0, 0, 0));
#ifdef DEBUG
  Serial.println("RTC began");
#endif
  sitename = website_name;

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  if (restoreConfig()) {
    if (checkConnection()) {
      if (mdns.begin(sitename.c_str(), WiFi.localIP())) {
#ifdef DEBUG
        Serial.println("MDNS responder started.");
#endif
      }
      settingMode = false;
    } else {
      settingMode = true;
    }
  } else {
    settingMode = true;
  }
  if (settingMode == true) {
    s7s.print("SET ");
    delay(500);
#ifdef DEBUG
    Serial.println("Setting mode");
#endif
    //WiFi.mode(WIFI_STA);
    //WiFi.disconnect();
#ifdef DEBUG
    Serial.println("Wifi disconnected");
#endif
    delay(100);
    WiFi.mode(WIFI_AP);
#ifdef DEBUG
    Serial.println("Wifi AP mode");
#endif
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apSSID);
    dnsServer.start(53, "*", apIP);
#ifdef DEBUG
    Serial.println("Wifi AP configured");
#endif
    startWebServer_setting();
#ifdef DEBUG
    Serial.print("Starting Access Point at \"");
    Serial.print(apSSID);
    Serial.println("\"");
#endif
  } else {
    s7s.print("noro");
    ntp.begin();
    delay(500);
    startWebServer_normal();
#ifdef DEBUG
    Serial.println("Starting normal operation.");
#endif
  }
  //ESP.wdtEnable(100);
}

void loop() {
  
  ESP.wdtFeed();
  if (settingMode) {
    dnsServer.processNextRequest();
  }
  webServer.handleClient();

  // 1秒毎に実行
  if (millis() > p_millis + 1000) {
    p_millis = millis();
    if (timer_count % 3600 == 0) {
      uint32_t epoch = ntp.getTime();
      //uint32_t epoch = getNTPtime();
      if (epoch > 0) {
        rtc.adjust(DateTime(epoch + SECONDS_UTC_TO_JST ));
      }
#ifdef DEBUG
      Serial.print("epoch:");
      Serial.println(epoch);
#endif
    }
    if (!settingMode) {
      DateTime now = rtc.now();
      int st = light.control(now.hour(), now.minute());
      if (st == LIGHT_ON) {
        if (prev_m != now.minute()) {
          s7s.clearDisplay();
          s7s.setBrightness(255);
          s7s.printTime(timestamp_s7s());
        }
      } else if (st == LIGHT_OFF) {
        if (prev_m != now.minute()) {
          s7s.clearDisplay();
          s7s.setBrightness(127);
          s7s.printTime(timestamp_s7s());
        }
      } else {
//        if (prev_s != st) {
          s7s.clearDisplay();
          s7s.setBrightness(255);
          s7s.print4d(light.power());
//          s7s.print("dirn");
//        }
      }
      prev_m = now.minute();

    }
    timer_count++;
    timer_count %= (86400UL);
  }
  delay(10);
}

/***************************************************************
   EEPROM restoring functions
 ***************************************************************/

boolean restoreConfig() {
#ifdef DEBUG
  Serial.println("Reading EEPROM...");
#endif
  String ssid = "";
  String pass = "";

  // Initialize on first boot
  if (EEPROM.read(0) == 255) {
#ifdef DEBUG
    Serial.println("Initialize EEPROM...");
#endif
    for (int i = 0; i < EEPROM_LAST_ADDR; ++i) {
      EEPROM.write(i, 0);
    }
#ifdef DEBUG
    Serial.println("Erasing EEPROM...");
#endif
    EEPROM.commit();
  }

#ifdef DEBUG
  Serial.println("Reading EEPROM(2)...");
#endif

  if (EEPROM.read(EEPROM_SSID_ADDR) != 0) {
    for (int i = EEPROM_SSID_ADDR; i < EEPROM_SSID_ADDR + 32; ++i) {
      ssid += char(EEPROM.read(i));
    }
    for (int i = EEPROM_PASS_ADDR; i < EEPROM_PASS_ADDR + 64; ++i) {
      pass += char(EEPROM.read(i));
    }
#ifdef DEBUG
    Serial.print("ssid:");
    Serial.println(ssid);
    Serial.print("pass:");
    Serial.println(pass);
#endif
    delay(100);
    WiFi.begin(ssid.c_str(), pass.c_str());
#ifdef DEBUG
    Serial.println("WiFi started");
#endif
    delay(100);
    if (EEPROM.read(EEPROM_MDNS_ADDR) != 0) {
      sitename = "";
      for (int i = 0; i < 32; ++i) {
        byte c = EEPROM.read(EEPROM_MDNS_ADDR + i);
        if (c == 0) {
          break;
        }
        sitename += char(c);
      }
    }

    int e_schedule  = EEPROM.read(EEPROM_SCHEDULE_ADDR) == 1 ? 1 : 0;
    light.enable(e_schedule == 0 ? 0 : 1);

    int e_on_h  = EEPROM.read(EEPROM_SCHEDULE_ADDR + 1);
    int e_on_m  = EEPROM.read(EEPROM_SCHEDULE_ADDR + 2);
    int e_off_h = EEPROM.read(EEPROM_SCHEDULE_ADDR + 3);
    int e_off_m = EEPROM.read(EEPROM_SCHEDULE_ADDR + 4);
    light.schedule(e_on_h, e_on_m, e_off_h, e_off_m);

    int e_dim = EEPROM.read(EEPROM_DIM_ADDR) | EEPROM.read(EEPROM_DIM_ADDR + 1) << 8;
    light.dim(e_dim);
#ifdef DEBUG
    Serial.print("schedule:");
    Serial.println(light.enable());
    Serial.print("dim:");
    Serial.println(light.dim());
    Serial.print("schedule: ");
    Serial.print(light.on_h());
    Serial.print(":");
    Serial.print(light.on_m());
    Serial.print("-");
    Serial.print(light.off_h());
    Serial.print(":");
    Serial.println(light.off_m());
#endif

    return true;
  } else {
#ifdef DEBUG
    Serial.println("restore config fails...");
#endif
    return false;
  }
}

/***************************************************************
   Network functions
 ***************************************************************/

boolean checkConnection() {
  int count = 0;
  while ( count < 60 ) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
    count++;
  }
  //  Serial.println("Timed out.");
  return false;
}

/***********************************************************
   WiFi Client functions

 ***********************************************************/

/***************************************************************
   Web server functions
 ***************************************************************/

void startWebServer_setting() {
#ifdef DEBUG
  Serial.print("Starting Web Server at ");
  Serial.println(WiFi.softAPIP());
#endif
  webServer.on("/pure.css", handleCss);
  webServer.on("/setap", []() {
#ifdef DEBUG
    Serial.print("Set AP ");
    Serial.println(WiFi.softAPIP());
#endif
    for (int i = 0; i < EEPROM_MDNS_ADDR; ++i) {
      EEPROM.write(i, 0);
    }
    String ssid = urlDecode(webServer.arg("ssid"));
    String pass = urlDecode(webServer.arg("pass"));
    String site = urlDecode(webServer.arg("site"));
    for (int i = 0; i < ssid.length(); ++i) {
      EEPROM.write(EEPROM_SSID_ADDR + i, ssid[i]);
    }
    for (int i = 0; i < pass.length(); ++i) {
      EEPROM.write(EEPROM_PASS_ADDR + i, pass[i]);
    }
    if (site != "") {
      for (int i = EEPROM_MDNS_ADDR; i < EEPROM_MDNS_ADDR + 32; ++i) {
        EEPROM.write(i, 0);
      }
      for (int i = 0; i < site.length(); ++i) {
        EEPROM.write(EEPROM_MDNS_ADDR + i, site[i]);
      }
    }
    EEPROM.commit();
    String s = "<h2>Setup complete</h2><p>Device will be connected to \"";
    s += ssid;
    s += "\" after the restart.</p><p>Your computer also need to re-connect to \"";
    s += ssid;
    s += "\".</p><p><button class=\"pure-button\" onclick=\"return quitBox();\">Close</button></p>";
    s += "<script>function quitBox() { open(location, '_self').close();return false;};setTimeout(\"quitBox()\",10000);</script>";
    webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    timer_count = 0;
  });
  webServer.onNotFound([]() {
#ifdef DEBUG
    Serial.println("captive webpage ");
#endif
    int n = WiFi.scanNetworks();
    delay(100);
    ssidList = "";
    for (int i = 0; i < n; ++i) {
      ssidList += "<option value=\"";
      ssidList += WiFi.SSID(i);
      ssidList += "\">";
      ssidList += WiFi.SSID(i);
      ssidList += "</option>";
    }
    String s = R"=====(
<div class="l-content">
<div class="l-box">
<h3 class="if-head">WiFi Setting</h3>
<p>Please enter your password by selecting the SSID.<br />
You can specify site name for accessing a name like http://aquamonitor.local/</p>
<form class="pure-form pure-form-stacked" method="get" action="setap" name="tm"><label for="ssid">SSID: </label>
<select id="ssid" name="ssid">
)=====";
    s += ssidList;
    s += R"=====(
</select>
<label for="pass">Password: </label><input id="pass" name="pass" length=64 type="password">
<label for="site" >Site name: </label><input id="site" name="site" length=32 type="text" placeholder="Site name">
<button class="pure-button pure-button-primary" type="submit">Submit</button></form>
</div>
</div>
)=====";
  webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
  });
  webServer.begin();
}

/*
 * Web server for normal operation
 */
void startWebServer_normal() {
#ifdef DEBUG
  Serial.print("Starting Web Server at ");
  Serial.println(WiFi.localIP());
#endif
  webServer.on("/reset", []() {
    for (int i = 0; i < EEPROM_LAST_ADDR; ++i) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    String s = "<h3 class=\"if-head\">Reset ALL</h3><p>Cleared all settings. Please reset device.</p>";
    s += "<p><button class=\"pure-button\" onclick=\"return quitBox();\">Close</button></p>";
    s += "<script>function quitBox() { open(location, '_self').close();return false;};</script>";
    webServer.send(200, "text/html", makePage("Reset ALL Settings", s));
    timer_count = 0;
  });
  webServer.on("/wifireset", []() {
    for (int i = 0; i < EEPROM_MDNS_ADDR; ++i) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    String s = "<h3 class=\"if-head\">Reset WiFi</h3><p>Cleared WiFi settings. Please reset device.</p>";
    s += "<p><button class=\"pure-button\" onclick=\"return quitBox();\">Close</button></p>";
    s += "<script>function quitBox() { open(location, '_self').close();return false;}</script>";
    webServer.send(200, "text/html", makePage("Reset WiFi Settings", s));
    timer_count = 0;
  });
  webServer.on("/", handleRoot);
  webServer.on("/pure.css", handleCss);
  webServer.on("/reboot", handleReboot);
  webServer.on("/on", handleActionOn);
  webServer.on("/off", handleActionOff);
//  webServer.on("/power", handleActionPower);
  webServer.on("/status", handleStatus);
  webServer.on("/config", handleConfig);
  webServer.begin();
}

void handleRoot() {
  send_fs("/index.html","text/html");  
}

void handleCss() {
  send_fs("/pure.css","text/css");  
}

void handleReboot() {
  String message;
  message = "{reboot:\"done\"}";
  webServer.send(200, "application/json", message);
  ESP.restart();
}

void handleActionOn() {
  String message, argname, argv;
  int p = -1,err;

  // on
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  for (int i = 0; i < webServer.args(); i++) {
    argname = webServer.argName(i);
    argv = webServer.arg(i);
    if (argname == "power") {
      p = argv.toInt();
    }
  }
  if (p >= 0) {
    err = light.control(p);
  } else {
    err = light.control(MAX_PWM_VALUE);    
  }
  if (err < 0) {
    json["error"] = "Cannot turn on/off light while schedule is set.";
  }
  json["power"] = light.power();
  json["status"] = light.status();
  json["timestamp"] = timestamp();  
  json.printTo(message);
  webServer.send(200, "application/json", message);
}

void handleActionPower() {
  String message;

  // on
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  int p = webServer.arg("value").toInt();

  int err = light.control(p);
  if (err < 0) {
    json["error"] = "Cannot turn on/off light while schedule is set.";
  }
  json["power"] = light.power();
  json["status"] = light.status();
  json["timestamp"] = timestamp();  
  json.printTo(message);
  webServer.send(200, "application/json", message);
}

void handleActionOff() {
  String message;
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  // off
  int err = light.control(0);
  if (err < 0) {
    json["error"] = "Cannot turn on/off light while schedule is set.";
  }
  json["power"] = light.power();
  json["status"] = light.status();
  json["timestamp"] = timestamp();  
  json.printTo(message);
  webServer.send(200, "application/json", message);
}

void handleConfig() {
  String message,argname,argv;
  int en,on_h,on_m,off_h,off_m,dim;
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  on_h = on_m = off_h = off_m = dim = -1;

  for (int i = 0; i < webServer.args(); i++) {
    argname = webServer.argName(i);
    argv = webServer.arg(i);
#ifdef DEBUG
    Serial.print("argname:");
    Serial.print(argname);
    Serial.print(" = ");
    Serial.println(argv);
#endif
    if (argname == "enable") {
       en = (argv ==  "true" ? 1 : 0);
       light.enable(en);
       EEPROM.write(EEPROM_SCHEDULE_ADDR, char(light.enable()));
       EEPROM.commit();
   } else if (argname == "on_h") {
       on_h  = argv.toInt();
    } else if (argname == "on_m") {
       on_m  = argv.toInt();
    } else if (argname == "off_h") {
       off_h  = argv.toInt();
    } else if (argname == "off_m") {
       off_m  = argv.toInt();
    } else if (argname == "dim") {
       dim  = argv.toInt();
       light.dim(dim);
       EEPROM.write(EEPROM_DIM_ADDR,   char(light.dim() & 0xFF));
       EEPROM.write(EEPROM_DIM_ADDR+1,   char(light.dim() >> 8));
       EEPROM.commit();
    }
  }

  if (!(on_h < 0 || on_m < 0 || off_h < 0 || off_m < 0)) {
    light.schedule(on_h,on_m,off_h,off_m);
    EEPROM.write(EEPROM_SCHEDULE_ADDR + 1, char(light.on_h()));
    EEPROM.write(EEPROM_SCHEDULE_ADDR + 2, char(light.on_m()));
    EEPROM.write(EEPROM_SCHEDULE_ADDR + 3, char(light.off_h()));
    EEPROM.write(EEPROM_SCHEDULE_ADDR + 4, char(light.off_m()));
    EEPROM.commit();
  }
  
  json["enable"] = boolstr[light.enable()];
  json["dim"] = light.dim();
  json["on_h"] = light.on_h();
  json["on_m"] = light.on_m();
  json["off_h"] = light.off_h();
  json["off_m"] = light.off_m();
  json["timestamp"] = timestamp();  
  json.printTo(message);
  webServer.send(200, "application/json", message);
}


void handleStatus() {
  String message;
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["power"] = light.power();
  json["status"] = light.status();
  json["timestamp"] = timestamp();  
  json.printTo(message);
  webServer.send(200, "application/json", message);
}

String timestamp_s7s() {
  String ts;
  DateTime now = rtc.now();
  char str[20];
  sprintf(str,"%02d%02d",now.hour(),now.minute());
  ts = str;
  return ts; 
}

String timestamp() {
  String ts;
  DateTime now = rtc.now();
  char str[20];
  sprintf(str,"%04d-%02d-%02d %02d:%02d:%02d",now.year(),now.month(),now.day(),now.hour(),now.minute(),now.second());
  ts = str;
  return ts; 
}

void send_fs (String path,String contentType) {
  if(SPIFFS.exists(path)){
    File file = SPIFFS.open(path, "r");
    size_t sent = webServer.streamFile(file, contentType);
    file.close();
  } else{
    webServer.send(500, "text/plain", "BAD PATH");
  }  
}

String makePage(String title, String contents) {
  String s = R"=====(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<link rel="stylesheet" href="/pure.css">
)=====";  
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += R"=====(
<div class="footer l-box">
<p>WiFi Aquatan-Light by @omzn 2017 / All rights researved</p>
</div>
)=====";
  s += "</body></html>";
  return s;
}

String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}


/*
   NTP related functions
*/
/*
// send an NTP request to the time server at the given address
void sendNTPpacket(const char* address) {
  //  Serial.print("sendNTPpacket : ");
  //  Serial.println(address);

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0]  = 0b11100011;   // LI, Version, Mode
  packetBuffer[1]  = 0;     // Stratum, or type of clock
  packetBuffer[2]  = 6;     // Polling Interval
  packetBuffer[3]  = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

uint32_t readNTPpacket() {
  //  Serial.println("Receive NTP Response");
  udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
  unsigned long secsSince1900 = 0;
  // convert four bytes starting at location 40 to a long integer
  secsSince1900 |= (unsigned long)packetBuffer[40] << 24;
  secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
  secsSince1900 |= (unsigned long)packetBuffer[42] <<  8;
  secsSince1900 |= (unsigned long)packetBuffer[43] <<  0;
  return secsSince1900 - 2208988800UL; // seconds since 1970
}

uint32_t getNTPtime() {
  while (udp.parsePacket() > 0) ; // discard any previously received packets
#ifdef DEBUG
  Serial.println("Transmit NTP Request");
#endif
  sendNTPpacket(timeServer);

  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      return readNTPpacket();
    }
  }

  return 0; // return 0 if unable to get the time
}
*/
