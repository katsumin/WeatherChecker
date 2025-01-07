#define PROGMEM
#include <gfx.h>
#include <W55RP20lwIP.h>
#include <SSLClient.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <images.h>
#include <trust_anchors.h>
#include <NTPClient.h>
#include <EthernetCompat.h>
#include <map>
#include "config.h"

// defines
#define BTN_A 15
#define BTN_B 17
#define BTN_X 2
#define BTN_Y 3
#define LCD_BKLT 13

// Ethernet instance
Wiznet55rp20lwIP eth(1 /* chip select */);

// Display instance
LGFX display;

// Network instance
WiFiUDP udpNtp;
WiFiClient client;

// NTP
const long gmtOffset_sec = 9 * 3600; // 9時間の時差を入れる
NTPClient timeClient(udpNtp, NTP_SERVER, gmtOffset_sec, 10 * 60 * 1000);

// LCD Backlight
int lcd_backlight = HIGH;

StaticJsonDocument<4096> weatherMap;
std::map<uint16_t, const unsigned char *> image_map;
void setup()
{
  image_map[100] = gImage_100;
  image_map[101] = gImage_101;
  image_map[102] = gImage_102;
  image_map[104] = gImage_104;
  image_map[110] = gImage_110;
  image_map[112] = gImage_112;
  image_map[115] = gImage_115;
  image_map[200] = gImage_200;
  image_map[201] = gImage_201;
  image_map[202] = gImage_202;
  image_map[204] = gImage_204;
  image_map[210] = gImage_210;
  image_map[212] = gImage_212;
  image_map[215] = gImage_215;
  image_map[300] = gImage_300;
  image_map[301] = gImage_301;
  image_map[302] = gImage_302;
  image_map[303] = gImage_303;
  image_map[308] = gImage_308;
  image_map[311] = gImage_311;
  image_map[313] = gImage_313;
  image_map[314] = gImage_314;
  image_map[400] = gImage_400;
  image_map[401] = gImage_401;
  image_map[402] = gImage_402;
  image_map[403] = gImage_403;
  image_map[406] = gImage_406;
  image_map[411] = gImage_411;
  image_map[413] = gImage_413;
  image_map[414] = gImage_414;

  // Serial initialize
  Serial.begin(115200);
  while (!Serial && millis() < 5000)
    ;

  // Display initialize
  display.init();
  //   display.setTextSize((std::max(display.width(), display.height()) + 255)
  //   >> 8);
  display.fillScreen(TFT_BLACK);
  // display.setRotation(1);

  // Pico-LCD-2
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_X, INPUT_PULLUP);
  pinMode(BTN_Y, INPUT_PULLUP);
  pinMode(LCD_BKLT, OUTPUT);
  digitalWrite(LCD_BKLT, lcd_backlight);

  // Start the Ethernet port
  if (!eth.begin()) {
    Serial.println(
        "No wired Ethernet hardware detected. Check pinouts, wiring.");
    while (1) {
      delay(1000);
    }
  }

  while (!eth.connected()) {
    Serial.print(".");
    delay(500);
  }

  Serial.print(F("Connected! IP address: "));
  char buf[32];
  IPAddress addr = eth.localIP();
  snprintf(buf, sizeof(buf), "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
  Serial.println(buf);
  // snprintf(buf, sizeof(buf), "%p", lgfxJapanGothic_16);
  // Serial.println(buf);
  display.setFont(&fonts::lgfxJapanGothic_16);
  display.setTextColor(TFT_YELLOW);
  display.setTextDatum(textdatum_t::top_left);
  display.drawString(buf, 0, 0);
  display.drawRoundRect(0, 20, LCD_WIDTH, LCD_HEIGHT - 20 * 2, 5);
  display.drawFastVLine(LCD_WIDTH / 2, 20, LCD_HEIGHT - 20 * 2, TFT_YELLOW);
  display.drawFastHLine(0, 40, LCD_WIDTH, TFT_LIGHTGRAY);
  display.drawFastHLine(0, 60, LCD_WIDTH, TFT_LIGHTGRAY);
  // display.drawFastHLine(0, (LCD_HEIGHT - 20) - 80, LCD_WIDTH, 0xD600);

  // NTP start
  timeClient.begin();
  timeClient.update();

  //
  DeserializationError error = deserializeJson(weatherMap, WEATHER_CODE_MAP);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  }
}

