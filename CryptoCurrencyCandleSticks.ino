// #include <WiFi.h>
#include <WiFiClientSecure.h>
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <SimpleTimer.h>
#include <TimeLib.h>

#include "Free_Fonts.h"
#include "auth.h"

const char* bitbank_root_ca= \
     "-----BEGIN CERTIFICATE-----\n" \
     "MIIEDzCCAvegAwIBAgIBADANBgkqhkiG9w0BAQUFADBoMQswCQYDVQQGEwJVUzEl\n" \
     "MCMGA1UEChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjEyMDAGA1UECxMp\n" \
     "U3RhcmZpZWxkIENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMDQw\n" \
     "NjI5MTczOTE2WhcNMzQwNjI5MTczOTE2WjBoMQswCQYDVQQGEwJVUzElMCMGA1UE\n" \
     "ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjEyMDAGA1UECxMpU3RhcmZp\n" \
     "ZWxkIENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwggEgMA0GCSqGSIb3\n" \
     "DQEBAQUAA4IBDQAwggEIAoIBAQC3Msj+6XGmBIWtDBFk385N78gDGIc/oav7PKaf\n" \
     "8MOh2tTYbitTkPskpD6E8J7oX+zlJ0T1KKY/e97gKvDIr1MvnsoFAZMej2YcOadN\n" \
     "+lq2cwQlZut3f+dZxkqZJRRU6ybH838Z1TBwj6+wRir/resp7defqgSHo9T5iaU0\n" \
     "X9tDkYI22WY8sbi5gv2cOj4QyDvvBmVmepsZGD3/cVE8MC5fvj13c7JdBmzDI1aa\n" \
     "K4UmkhynArPkPw2vCHmCuDY96pzTNbO8acr1zJ3o/WSNF4Azbl5KXZnJHoe0nRrA\n" \
     "1W4TNSNe35tfPe/W93bC6j67eA0cQmdrBNj41tpvi/JEoAGrAgEDo4HFMIHCMB0G\n" \
     "A1UdDgQWBBS/X7fRzt0fhvRbVazc1xDCDqmI5zCBkgYDVR0jBIGKMIGHgBS/X7fR\n" \
     "zt0fhvRbVazc1xDCDqmI56FspGowaDELMAkGA1UEBhMCVVMxJTAjBgNVBAoTHFN0\n" \
     "YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4xMjAwBgNVBAsTKVN0YXJmaWVsZCBD\n" \
     "bGFzcyAyIENlcnRpZmljYXRpb24gQXV0aG9yaXR5ggEAMAwGA1UdEwQFMAMBAf8w\n" \
     "DQYJKoZIhvcNAQEFBQADggEBAAWdP4id0ckaVaGsafPzWdqbAYcaT1epoXkJKtv3\n" \
     "L7IezMdeatiDh6GX70k1PncGQVhiv45YuApnP+yz3SFmH8lU+nLMPUxA2IGvd56D\n" \
     "eruix/U0F47ZEUD0/CwqTRV/p2JdLiXTAAsgGh1o+Re49L2L7ShZ3U0WixeDyLJl\n" \
     "xy16paq8U4Zt3VekyvggQQto8PT7dL5WXXp59fkdheMtlb71cZBDzI0fmgAKhynp\n" \
     "VSJYACPq4xJDKVtHCN2MQWplBqjlIapBtJUhlbl90TSrE9atvNziPTnNvT51cKEY\n" \
     "WQPJIrSPnNVeKtelttQKbfi3QBFGmh95DmK/D5fs4C8fF5Q=\n" \
     "-----END CERTIFICATE-----\n";

#define HORIZONTAL_RESOLUTION 240 // width of TTGO-T-display

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

WiFiClientSecure client;

SimpleTimer timer;

#define SERVER "public.bitbank.cc"

/*
  function to convert unsigned integer to comma separated numerical string
*/

void
itocsa(char *buf, unsigned bufsiz, unsigned n)
{
  char *p = buf;
  unsigned len, i;

  for (i = 1 ; i < n ; i *= 1000);
  i /= 1000;
  
  snprintf(p, bufsiz, "%d", n / i);
  n = n % i;
  len = strlen(p);
  p += len;
  bufsiz -= len;
  i /= 1000;
  while (0 < i) {
    snprintf(p, bufsiz, ",%03d", n / i);
    n = n % i;
    p += 4;
    bufsiz -= 4;
    i /= 1000;
  }
}

