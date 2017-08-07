#include <BlynkSimpleEsp8266.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <math.h>
#include "TembooAccount.h"
#include <Temboo.h>

WiFiClient client;
//NTP clock
static const char ntpServerName[] = "us.pool.ntp.org";
WiFiUDP Udp;
unsigned int localPort = 115000;
time_t getNtpTime();
const int timeZone = -4;  // Eastern Daylight Time (USA)

TembooChoreo AppendValuesChoreo(client);

Ticker secondTick;

volatile int watchdogCount = 0;
volatile unsigned long previousMillis = 0;
volatile const long interval = 300000; //15 minutes
volatile int currentStatus = -1;
volatile int prevHour = -1;
volatile int watering;
volatile boolean loggedData;
volatile float humidityPercentage=0;
volatile int waterLevel = 0;
volatile int button;
volatile int runHour = 1;
volatile boolean systemOn = true;
volatile int prevSecond = 0;
volatile int prevMinute = 0;
volatile int phone = LOW;
volatile unsigned long currentMillis = millis();
const int BUTTON_PIN = 0;
const int interuptPin = 0;
const int PUMP_PIN = 5;
const int RELAY_PIN = 14;
const int LED_PIN = 15;
const int RUNNING = 0;
const int LOW_WATER = 1;
const int DRY_SOIL = 2;
const int SETUP = 3;
const int EXCEPTION = 4;
const int LOW_HUMID = 600;
const int HIGH_HUMID = 975;
const int MIN_WATER = 500; 
char auth[] = "ca25e840278f4058a542986218e55cec";
char ssid[] = "CS390IOT";
char  password[] = "12345678";

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  //ESP.wdtDisable();
  Serial.begin(9600);
  
  //secondTick.attach(1, ISRwatchdog);
  watering = LOW;
  loggedData = false;
  pixel.begin();
  pixel.show();
  setStatus(SETUP);
  pinMode(A0, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(PUMP_PIN,OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(interuptPin), toggleWater, CHANGE);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
 {
   delay(500);
   Serial.print(".");
 }
 Serial.println();
 Serial.print("Connected, IP address: ");
 Serial.println(WiFi.localIP());
 Udp.begin(localPort);
 setSyncProvider(getNtpTime);
 setSyncInterval(300);
 Blynk.config(auth); 
 Blynk.connect();
 AppendValuesChoreo.begin();
 AppendValuesChoreo.setAccountName(TEMBOO_ACCOUNT);
 AppendValuesChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
 AppendValuesChoreo.setAppKey(TEMBOO_APP_KEY);
 setStatus(RUNNING);

}

BLYNK_CONNECTED() {
  if (true) {
    Blynk.syncAll();
  }
}

void loop() {
  Blynk.run();
  button = digitalRead(BUTTON_PIN);
  runCycle(1);
  checkSensors();
  watchdogCount = 0;
  
}

void runCycle(int i){
  if (watering == LOW) {
    if((hour() % i == 0 && watering == LOW && hour() != prevHour) && systemOn == HIGH){
    Serial.println("Starting to water the plant.");
    watering = HIGH;
    prevHour = hour();
    digitalWrite(PUMP_PIN,HIGH);
  } 
  } 
  else if (currentMillis - previousMillis >= interval) {
    Serial.println("Completed watering the plant.");
    previousMillis = currentMillis;
    watering = LOW;
    digitalWrite(PUMP_PIN,LOW);
  }
}


//blynk button to control this as well.
void toggleWater() {

  if(button == LOW){
    systemOn  = !systemOn;
  }

  if(phone == HIGH){
    phone = LOW;
    systemOn = !systemOn;
  }
  
  if (systemOn == LOW) {
    Serial.println("Watering has been terminated");
    digitalWrite(PUMP_PIN, LOW);
    prevHour = -1;
  }
  else{
    watering = LOW;
  }
  
 
}

