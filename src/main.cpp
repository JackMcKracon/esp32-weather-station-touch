// SPDX-FileCopyrightText: 2023 ThingPulse Ltd., https://thingpulse.com
// SPDX-License-Identifier: MIT

#include <LittleFS.h>

#include <OpenFontRender.h>
#include <TJpg_Decoder.h>

#include "fonts/open-sans.h"
#include "GfxUi.h"

#include <JsonListener.h>
#include <OpenWeatherMapCurrent.h>
#include <OpenWeatherMapForecast.h>
#include <SunMoonCalc.h>
#include <TaskScheduler.h>

#include "connectivity.h"
#include "display.h"
#include "persistence.h"
#include "settings.h"
#include "util.h"
#include <FT6236.h>

#include <string>


// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------
OpenFontRender ofr;
FT6236 ts = FT6236(TFT_HEIGHT, TFT_WIDTH);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite timeSprite = TFT_eSprite(&tft);
GfxUi ui = GfxUi(&tft, &ofr);

// time management variables
int updateIntervalMillis = UPDATE_INTERVAL_MINUTES * 60 * 1000;
unsigned long lastTimeSyncMillis = 0;
unsigned long lastUpdateMillis = 0;

const int16_t centerWidth = tft.width() / 2;

int pageId = 0;
const unsigned int lastPage = 1;
const unsigned int firstPage =  -1;

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapForecastData forecasts[NUMBER_OF_FORECASTS];

Scheduler scheduler;




