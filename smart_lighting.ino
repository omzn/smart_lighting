/*
   Aqua-tan smart light manager
   v2.0b

   API

   /on
      power=(int)
   /off
   /status
   /config
      enable=[true|false]
      p[x]_[hhmm]=uint8_t (x -> LED id, hhmm -> time)
      max_power=(int)
      //time[1-10]=(hh:mm)
      //power[1-10]=(int)
   /wifireset
   /reset
   /reboot
*/

#include "esp_lighting.h" // Configuration parameters

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <RTClib.h>
#include <SPI.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <pgmspace.h>

#include "SF_s7s_hw.h"
#include "ledlight.h"
#include "ntp.h"
#include "pca9633.h"

const String boolstr[2] = {"false", "true"};

String website_name = "newaqualight";
const char *apSSID = "WIFI_LIGHT_TAN";
boolean settingMode;
String ssidList;

const char *localserver = "mowatmirror.local";
const int localport = 3000;

uint32_t timer_count = 0;

uint32_t p_millis;
int prev_m = 0;
int prev_s[MAX_LED_NUM];

DNSServer dnsServer;
MDNSResponder mdns;
const IPAddress apIP(192, 168, 1, 1);
ESP8266WebServer webServer(80);
WiFiClient client;

RTC_Millis rtc;

NTP ntp("ntp.nict.jp");
ledLight light;
S7S s7s;

/* Setup and loop */

void setup() {
  ESP.wdtDisable();
  pinMode(PIN_LIGHT, OUTPUT);
  digitalWrite(PIN_LIGHT, LOW);

  //  analogWriteRange(1024);
  //  analogWriteFreq(200);

  p_millis = millis();
#ifdef DEBUG
  Serial.begin(115200);
#else
  Serial.begin(9600);
#endif
  EEPROM.begin(1024);
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
  Wire.begin(PIN_SDA,PIN_SCL);
  light.begin(PCA9633_ADDRESS_1);

  settingMode = true;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  if (restoreConfig()) {
    if (checkConnection()) {
      setupArduinoOTA();
      settingMode = false;
    }
  }
  if (settingMode == true) {
    s7s.clearDisplay();
    s7s.print("SET ");
    delay(500);
#ifdef DEBUG
    Serial.println("Setting mode");
#endif
    // WiFi.mode(WIFI_STA);
    // WiFi.disconnect();
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
    s7s.clearDisplay();
    s7s.print("norl");
    ntp.begin();
    delay(250);
    s7s.clearDisplay();
    startWebServer_normal();
#ifdef DEBUG
    Serial.println("Starting normal operation.");
#endif
  }
  ESP.wdtEnable(WDTO_8S);
}

void loop() {
  ESP.wdtFeed();
  if (settingMode) {
    dnsServer.processNextRequest();
  } else {
    ArduinoOTA.handle();
  }
  webServer.handleClient();

  // 1秒毎に実行
  if (millis() > p_millis + 1000) {
    p_millis = millis();
    if (timer_count % 3600 == 0) {
      uint32_t epoch = ntp.getTime();
      // uint32_t epoch = getNTPtime();
      if (epoch > 0) {
        rtc.adjust(DateTime(epoch + SECONDS_UTC_TO_JST));
      }
#ifdef DEBUG
      Serial.print("epoch:");
      Serial.println(epoch);
#endif
    }
    if (!settingMode) {
      DateTime now = rtc.now();
      for (int jj = 0; jj < MAX_LED_NUM; jj++) {
        int st = light.control(jj, now.hour(), now.minute(), now.second());
        if (st == LIGHT_ON) {
          if (prev_s[jj] != st) {
            post_data(5, "light" + String(jj), st);
          }
          if (prev_m != now.minute()) {
            s7s.clearDisplay();
            s7s.setBrightness(255);
            s7s.printTime(timestamp_s7s());
          }
        } else if (st == LIGHT_OFF) {
          if (prev_s[jj] != st) {
            post_data(5, "light", st);
          }
          if (prev_m != now.minute()) {
            s7s.clearDisplay();
            s7s.setBrightness(127);
            s7s.printTime(timestamp_s7s());
          }
        } else {
          //        if (prev_s != st) {
//          s7s.clearDisplay();
//          s7s.setBrightness(255);
//          s7s.print4d(light.power());
          //          s7s.print("dirn");
          //        }
        }
        prev_m = now.minute();
        prev_s[jj] = st;
      }
    }
    timer_count++;
    timer_count %= (86400UL);
  }
  delay(10);
}

