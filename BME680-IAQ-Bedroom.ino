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

//3 seconds WDT
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


String location = "Bedroom";
//String location = "Kitchen";

// Define CS pin for the SD card module
#define SD_CS 5

//unsigned long startTime;
////
//#define INTERVAL 300000  // time between reads
unsigned long lastRead = 0;


void setup() {
// startTime = millis();
 // Sets rate for serial data transmission
  Serial.begin(115200);

  //Watchdog Timer
  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT 

  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  Serial.println(F("BME680 test"));
  Wire.begin();

    // Initialize SD card
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/WBedroomData.txt");
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/WBedroomData.txt", "Day, Timestamp, Temperature, Pressure, Humidity, Gas, Score, Location \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
//
//  if (!bme.begin(0x76)) {
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
      setColor(128,0,128);
    while (1);
  } else Serial.println("Found a sensor");

    char ssid[] = "BTHub6-56R6";
  const char* password = "MwFd7LmAPRqV";
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

    Serial.println("WiFi Connection Succesful!");
    Serial.println(WiFi.localIP());
// Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
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
    
  if (millis() - lastRead >= last) {
      Serial.println("Resetting WDT...");
      esp_task_wdt_reset();
      last = millis();
      i++;
      if (i == 5) {
        Serial.println("Stopping WDT reset. CPU should reboot in 3s");
      }   
 
   logSDCard();
   getTimeStamp();

  Serial.println("Sensor Readings:");
  Serial.println("  Temperature = " + String(bme.readTemperature(), 2)     + "°C");
  int temp = bme.readTemperature();
  Serial.println("     Pressure = " + String(bme.readPressure() / 100.0F) + " hPa");
  int pres = bme.readPressure() / 100.0F;
  Serial.println("     Humidity = " + String(bme.readHumidity() - 4 , 1)        + "%");
  //Serial.println("     Humidity = " + String(bme.readHumidity() , 1)        + "%");
  int hum = bme.readHumidity();
  Serial.println("          Gas = " + String(gas_reference)               + " ohms\n");  
  int gas = gas_reference;
  Serial.print("Qualitative Air Quality Index ");

  

  humidity_score = GetHumidityScore();
  gas_score      = GetGasScore();

 

  //const char* serverName = "http://dylaniot.ddns.net/IAQIOT/BME680Data.php";
  const char* serverName = "http://192.168.1.64/AirQualityProject/BME680Data.php";
  
  //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
  float air_quality_score = humidity_score + gas_score;
  Serial.println(" comprised of " + String(humidity_score) + "% Humidity and " + String(gas_score) + "% Gas");
  if ((getgasreference_count++) % 5 == 0) GetGasReference();
  Serial.println(CalculateIAQ(air_quality_score));
  Serial.println("--------------------------------------------------------------");
  score          = air_quality_score;
  score = (100 - score) * 5;
// --------Testing----------
//    Serial.println(temp);
//    Serial.println(pres);
//    Serial.println(hum);


  //Check WiFi connection status
  if(WiFi.status()== WL_CONNECTED){
    HTTPClient http;
    
    // Your Domain name with URL path or IP address with path
    http.begin(serverName);
    
    // Specify content-type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    // Prepare your HTTP POST request data
    String httpRequestData = "&temp=" + String(bme.readTemperature(), 2) + "&pres=" + String(bme.readPressure() / 100.0F) +"&hum=" + String(bme.readHumidity() -4, 1) + "&gas=" + String(gas_reference / 1000)  + "&score=" + String(score) + "&location=" + String(location);
    Serial.print("httpRequestData: ");
    Serial.println(httpRequestData);

    
    // Send HTTP POST request
    int httpResponseCode = http.POST(httpRequestData);
         
    if (httpResponseCode>0) {
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
}


void GetGasReference() {
  // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
  //Serial.println("Getting a new gas reference value");
  int readings = 10;
  for (int i = 1; i <= readings; i++) { // read gas for 10 x 0.150mS = 1.5secs
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
  //Serial.println("Gas Reference = "+String(gas_reference,3));
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
  float current_humidity = bme.readHumidity() -4 ;
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



// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");

   if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}


// Write the sensor readings on the SD card - String(printLocalTime()) + "," +
void logSDCard() {
  dataMessage =String(dayStamp) + "," + String(timeStamp) + "," +String(bme.readTemperature(), 2) + "," + String(bme.readPressure() / 100.0F) + "," + String(bme.readHumidity() - 4, 1)  + "," +  String(gas_reference / 1000)  + "," + String(score) + + "," + String(location)+"\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/WBedroomData.txt", dataMessage.c_str());
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
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  
}
 