#define CANDLESTICK_WIDTH "5min"

#define BUTTON1 35 // GPIO35
#define BUTTON2 0 // GPIO0

#define STICK_WIDTH 3 // width of a candle stick
#define NUM_STICKS (HORIZONTAL_RESOLUTION / STICK_WIDTH)

class Currency {
public:
  char *name, *pair;
  unsigned priceline;
  struct {
    unsigned startPrice, endPrice, lowestPrice, highestPrice;
    float relative;
    unsigned long timeStamp;
  } candlesticks[NUM_STICKS];
  unsigned todayshigh = 0;
  unsigned todayslow = 0;
  unsigned prevPrice = 0;
  float prevPriceAgainstOtherCurrency = 0.0;
  int prevPricePixel;
  unsigned prevTimeStamp = 0;

  Currency (char *na, char *pa, unsigned pl) {
    name = na;
    pair = pa;
    priceline = pl;
  }

  unsigned obtainLastPrice(unsigned long *t);
  void obtainSticks(unsigned n, unsigned long t);
  void ShowCurrentPrice();
  void SwitchCurrency();
  
} currencies[2] = {{"ETH", "eth_jpy", 5000}, {"BTC", "btc_jpy", 100000}};

static unsigned cIndex = 0; // ETH by default.

void
Currency::obtainSticks(unsigned n, unsigned long t)
{
  // Serial.println("\nobtainSticks called.");

  while (0 < n) {
    if (!client.connect(SERVER, 443)) {
      Serial.println("\nConnection failed!");
      return;
    }
    else {
      Serial.println("\nConnected to http server.");

      // Make another HTTP request:
      client.print("GET https://" SERVER "/");
      client.print(pair);
      client.print("/candlestick/" CANDLESTICK_WIDTH "/");
      {
	char yyyymmdd[9]; // 9 for "yyyymmdd"
	sprintf(yyyymmdd, "%04d%02d%02d", year(t), month(t), day(t));
	client.print(yyyymmdd);
	client.println(" HTTP/1.0");
      }
      client.println("Host: " SERVER);
      client.println("Connection: close");
      client.println();
    
// #define SHOW_HTTPHEADERS
    
      while (client.connected()) {
	String line = client.readStringUntil('\n');
#ifdef SHOW_HTTPHEADERS
	Serial.println(line); // echo response headers
#endif
	if (line == "\r") {
	  // Serial.println("headers received");
	  break;
	}
      }

      DynamicJsonDocument doc(50000);
      DeserializationError error = deserializeJson(doc, client);
      client.stop();

      if (error) {
	Serial.print(F("deserializeJson() failed: "));
	Serial.println(error.f_str());
      }
      else {
	int success = doc["success"]; // 1

	JsonArray ohlcv = doc["data"]["candlestick"][0]["ohlcv"];
	unsigned numSticks = ohlcv.size();

	// Serial.print("Success = ");
	// Serial.println(success);

	Serial.print("Number of sticks = ");
	Serial.print(numSticks);
	if (n <= numSticks) { // enough sticks obtained
	  Serial.println(" (enough)");
	  for (unsigned i = 0 ; i < n ; i++) {
	    // copy the last n data from JSON
	    unsigned ohlcvIndex = i + numSticks - n;
	    candlesticks[i].startPrice = ohlcv[ohlcvIndex][0].as<unsigned>();
	    candlesticks[i].highestPrice = ohlcv[ohlcvIndex][1].as<unsigned>();
	    candlesticks[i].lowestPrice = ohlcv[ohlcvIndex][2].as<unsigned>();
	    candlesticks[i].endPrice = ohlcv[ohlcvIndex][3].as<unsigned>();
	    candlesticks[i].timeStamp =
	      (unsigned long)(ohlcv[ohlcvIndex][5].as<unsigned long long>() / 1000);
	    unsigned long aho = (unsigned long)ohlcv[ohlcvIndex][5].as<unsigned long long>();
	  }
	  n = 0;
	}
	else {
	  Serial.println(" (not enough)");
	  for (unsigned i = 0 ; i < numSticks ; i++) {
	    // copy the all n data from JSON
	    unsigned stickIndex = i + n - numSticks;
	    candlesticks[stickIndex].startPrice = ohlcv[i][0].as<unsigned>();
	    candlesticks[stickIndex].highestPrice = ohlcv[i][1].as<unsigned>();
	    candlesticks[stickIndex].lowestPrice = ohlcv[i][2].as<unsigned>();
	    candlesticks[stickIndex].endPrice = ohlcv[i][3].as<unsigned>();
	    candlesticks[stickIndex].timeStamp =
	      (unsigned long)(ohlcv[i][5].as<unsigned long long>() / 1000);
	  }
	  n -= numSticks; // to fill remaining slots
	  t -= 24 * 60 * 60; // for data one day before
	}
      }
    }
  }
}

