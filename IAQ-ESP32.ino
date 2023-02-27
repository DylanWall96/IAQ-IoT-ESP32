#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <WiFi.h>
#include "HTTPClient.h"
#include <SD.h> 
#include<SPI.h> 
#include <NTPClient.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>

#define WDT_TIMEOUT 120
int i = 0;
int last = millis();

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme; // I2C

float hum_weighting = 0.25; // so hum effect is 25% of the total air quality score
float gas_weighting = 0.75; // so gas effect is 75% of the total air quality score

int   humidity_score, gas_score, score;
float gas_reference = 2500;
float hum_reference = 40;
int   getgasreference_count = 0;
int   gas_lower_limit = 10000;  // Bad air quality limit
int   gas_upper_limit = 300000; // Good air quality limit
unsigned long lastMillis;


// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

// GPIO pin numbers set
int greenPin = 4, redPin = 2, bluePin = 13;

String dataMessage;


/* COMMENT THIS OUT IF SENSOR DATA IS IN THE BEDROOM/STUDY */
//String location = "Bar";

/* COMMENT THIS OUT IF SENSOR DATA IS IN THE KITCHEN */
 String location = "Kitchen";

/* COMMENT THIS OUT IF SENSOR DATA IS IN THE OFFICE */
//String location = "Office";

// Define CS pin for the SD card module
#define SD_CS 5


unsigned long lastRead = 0;

void setup() {
 // Sets rate for serial data transmission
  Serial.begin(115200);

//Watchdog Timer
  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  Serial.println(F("BME680 test"));
  Wire.begin();

  
  /* Depending on which BME680 is being used the API begin function may differ. 
  If you get a "Could not find a valid BME680 sensor, check wiring!" in the serial 
  monitor, uncomment the begin function beneath and and comment out "if (!bme.begin()) {" and try again*/
  
if (!bme.begin(0x76)) {
 
//if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
      setColor(128,0,128);
    while (1);
  } else Serial.println("Found a sensor");

  //WI-FI parameters- CHANGE THIS TO YOUR OWN PARAMETERS TO ALLOW ESP32 TO HAVE INTERNET CONNECTIVITY.
    char ssid[] = "";
    const char* password = "";
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
    Serial.println("WiFi Connection Succesful!");
    Serial.println(WiFi.localIP());
    
// Initialize a NTPClient to get time
  timeClient.begin();
  timeClient.setTimeOffset(3600);
  delay(1000);
    
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C for 150 ms
  // Now run the sensor to normalise the readings, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
  // The sensor takes ~30-mins to fully stabilise
  GetGasReference();
 
 // Flash for Setup Complete
 setColor(0,255,255);
  delay(500);
  setColor(255,0,255);
  delay(500);
  setColor(0,0,255);
  delay(500);
  setColor(192,192,192);
  delay(500);
  setColor(0, 255, 0);
  delay(500);
 setColor(0, 0, 0);

  Serial.println("Ready and raring to go!");

}

void loop() {
    
    //activates watch dog reset to reboot ESP32 after 2 minutes of nothing happening, and resets timer if all is working as normal
  if (millis() - lastRead >= last) {
      Serial.println("Resetting WDT...");
      esp_task_wdt_reset();
      last = millis();
      i++;
      if (i == 5) {
        Serial.println("Stopping WDT reset. CPU should reboot in 3s");
      }
 
   getTimeStamp();

  Serial.println("Sensor Readings:");
  Serial.println("  Temperature = " + String(bme.readTemperature(), 2)     + "°C");
  int temp = bme.readTemperature();
  Serial.println("     Pressure = " + String(bme.readPressure() / 100.0F) + " hPa");
  int pres = bme.readPressure() / 100.0F;
  Serial.println("     Humidity = " + String(bme.readHumidity(), 1)        + "%");
  //Serial.println("     Humidity = " + String(bme.readHumidity() , 1)        + "%");
  int hum = bme.readHumidity();
  Serial.println("          Gas = " + String(gas_reference)               + " ohms\n");  
  int gas = gas_reference;
  Serial.print("Qualitative Air Quality Index ");
  
  humidity_score = GetHumidityScore();
  gas_score      = GetGasScore();
    
  //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
  float air_quality_score = humidity_score + gas_score;
  Serial.println(" comprised of " + String(humidity_score) + "% Humidity and " + String(gas_score) + "% Gas");
  if ((getgasreference_count++) % 5 == 0) GetGasReference();
  Serial.println(CalculateIAQ(air_quality_score));
  Serial.println("--------------------------------------------------------------");
  score          = air_quality_score;
  score = (100 - score) * 5;
  
 if (millis() - lastMillis >= 1*60*1000UL) 
  {
   lastMillis = millis();  //get ready for the next iteration
   HTTPRequest();
  }
}
    }