void post_data(int room, String label, float value) {
  if (client.connect(localserver, localport)) {
    // Create HTTP POST Data
    String postData;
    char str[64];

    //    postData = "room=" + String(room) + "&label=" + label + "&value=" +
    //    value;
    sprintf(str, "room=%d&label=%s&value=%4.1f", room, label.c_str(), value);
    postData += str;

    client.print("POST /api/v1/add HTTP/1.1\n");
    client.print("Host: ");
    client.print(localserver);
    client.print("\n");
    client.print("Connection: close\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postData.length());
    client.print("\n\n");

    client.print(postData);
    client.stop();
  }
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
      website_name = "";
      for (int i = 0; i < 32; ++i) {
        byte c = EEPROM.read(EEPROM_MDNS_ADDR + i);
        if (c == 0) {
          break;
        }
        website_name += char(c);
      }
    }

    int e_schedule = EEPROM.read(EEPROM_SCHEDULE_ADDR) == 1 ? 1 : 0;
    light.enable(e_schedule == 0 ? 0 : 1);

    int e_max = EEPROM.read(EEPROM_MAX_ADDR) | EEPROM.read(EEPROM_MAX_ADDR + 1)
                                                   << 8;
    light.max_power(e_max);
#ifdef DEBUG
    Serial.print("schedule:");
    Serial.println(light.enable());
#endif
    int k = 0;
    for (int jj = 0; jj < MAX_LED_NUM; jj++) {
      for (int j = 0; j < 24; j++) {
        for (int i = 0; i < 60; i += 10) {
          uint8_t val = EEPROM.read(EEPROM_POWER_ADDR + 144 * jj + k);
          val = val > 100 ? 100 : val;
          light.powerAtTime(jj, val, j, i);
          k++;
        }
      }
    }

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
  while (count < 10) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    ESP.wdtFeed();
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
    s += "\" after the restart.</p><p>Your computer also need to re-connect to "
         "\"";
    s += ssid;
    s += "\".</p><p><button class=\"pure-button\" onclick=\"return "
         "quitBox();\">Close</button></p>";
    s += "<script>function quitBox() { open(location, '_self').close();return "
         "false;};setTimeout(\"quitBox()\",10000);</script>";
    webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    timer_count = 0;
    ESP.restart();
    while (1) {
      delay(0);
    }
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
    String s = "<h3 class=\"if-head\">Reset ALL</h3><p>Cleared all settings. "
               "Please reset device.</p>";
    s += "<p><button class=\"pure-button\" onclick=\"return "
         "quitBox();\">Close</button></p>";
    s += "<script>function quitBox() { open(location, '_self').close();return "
         "false;};</script>";
    webServer.send(200, "text/html", makePage("Reset ALL Settings", s));
    timer_count = 0;
    ESP.restart();
    while (1) {
      delay(0);
    }
  });
  webServer.on("/wifireset", []() {
    for (int i = 0; i < EEPROM_MDNS_ADDR; ++i) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    String s = "<h3 class=\"if-head\">Reset WiFi</h3><p>Cleared WiFi settings. "
               "Please reset device.</p>";
    s += "<p><button class=\"pure-button\" onclick=\"return "
         "quitBox();\">Close</button></p>";
    s += "<script>function quitBox() { open(location, '_self').close();return "
         "false;}</script>";
    webServer.send(200, "text/html", makePage("Reset WiFi Settings", s));
    timer_count = 0;
    ESP.restart();
    while (1) {
      delay(0);
    }
  });
  webServer.on("/", handleRoot);
  webServer.on("/jqp_min.js", handle_jqp_min_js);
  webServer.on("/jqp_cursor.js", handle_jqp_cursor_js);
  webServer.on("/jqp_dragable.js", handle_jqp_dragable_js);
  webServer.on("/jqp_dateAxisRenderer.js", handle_jqp_dateAxisRenderer_js);
  webServer.on("/jqp_min.css", handle_jqp_css);
  webServer.on("/pure.css", handleCss);
  webServer.on("/reboot", handleReboot);
  webServer.on("/on", handleActionOn);
  webServer.on("/off", handleActionOff);
  webServer.on("/status", handleStatus);
  webServer.on("/config", handleConfig);
  webServer.begin();
}

void handleRoot() { send_fs("/index.html", "text/html"); }

void handle_jqp_min_js() { send_fs("/jqp_min.js", "application/javascript"); }

void handle_jqp_cursor_js() {
  send_fs("/jqp_cursor.js", "application/javascript");
}

void handle_jqp_dateAxisRenderer_js() {
  send_fs("/jqp_dateAxisRenderer.js", "application/javascript");
}

void handle_jqp_dragable_js() {
  send_fs("/jqp_dragable.js", "application/javascript");
}

void handle_jqp_css() { send_fs("/jqp_min.css", "text/css"); }

void handleCss() { send_fs("/pure.css", "text/css"); }