// Output timestamp to serial terminal
void
SerialPrintTimestamp(unsigned t, unsigned tz)
{
  Serial.print("timestamp = ");
  Serial.print(t);
  t += tz;
  Serial.print(" = ");
  Serial.print(year(t));
  Serial.print("/");
  Serial.print(month(t));
  Serial.print("/");
  Serial.print(day(t));
  Serial.print(" ");
  Serial.print(hour(t));
  Serial.print(":");
  if (minute(t) < 10) {
    Serial.print("0");
  }
  Serial.print(minute(t));
  Serial.print(":");
  if (second(t) < 10) {
    Serial.print("0");
  }
  Serial.print(second(t));
  Serial.println(" JST");
}

unsigned
Currency::obtainLastPrice(unsigned long *t)
{
  unsigned retval = 0;
  unsigned long timestamp = 0;
  if (!client.connect(SERVER, 443)) {
    Serial.println("Connection failed!");
  }
  else {
    Serial.println("Connected to http server.");
    // Make a HTTP request:
    client.print("GET https://" SERVER "/");
    client.print(pair);
    client.println("/ticker HTTP/1.0");
    client.println("Host: " SERVER);
    client.println("Connection: close");
    client.println();

    while (client.connected()) {
      String line = client.readStringUntil('\n');
#ifdef SHOW_HTTPHEADERS
      Serial.println(line); // echo response headers
#endif
      if (line == "\r") {
        // Serial.println("headers received");
        break;
      }
    }

    // Allocate the JSON document
    // Use arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<256> doc;

    // Parse JSON object
    DeserializationError error = deserializeJson(doc, client);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
    else {
      // Extract values
      if (doc["success"]) {

	// Obtaining time stamp of ticker response and use it as the current time,
	// instead of obtaining current time by NTP.
	timestamp = (unsigned long)(doc["data"]["timestamp"].as<unsigned long long>() / 1000);

#define TIMEZONE (9 * 60 * 60)
  
	SerialPrintTimestamp(timestamp, TIMEZONE);
	retval = (unsigned)doc["data"]["last"].as<long>();
#if 0
	if (prevTimeStamp == 0 || // for the first access, or ...
	    /// caution!!!
	    day(timestamp + TIMEZONE) != day(prevTimeStamp + TIMEZONE)) { // day changed
	  todayshigh = todayslow = retval;
	}
#endif
      }
    }
    client.stop();
  }
  if (t) {
    *t = timestamp;
  }
  // prevTimeStamp = timestamp;
  return retval;
}

#define ALERT_INTERVAL 500 // msec    
#define ALERT_DURATION (40 /* sec */ * (1000 / ALERT_INTERVAL)) // times ALERT_INTERVAL
static unsigned alertDuration = 0;
static unsigned long prevCandlestickTimestamp = 0;
static unsigned long prevTimestamp = 0;
static bool changeTriggered = false;

#define PRICEBUFSIZE 24

#define PRICE_MIN_X 5
#define PRICE_PAD_X 3
#define PRICE_PAD_Y 10
#define BORDER_WIDTH 2

#define PRICE_FONT FF44 // 20, 24, (36,) 44 are candidates for a price font
  
#define OTHER_CURRENCY_BASE_VALUE_FONT 4
#define TFT_DOWNRED 0xC000 /* 127,   0,   0 */
#define TFT_UPGREEN 0x0600 /*   0, 128,   0 */
// #define TFT_RED         0xF800      /* 255,   0,   0 */
// #define TFT_GREEN       0x07E0      /*   0, 255,   0 */