#define SECOONDS_AT_DAY (24 * 60 * 60)
#define BATTERY_UPDATE_TIMING (23 * 60 * 60 + 30 * 60) // 23:30
#define SECOONDS_AT_HOUR (60 * 60)
#define WEATHER_REQUEST_TIMING (0)
#define NTP_REQUEST_TIMING (0)
const char host[] = "www.jma.go.jp";
SSLClient cl(client, TAs, (size_t)TAs_NUM, 0);
StaticJsonDocument<8192> doc;
char weather_json[64];

const pin_size_t button_pins[] PROGMEM = {BTN_A, BTN_B, BTN_X, BTN_Y};
const char *button_names[] PROGMEM = {"A", "B", "X", "Y"};
PinStatus button_state[sizeof(button_pins)][3];
uint16_t timer_bklt = 60 * 3;

#define POS_Y_TIME (20)
#define POS_Y_CODE (40)
#define POS_Y_MSG (60)
#define POS_Y_IMG ((LCD_HEIGHT - 1 - HEIGHT_IMAGE - IAMGE_MARGIN) - HEIGHT_TIME)
#define HEIGHT_TIME (20)
#define HEIGHT_CODE (20)
#define WIDTH_TIME (LCD_WIDTH / 2)
#define WIDTH_MSG (LCD_WIDTH / 2)
#define HEIGHT_IMAGE (60)
#define MAXLEN_MSG (3 * 10)
#define TEXT_MARGIN (2)
#define IAMGE_MARGIN (10)

const char *wday[] = {"日", "月", "火", "水", "木", "金", "土"};
char bufDate[32];
/**
 * @brief 日付表示
 *
 * @param index 天気情報インデックス
 * @param time 日付文字列
 */