void handleReboot() {
  String message;
  message = "{reboot:\"done\"}";
  webServer.send(200, "application/json", message);
  ESP.restart();
  while (1) {
    delay(0);
  }
}

void handleActionOn() {
  String message, argname, argv;
  int p = -1, l = 0, err;

  // on
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();

  for (int i = 0; i < webServer.args(); i++) {
    argname = webServer.argName(i);
    argv = webServer.arg(i);
    if (argname == "light") {
      l = argv.toInt();
    } else if (argname == "brightness") {
      p = argv.toInt();
    }
  }
  if (p >= 0) {
    err = light.control(l, p);
  } else {
    err = light.control(l, 100);
  }
  if (err < 0) {
    json["error"] = "Cannot turn on/off light while schedule is enabled.";
  } else {
    json["light"] = l;
    json["brightness"] = light.brightness(l);
    json["status"] = light.status(l);
    json["timestamp"] = timestamp();
  }
  json.printTo(message);
  webServer.send(200, "application/json", message);
}

void handleActionOff() {
  String message, argname, argv;
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();
  uint8_t l = 0;

  for (int i = 0; i < webServer.args(); i++) {
    argname = webServer.argName(i);
    argv = webServer.arg(i);
    if (argname == "light") {
      l = argv.toInt();
    }
  }
  // off

  int err = light.control(l, 0);
  if (err < 0) {
    json["error"] = "Cannot turn on/off light while schedule is set.";
  }
  json["light"] = l;
  json["brightness"] = light.brightness(l);
  json["status"] = light.status(l);
  json["timestamp"] = timestamp();
  json.printTo(message);
  webServer.send(200, "application/json", message);
}

void handleConfig() {
  String message, argname, argv;
  char buf[5];
  int en, on_h, on_m, off_h, off_m, dim;
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();
  JsonArray &powerarray = json.createNestedArray("power");

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
      en = (argv == "true" ? 1 : 0);
      light.enable(en);
      EEPROM.write(EEPROM_SCHEDULE_ADDR, char(light.enable()));
      EEPROM.commit();
    } else if (argname.substring(0, 1) == "p") {
      uint8_t ll = argname.substring(1, 2).toInt();
      uint8_t hh = argname.substring(3, 5).toInt();
      uint8_t m = argname.substring(5, 6).toInt();
      uint16_t address = ll * 144 + hh * 6 + m;
      uint8_t val = argv.toInt();
      light.powerAtTime(ll, val, hh, m * 10);
      EEPROM.write(EEPROM_POWER_ADDR + address, argv.toInt());
      EEPROM.commit();
    } else if (argname == "max_power") {
      int max_v = argv.toInt();
      light.max_power(max_v);
      EEPROM.write(EEPROM_MAX_ADDR, char(light.max_power() & 0xFF));
      EEPROM.write(EEPROM_MAX_ADDR + 1, char(light.max_power() >> 8));
      EEPROM.commit();
    }
  }

  for (int jj = 0; jj < MAX_LED_NUM; jj++) {
    JsonArray &powerseq = powerarray.createNestedArray();
    for (int j = 0; j < 24; j++) {
      for (int i = 0; i < 60; i += 10) {
        powerseq.add(light.powerAtTime(jj, j, i));
      }
    }
  }

  json["enable"] = boolstr[light.enable()];
  //  json["power"] = power;
  json["max_power"] = light.max_power();
  json["timestamp"] = timestamp();
  json.printTo(message);
  webServer.send(200, "application/json", message);
}

void handleStatus() {
  String message;
  DynamicJsonBuffer jsonBuffer;
  JsonArray &arr = jsonBuffer.createArray();
  for (int jj = 0; jj < MAX_LED_NUM; jj++) {
    JsonObject &json = arr.createNestedObject();
    json["brightness"] = light.brightness(jj);
    json["status"] = light.status(jj);
    json["timestamp"] = timestamp();
  }
  arr.printTo(message);
  webServer.send(200, "application/json", message);
}

String timestamp_s7s() {
  String ts;
  DateTime now = rtc.now();
  char str[20];
  sprintf(str, "%02d%02d", now.hour(), now.minute());
  ts = str;
  return ts;
}

String timestamp() {
  String ts;
  DateTime now = rtc.now();
  char str[20];
  sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(),
          now.day(), now.hour(), now.minute(), now.second());
  ts = str;
  return ts;
}

void send_fs(String path, String contentType) {
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    size_t sent = webServer.streamFile(file, contentType);
    file.close();
  } else {
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

void setupArduinoOTA() {
  ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(website_name.c_str());
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS
    // using SPIFFS.end()
    // Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("%u", (progress / (total / 100)));
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
  delay(100);
}