#define ONEMINUTE_THRESHOLD 1 // per cent
#define FIVEMINUTES_THRESHOLD 1 // per cent
#define PADX 5
#define PADY 5
#define MESGSIZE 64
#define ALERT_BLACK_DURATION 200 // msec

void
ShowLastPrice(char *buf, int lastPricePixel, unsigned priceColor)
{
  int textY = lastPricePixel - (tft.fontHeight(GFXFF) / 2);
  if (textY < 0) {
    textY = 0;
  }
  else if (tft.height() - tft.fontHeight(GFXFF) + PRICE_PAD_Y < textY) {
    textY = tft.height() - tft.fontHeight(GFXFF) + PRICE_PAD_Y;
  }
  tft.setTextColor(TFT_BLACK);
  tft.drawString(buf, - BORDER_WIDTH, textY - BORDER_WIDTH, GFXFF);
  tft.drawString(buf, BORDER_WIDTH, textY + BORDER_WIDTH, GFXFF);
  tft.setTextColor(priceColor);
  tft.drawString(buf, 0, textY, GFXFF);
}

void
ShowPriceAgainstOtherCurrency(char *buf, int lastPricePixel, unsigned priceColor)
{
  int textY = lastPricePixel - (tft.fontHeight(GFXFF) / 2);
  if (textY < 0) {
    textY = 0;
  }
  else if (tft.height() - tft.fontHeight(GFXFF) + PRICE_PAD_Y < textY) {
    textY = tft.height() - tft.fontHeight(GFXFF) + PRICE_PAD_Y;
  }
  if (lastPricePixel < tft.height() / 2) {
    textY += tft.fontHeight(GFXFF);
  }
  else {
    textY -= tft.fontHeight(OTHER_CURRENCY_BASE_VALUE_FONT);
  }
  tft.setTextColor(TFT_BLACK);
  tft.drawString(buf, - BORDER_WIDTH, textY - BORDER_WIDTH, OTHER_CURRENCY_BASE_VALUE_FONT);
  tft.drawString(buf, BORDER_WIDTH, textY + BORDER_WIDTH, OTHER_CURRENCY_BASE_VALUE_FONT);
  tft.setTextColor(priceColor);
  tft.drawString(buf, PADX, textY, OTHER_CURRENCY_BASE_VALUE_FONT);
#define BASE_DIFF 4
  tft.drawString(currencies[cIndex == 0 ? 1 : 0].name,
		 PADX + tft.textWidth(buf, OTHER_CURRENCY_BASE_VALUE_FONT),
		 textY + tft.fontHeight(OTHER_CURRENCY_BASE_VALUE_FONT) - tft.fontHeight(2) - BASE_DIFF, 2);
}

void
ShowCurrencyName(char *buf, int lastPricePixel)
{
  int textY;
  if (lastPricePixel < tft.height() / 2) {
    textY = tft.height() - tft.fontHeight(2) * 2;
  }
  else {
    textY = tft.fontHeight(2);
  }
  tft.drawString(buf, tft.width() - tft.textWidth(buf, 2) - 1, textY, 2);
}

class alert {
public:
  void setBackColor(unsigned color) {
    alertbgcolor = color;
  }
  char mesgbuf[MESGSIZE];
  int alertId;
  void setMesg1(char *s) {
    alertmesg1 = s;
  }
  void setMesg2(char *s) {
    alertmesg2 = s;
  }
  void setLastPrice(unsigned p) {
    lastPrice = p;
  }
  void beginAlert() {
    tft.fillScreen(alertbgcolor);
    tft.setTextColor(TFT_WHITE);
    itocsa(buf, PRICEBUFSIZE, lastPrice);
    textColor = TFT_WHITE;
    showAlert();
  }
  void flashAlert() {
    // tft.invertDisplay(false);
    // invert text color
    textColor = (textColor == TFT_WHITE) ? TFT_BLACK : TFT_WHITE;
    tft.setTextColor(textColor);
    showAlert();
  }
private:
  void showAlert() {
    tft.drawString(alertmesg1, PADX, PADY, 4);
    tft.drawString(alertmesg2, PADX, tft.fontHeight(4) + PADY, 4);
    tft.drawString(buf,
		   tft.width() / 2 - tft.textWidth(buf, GFXFF) / 2,
		   tft.height() - tft.fontHeight(GFXFF), GFXFF);
  }
  char buf[PRICEBUFSIZE];
  char *alertmesg1;
  char *alertmesg2;
  unsigned textColor = TFT_WHITE;
  unsigned alertbgcolor = TFT_DOWNRED; // alert color by default
  unsigned lastPrice;
} Alert;

