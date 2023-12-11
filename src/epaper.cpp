#include "epaper.h"

void drawSections()
{
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  //display.drawLine(0, 12, SCREEN_WIDTH, 12, GxEPD_BLACK);

  //display.drawLine(0, (SCREEN_HEIGHT / 1.8) + 35, SCREEN_WIDTH, (SCREEN_HEIGHT / 1.8) + 35, GxEPD_BLACK);
  drawStringLine(SCREEN_WIDTH / 2, (SCREEN_HEIGHT / 1.8) + 45, "INSIDE", CENTER, u8g2_font_helvB08_tf);

  //display.drawLine(0, (SCREEN_HEIGHT / 2.8) + 50, SCREEN_WIDTH, (SCREEN_HEIGHT / 2.8) + 50, GxEPD_BLACK);
  drawStringLine(SCREEN_WIDTH / 2, (SCREEN_HEIGHT / 2.8) + 20, "OUTSIDE", CENTER, u8g2_font_helvB08_tf);
}

void drawString(int x, int y, String text, alignment align, const uint8_t *font)
{
  u8g2Fonts.setFont(font);
  char textArray[text.length() + 1];
  text.toCharArray(textArray, text.length() + 1);
  int16_t textWidth = u8g2Fonts.getUTF8Width(textArray);
  int16_t textAscent = u8g2Fonts.getFontAscent();
  int16_t textDescent = u8g2Fonts.getFontDescent();
  int16_t boxWidth = textWidth + 2;
  int16_t boxHeight = textAscent + textDescent + 6; // 10

  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)
    x = x - w - 10;
  if (align == CENTER)
  {
    // x = x - w / 2;
    x = x - textWidth / 2;
  }
  display.fillRect(x - 4, y + h - boxHeight + 4, boxWidth + 8, boxHeight, GxEPD_WHITE);
  // display.drawRect(x - 4, y + h - boxHeight + 4, boxWidth + 8, boxHeight, GxEPD_BLACK);
  u8g2Fonts.setCursor(x, y + h + 2); // +2
  u8g2Fonts.print(text);
  display.display(true); // partial update
}

void drawStringLine(int x, int y, String text, alignment align, const uint8_t *font)
{
  u8g2Fonts.setFont(font);
  char textArray[text.length() + 1];
  text.toCharArray(textArray, text.length() + 1);
  int16_t textWidth = u8g2Fonts.getUTF8Width(textArray);
  int16_t textAscent = u8g2Fonts.getFontAscent();
  int16_t textDescent = u8g2Fonts.getFontDescent();
  int16_t boxWidth = textWidth + 2;
  int16_t boxHeight = textAscent - textDescent + 4; // + textDescent + 15; // 10
#ifdef DEBUG
  Serial.printf("[DEBUG]: drawStringLine box:\ntxtW: %d, txtAsc: %d, txtDesc: %d, boxW: %d, boxH: %d\n", textWidth, textAscent, textDescent, boxWidth, boxHeight);
#endif
  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
#ifdef DEBUG
  Serial.printf("[DEBUG]: drawStringLine text:\nx: %d, y: %d, x1: %d, y1: %d, w: %d, h: %d\n", x, y, &x1, &y1, &w, &h);
#endif
  if (align == RIGHT)
    x = x - w - 10;
  if (align == CENTER)
  {
    // x = x - w / 2;
    x = x - textWidth / 2;
  }

#ifdef DEBUG
  display.drawRect(0, y + h - boxHeight + 8, SCREEN_WIDTH, boxHeight, GxEPD_BLACK);
  // display.drawRect(0, y, SCREEN_WIDTH, boxHeight, GxEPD_BLACK); // y - h * 4
#else
  display.fillRect(0, y + h - boxHeight + 8, SCREEN_WIDTH, boxHeight, GxEPD_WHITE);
  display.drawLine(0, y + h - boxHeight + 8, SCREEN_WIDTH, y + h - boxHeight + 8, GxEPD_BLACK);
  // display.fillRect(0, y - h * 2, SCREEN_WIDTH, boxHeight, GxEPD_WHITE);
#endif
  u8g2Fonts.setCursor(x, y + h + 2); // +2
  u8g2Fonts.print(text);
  display.display(true); // partial update
}

void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align)
{
  int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)
    x = x - w;
  if (align == CENTER)
    x = x - w / 2;
  u8g2Fonts.setCursor(x, y);
  if (text.length() > text_width * 2)
  {
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    text_width = 42;
    y = y - 3;
  }
  u8g2Fonts.println(text.substring(0, text_width));
  if (text.length() > text_width)
  {
    u8g2Fonts.setCursor(x, y + h + 15);
    String secondLine = text.substring(text_width);
    secondLine.trim(); // Remove any leading spaces
    u8g2Fonts.println(secondLine);
  }
}

