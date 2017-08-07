#include <BlynkSimpleEsp8266.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
//#include "pitches.h"
#include <Temboo.h>
//#include "TembooAccount.h"

WiFiClient client;
//NTP clock
static const char ntpServerName[] = "us.pool.ntp.org";
WiFiUDP Udp;
unsigned int localPort = 115000;
time_t getNtpTime();
const int timeZone = -4;  // Eastern Daylight Time (USA)

//char auth[] = "4e1898b8c2794addb1468dc26f36ca9d";
char ssid[] = "CS390IOT";
char password[] = "12345678";

boolean watering;
const int BUTTON_PIN = 0;
const int PUMP_PIN = 5;
unsigned long previousMillis= 0;
const int interval = 500; //30 minutes


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  watering = false;
  pinMode(A0, INPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(PUMP_PIN,OUTPUT);
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
 //Blynk.config(auth); 
 //Blynk.connect();
 Udp.begin(localPort);
 setSyncProvider(getNtpTime);
 setSyncInterval(300);
}

void loop() {
  // put your main code here, to run repeatedly:
  
  int pinRead = analogRead(A0);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), toggleWater, FALLING);
  if (!watering)
    cycle(5);
  //digitalWrite(PUMP_PIN,LOW);
  
}

void toggleWater() {
  watering = !watering;
}




void cycle(int i){
  

  if(second() % i == 0){
    Serial.println("Cycle");
    watering = true;
    digitalWrite(PUMP_PIN,HIGH);
    unsigned long currentMillis = millis();
    while(currentMillis - previousMillis >= interval && watering) { //Blink one second interval
      previousMillis = currentMillis;
      currentMillis = millis();
    }  
    //when complete
    watering = false;
    digitalWrite(PUMP_PIN,LOW);
  }
}


//Return the current time as a string
 String Time(){
  String t = "";
  t += hour();
  t += ":";
  t += minute();
  t += ":";
  t += second();
  t += ":";
  t += day();
  t += ":";
  t += month(); 
  t += ":";
  t += year();
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