// ----------------------------------------------------------------------------
// Function prototypes (declarations)
// ----------------------------------------------------------------------------
void drawAstro();
void drawCurrentWeather();
void drawForecast();
void drawProgress(const char *text, int8_t percentage);
void drawTimeAndDate();
String getWeatherIconName(uint16_t id, bool today);
void initJpegDecoder();
void initOpenFontRender();
bool pushImageToTft(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
void syncTime();
void repaint();
void repaint2();
void drawWeather();
void updateData(boolean updateProgressBar);


Task clockTask(1000, TASK_FOREVER, &drawTimeAndDate);

class button {
public:
    enum buttonSign{
      NONE,
      VIERECK,
      DREIECK
    };

    button(int theBx, int theBy, int theBH, int theBW, const char* theInhalt, buttonSign theZeichen, int theZeichenGroesse) 
        : Bx(theBx), 
          By(theBy), 
          BH(theBH), 
          BW(theBW), 
          ZeichenGroesse(theZeichenGroesse), 
          zeichen(theZeichen) 
    {
        // Copy the content into the Inhalt array
        strncpy(Inhalt, theInhalt, sizeof(Inhalt) - 1);
        Inhalt[sizeof(Inhalt) - 1] = '\0'; // Ensure null-termination

        // Calculate Zx and Zy
        Zx = this->Bx + (this->BW - (this->ZeichenGroesse / 2));
        Zy = this->By + (this->BH - (this->ZeichenGroesse / 2));
    }

    
    
    
    boolean Button() {
      //boolean value = 1;
      TS_Point p = ts.getPoint();
      return (p.x <= Bx + BW && p.x >= Bx  && p.y <= By + BH && p.y >= By);
    }

    void Kasten() {
      tft.drawRoundRect(Bx, By, BW, BH, 20, TFT_WHITE);
    }

    void Bezeichnung() {
      tft.setTextSize(2);
      int Txt = Bx + (BW / 2);
      int Tyt = By + (BH / 2);
      int Tx = Txt - (tft.textWidth(Inhalt)/ 2);
      int Ty = Tyt - (8*tft.textsize / 2);
      tft.setTextSize(2);
      tft.drawString(Inhalt, Tx, Ty);
    }

    void Zeichen() {
      if (zeichen == VIERECK) {
        tft.drawRect(Zx, Zy, ZeichenGroesse, ZeichenGroesse, TFT_WHITE);
      }
      else if (zeichen == DREIECK) {
        int Zx1 = Zx;
        int Zy1 = Zy;

      //tft.drawTriangle()
      }
    }
  private:
    int Bx;
    int By;
    int BH;
    int BW;
    char Inhalt[128];
    buttonSign zeichen;  
    int ZeichenGroesse;
    int Zx;
    int Zy;
};

 button buttonone(200, 200, 100, 100,"",button::NONE,0);
 button buttontwo(0, 200, 100, 100,"",button::NONE,0);
 button buttonDeckone(40, 45, 100, 100,"",button::VIERECK,0);
 button buttonDecktwo(180, 190, 100, 100,"",button::DREIECK,0);
 button buttonDeckthree(40, 335, 100, 100,"",button::VIERECK,0);
 button buttonDeckfour(180, 45, 100, 100,"",button::DREIECK,0);
 button buttonDeckfive(40, 190, 100, 100,"",button::VIERECK,0);
 button buttonDecksix(180, 335, 100, 100,"",button::VIERECK,0);
// ----------------------------------------------------------------------------
// setup() & loop()
// ----------------------------------------------------------------------------
void setup(void) {


  Serial.begin(115200);
  delay(1000);

  logBanner();
  logMemoryStats();

  initJpegDecoder();
  initTouchScreen(&ts);
  initTft(&tft);
  timeSprite.createSprite(timeSpritePos.width, timeSpritePos.height);
  logDisplayDebugInfo(&tft);

  initFileSystem();
  initOpenFontRender();

  scheduler.init();
  scheduler.addTask(clockTask);
  clockTask.enable();


  //begin(40, SDA, SCL) for custom pins
  if (!ts.begin(40)) //40 in this case represents the sensitivity. Try higer or lower for better response. 
  {
    Serial.println("Unable to start the capacitive touchscreen.");
  }

  

}

void loop(void) {



  
  // update if
  // - never (successfully) updated before OR
  // - last sync too far back
  if (lastTimeSyncMillis == 0 ||
      lastUpdateMillis == 0 ||
      (millis() - lastUpdateMillis) > updateIntervalMillis) {
    int pageideAlt = pageId;
    pageId = 0;
    repaint();
    pageId = pageideAlt;
  }
  if (ts.touched())
    {
      log_i("Page Id before click : %d",pageId);
      TS_Point p = ts.getPoint();
      log_i("Klick: %d,%d", p.x, p.y);
      if (pageId != -1){
        if (buttonone.Button()) {
          log_i("Butten 1");
          pageId += 1;
          
        }
        else if ( buttontwo.Button()) {
          log_i("Butten 2");
          pageId -= 1;
        }

        log_i("Page Id after click: %d",pageId);

        if (pageId > 1) {
          log_i("To high Page");
          pageId=lastPage;
        }
        else if (pageId < -1) {
          log_i("To low Page");
          pageId = firstPage;
        }
      if (p.x == 300 and p.y == 244){
        log_i("Huhu");
      }
    }
      /*ofr.setFontSize(30);
      char aString[256];
      sprintf(aString,"%d,%d Huhu",p.x,p.y);
      ofr.cdrawString(aString,p.x,p.y);*/
      log_i("Set Page Id to %d",pageId);
      delay(50);
      if (pageId == 0 || pageId == 1) {
        drawWeather();
      }
      else if (pageId == -1) {
        repaint2();
      }   
    }

  // if (ts.touched()) {
  //   TS_Point p = ts.getPoint();

  //   uint16_t touchX = p.x;
  //   uint16_t touchY = p.y;

  //   log_d("Touch coordinates: x=%d, y=%d", touchX, touchY);
  //   // Debouncing; avoid returning the same touch multiple times.
  //   delay(50);
  // }
  if (pageId != -1) {  
    scheduler.execute();
  }
}



// ----------------------------------------------------------------------------
// Functions
// ----------------------------------------------------------------------------
void drawAstro() {
  time_t tnow = time(nullptr);
  struct tm *nowUtc = gmtime(&tnow);

  SunMoonCalc smCalc = SunMoonCalc(mkgmtime(nowUtc), currentWeather.lat, currentWeather.lon);
  const SunMoonCalc::Result result = smCalc.calculateSunAndMoonData();

  ofr.setFontSize(24);
  ofr.cdrawString(SUN_MOON_LABEL[0].c_str(), 60, 365);
  ofr.cdrawString(SUN_MOON_LABEL[1].c_str(), tft.width() - 60, 365);

  ofr.setFontSize(18);
  // Sun
  strftime(timestampBuffer, 26, UI_TIME_FORMAT_NO_SECONDS, localtime(&result.sun.rise));
  ofr.cdrawString(timestampBuffer, 60, 400);
  strftime(timestampBuffer, 26, UI_TIME_FORMAT_NO_SECONDS, localtime(&result.sun.set));
  ofr.cdrawString(timestampBuffer, 60, 425);

  // Moon
  strftime(timestampBuffer, 26, UI_TIME_FORMAT_NO_SECONDS, localtime(&result.moon.rise));
  ofr.cdrawString(timestampBuffer, tft.width() - 60, 400);
  strftime(timestampBuffer, 26, UI_TIME_FORMAT_NO_SECONDS, localtime(&result.moon.set));
  ofr.cdrawString(timestampBuffer, tft.width() - 60, 425);

  // Moon icon
  int imageIndex = round(result.moon.age * NUMBER_OF_MOON_IMAGES / LUNAR_MONTH);
  if (imageIndex == NUMBER_OF_MOON_IMAGES) imageIndex = NUMBER_OF_MOON_IMAGES - 1;
  ui.drawBmp("/moon/m-phase-" + String(imageIndex) + ".bmp", centerWidth - 37, 365);

  ofr.setFontSize(14);
  ofr.cdrawString(MOON_PHASES[result.moon.phase.index].c_str(), centerWidth, 455);

  log_i("Moon phase: %s, illumination: %f, age: %f -> image index: %d",
        result.moon.phase.name.c_str(), result.moon.illumination, result.moon.age, imageIndex);
}

void drawCurrentWeather() {
  // re-use variable throughout function
  String text = "";

  // icon
  String weatherIcon = getWeatherIconName(currentWeather.weatherId, true);
  ui.drawBmp("/weather/" + weatherIcon + ".bmp", 5, 125);
  // tft.drawRect(5, 125, 100, 100, 0x4228);

  // condition string
  ofr.setFontSize(24);
  ofr.cdrawString(currentWeather.description.c_str(), centerWidth, 95);

  // temperature incl. symbol, slightly shifted to the right to find better balance due to the ° symbol
  ofr.setFontSize(48);
  text = String(currentWeather.temp, 1) + "°";
  ofr.cdrawString(text.c_str(), centerWidth + 10, 120);

  ofr.setFontSize(18);

  // humidity
  text = String(currentWeather.humidity) + " %";
  ofr.cdrawString(text.c_str(), centerWidth, 178);

  // pressure
  text = String(currentWeather.pressure) + " hPa";
  ofr.cdrawString(text.c_str(), centerWidth, 200);

  // wind rose icon
  int windAngleIndex = round(currentWeather.windDeg * 8 / 360);
  if (windAngleIndex > 7) windAngleIndex = 0;
  ui.drawBmp("/wind/" + WIND_ICON_NAMES[windAngleIndex] + ".bmp", tft.width() - 80, 125);
  // tft.drawRect(tft.width() - 80, 125, 75, 75, 0x4228);

  // wind speed
  text = String(currentWeather.windSpeed, 0);
  if (IS_METRIC) text += " m/s";
  else text += " mph";
  ofr.cdrawString(text.c_str(), tft.width() - 43, 200);
}

void drawForecast() {
  DayForecast* dayForecasts = calculateDayForecasts(forecasts);
  for (int i = 0; i < 4; i++) {
    log_i("[%d] condition code: %d, hour: %d, temp: %.1f/%.1f", dayForecasts[i].day,
          dayForecasts[i].conditionCode, dayForecasts[i].conditionHour, dayForecasts[i].minTemp,
          dayForecasts[i].maxTemp);
  }

  int widthEigth = tft.width() / 8;
  for (int i = 0; i < 4; i++) {
    int x = widthEigth * ((i * 2) + 1);
    ofr.setFontSize(24);
    ofr.cdrawString(WEEKDAYS_ABBR[dayForecasts[i].day].c_str(), x, 235);
    ofr.setFontSize(18);
    ofr.cdrawString(String(String(dayForecasts[i].minTemp, 0) + "-" + String(dayForecasts[i].maxTemp, 0) + "°").c_str(), x, 265);
    ui.drawBmp("/weather-small/" + getWeatherIconName(dayForecasts[i].conditionCode, false) + ".bmp", x - 25, 295);
  }
}

void drawForecast2() {
  DayForecast* dayForecasts = calculateDayForecasts(forecasts);
  int widthEigth = tft.width() / 8;
  for (int i = 4; i < 5; i++) {
    int j = i -4;
    log_i("[%d] condition code: %d, hour: %d, temp: %.1f/%.1f", dayForecasts[i].day,
          dayForecasts[i].conditionCode, dayForecasts[i].conditionHour, dayForecasts[i].minTemp,
          dayForecasts[i].maxTemp);
    int x = widthEigth * ((j * 2) + 1);
    ofr.setFontSize(24);
    ofr.cdrawString(WEEKDAYS_ABBR[dayForecasts[i].day].c_str(), x, 235);
    ofr.setFontSize(18);
    ofr.cdrawString(String(String(dayForecasts[i].minTemp, 0) + "-" + String(dayForecasts[i].maxTemp, 0) + "°").c_str(), x, 265);
    ui.drawBmp("/weather-small/" + getWeatherIconName(dayForecasts[i].conditionCode, false) + ".bmp", x - 25, 295);
  }
}

void drawProgress(const char *text, int8_t percentage) {
  ofr.setFontSize(24);
  int pbWidth = tft.width() - 100;
  int pbX = (tft.width() - pbWidth)/2;
  int pbY = 260;
  int progressTextY = 210;

  tft.fillRect(0, progressTextY, tft.width(), 40, TFT_BLACK);
  ofr.cdrawString(text, centerWidth, progressTextY);
  ui.drawProgressBar(pbX, pbY, pbWidth, 15, percentage, TFT_WHITE, TFT_TP_BLUE);
}

void drawSeparator(uint16_t y) {
  tft.drawFastHLine(10, y, tft.width() - 2 * 15, 0x4228);
}

void drawTimeAndDate() {
  timeSprite.fillSprite(TFT_BLACK);
  ofr.setDrawer(timeSprite);  

  // Überschrift
  ofr.setFontSize(16);
  ofr.cdrawString(
    ("Jakobs Wetter"),centerWidth,10);
    
    //String(WEEKDAYS[getCurrentWeekday()] + ", " + getCurrentTimestamp(UI_DATE_FORMAT)).c_str(),
    //centerWidth,
    //10
  //);

  ofr.setFontSize(16);
  ofr.cdrawString(String(WEEKDAYS[getCurrentWeekday()] + ", " + getCurrentTimestamp(UI_DATE_FORMAT)).c_str(),
    centerWidth,
    33
    );

  // Time
  ofr.setFontSize(16);
  // centering that string would look optically odd for 12h times -> manage pos manually
  ofr.drawString(getCurrentTimestamp(UI_TIME_FORMAT).c_str(), centerWidth, 50);
  timeSprite.pushSprite(timeSpritePos.x, timeSpritePos.y);

  // set the drawer back since we temporarily changed it to the time sprite above
  ofr.setDrawer(tft);
}

String getWeatherIconName(uint16_t id, bool today) {
  // Weather condition codes: https://openweathermap.org/weather-conditions#Weather-Condition-Codes-2

  // For the 8xx group we also have night versions of the icons.
  // Switch to night icons? This could be written w/o if-else but it'd be less legible.
  if ( today && id/100 == 8) {
    if (today && (currentWeather.observationTime < currentWeather.sunrise ||
                  currentWeather.observationTime > currentWeather.sunset)) {
      id += 1000;
    } else if(!today && false) {
      // NOT-SUPPORTED-YET
      // We currently don't need the night icons for forecast.
      // Hence, we don't even track those properties in the DayForecast struct.
      // forecast->dt[0] < forecast->sunrise || forecast->dt[0] > forecast->sunset
      id += 1000;
    }
  }

  if (id/100 == 2) return "thunderstorm";
  if (id/100 == 3) return "drizzle";
  if (id == 500) return "light-rain";
  if (id == 504) return "extrem-rain";
  else if (id == 511) return "sleet";
  else if (id/100 == 5) return "rain";
  if (id >= 611 && id <= 616) return "sleet";
  else if (id/100 == 6) return "snow";
  if (id/100 == 7) return "fog";
  if (id == 800) return "clear-day";
  if (id >= 801 && id <= 803) return "partly-cloudy-day";
  else if (id/100 == 8) return "cloudy";
  // night icons
  if (id == 1800) return "clear-night";
  if (id == 1801) return "partly-cloudy-night";
  else if (id/100 == 18) return "cloudy";

  return "unknown";
}

void initJpegDecoder() {
    // The JPEG image can be scaled by a factor of 1, 2, 4, or 8 (default: 0)
  TJpgDec.setJpgScale(1);
  // The decoder must be given the exact name of the rendering function
  TJpgDec.setCallback(pushImageToTft);
}

void initOpenFontRender() {
  ofr.loadFont(opensans, sizeof(opensans));
  ofr.setDrawer(tft);
  ofr.setFontColor(TFT_WHITE);
  ofr.setBackgroundColor(TFT_BLACK);
}

// Function will be called as a callback during decoding of a JPEG file to
// render each block to the TFT.
bool pushImageToTft(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height()) {
    return 0;
  }

  // Automatically clips the image block rendering at the TFT boundaries.
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

void syncTime() {
  if (initTime()) {
    lastTimeSyncMillis = millis();
    setTimezone(TIMEZONE);
    log_i("Current local time: %s", getCurrentTimestamp(SYSTEM_TIMESTAMP_FORMAT).c_str());
  }
}

void repaint() {
  tft.fillScreen(TFT_BLACK);
  ui.drawLogo();

  ofr.setFontSize(16);
  ofr.cdrawString(APP_NAME, centerWidth, tft.height() - 50);
  ofr.cdrawString(VERSION, centerWidth, tft.height() - 30);

  drawProgress("Starting WiFi...", 10);
  if (WiFi.status() != WL_CONNECTED) {
    startWiFi();
  }

  drawProgress("Synchronizing time...", 30);
  syncTime();

  updateData(true);

  drawProgress("Ready", 100);
  lastUpdateMillis = millis();

  drawWeather();
}

void repaint2() {
  tft.fillScreen(TFT_BLACK);
  buttonDeckone.Kasten();
  buttonDecktwo.Kasten();
  buttonDeckthree.Kasten();
  buttonDeckfour.Kasten();
  buttonDeckfive.Kasten();
  buttonDecksix.Kasten();  
  buttonDeckone.Bezeichnung();
}

void drawWeather() {
  if (pageId != -1) {
    tft.fillScreen(TFT_BLACK);

    drawTimeAndDate();
    drawSeparator(90);

    drawCurrentWeather();
    drawSeparator(230);
  }
  if (pageId == 1)
  {
    drawForecast2();
  }
  else
  {
    drawForecast();
  }
  drawSeparator(355);

  drawAstro();
}

void updateData(boolean updateProgressBar) {
  if(updateProgressBar) drawProgress("Updating weather...", 70);
  OpenWeatherMapCurrent *currentWeatherClient = new OpenWeatherMapCurrent();
  currentWeatherClient->setMetric(IS_METRIC);
  currentWeatherClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient->updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_API_KEY, OPEN_WEATHER_MAP_LOCATION_ID);
  delete currentWeatherClient;
  currentWeatherClient = nullptr;
   log_i("Current weather in %s: %s, %.1f°", currentWeather.cityName, currentWeather.description.c_str(), currentWeather.feelsLike);
 
  if(updateProgressBar) drawProgress("Updating forecast...", 90);
  OpenWeatherMapForecast *forecastClient = new OpenWeatherMapForecast();
  forecastClient->setMetric(IS_METRIC);
  forecastClient->setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  forecastClient->setAllowedHours(forecastHoursUtc, sizeof(forecastHoursUtc));
  forecastClient->updateForecastsById(forecasts, OPEN_WEATHER_MAP_API_KEY, OPEN_WEATHER_MAP_LOCATION_ID, NUMBER_OF_FORECASTS);
  delete forecastClient;
  forecastClient = nullptr;
}
