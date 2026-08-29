#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "fonts.h"

#include <HTTPClient.h>
#include <time.h>

WiFiClientSecure espClient;
PubSubClient client(espClient);

// WLAN
const char* ssid = "FRITZ!BoxXXXXX"; // "MEIN_WLAN"
const char* password = "XXXXX";	     // "MEIN_PASSWORT"

// MQTT
const char* mqtt_server = "eu1.cloud.thethings.network";
const int mqtt_port = 8883;

const char* mqtt_user = "XXXXXXXXXXXXXX"; // MEIN_TTN_USER
const char* mqtt_pass = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXA"; // MEIN_TTN_KEY

const char* topic = "v3/015@ttn/devices/+/up";

// Sensorwerte
float temp_raum1 = 0;
float temp_aussen = 0;
float hum_raum1 = 0;
String last15_01 = "---";


float temp_raum2 = 0;
float distance_m = 0;
String last15_02 = "---";

bool mqttConnected = false;
volatile bool updateNeeded = true;


// Forecast
struct ForecastDay
{
    String day;
    float tempMin;
    float tempMax;
    String weather;
    String icon;
};

ForecastDay forecast[3];
unsigned long lastForecastUpdate = 0;
bool firstDisplay = true;


// Displaybuffer
UBYTE *BlackImage = NULL;

void updateDisplay();

void callback(char* topic, byte* payload, unsigned int length)
{
    StaticJsonDocument<2048> doc;

    DeserializationError err =
        deserializeJson(doc, payload, length);

    if (err) return;

    JsonObject root =
        doc["uplink_message"]["decoded_payload"];

    if (root.isNull()) return;

    const char* device = doc["end_device_ids"]["device_id"];
    String timestamp = doc["received_at"] | "";
    if(timestamp.length() >= 19)
    {
        timestamp = timestamp.substring(11,19);
    }


    if (strcmp(device, "15-01") == 0)
    {
        temp_raum1  = root["TempC_SHT"] | 0.0;
        hum_raum1   = root["Hum_SHT"]   | 0.0;
        temp_aussen = root["TempC_DS"]  | 0.0;
        last15_01 = timestamp;
    }
    else if (strcmp(device, "15-02") == 0)
    {
        temp_raum2 = root["temperature"] | 0.0;
        temp_raum2 -= 4;

        distance_m = root["distance_m"] | 0.0;
        last15_02 = timestamp;
    }

    updateNeeded = true;
}

void reconnect()
{
    while (!client.connected())
    {
        Serial.println("MQTT verbinden...");

        String clientId =
            "ESP32-" + String(random(0xffff), HEX);

        if (client.connect(
                clientId.c_str(),
                mqtt_user,
                mqtt_pass))
        {
            mqttConnected = true;

            client.subscribe(topic);

            Serial.println("MQTT verbunden");
            updateNeeded = true;
        }
        else
        {
            mqttConnected = false;

            Serial.print("MQTT Fehler ");
            Serial.println(client.state());

            delay(3000);
        }
    }
}

