// Color E-Paper Photo Frame.
// 2022-04-18  T. Nakagawa

#include <LittleFS.h>
#include <Preferences.h>
#include <SimpleFTPServer.h>
#include <WiFi.h>
#include <soc/rtc_cntl_reg.h>
#include "EPD.h"
#include "GIF.h"

extern "C" int rom_phy_get_vdd33();

constexpr int PIN_DIN = 23;
constexpr int PIN_SCK = 18;
constexpr int PIN_CS = 22;
constexpr int PIN_DC = 21;
constexpr int PIN_RST = 17;
constexpr int PIN_BUSY = 16;
constexpr int PIN_SW0 = 34;
constexpr int PIN_SW1 = 35;
constexpr int PIN_LED = 32;
constexpr int PIN_POWER = 33;
constexpr float SHUTDOWN_VOLTAGE = 2.6f;
constexpr float VOLTAGE_ADJUST = 3.30f / 3.50f;
constexpr int PATH_LEN = 32;

Preferences preferences;
FtpServer ftp;
EPD epd(PIN_DIN, PIN_SCK, PIN_CS, PIN_DC, PIN_RST, PIN_BUSY);
RTC_DATA_ATTR char photo_path[PATH_LEN];

float getVoltage() {
  btStart();
  int v = 0;
  for (int i = 0; i < 20; i++) v += rom_phy_get_vdd33();
  btStop();
  v /= 20;
  const float vdd =  (0.0005045f * v + 0.3368f) * VOLTAGE_ADJUST;
  return vdd;
}

void shutdown() {
  Serial.println("Shutting down due to low battery voltage.");
  esp_deep_sleep_start();  // Sleep indefinitely.
}

void config() {
  Serial.println("Entering the configuration mode.");
  preferences.begin("config", false);
  Serial.println("Free entries: " + String(preferences.freeEntries()));
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32", "12345678");
  delay(100);
  WiFi.softAPConfig(IPAddress(192, 168, 0, 1), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));

  WiFiServer server(80);
  server.begin();
  while (true) {
    WiFiClient client = server.available();
    if (client) {
      const String line = client.readStringUntil('\n');
      Serial.println("Accessed: " + line);
      String message;
      if (line.startsWith("GET /?")) {
        String key;
        String val;
        String buf = line.substring(6);
        int pos = buf.indexOf(" ");
        if (pos < 0) pos = 0;
        buf = buf.substring(0, pos);
        buf.concat("&");
        while (buf.length()) {
          int pos = buf.indexOf("&");
          const String param = buf.substring(0, pos);
          buf = buf.substring(pos + 1);
          pos = param.indexOf("=");
          if (pos < 0) continue;
          String tmp = param.substring(pos + 1);
          tmp.replace('+', ' ');
          for (int c = 0; c < 256; c++) {
            String hex = String("%") + ((c < 16) ? "0" : "") + String(c, HEX);
            tmp.replace(hex, String((char)c));
            hex.toLowerCase();
            tmp.replace(hex, String((char)c));
          }
          if (param.substring(0, pos) == "key") {
            key = tmp;
          }
          else if (param.substring(0, pos) == "val") {
            val = tmp;
          }
        }
        key.trim();
        val.trim();
        Serial.println("key=" + key + ", val=" + val);
        if (key.length()) {
          preferences.putString(key.c_str(), val);
          if (preferences.getString(key.c_str()) == val) {
            message = "Succeeded to update: " + key;
          } else {
            message = "Failed to write: " + key;
          }
        } else {
          message = "Key was not specified.";
        }
      }

      const float voltage = getVoltage();
      message += "\nBattery: " + String(voltage) + "V";

      client.println("<!DOCTYPE html>");
      client.println("<head><title>Configuration</title></head>");
      client.println("<body>");
      client.println("<h1>Configuration</h1>");
      client.println("<form action=\"/\" method=\"get\">Key: <input type=\"text\" name=\"key\" size=\"10\"> Value: <input type=\"text\" name=\"val\" size=\"20\"> <input type=\"submit\"></form>");
      client.println("<p>" + message + "</p>");
      client.println("</body>");
      client.println("</html>");
      client.stop();
    }

    if (millis() > 5 * 60 * 1000 || digitalRead(PIN_SW1) == HIGH) break;
    digitalWrite(PIN_LED, (millis() & (1 << 8)) ? HIGH : LOW);	// LED blink at 1000/256/2Hz.
  }

  Serial.println("Disconnected.");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  preferences.end();
  digitalWrite(PIN_LED, LOW);
}