void
Currency::ShowCurrentPrice()
{
  unsigned long t; // for current time
  char buf[PRICEBUFSIZE], buf2[PRICEBUFSIZE];
  unsigned lastPrice = 0, lastPriceOfOtherCurrency = 0;
  float lastPriceAgainstOtherCurrency = 0.0;
  int lastPricePixel = 0;
  unsigned stickColor = TFT_DOWNRED, priceColor = TFT_GREEN;

  if (0 < alertDuration) {
    return;
  }

  client.setCACert(bitbank_root_ca);

  if (WiFi.status() != WL_CONNECTED) {
    //WiFi.disconnect();

    // Grey out the price display
    itocsa(buf, PRICEBUFSIZE, prevPrice);
    ShowLastPrice(buf, prevPricePixel, TFT_DARKGREY); // make the price grey
    snprintf(buf2, PRICEBUFSIZE, "%.5f", lastPriceAgainstOtherCurrency);
    ShowPriceAgainstOtherCurrency(buf2, prevPricePixel, TFT_DARKGREY);

#define CONNECTION_LOST "Reconnecting ..."
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawString(CONNECTION_LOST,
		   tft.width() / 2 - tft.textWidth(CONNECTION_LOST, 4) / 2,
		   tft.height() / 2 - tft.fontHeight(4) / 2, 4);
    
    Serial.print("WiFi connection was lost.\nAttempting to reconnect to WiFi ");
    // WiFi.begin(WIFIAP, WIFIPW);
    WiFi.reconnect();

#define WIFI_ATTEMPT_LIMIT 45
    unsigned i;
    
    // attempt to connect to Wifi network:
    for(i = 0 ; i < WIFI_ATTEMPT_LIMIT && WiFi.status() != WL_CONNECTED ; i++) {
      Serial.print(".");
      // wait 1 second for re-trying
      delay(1000);
    }
    if (i < WIFI_ATTEMPT_LIMIT) {
      Serial.println(" Connected");
    }
    else {
      Serial.println(" Failed to recoonect");
      return;
    }
  }
  
  Serial.println("\n==== Starting connection to server...");

  lastPriceOfOtherCurrency = currencies[cIndex == 0 ? 1 : 0].obtainLastPrice(&t);
  lastPrice = obtainLastPrice(&t);
  lastPriceAgainstOtherCurrency = (float)lastPrice / (float)lastPriceOfOtherCurrency;
  if (day(t + TIMEZONE) != day(prevTimestamp + TIMEZONE)) {
    prevTimestamp = t;
    todayshigh = todayslow = 0;
  }
  itocsa(buf, PRICEBUFSIZE, lastPrice);
  Serial.print("last price = ");
  Serial.println(buf);
  // obtaining today's low and today's high
  if (0 < lastPrice) {
    /// caution!!!! 
    obtainSticks(NUM_STICKS, t);
    if (todayshigh == 0) {
      unsigned today = day(t + TIMEZONE);
      for (unsigned i = 0 ; i < NUM_STICKS ; i++) {
	if (day(candlesticks[i].timeStamp + TIMEZONE) == today) {
	  if (todayshigh == 0) {
	    todayshigh = candlesticks[i].highestPrice;
	    todayslow = candlesticks[i].lowestPrice;
	  }
	  else if (todayshigh < candlesticks[i].highestPrice) {
	    todayshigh = candlesticks[i].highestPrice;
	  }
	  else if (candlesticks[i].lowestPrice < todayslow) {
	    todayslow = candlesticks[i].lowestPrice;
	  }
	}
      }
    }
  }

  // obtaining chart's high and low
  unsigned lowest = candlesticks[0].lowestPrice; // chart's low
  unsigned highest = candlesticks[0].highestPrice; // chart's high
  unsigned prevHour = hour(candlesticks[0].timeStamp + TIMEZONE);
  for (unsigned i = 1 ; i < NUM_STICKS ; i++) {
    if (candlesticks[i].lowestPrice < lowest) {
      lowest = candlesticks[i].lowestPrice;
    }
    if (highest < candlesticks[i].highestPrice) {
      highest = candlesticks[i].highestPrice;
    }
  }
  Serial.print("\ntoday = ");
  Serial.print(todayslow);
  Serial.print(" - ");
  Serial.println(todayshigh);
  Serial.print("chart = ");
  Serial.print(lowest);
  Serial.print(" - ");
  Serial.println(highest);

  SerialPrintTimestamp(candlesticks[NUM_STICKS - 1].timeStamp, TIMEZONE);

  Serial.print("Vertical price line in pixel = ");
  Serial.println(map(candlesticks[NUM_STICKS - 1].endPrice, lowest, highest, tft.height(), 0));

  // check events...
  // last event has the highest priority

  // today's high or low
  if (lastPrice < todayslow) {
    todayslow = lastPrice;

    Alert.setMesg1("Updated");
    Alert.setMesg2("today's low");
    Alert.setBackColor(TFT_DOWNRED);
    alertDuration = ALERT_DURATION;
  }
  else if (todayshigh < lastPrice) {
    todayshigh = lastPrice;

    Alert.setMesg1("Updated");
    Alert.setMesg2("today's high");
    Alert.setBackColor(TFT_UPGREEN);
    alertDuration = ALERT_DURATION;
  }

  // not event but set today's high and low properly
  if (todayshigh < candlesticks[NUM_STICKS - 1].highestPrice) {
    todayshigh = candlesticks[NUM_STICKS - 1].highestPrice;
  }
  if (candlesticks[NUM_STICKS - 1].lowestPrice < todayslow) {
    todayslow = candlesticks[NUM_STICKS - 1].lowestPrice;
  }

  // five minutes significant price change
  if (prevCandlestickTimestamp != candlesticks[NUM_STICKS - 1].timeStamp &&
	   FIVEMINUTES_THRESHOLD <=
	   abs((long)candlesticks[NUM_STICKS -1].startPrice - (long)candlesticks[NUM_STICKS -1].endPrice)
	   * 100 / (long)candlesticks[NUM_STICKS - 1].startPrice) {
    prevCandlestickTimestamp = candlesticks[NUM_STICKS - 1].timeStamp;
    if (candlesticks[NUM_STICKS - 1].startPrice < candlesticks[NUM_STICKS - 1].endPrice) {
      Alert.setBackColor(TFT_UPGREEN);
      snprintf(Alert.mesgbuf, MESGSIZE, "%.1f%% up within",
	       (float)(candlesticks[NUM_STICKS -1 ].endPrice - candlesticks[NUM_STICKS - 1].startPrice) * 100.0
	       / (float)candlesticks[NUM_STICKS - 1].startPrice);
    }
    else {
      Alert.setBackColor(TFT_DOWNRED);
      snprintf(Alert.mesgbuf, MESGSIZE, "%.1f%% down within",
	       (float)(candlesticks[NUM_STICKS -1 ].startPrice - candlesticks[NUM_STICKS - 1].endPrice) * 100.0
	       / (float)candlesticks[NUM_STICKS - 1].startPrice); 
    }
    Alert.setMesg1(Alert.mesgbuf);
    Alert.setMesg2("5 minutes.");
    alertDuration = ALERT_DURATION;
  }

  // in a minute significant price change
  if (0 < prevPrice && ONEMINUTE_THRESHOLD <= abs((long)lastPrice - (long)prevPrice) * 100 / (long)prevPrice) {
    if (prevPrice < lastPrice) {
      snprintf(Alert.mesgbuf, MESGSIZE, "%.1f%% up within",
	       (float)(lastPrice - prevPrice) * 100.0 / (float)prevPrice);
      Alert.setBackColor(TFT_UPGREEN);
    }
    else {
      snprintf(Alert.mesgbuf, MESGSIZE, "%.1f%% down within",
	       (float)(prevPrice - lastPrice) * 100.0 / (float)prevPrice);
      Alert.setBackColor(TFT_DOWNRED);
    }
    Alert.setMesg1(Alert.mesgbuf);
    Alert.setMesg2("a minute.");
    alertDuration = ALERT_DURATION;
  }

  if (0 < alertDuration) {
    Alert.setLastPrice(lastPrice);
    Alert.beginAlert();
    Alert.alertId = timer.setTimer(ALERT_INTERVAL, alertProc, ALERT_DURATION * (1000 / ALERT_INTERVAL) + 1);
    prevPrice = lastPrice;
  }
  else {
    // show the chart
    tft.fillScreen(TFT_BLACK);

#define TFT_DARKBLUE        0x000F      /*   0,   0, 127 */

    // draw horizontal price lines
    if (lastPrice < prevPrice) {
      priceColor = TFT_RED;
    }
    prevPrice = lastPrice;
    for (unsigned i = lowest / priceline + 1 ; i * priceline < highest ; i++) {
      unsigned y = map(i * priceline, lowest, highest, tft.height(), 0);
      tft.drawFastHLine(0, y, tft.width(), TFT_DARKBLUE);
    }

    // get the position to draw last price
    prevPricePixel = lastPricePixel = map(lastPrice, lowest, highest, tft.height(), 0);

    // draw candlesticks
    for (unsigned i = 0 ; i < NUM_STICKS ; i++) {
      // draw vertical hour line
      unsigned curHour = hour(candlesticks[i].timeStamp + TIMEZONE);
      if (curHour != prevHour) {
	prevHour = curHour;
	tft.drawFastVLine(i * 3 + 1, 0, tft.height(), TFT_DARKBLUE);
	if (curHour % 3 == 0) {
	  if (i * 3 - 5 < tft.width() - tft.textWidth(buf, 2) - 10) {
	    // if we have enough space around vertical line, draw the time
	    unsigned textY = 0;

	    if (lastPricePixel < tft.height() / 2) {
	      textY = tft.height() - tft.fontHeight(2);
	    }
	    tft.setTextColor(TFT_WHITE);
	    tft.drawNumber(curHour, i * 3 - 5, textY, 2);
	  }
	}
      }

      // draw candlesticks
      int lowestPixel, highestPixel, lowPixel, pixelHeight;
    
      lowestPixel = map(candlesticks[i].lowestPrice, lowest, highest, tft.height(), 0);
      highestPixel = map(candlesticks[i].highestPrice, lowest, highest, tft.height(), 0);

      if (candlesticks[i].startPrice < candlesticks[i].endPrice) {
	lowPixel = map(candlesticks[i].endPrice, lowest, highest, tft.height(), 0);
	pixelHeight = map(candlesticks[i].startPrice, lowest, highest, tft.height(), 0)
	  - lowPixel;
	stickColor = TFT_UPGREEN;
      }
      else {
	lowPixel = map(candlesticks[i].startPrice, lowest, highest, tft.height(), 0);
	pixelHeight = map(candlesticks[i].endPrice, lowest, highest, tft.height(), 0)
	  - lowPixel;
	stickColor = TFT_DOWNRED;
      }

      tft.drawFastVLine(i * 3 + 1, highestPixel, lowestPixel - highestPixel, TFT_LIGHTGREY);
      tft.fillRect(i * 3, highestPixel, 3, pixelHeight, stickColor);
    }

    // draw price horizontal line
    unsigned stringWidth = tft.textWidth(buf, GFXFF) + PRICE_PAD_X;
    tft.drawFastHLine(0, lastPricePixel, PRICE_MIN_X, priceColor);
    tft.drawFastHLine(stringWidth, lastPricePixel, tft.width() - stringWidth, priceColor);
    // tft.drawFastHLine(0, lastPricePixel, tft.width(), priceColor);

    // draw highest and lowest price in the chart
    itocsa(buf, PRICEBUFSIZE, highest);
    tft.setTextColor(TFT_BLACK);
    tft.drawString(buf, tft.width() - tft.textWidth(buf, 2) - 2, -1, 2);
    tft.drawString(buf, tft.width() - tft.textWidth(buf, 2), 1, 2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(buf, tft.width() - tft.textWidth(buf, 2) - 1, 0, 2);

    itocsa(buf, PRICEBUFSIZE, lowest);
    tft.setTextColor(TFT_BLACK);
    tft.drawString(buf,
		   tft.width() - tft.textWidth(buf, 2) - 2,
		   tft.height() - tft.fontHeight(2) - 1, 2);
    tft.drawString(buf,
		   tft.width() - tft.textWidth(buf, 2),
		   tft.height() - tft.fontHeight(2) + 1, 2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(buf,
		   tft.width() - tft.textWidth(buf, 2) - 1,
		   tft.height() - tft.fontHeight(2), 2);

    // show currency name
    ShowCurrencyName(name, lastPricePixel);
  
    // draw last price
    itocsa(buf, PRICEBUFSIZE, lastPrice);

    // show the current cryptocurrency price on TTGO-T-display

    ShowLastPrice(buf, lastPricePixel, priceColor);
    snprintf(buf2, PRICEBUFSIZE, "%.5f", lastPriceAgainstOtherCurrency);
    ShowPriceAgainstOtherCurrency(buf2, prevPricePixel,
				  lastPriceAgainstOtherCurrency < prevPriceAgainstOtherCurrency ? TFT_RED: TFT_GREEN);
    prevPriceAgainstOtherCurrency = lastPriceAgainstOtherCurrency;
  }
}

void
alertProc()
{
  if (0 < alertDuration) {
    Alert.flashAlert();
    alertDuration--;
    if (alertDuration == 0) {
      timer.deleteTimer(Alert.alertId);
      currencies[cIndex].ShowCurrentPrice();
    }
  }
}

void buttonEventProc()
{
  changeTriggered = true;
}

void Currency::SwitchCurrency()
{
  char buf[PRICEBUFSIZE], buf2[PRICEBUFSIZE];
  
  // clear global variables..
  prevCandlestickTimestamp = prevTimestamp = 0;
  todayshigh = todayslow = 0;
  alertDuration = 0;
    
  Serial.println("\nChange triggered.");

  // Grey out the price display
  itocsa(buf, PRICEBUFSIZE, prevPrice);
  ShowLastPrice(buf, prevPricePixel, TFT_DARKGREY); // make the price grey
  snprintf(buf2, PRICEBUFSIZE, "%.5f", prevPriceAgainstOtherCurrency);
  ShowPriceAgainstOtherCurrency(buf2, prevPricePixel, TFT_DARKGREY);

#define SWITCHING "Switching ..."
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString(SWITCHING,
		 tft.width() / 2 - tft.textWidth(SWITCHING, 4) / 2,
		 tft.height() / 2 - tft.fontHeight(4) / 2, 4);
  
  prevPrice = prevPricePixel = 0;
  prevPriceAgainstOtherCurrency = 0.0;
  cIndex = (cIndex == 0) ? 1 : 0;
  
  currencies[cIndex].ShowCurrentPrice();
}

void SecProc()
{
  if (changeTriggered) {
    changeTriggered = false;
    currencies[cIndex].SwitchCurrency();
  }
}

void
_ShowCurrentPrice()
{
  currencies[cIndex].ShowCurrentPrice();
}

void setup()
{
  Serial.begin(115200);
  
  Serial.println("");
  Serial.println("CryptoCurrency candlestick chart display terminal started.");

  // initialize TFT screen
  tft.init(); // equivalent to tft.begin();
  tft.setRotation(1); // set it to 1 or 3 for landscape resolution
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextPadding(PADX); // seems no effect by this line.
  tft.drawString("Connecting ...",
		 PADX, tft.height() / 2 - tft.fontHeight(4) / 2, 4);
  tft.setTextSize(1);
  tft.setFreeFont(PRICE_FONT); // Select a font for last price display

  Serial.print("Attempting to connect to WiFi ");
  WiFi.begin(WIFIAP, WIFIPW);
  //  WiFi.disconnect();
  //WiFi.stop();

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }
  Serial.println(" Connected");

  currencies[cIndex].ShowCurrentPrice();

  Serial.println("");

  attachInterrupt(digitalPinToInterrupt(BUTTON1), buttonEventProc, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON2), buttonEventProc, FALLING);
  
  timer.setInterval(1000, SecProc);
  timer.setInterval(60000, _ShowCurrentPrice);
}

void loop()
{
  timer.run();
}