void updateDisplay()
{
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);


    // Titel
    Paint_DrawString_EN(
        20, 25,
        "TTN WETTERSTATION",
        &Font24,
        WHITE,
        BLACK);

    Paint_DrawLine(
        20, 60,
        410, 60,
        BLACK,
        DOT_PIXEL_1X1,
        LINE_STYLE_SOLID);

    char txt[50];

    // Außen
    sprintf(txt, "Aussen : %.1f C", temp_aussen);
    Paint_DrawString_EN(
        20, 100,
        txt,
        &Font24,
        WHITE,
        BLACK);

    // Schlafzimmer
    sprintf(txt, "Schlafzimmer : %.1f C", temp_raum1);
    Paint_DrawString_EN(
        20, 150,
        txt,
        &Font24,
        WHITE,
        BLACK);

    // Innen
    sprintf(txt, "Innen : %.1f C", temp_raum2);
    Paint_DrawString_EN(
        20, 200,
        txt,
        &Font24,
        WHITE,
        BLACK);

    // Luftfeuchte
    sprintf(txt, "Feuchte : %.0f %%", hum_raum1);
    Paint_DrawString_EN(
        20, 250,
        txt,
        &Font24,
        WHITE,
        BLACK);

    // Distanz
    sprintf(txt, "Distanz : %.2f m", distance_m);
    Paint_DrawString_EN(
        20, 300,
        txt,
        &Font24,
        WHITE,
        BLACK);


    sprintf(txt,
        "Letzter Upload 15-01 : %s",
        last15_01.c_str());
    Paint_DrawString_EN(
        20, 340,
        txt,
        &Font16,
        WHITE,
        BLACK);

    sprintf(txt,
        "Letzter Upload 15-02 : %s",
        last15_02.c_str());
    Paint_DrawString_EN(
        20, 360,
        txt,
        &Font16,
        WHITE,
        BLACK);

    // Status
    if(distance_m < 1.3)
    {
        Paint_DrawString_EN(
            20, 390,
            "STATUS : BESETZT",
            &Font20,
            WHITE,
            BLACK);
    }
    else
    {
        Paint_DrawString_EN(
            20, 390,
            "STATUS : FREI",
            &Font20,
            WHITE,
            BLACK);;
    }

    // WLAN
    if(WiFi.status() == WL_CONNECTED)
    {
        Paint_DrawString_EN(
            20, 420,
            "WLAN OK",
            &Font16,
            BLACK,
            WHITE);
    }
    else
    {
        Paint_DrawString_EN(
            20, 420,
            "WLAN FEHLER",
            &Font16,
            BLACK,
            WHITE);
    }

    // MQTT
    if(mqttConnected)
    {
        Paint_DrawString_EN(
            260, 420,
            "MQTT OK",
            &Font16,
            BLACK,
            WHITE);
    }
    else
    {
        Paint_DrawString_EN(
            260, 420,
            "MQTT FEHLER",
            &Font16,
            BLACK,
            WHITE);
    }


    Paint_DrawString_EN(
        500, 25,
        "3 TAGE WETTER",
        &Font24,
        WHITE,
        BLACK);

    Paint_DrawLine(
        440, 60,
        780, 60,
        BLACK,
        DOT_PIXEL_1X1,
        LINE_STYLE_SOLID);

    char txt2[80];

    // Tag 1
    sprintf(txt2,
            "%s",
            forecast[0].day.c_str());

    Paint_DrawString_EN(
            450,80,
            txt2,
            &Font16,
            WHITE,
            BLACK);

    sprintf(txt2,
            "%.0f / %.0f C",
            forecast[0].tempMin,
            forecast[0].tempMax);

    Paint_DrawString_EN(
            450,110,
            txt2,
            &Font16,
            WHITE,
            BLACK);

    forecast[0].weather.toCharArray(txt2,80);

    Paint_DrawString_EN(
            450,140,
            txt2,
            &Font16,
            WHITE,
            BLACK);

    drawWeatherIcon(
        forecast[0].icon,
        620,
        80);
    

    // Tag 2
    sprintf(txt2,
            "%s",
            forecast[1].day.c_str());

    Paint_DrawString_EN(
            450,220,
            txt2,
            &Font16,
            WHITE,
            BLACK);

    sprintf(txt2,
            "%.0f / %.0f C",
            forecast[1].tempMin,
            forecast[1].tempMax);

    Paint_DrawString_EN(
            450,250,
            txt2,
            &Font16,
            WHITE,
            BLACK);

    forecast[1].weather.toCharArray(txt2,80);

    Paint_DrawString_EN(
            450,280,
            txt2,
            &Font16,
            WHITE,
            BLACK);

    drawWeatherIcon(
        forecast[1].icon,
        620,
        220);


    // Tag 3
    sprintf(txt2,
            "%s",
            forecast[2].day.c_str());

    Paint_DrawString_EN(
            450,360,
            txt2,
            &Font16,
            WHITE,
            BLACK);

    sprintf(txt2,
            "%.0f / %.0f C",
            forecast[2].tempMin,
            forecast[2].tempMax);

    Paint_DrawString_EN(
            450,390,
            txt2,
            &Font16,
            WHITE,
            BLACK);

    forecast[2].weather.toCharArray(txt2,80);

    Paint_DrawString_EN(
            450,420,
            txt2,
            &Font16,
            WHITE,
            BLACK);

    drawWeatherIcon(
        forecast[2].icon,
        620,
        360);

    /*
    if(firstDisplay)
    {
        EPD_7IN5_V2_Init();
        EPD_7IN5_V2_Display(BlackImage);
        firstDisplay = false;
    }
    else
    {
        EPD_7IN5_V2_Init_Fast();
        EPD_7IN5_V2_Display(BlackImage);
    }
        */
    EPD_7IN5_V2_Init();
    EPD_7IN5_V2_Display(BlackImage);
}

