#include <AWS_IOT.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <SSD1306.h>
#include <NTPClient.h>

#include "DHT.h"

#define DHTPIN 0     // what digital pin we're connected to

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

#include <SDS011.h>

WiFiUDP ntpUDP;

SSD1306 display(0x3c, 5, 4);

float p10, p25;
int err;

int max_width = 128;

SDS011 sds011;

DHT dht(DHTPIN, DHTTYPE);

AWS_IOT hornbill;   // AWS_IOT instance

char WIFI_SSID[]="";
char WIFI_PASSWORD[]="";
char BACKUP_WIFI_SSID[]="";
char BACKUP_WIFI_PASSWORD[]="";

char * SSIDS[] = {
    WIFI_SSID,
    BACKUP_WIFI_SSID,
};

char * PASSWORDS[] = {
    WIFI_PASSWORD,
    BACKUP_WIFI_PASSWORD,
};

char HOST_ADDRESS[]=".iot.us-east-1.amazonaws.com";
char CLIENT_ID[]= "Outside ESP32";
char TOPIC_NAME[]= "Outside/air";
String topic_name = String(TOPIC_NAME);
String client_id = String(CLIENT_ID);


int status = WL_IDLE_STATUS;
int tick=0,msgCount=0,msgReceived = 0;
char payload[512];
char rcvdPayload[512];
const size_t capacity = JSON_OBJECT_SIZE(5);

NTPClient timeClient(ntpUDP);

DynamicJsonDocument root(1024);
String strObj;

long last_submission;
long last_scan;
long last_temp;
float pm25;
float pm10;

char jsonChar[100];
long current_epoch;
long since_submission = 0; 
long since_scan = 0;
long since_temp = 0;
float h;
float f;

void setup() {
    //StaticJsonBuffer<300> jsonBuffer;

    display.init();
    display.flipScreenVertically();

    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 20, "x_x");
    display.display();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    
    delay(2000);

    while (status != WL_CONNECTED) {
        for (int i=0; i<sizeof(SSIDS); i++) {
            for (int x=0; x<5; x++) {

                display.clear();
                String connecting = "Connecting to " + String(SSIDS[i]) + " with " + String(WiFi.macAddress());
                display.drawStringMaxWidth(0, 0, max_width, connecting);
                display.display();

                // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
                status = WiFi.begin(SSIDS[i], PASSWORDS[i]);
                if ( status == WL_CONNECTED ) {
                    display.clear();
                    String _connected = "Connected to " + String(SSIDS[i]) + " at " + WiFi.localIP().toString().c_str();
                    display.drawStringMaxWidth(0, 0, max_width, _connected);
                    display.display();
                    delay(2000);
                    break;
                }
                // wait 5 seconds for connection:
                delay(5000);
            }
            if ( status == WL_CONNECTED ) {
                break;
            }
        }
        if ( status == WL_CONNECTED ) {
            break;
        }
    }



	display.clear();
	display.drawStringMaxWidth(0, 0, max_width, "Connecting to AWS as " + client_id);
	display.display();

    if(hornbill.connect(HOST_ADDRESS,CLIENT_ID) == 0) {
        display.clear();
        display.drawStringMaxWidth(0, 0, max_width, "Connected to AWS");
        display.display();
        delay(1000);
    } else {
        display.clear();
        display.drawStringMaxWidth(0, 0, max_width, "AWS connection failed");
        display.display();
        while(1);
    }

	timeClient.begin();
	timeClient.setTimeOffset(-18000);
	timeClient.update();

	last_submission = timeClient.getEpochTime();
	last_scan = timeClient.getEpochTime();
	last_temp = timeClient.getEpochTime();

    delay(2000);
	sds011.setup(&Serial);
	sds011.onData([](float pm25Value, float pm10Value) {
        root["pm25"] = pm25Value;
        root["pm10"] = pm10Value;
        pm25 = pm25Value;
        pm10 = pm10Value;
		last_scan = timeClient.getEpochTime();
	});
	sds011.setWorkingPeriod(3);

    dht.begin(); //Initialize the DHT11 sensor

	display.setFont(ArialMT_Plain_10);
}
void loop() {
	timeClient.update();

    // read temp/humidity 
    if (since_temp > 10) {
        h = dht.readHumidity();
        f = dht.readTemperature(true);
        if (!isnan(h) &&  !isnan(f)) {
            root["temp"] = f;
            root["humidity"] = h;
            last_temp = timeClient.getEpochTime();
        } 
    }

    // read air quality 
    sds011.loop();

    if (since_submission > 340) {
        ESP.restart();
    }

    // submit to AWS
    if (since_submission > 300) {
        serializeJson(root, jsonChar);
		if(hornbill.publish(TOPIC_NAME,jsonChar) == 0) {        
            root.remove("temp");
            root.remove("humidity");
            root.remove("pm25");
            root.remove("pm10");
			last_submission = timeClient.getEpochTime();
		}
    }
    
    
    // update counts
	current_epoch = timeClient.getEpochTime();
	since_submission = current_epoch - last_submission;
	since_scan = current_epoch - last_scan;
    since_temp = current_epoch - last_temp;

    // display 
	display.clear();
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.drawStringMaxWidth(50, 0, max_width, timeClient.getFormattedTime());
	display.drawStringMaxWidth(0, 12, max_width, "temp: ");
	display.drawStringMaxWidth(0, 24, max_width, "humidity: ");
	display.drawStringMaxWidth(0, 36, max_width, "pm 2.5: ");
	display.drawStringMaxWidth(0, 48, max_width, "pm 10: ");

	display.drawStringMaxWidth(50, 12, max_width, String(f));
	display.drawStringMaxWidth(50, 24, max_width, String(h));
	display.drawStringMaxWidth(50, 36, max_width, String(pm25));
	display.drawStringMaxWidth(50, 48, max_width, String(pm10));

	display.setTextAlignment(TEXT_ALIGN_RIGHT);
	display.drawString(128, 0, String(since_submission));
	display.drawString(128, 24, String(since_temp));
	display.drawString(128, 48, String(since_scan));
	display.display();

    delay(1000);
}