void outputDate(int index, String time)
{
  // 曜日の算出
  int year, month, day;
  sscanf(time.substring(0, 10).c_str(), "%04d-%02d-%02d", &year, &month, &day);
  tm t = {0};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  time_t epoch = mktime(&t);
  tm *pt = localtime(&epoch);
  sprintf(bufDate, "%04d/%02d/%02d(%s)\n", pt->tm_year + 1900, pt->tm_mon + 1,
          pt->tm_mday, wday[pt->tm_wday]);

  int32_t img_x = (LCD_WIDTH / 4) + WIDTH_TIME * index;
  // int32_t tw = display.textWidth(time);
  display.fillRect(WIDTH_TIME * index + TEXT_MARGIN, POS_Y_TIME + TEXT_MARGIN,
                   WIDTH_TIME - TEXT_MARGIN * 2, HEIGHT_TIME - TEXT_MARGIN * 2,
                   TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setTextDatum(textdatum_t::middle_center);
  display.drawString(bufDate, img_x, POS_Y_TIME + HEIGHT_TIME / 2);
}

/**
 * @brief 天気コード表示
 *
 * @param index 天気情報インデックス
 * @param time 天気コード文字列
 */
void outputCode(int index, String code)
{
  int32_t img_x = (LCD_WIDTH / 4) + WIDTH_TIME * index;
  // int32_t tw = display.textWidth(code);
  display.fillRect(WIDTH_TIME * index + TEXT_MARGIN, POS_Y_CODE + TEXT_MARGIN,
                   WIDTH_TIME - TEXT_MARGIN * 2, HEIGHT_CODE - TEXT_MARGIN * 2,
                   TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setTextDatum(textdatum_t::middle_center);
  display.drawString(code, img_x, POS_Y_CODE + HEIGHT_CODE / 2);
}

/**
 * @brief 天気メッセージ表示
 *
 * @param index 天気情報インデックス
 * @param weather_msg 天気メッセージ
 */
void outputMessage(int index, String weather_msg)
{
  int32_t th = display.fontHeight();
  const int32_t msg_y = POS_Y_MSG + TEXT_MARGIN;
  const int32_t msg_h = POS_Y_IMG - msg_y;
  display.fillRect(WIDTH_MSG * index + TEXT_MARGIN, msg_y,
                   WIDTH_MSG - TEXT_MARGIN * 2, msg_h - TEXT_MARGIN * 2,
                   TFT_BLACK);
  // size_t l = strlen(weather_msg) / 3;
  // Serial.println(l);
  // String msg = String(weather_msg);
  size_t l = weather_msg.length();
  // Serial.println(l);
  int j = 0;
  display.setTextDatum(textdatum_t::top_left);
  for (; j < l / MAXLEN_MSG; j++) {
    String s =
        weather_msg.substring(j * MAXLEN_MSG, j * MAXLEN_MSG + MAXLEN_MSG);
    display.drawString(s, 2 + WIDTH_MSG * index, msg_y + j * th);
    Serial.println(s.c_str());
  }
  if (l % MAXLEN_MSG != 0) {
    String s = weather_msg.substring(j * MAXLEN_MSG);
    display.drawString(s, 2 + WIDTH_MSG * index, msg_y + j * th);
    Serial.println(s.c_str());
  }
}

/**
 * @brief 天気アイコン表示
 *
 * @param index 天気情報インデックス
 * @param weather_img_code アイコン用天気コード
 */
void outputImage(int index, uint16_t weather_img_code)
{
  int32_t img_x = (LCD_WIDTH / 4) + WIDTH_TIME * index;
  const unsigned char *img_data =
      image_map[weather_img_code]; // img_data = { xx, xx, width lower, width
  // higher, height lower, height higher }
  int32_t img_w = (int)img_data[3] * 256 + (int)img_data[2];
  int32_t img_h = (int)img_data[5] * 256 + (int)img_data[4];
  display.setSwapBytes(true); // バイト順の変換を有効にする。
  display.pushImage(
      img_x - img_w / 2, POS_Y_IMG, img_w, img_h,
      (uint16_t *)&img_data[8]); // RGB565の16bit画像データを描画。
}

char bufTime[32];
/**
 * @brief 時刻表示
 *
 * @param epoch 時刻（epoch time）
 */
void outputTime(time_t epoch)
{
  tm *t = localtime(&epoch);
  snprintf(bufTime, sizeof(bufTime), "%04d/%02d/%02d %02d:%02d:%02d",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min,
           t->tm_sec);
  int32_t th = display.fontHeight();
  int32_t tw = display.textWidth(bufTime);
  display.fillRect(LCD_WIDTH - tw, LCD_HEIGHT - th, tw, th, TFT_BLACK);
  display.setTextColor(TFT_YELLOW);
  display.setTextDatum(textdatum_t::bottom_right);
  display.drawString(bufTime, LCD_WIDTH - 1, LCD_HEIGHT - 1);
}

time_t pre_epoch = timeClient.getEpochTime();
bool first = true;
void loop()
{
  for (int i = 0; i < sizeof(button_pins); i++) {
    button_state[i][2] = button_state[i][1];
    button_state[i][1] = button_state[i][0];
    button_state[i][0] = digitalRead(button_pins[i]);
    if (button_state[i][0] == LOW && button_state[i][1] == LOW &&
        button_state[i][2] == HIGH) {
      Serial.println(button_names[i]);
      if (i == 0) {
        digitalWrite(LCD_BKLT, HIGH);
        timer_bklt = 60 * 1;
      }
    }
  }

  time_t epoch = timeClient.getEpochTime();
  if (epoch != pre_epoch) {
    pre_epoch = epoch;

    // backlight
    if (timer_bklt > 0) {
      Serial.println(timer_bklt);
      timer_bklt--;
      if (timer_bklt == 0) {
        digitalWrite(LCD_BKLT, LOW);
        Serial.println("off");
      }
    }

    // display time
    outputTime(epoch);

    if (epoch % SECOONDS_AT_DAY == NTP_REQUEST_TIMING) // 00:00
      timeClient.update();
    if (first || epoch % SECOONDS_AT_HOUR == WEATHER_REQUEST_TIMING) // xx:00
    {
      // static bool wait = true;

      // Serial.print("connecting to ");
      // Serial.print(host);
      // Serial.print(':');
      // Serial.println(port);

      // Use WiFiClient class to create TCP connections
      // SSLClient cl(client, TAs, (size_t)TAs_NUM, 0);
      Serial.print("connecting: ");
      Serial.println(host);
      first = false;
      HttpClient hcl = HttpClient(cl, host, 443);
      // if (!client.connect(host, port))
      snprintf(weather_json, sizeof(weather_json),
               "/bosai/forecast/data/forecast/%d.json", WEATHER_AREA_CODE);
      int res = hcl.get(weather_json);
      Serial.print("response json: ");
      Serial.println(res);
      if (res != 0) {
        Serial.println("connection failed");
        delay(5000);
        return;
      } else {
        int statusCode = hcl.responseStatusCode();
        Serial.print("status code: ");
        Serial.println(statusCode);
        String response = hcl.responseBody();
        DeserializationError error = deserializeJson(doc, response);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
        } else {
          const JsonArray arrTimes = doc[0]["timeSeries"][0]["timeDefines"];
          size_t index = arrTimes.size() - 2;
          // Serial.println(index);
          for (int i = 0; i < 2; i++) {
            const String time =
                String(doc[0]["timeSeries"][0]["timeDefines"][i + index]
                           .as<const char *>());
            Serial.print("time: ");
            Serial.println(time);
            const String weather_code =
                String(doc[0]["timeSeries"][0]["areas"][WEATHER_SUB_AREA_INDEX]
                          ["weatherCodes"][i + index]
                              .as<const char *>());
            Serial.print("weatherCode: ");
            Serial.println(weather_code);
            const String weather_msg =
                String(doc[0]["timeSeries"][0]["areas"][WEATHER_SUB_AREA_INDEX]
                          ["weathers"][i + index]
                              .as<const char *>());
            Serial.print("weatherMessage: ");
            Serial.println(weather_msg);

            // time
            outputDate(i, time);
            // code
            outputCode(i, weather_code);
            // message
            outputMessage(i, weather_msg);
            // image
            const uint16_t weather_img_code =
                weatherMap[weather_code].as<const uint16_t>();
            outputImage(i, weather_img_code);
          }
        }
        Serial.println();
        Serial.println("closing connection");
        client.stop();
        // hcl.stop();
      }

      // // This will send a string to the server
      // Serial.println("sending data to server");
      // if (client.connected())
      // {
      //     client.println("hello from RP2040");
      // }

      // // wait for data to be available
      // unsigned long timeout = millis();
      // while (client.available() == 0)
      // {
      //     if (millis() - timeout > 5000)
      //     {
      //         Serial.println(">>> Client Timeout !");
      //         client.stop();
      //         delay(60000);
      //         return;
      //     }
      // }

      // // Read all the lines of the reply from server and print them to
      // Serial Serial.println("receiving from remote server");
      // // not testing 'client.connected()' since we do not need to send
      // data here while (client.available())
      // {
      //     char ch = static_cast<char>(client.read());
      //     Serial.print(ch);
      // }

      // Close the connection
    }
  }
  delay(10);
}
