#include "SimpleTimer.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DHT.h>
#include <WiFiUdp.h>
#include <LinkedList.h>
#include <DataP.h>

#define DHTTYPE DHT22
#define DHTPIN  5
 
const char* ssid     = "***********"; 
const char* password = "***********"; 

LinkedList<DataP*> myList = LinkedList<DataP*>();
SimpleTimer tRH, tWiFi;
 
ESP8266WebServer webServer(80);
unsigned int localPort = 2390;
IPAddress timeServerIP; 
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;         // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];    //buffer to hold incoming and outgoing packets
WiFiUDP udp;
DHT dht(DHTPIN, DHTTYPE, 11);           // 11 works fine for ESP8266

int readCount=0;
int timer_h; 
float humidity, temp_f;                 // Values read from sensor
String webString="";                    // String to display

unsigned long previousMillis = 0;        // will store last temp was read
const long interval = 2000;              // interval at which to read sensor
unsigned long secsSince1900 =0;

bool isWiFiConnected = false;
 
void handle_root() {
  String s = "<h1>Welcome to BeeMonitor</h1>The Current Temp is : " + String((int)temp_f) + "And Humidity is : " + String((int)humidity) ;
  s +="<br>The UTC Time is : " + getTimeNow() ;
  s +="<br>Click <a href='resetTime'><button>Reset Time</button></a>";
  
 
  webServer.send(200, "text/html", s);
  delay(100);
}

void handle_time() {
  webServer.send(200, "text/plain", "The Time now is : " + getTimeNow());
  delay(100);
}

void handle_history() {
  tRH.disable(timer_h);
  String s="<br>";

  DataP *dd;
  for(int i = 0; i < myList.size(); i++){

    // Get animal from list
    dd = myList.get(i);
    s += "Id :" + dd->id;
    s += " Time :" + dd->time;
    s += " Humidity :" + dd->humidity;
    s += " Temp :" + dd->temp + "<br>";
    
  }
  
  webServer.send(200, "text/html", s);
  delay(100);
  tRH.enable(timer_h);
}
 
void setup()
{
  
  Serial.begin(115200);  
  timer_h = tRH.setInterval(15000, gettemperature);
  tWiFi.setInterval(300000, checkWiFiAndConnect);
  dht.begin();           
 
  
    
  udp.begin(localPort);
   
  webServer.on("/", handle_root);
  webServer.on("/time", handle_time);
  webServer.on("/history", handle_history);
  webServer.on("/temp", [](){  
      
    webString="Temperature: "+String((int)temp_f)+" F Humidity: "+String((int)humidity)+"%";  
    webServer.send(200, "text/plain", webString);            
  });
  
  webServer.begin();
  Serial.println("HTTP server started");
  
  if (!MDNS.begin("esp8266")) {
    Serial.println("Error setting up MDNS responder!");
    while(1) { 
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  MDNS.addService("http", "tcp", 80);
  Serial.println(getUTCTime());
}
 
void loop()
{
  webServer.handleClient();
  tRH.run();
  tWiFi.run();
} 

//==========================================================================================
void checkWiFiAndConnect()
{
 if(isWiFiConnected)
  return;
 
 isWiFiConnected = false;
 WiFi.begin(ssid, password);
  Serial.print("\n\r \n\rWorking to connect");
    
  for(int a = 0 ; a<15 ; a++)
  {
   if(WiFi.status() != WL_CONNECTED)
     delay(1000);
   else
   { 
    isWiFiConnected  = true;
    Serial.println("");
    Serial.println("DHT Weather Reading webServer");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    break;
   }
  }

} 

void gettemperature() {
  
    humidity = dht.readHumidity();          
    temp_f = dht.readTemperature(true);
  if(readCount > 671) // Take 7 days worth of RH readings 24 * 4 * 7 = 672
    readCount = 0;
    
    readCount++;
    DataP *dp = new DataP("App_ID-" + String(readCount), String((int)temp_f), String((int)humidity),getTimeNow());
    myList.add(dp);
    
    if (isnan(humidity) || isnan(temp_f)) 
      return;
    
}

unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

String getUTCTime(){

  WiFi.hostByName(ntpServerName, timeServerIP); 

  sendNTPpacket(timeServerIP); 
  
  delay(1000);
  
  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
  }
  else {
    Serial.print("packet received, length=");
    Serial.println(cb);
    
    udp.read(packetBuffer, NTP_PACKET_SIZE); 

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);

    String s = "The UTC Time is ";
    
    s += String((epoch  % 86400L) / 3600) + ":";
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      s += "0";
    }
    s += String((epoch  % 3600) / 60) + ":";
    
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      s += "0";
    }
    s += String(epoch % 60);
    return s;
  }  
}

String getTimeNow(){
    String s = "";
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears + (millis()/1000);
    // print Unix time:
    Serial.println(readCount);
    Serial.println(epoch);
    s += String((epoch  % 86400L) / 3600) + ":";
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      s += "0";
    }
    s += String((epoch  % 3600) / 60) + ":";
    
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      s += "0";
    }
    s += String(epoch % 60);
    return s;

}