void transfer() {
  auto ftp_callback = [](FtpOperation ftpOperation, unsigned int freeSpace, unsigned int totalSpace) {
    switch (ftpOperation) {
    case FTP_CONNECT:
      Serial.println("Client connected.");
      break;
    case FTP_DISCONNECT:
      Serial.println("Client disconnected.");
      break;
    }
  };
  auto ftp_transfer_callback = [](FtpTransferOperation ftpOperation, const char* name, unsigned int transferredSize) {
    switch (ftpOperation) {
    case FTP_UPLOAD_START:
      Serial.println("File upload started.");
      break;
    case FTP_TRANSFER_STOP:
      Serial.println("File transfer finished.");
      break;
    }
  };

  Serial.println("Entering the transfer mode.");
  preferences.begin("config", true);
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to " + preferences.getString("SSID"));
  WiFi.begin(preferences.getString("SSID").c_str(), preferences.getString("PASS").c_str());
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() >= 30000) return;
    Serial.print(".");
    delay(500);
  }
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

  LittleFS.begin(true);
  ftp.setCallback(ftp_callback);
  ftp.setTransferCallback(ftp_transfer_callback);
  ftp.begin();	// FTP for anonymous connection.
  while (true) {
    const uint8_t status = ftp.handleFTP();
    if (millis() > 5 * 60 * 1000 || digitalRead(PIN_SW0) == HIGH) break;
    digitalWrite(PIN_LED, (millis() & (1 << 9)) ? HIGH : LOW);	// LED blink at 1000/512/2Hz.
  }

  Serial.println("Disconnected.");
  ftp.end();
  LittleFS.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  preferences.end();
  digitalWrite(PIN_LED, LOW);
}

String find_path(const char *dir, const String& path_old, int delta) {
  String path_head, path_tail, path_prev, path_next;
  int num = 0;

  File root = LittleFS.open(dir);
  if (root) {
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
      num++;
      const String path = file.name();
      file.close();
      if (path.length() > PATH_LEN - 1) continue;
      if (!path.endsWith(".gif")) continue;
      if (path_head == "" || path < path_head) path_head = path;
      if (path_tail == "" || path > path_tail) path_tail = path;
      if (path < path_old && (path_prev == "" || path > path_prev)) path_prev = path;
      if (path > path_old && (path_next == "" || path < path_next)) path_next = path;
    }
    root.close();
  }
  Serial.println("Number of files: " + String(num));
  if (!path_prev.length()) path_prev = path_tail;
  if (!path_next.length()) path_next = path_head;
  const String path_new = (delta > 0) ? path_next : path_prev;
  return path_new;
}

void display(int delta) {
  LittleFS.begin();

  String path = String(photo_path);
  Serial.println("Previous photo: " + path);
  const int sign = (delta > 0) ? 1 : -1;
  while (delta) {
    path = find_path("/", path, sign);
    delta -= sign;
  }
  path.toCharArray(photo_path, PATH_LEN);
  Serial.println("Drawing the photo: " + path);

  if (path.length()) {
    digitalWrite(PIN_POWER, LOW);
    delay(100);
    epd.begin();
    File file = LittleFS.open(("/" + path).c_str(), "r");
    const int status = GIF::read(&file, EPD::WIDTH, EPD::HEIGHT, [&](uint8_t *data, int size) { return epd.transfer(data, size); });
    file.close();
    epd.end();
    digitalWrite(PIN_POWER, HIGH);
    Serial.println("Status=" + String(status));
  }
  LittleFS.end();
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brown-out detection.
  pinMode(PIN_SW0, INPUT);
  pinMode(PIN_SW1, INPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_POWER, HIGH);
  pinMode(PIN_POWER, OUTPUT);
  Serial.begin(115200);
  while (!Serial) ;
  Serial.println("Color E-Paper Photo Frame");

  // Determine the mode.
  enum Mode {NONE, CONFIG, TRANSFER, FORWARD, BACKWARD} mode = NONE;
  int delta = 1;
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_TIMER) {
    mode = FORWARD;
  } else if (reason == ESP_SLEEP_WAKEUP_EXT1) {
    uint64_t status = esp_sleep_get_ext1_wakeup_status();
    if (status & (1ULL << PIN_SW0)) mode = BACKWARD;
    if (status & (1ULL << PIN_SW1)) mode = FORWARD;
    while (digitalRead(PIN_SW0) == HIGH || digitalRead(PIN_SW1) == HIGH) {
      if (digitalRead(PIN_SW0) == HIGH && digitalRead(PIN_SW1) == HIGH) {
        delta = 10;
        break;
      }
      if (millis() > 3000) {
        mode = (digitalRead(PIN_SW0) == HIGH) ? CONFIG : TRANSFER;
        break;
      }
    }
  }
  Serial.println("Mode=" + String(mode));

  if (mode == CONFIG) {
    config();
  } else if (mode == TRANSFER) {
    transfer();
  } else if (mode == FORWARD || mode == BACKWARD) {
    if (mode == BACKWARD) delta = -delta;
    display(delta);
  }

  // Check the battery voltage.
  const float voltage = getVoltage();
  Serial.println("Battery voltage: " + String(voltage));
  if (voltage < SHUTDOWN_VOLTAGE) shutdown();

  // Deep sleep.
  preferences.begin("config", true);
  unsigned long long sleep = preferences.getString("SLEEP", "86400").toInt();
  preferences.end();
  Serial.println("Sleeping: " + String((unsigned long)sleep) + "sec.");
  esp_sleep_enable_timer_wakeup(sleep * 1000000);
  esp_sleep_enable_ext1_wakeup(((1ULL << PIN_SW0) | (1ULL << PIN_SW1)), ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_deep_sleep_start();
}

void loop() {
}