void GetGasReference() {
  // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
  int readings = 10;
  for (int i = 1; i <= readings; i++) { // read gas for 10 x 0.150mS = 1.5secs
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
}

void HTTPRequest(){
   /* COMMENT THIS OUT IF SENSOR DATA IS IN THE KITCHEN */
  const char* serverName = "";
  //Check WiFi connection status
  if(WiFi.status()== WL_CONNECTED){
    HTTPClient http;
    
    // Your Domain name with URL path or IP address with path
    http.begin(serverName);
    
    // Specify content-type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    // Prepare your HTTP POST request data
    String httpRequestData = "&temp=" + String(bme.readTemperature(), 2) + "&pres=" + String(bme.readPressure() / 100.0F) +"&hum=" + String(bme.readHumidity(), 1) + "&gas=" + String(gas_reference / 1000)  + "&score=" + String(score) + "&location=" + String(location);
    Serial.print("httpRequestData: ");
    Serial.println(httpRequestData);
    
    // Send HTTP POST request
   
    int httpResponseCode = http.POST(httpRequestData);

    //Setting colour of RGB LED depending on the http response code     
    if (httpResponseCode=200) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
        setColor(0, 255, 0);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
        setColor(255, 0, 0);
    }
    // Free resources
    http.end();
      }

}


String CalculateIAQ(int score) {
  String IAQ_text = "air quality is ";
  score = (100 - score) * 5;
  if      (score >= 301)                  IAQ_text += "Hazardous";
  else if (score >= 201 && score <= 300 ) IAQ_text += "Very Unhealthy";
  else if (score >= 176 && score <= 200 ) IAQ_text += "Unhealthy";
  else if (score >= 151 && score <= 175 ) IAQ_text += "Unhealthy for Sensitive Groups";
  else if (score >=  51 && score <= 150 ) IAQ_text += "Moderate";
  else if (score >=  00 && score <=  50 ) IAQ_text += "Good";
  Serial.print("IAQ Score = " + String(score) + ", ");
  return IAQ_text;
}

int GetHumidityScore() {  //Calculate humidity contribution to IAQ index
  float current_humidity = bme.readHumidity();
  if (current_humidity >= 38 && current_humidity <= 42) // Humidity +/-5% around optimum
    humidity_score = 0.25 * 100;
  else
  { // Humidity is sub-optimal
    if (current_humidity < 38)
      humidity_score = 0.25 / hum_reference * current_humidity * 100;
    else
    {
      humidity_score = ((-0.25 / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
    }
  }
  return humidity_score;
}

int GetGasScore() {
  //Calculate gas contribution to IAQ index
  gas_score = (0.75 / (gas_upper_limit - gas_lower_limit) * gas_reference - (gas_lower_limit * (0.75 / (gas_upper_limit - gas_lower_limit)))) * 100.00;
  if (gas_score > 75) gas_score = 75; // Sometimes gas readings can go outside of expected scale maximum
  if (gas_score <  0) gas_score = 0;  // Sometimes gas readings can go outside of expected scale minimum
  return gas_score;
}

// Sets the colour of the LED
void setColor(int redValue, int greenValue, int blueValue) {
  // Sets the output values of the LED GPIO pins to change the LED colour
  digitalWrite(redPin, redValue);
  digitalWrite(greenPin, greenValue);
  digitalWrite(bluePin, blueValue);
}

// Function to get date and time from NTPClient
void getTimeStamp() {
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  
  // Extract date and time
  formattedDate = timeClient.getFormattedDate();

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  
}
 