void getForecast()
{
    HTTPClient http;

    String url =
      "http://api.openweathermap.org/data/2.5/forecast"
      "?q=Halle,de"
      "&units=metric"
      "&lang=de"
      "&appid=XXXXXXXXXXXXXXXXXXXXXXXXX"; // OPENWEATHER_KEY

    http.begin(url);

    int httpCode = http.GET();

    if(httpCode != 200)
    {
        Serial.println("Forecast Fehler");
        http.end();
        return;
    }

    DynamicJsonDocument doc(32768);

    deserializeJson(doc, http.getString());

    JsonArray list = doc["list"];

    int dayIndex = 0;
    String lastDate = "";

    for(JsonObject item : list)
    {
        String dt = item["dt_txt"];

        if(dt.indexOf("12:00:00") < 0) continue;

        String currentDate = dt.substring(0,10);

        if(currentDate != lastDate)
        {
            if(dayIndex >= 3) break;

            forecast[dayIndex].day =
                currentDate;

            forecast[dayIndex].tempMin =
                item["main"]["temp_min"];

            forecast[dayIndex].tempMax =
                item["main"]["temp_max"];

            forecast[dayIndex].weather =
                fixUmlauts(
                item["weather"][0]["description"]
                .as<String>());

            forecast[dayIndex].icon =
                item["weather"][0]["icon"]
                .as<String>();

            lastDate = currentDate;
            dayIndex++;
        }
    }

    http.end();

    //updateNeeded = true;

    Serial.println("Forecast aktualisiert");
}

void drawWeatherIcon(String icon, int x, int y)
{
    if(icon.startsWith("01"))
        drawSun(x,y);

    else if(icon.startsWith("02"))
        drawPartlyCloudy(x,y);

    else if(icon.startsWith("03") ||
            icon.startsWith("04"))
        drawCloud(x,y);

    else if(icon.startsWith("09") ||
            icon.startsWith("10"))
        drawRain(x,y);

    else if(icon.startsWith("11"))
        drawThunder(x,y);

    else if(icon.startsWith("13"))
        drawSnow(x,y);

    else
        drawCloud(x,y);
}

String fixUmlauts(String text)
{
    text.replace("ä", "ae");
    text.replace("ö", "oe");
    text.replace("ü", "ue");
    text.replace("Ä", "Ae");
    text.replace("Ö", "Oe");
    text.replace("Ü", "Ue");
    text.replace("ß", "ss");

    return text;
}