void initDisplay()
{
  display.init(115200, true, 2, false);
  display.setRotation(1); // 3
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  u8g2Fonts.begin(display);                  // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  // u8g2Fonts.setFont(u8g2_font_helvB08_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}

void displayData()
{
  drawSections(); // Top line of the display
}

// Fetch
void fetchJson(const char *url)
{
  httpClient.useHTTP10(true);
  httpClient.begin(wifiClient, url);
  httpClient.GET();

  // Parse response
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, httpClient.getStream());

  String ret;

  if (doc.containsKey("switch:0"))
  {
    bool output = doc["switch:0"]["output"];
    ret = "HZG: ";
    ret += output ? "Ein" : "Aus";
    if (!ret.isEmpty())
    {
      drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.03, ret, CENTER, SFProTextBold32);
    }
  }
  if (!ret.isEmpty())
  {
    Serial.print("[FETCH]: Fetching... ");
    Serial.println(ret);
    printLocalTime();
  }

  // Disconnect
  httpClient.end();
}

void printLocalTime()
{
  printLocalTime(false);
}

void printLocalTime(boolean updateTime)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("[TIME]: Failed to obtain time");
    return;
  }
  Serial.print("[TIME]: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  char timeBuff[6];
  strftime(timeBuff, sizeof(timeBuff), "%H:%M", &timeinfo);
  String ret = timeBuff;

  if (updateTime)
  {
    drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 5.0, ret, CENTER, SFProTextBold55);
  }
  else
  {
    drawString(SCREEN_WIDTH / 2, 0, ret, CENTER, u8g2_font_helvB08_tf);
  }
}

void loopTime()
{
  time_t currentTime;
  time(&currentTime);
  struct tm *timeinfo = localtime(&currentTime);

  if (timeinfo->tm_min != previousMinute)
  {
    previousMinute = timeinfo->tm_min;
    Serial.print("[TIME]: ");
    Serial.println(ctime(&currentTime));
    printLocalTime(true);
  }
}

void onMqttMessage(char *topic, byte *payload, unsigned int len)
{
  /*Serial.print("Received message on topic: ");
  Serial.print(topic);
  Serial.print(", payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();*/

  // Parse the JSON data
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, len);
  serializeJsonPretty(doc, Serial);
  Serial.println();

  String ret;

  if (doc.containsKey("N"))
  {
    if (doc["N"] == "22")
    {
      if (doc.containsKey("T2") && doc.containsKey("T4"))
      {
        float t2_22 = doc["T2"];
        float t4_22 = doc["T4"];
        ret += String((t2_22 + t4_22) / 2, 1);
        ret += "° ";
      }
      if (doc.containsKey("H4"))
      {
        float h1_22 = doc["H4"];
        ret += String(h1_22, 1);
        ret += "%";
      }
      if (!ret.isEmpty())
      {
        drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.31, ret, CENTER, SFProTextBold32);
      }
    }
    if (doc["N"] == "87")
    {
      if (doc.containsKey("T1") && doc.containsKey("T2"))
      {
        float t1_87 = doc["T1"];
        float t2_87 = doc["T2"];
        ret += String((t1_87 + t2_87) / 2, 1);
        ret += "° ";
      }
      if (doc.containsKey("H1"))
      {
        float h1_87 = doc["H1"];
        ret += String(h1_87, 1);
        ret += "%";
      }
      if (!ret.isEmpty())
      {
        drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.31, ret, CENTER, SFProTextBold32);
      }
    }
    if (doc["N"] == "d5")
    {
      if (doc.containsKey("T1") && doc.containsKey("T2"))
      {
        float t1_d5 = doc["T1"];
        float t2_d5 = doc["T2"];
        ret += String((t1_d5 + t2_d5) / 2, 1);
        ret += "° ";
      }
      if (doc.containsKey("H1"))
      {
        float h1_d5 = doc["H1"];
        ret += String(h1_d5, 1);
        ret += "%";
      }
      if (!ret.isEmpty())
      {
        drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2.0, ret, CENTER, SFProTextBold32);
      }
    }
  }

  if (doc.containsKey("output"))
  {
    bool output = doc["output"];
    ret += "HZG: ";
    ret += output ? "Ein" : "Aus";
    if (!ret.isEmpty())
    {
      drawStringLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 1.03, ret, CENTER, SFProTextBold32);
    }
  }

  if (!ret.isEmpty())
  {
    Serial.print("[MQTT]: ");
    Serial.println(ret);
    printLocalTime();
  }
}