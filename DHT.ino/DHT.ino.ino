#include "SimpleTimer.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DHT.h>
#include <WiFiUdp.h>
#include "FS.h"

#define DHTTYPE DHT22
#define DHTPIN  5
 
const char* ssid     = "Sunil iP7"; 
const char* password = "jensun2010"; 

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
int commitCount=4;
int dayCount=1;
int timer_h; 
float humidity, temp_f;                 // Values read from sensor
String webString="";                    // String to display
String dbString="";

unsigned long secsSince1900 =0;

bool isWiFiConnected = false;
 
void handle_root() {
  String s = "<h1>Welcome to BeeMonitor</h1>The Current Temp is : " + String((int)temp_f) + "*F And Humidity is : " + String((int)humidity) + "%" ;
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
  webServer.send(200, "text/html", "history");
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
  checkWiFiAndConnect();
  SPIFFS.begin();
  
     
  webServer.on("/", handle_root);
  webServer.on("/time", handle_time);
  webServer.on("/history", handle_history);
  webServer.on("/temp", [](){  
      
    webString="Temperature: "+String((int)temp_f)+" F Humidity: "+String((int)humidity)+"%";  
    webServer.send(200, "text/plain", webString);            
  });
  
  webServer.begin();
  Serial.println("\nHTTP server started");
  
  if (!MDNS.begin("esp8266")) {
  for(int i=0;i<5;i++)
  {   
    Serial.println("Error setting up MDNS responder!");
      delay(1000);
    }
  }
  else
  {
  Serial.println("mDNS responder started");
  MDNS.addService("http", "tcp", 80);
  }
  
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
 Serial.println("\n\r \n\rWorking to connect WiFi");
    
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
  Serial.println(getUTCTime());
    break;
   }
  }
  Serial.println("\n\r \n\rDone...isWiFiConnected " + String(isWiFiConnected));
  
} 

void gettemperature() {
   if (secsSince1900 ==0)
  { 
    Serial.println("Error Time Not Set. Please connect to WiFi");
  Serial.println("\n\r \n\...isWiFiConnected " + String(isWiFiConnected));
    return ;
  }
    humidity = dht.readHumidity();          
    temp_f = dht.readTemperature(true);
    readCount++;
    dbString += getTimeNow() +","+String((int)temp_f)+","+String((int)temp_f)+"=";
    if(readCount == commitCount)
    {
      
      if( writeToFile( dbString, "/f"+ String(dayCount) ))
      {
        dbString = "";
        readCount=0;
        dayCount++;
        if(dayCount==5)
        {
          dayCount=1;
          processData();
        }
      }
    }
    
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
    if (secsSince1900 ==0)
  { 
    Serial.println("Error Time Not Set. Please connect to WiFi");
    return "";
  }
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

bool writeToFile(String s, String fName)
{
  Serial.println("Trying File : " + fName);
  File f = SPIFFS.open(fName, "w");
  if (!f)
  { 
    Serial.println("file creation failed");
    return false;
  }  
  f.println(s);
  f.close();
  Serial.println("Done Creating File : " + fName);
  return true;
}

void processData()
{
  // halt
  //do upload
  for(int i=1;i<5;i++)
  {
    String ff = "/f" + String(i);
    File f = SPIFFS.open(ff, "r");
    if (!f) {
      Serial.println("file open failed");
    }
    while(f.available())
    {
      String line = f.readStringUntil('\n');
      Serial.println(ff+" ->"+line);
      uploadData(line);
    }
    SPIFFS.remove(ff);
  }
  // then delete
}

void uploadData(String s)
{
  char* host = "10.0.0.35";
 // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 8080;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // We now create a URI for the request
  String url = "/id/";
  url += "?name=" +s;
  
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }  
}