void setup()
{
    Serial.begin(115200);

    DEV_Module_Init();

    EPD_7IN5_V2_Init();
    EPD_7IN5_V2_Clear();

    UWORD Imagesize =
        ((EPD_7IN5_V2_WIDTH % 8 == 0) ?
        (EPD_7IN5_V2_WIDTH / 8) :
        (EPD_7IN5_V2_WIDTH / 8 + 1))
        * EPD_7IN5_V2_HEIGHT;

    BlackImage =
        (UBYTE *)malloc(Imagesize);

    if (BlackImage == NULL)
    {
        Serial.println("RAM Fehler");
        while(1);
    }

    Paint_NewImage(
        BlackImage,
        EPD_7IN5_V2_WIDTH,
        EPD_7IN5_V2_HEIGHT,
        0,
        WHITE);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WLAN verbunden");

    

    espClient.setInsecure();

    client.setServer(
        mqtt_server,
        mqtt_port);

    client.setCallback(callback);

    client.setBufferSize(2048);

    getForecast();
    updateNeeded = false;

    Serial.print("Freier Heap: ");
    Serial.println(ESP.getFreeHeap());

    updateDisplay();
}

void loop()
{
    if (!client.connected())
    {
        mqttConnected = false;
        reconnect();
    }

    client.loop();

    if(updateNeeded)
    {
        updateNeeded = false;

        Serial.println("Display Update");

        updateDisplay();
    }

    if(millis() - lastForecastUpdate > 1800000)
    {
        lastForecastUpdate = millis();

        getForecast();
    }

}


// ------  Symbole  ----------------
// ---------------------------------

void drawSun(int x, int y)
{
    Paint_DrawCircle(
        x+32, y+32,
        18,
        BLACK,
        DOT_PIXEL_2X2,
        DRAW_FILL_EMPTY);

    for(int a=0;a<360;a+=45)
    {
        float r1=24;
        float r2=32;

        int x1=x+32+r1*cos(a*3.14159/180);
        int y1=y+32+r1*sin(a*3.14159/180);

        int x2=x+32+r2*cos(a*3.14159/180);
        int y2=y+32+r2*sin(a*3.14159/180);

        Paint_DrawLine(
            x1,y1,
            x2,y2,
            BLACK,
            DOT_PIXEL_2X2,
            LINE_STYLE_SOLID);
    }
}

void drawCloud(int x,int y)
{
    Paint_DrawCircle(x+20,y+35,15,
                     BLACK,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);

    Paint_DrawCircle(x+40,y+25,18,
                     BLACK,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);

    Paint_DrawCircle(x+60,y+35,15,
                     BLACK,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);

    Paint_DrawLine(x+10,y+50,
                   x+70,y+50,
                   BLACK,DOT_PIXEL_2X2,
                   LINE_STYLE_SOLID);
}

void drawPartlyCloudy(int x,int y)
{
    drawSun(x-10,y-10);
    drawCloud(x+5,y+15);
}

void drawRain(int x,int y)
{
    drawCloud(x,y);

    for(int i=0;i<4;i++)
    {
        Paint_DrawLine(
            x+15+i*15,
            y+55,
            x+10+i*15,
            y+70,
            BLACK,
            DOT_PIXEL_2X2,
            LINE_STYLE_SOLID);
    }
}

void drawSnow(int x,int y)
{
    drawCloud(x,y);

    for(int i=0;i<3;i++)
    {
        int sx=x+20+i*20;
        int sy=y+65;

        Paint_DrawLine(sx-6,sy,sx+6,sy,
                       BLACK,DOT_PIXEL_1X1,
                       LINE_STYLE_SOLID);

        Paint_DrawLine(sx,sy-6,sx,sy+6,
                       BLACK,DOT_PIXEL_1X1,
                       LINE_STYLE_SOLID);
    }
}

void drawThunder(int x,int y)
{
    drawCloud(x,y);

    Paint_DrawLine(
        x+35,y+55,
        x+25,y+75,
        BLACK,
        DOT_PIXEL_2X2,
        LINE_STYLE_SOLID);

    Paint_DrawLine(
        x+25,y+75,
        x+40,y+75,
        BLACK,
        DOT_PIXEL_2X2,
        LINE_STYLE_SOLID);

    Paint_DrawLine(
        x+40,y+75,
        x+30,y+95,
        BLACK,
        DOT_PIXEL_2X2,
        LINE_STYLE_SOLID);
}