void checkSensors(){
    String values = "";
    int soilHumidity = 0;
    if (second() %  5==0  && prevSecond != second()) { //5
   prevSecond = second();
    digitalWrite(RELAY_PIN, LOW);
    delay(50);
     soilHumidity= analogRead(A0);
    humidityPercentage = ((float) round(soilHumidity - LOW_HUMID) / (float) round(HIGH_HUMID - LOW_HUMID)) * 100;
    if (humidityPercentage <= 20) {
      Serial.println("Soil humidity is dangerously low.");
      setStatus(DRY_SOIL);
    }
    digitalWrite(RELAY_PIN, HIGH);
    delay(50);
    waterLevel = analogRead(A0);
    
    if (waterLevel <= MIN_WATER && !watering) {
      Serial.println("Water reservoir is low on water.");
      setStatus(LOW_WATER);
    }
    if (humidityPercentage > 20 && waterLevel > MIN_WATER) {
      setStatus(RUNNING);
    }

    values = "[\n  [\n    \"";
    values += Time();
    values += "\",\n    \"";
    values += humidityPercentage;
    values += "% (";
    values += soilHumidity;
    values += ")";
    values += "\",\n    \"";
    if (watering == HIGH) {
      values += "Watering";
    } else if (waterLevel > MIN_WATER) {
      values += "Good";
    } else {
      values += "Low";
    }
    values += " (";
    values += waterLevel;
    values += ")";
    values += "\"\n  ]\n]";
  
    Serial.print("Water Level: ");
    Serial.print(waterLevel);
    Serial.println();
    Serial.print("Humidity: ");
    Serial.print(humidityPercentage);
    Serial.println();
    Blynk.virtualWrite(V2, waterLevel);
    Blynk.virtualWrite(V1, humidityPercentage);
    
 }
    if(minute() % 1 == 0 && prevMinute != minute()){
      prevMinute = minute();
      int wifiStatus = WiFi.status();
      if (wifiStatus == 3) {
        Serial.println("WiFi connection is currently strong.");
      } else {
        Serial.println("There is a problem with the WiFi connection.");
        setStatus(EXCEPTION);
      }
      Serial.println("Writing data to Google spread sheets.");
      updateSpreadSheet(values);
      delay(30);
    }
    
    
    
   


}

BLYNK_WRITE(V0){
  int val = param.asInt();
  
  if(val != systemOn){
    phone = HIGH;
    toggleWater();
    Serial.print("Blynk: ");
    Serial.print(val);
    Serial.println();
  }
  
}

void setStatus(int status) {
  if (currentStatus == status) {
    return;
  }
  switch(status) {
    case SETUP:
      pixel.setPixelColor(0, pixel.Color(0, 0, 255));
      break;
    case RUNNING:
      pixel.setPixelColor(0, pixel.Color(0, 255, 0));
      break;
    case LOW_WATER:
      pixel.setPixelColor(0, pixel.Color(255, 0, 0));
      Blynk.notify("Your water level is currently low!");
      break;
    case DRY_SOIL:
      pixel.setPixelColor(0, pixel.Color(128, 0, 128));
      Blynk.notify("Your soil appears to be very dry!");
      break;
  }
  currentStatus = status;
  if (status != EXCEPTION) {
    pixel.show();
  } else {
    ESP.restart();
  }
}

void ISRwatchdog() {
  watchdogCount++;
  if (watchdogCount == 300) {
    Serial.println("Watchdog has been caught, restarting program.");
    ESP.restart();
  }
}

void updateSpreadSheet(String values){
 // Set Choreo inputs
  AppendValuesChoreo.addInput("RefreshToken", "1/smgPU8jOw7SC-JH4PNgu1XR_lM7TWaLVXSNZv3KksCDf2t7h5TzTg9wW2b3D20t6");
  AppendValuesChoreo.addInput("ClientSecret", "B5l-eVOB-cLYLPZkL5RYKDBZ");
  AppendValuesChoreo.addInput("Values", values);
  AppendValuesChoreo.addInput("ClientID", "717279318768-4de0c6hc3cepq98m4e85p7ij1n060daf.apps.googleusercontent.com");
  AppendValuesChoreo.addInput("SpreadsheetID", "1oikjASRkI126TXtEyVQFrwBB85TpgPUjPQWUxRKTkrE");
  // Identify the Choreo to run
  AppendValuesChoreo.setChoreo("/Library/Google/Sheets/AppendValues");
  AppendValuesChoreo.run();
   
  
}

//Return the current time as a string
 String Time(){
  String t = "";
  t += month();
  t += "/";
  t += day();
  t += "/";
  t += year();
  t += " ";
  t += hour();
  t += ":";
  if (minute() < 10) {
    t += "0";
  }
  t += minute();
  return t;
}

 /*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
