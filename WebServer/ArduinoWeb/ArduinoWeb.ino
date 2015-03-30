#include <EEPROM.h>
#include "PROM.h"
#include "StringModule.h"

/* 1 KB (ATmega32u4)*/
String resp;
String Command;  /* Keep track of the command */
int    Decision; /* Decide which kind of behavior of ESP should have */
int   Count;

int dummyData = 40;
const String  API_KEY = "GPVP0E6QQVWU47LZ";
StringModule STR("");
PROM prom;

void setup()
{
  Serial.begin(115200);    // Arduino serial Monitor
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial1.begin(9600);   // The ESP8826 has a baud rate of 115200
  
  // Enable esp8266 
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  UIweb();
}

void loop()
{
}

void UIweb()
{
  char ChannelID; /*Channel is in scale of 0-6, No need to worry about more than 2 chars */
  Configuration();
  
  while(true)
  {
    CommResponse(1, 100); // Wait for the HTTP access
    if(STR.Storage.length()>0)
    {
      if(STR.Contains("+IPD,"))
      {
        Serial.println(F("------------------GET----------------------"));
        ChannelID = STR.Storage.charAt(STR.readPosition()); 
        STR.Storage = ""; // Clear the memory for nxt time
        CommResponse(0, 5*1000); // Wait for HTTP response to finish
        uploadUI(ChannelID);
        break;
      }
    }

    STR.Storage = "";// Clear the memory for nxt time
  }
}

boolean uploadUI(char ID)
{
  String msg,result;
  resp = "<h1> ESP8266 Web Server</h1>\n";
  resp +="<form  method=\"get\">";
  resp += "<p>SSID &nbsp; <input type=\"text\" name=\"SSID\"><br>";
  resp += "PWD &nbsp; <input type=\"text\" name=\"PAWD\"><br>";
  resp += "STA IP &nbsp; <input type=\"text\" name=\"STIP\"><br>";
  resp += "MQTT IP &nbsp; <input type=\"text\" name=\"MQIP\"><br>";
  resp += "CLOUD IP &nbsp; <input type=\"text\" name=\"CLIP\"><br>";
  resp += "MQTT PORT &nbsp; <input type=\"text\" name=\"MPRT\"><br>";
  resp += "CLOUD PORT &nbsp; <input type=\"text\" name=\"CPRT\"><br></p>";
  resp += "<a type=\"submit\" value=\"submit\"><button>SUBMIT</button></a>";
  resp += "</form>\r";
  msg = "AT+CIPSEND=";
  msg +=ID;
  msg += ",";
  msg += resp.length();
  msg +="\r";
 
  CommLaunch(msg, 2*1000, true, 0);
  CommLaunch("resp", 2*1000, true, 0);
  resp=""; // clean the string which is occupying space
  msg = "AT+CIPCLOSE=";
  msg +=ID;
  msg +="\r";
  CommLaunch(msg, 0, true, 0);
  
  result = Waitresponse();
  Serial.println(result);
  /*deliminate the extracted information*/
  STR.StoreKey(result);
  /*****************Information stored in the URL*************************/
  String ssid, pwd;
  uint8_t IP[3][4];
  uint16_t PORT[2];
  /*****************Information stored in the URL*************************/
  ssid = STR.Delimitation('&').substring(5);
  pwd = STR.Delimitation('&').substring(5);
  //Serial.println("Password:");
 // Serial.println(pwd);
  
  for(int i = 0; i < 3; i++)
  {
     IP[i][0] = (uint8_t)converToInt(STR.Delimitation('.').substring(5));
    for(int j=1; j<3; j++)
    {
      IP[i][j] = (uint8_t)converToInt(STR.Delimitation('.'));
    }
    IP[i][3] = (uint8_t)converToInt(STR.Delimitation('&'));
  }
  
  for(int i = 0; i < 2; i++)
  {
    PORT[i] = (uint16_t)converToInt(STR.Delimitation('&').substring(5));
    
  }
  prom.reset(512);
  prom.Flash(1, IP[0], IP[1], IP[2], PORT[0],PORT[1], ssid, pwd);  
  
  delay(5*1000);
  Serial.println(prom.readConfig());
  Serial.println(prom.readMode());
  Serial.println(prom.readSTAIP());
  Serial.println(prom.readMQTTIP());
  Serial.println(prom.readCLOUDIP());
  Serial.println(prom.readMQTTPort());
  Serial.println(prom.readCLOUDPort());
  Serial.println(prom.readSSID());
  Serial.println(prom.readPWD());
  
  return true;
}

String Waitresponse()
{
  String result="";
  int startP=0, endP=0;
  while(true)
  {
    CommResponse(1, 200); // Wait for the HTTP access
    if(STR.Storage.length()>0)
    {
      if(STR.Contains("GET /?"))
      {
        startP =STR.readPosition();
        if(STR.Contains("HTTP/"))
        {
          Serial.println(F("-----------------GET&&HTTP--------------------"));
          endP= STR.readPosition()-6;
          result = STR.Storage.substring(startP,endP);
          STR.Storage = "";
          break;
        }else
        {
          Serial.println(F("-----------------GET--------------------"));
         result = STR.Storage.substring(startP);
        }
      }else if(startP && STR.Contains("HTTP/"))
      {
        Serial.println(F("---------------------------HTTP----------------------"));
        endP= STR.readPosition()-6;
        result.concat(STR.Storage.substring(0,endP));
        STR.Storage = "";
        break;
      }
       STR.Storage="";
    }
  }
  return result;
}

int converToInt(String input)
{
  //Serial.println(input.toInt());
   return input.toInt();
}

void Configuration()
{
  CommLaunch("AT+RST\r", 2*1000, true, 0);
  CommLaunch("AT+CWMODE=2\r", 1000, true, 0);
  CommLaunch("AT+CIPMUX=1\r", 2*1000, true, 0);
  CommLaunch("AT+CIPSERVER=1,80\r", 2*1000, true, 0);
  CommLaunch("AT+CWSAP=\"AI-THINKER\",\"\",5,0\r", 5*1000, true, 0);
}

void CommLaunch(String cmd,unsigned int duration, boolean res, int keyword)
{
  if(cmd.equals("resp"))
    Serial1.println(resp);
  Serial1.println(cmd);    //send command to ESP8266
  
  if(res)
    CommResponse( keyword, duration);
}

void CommResponse(int keyword, unsigned int duration)
{
  String tmp ="";
  long deadline = millis()+duration;
  while(millis() < deadline)
  {
    while (Serial1.available()>0)  {
      tmp += char(Serial1.read());
    }

    if(tmp.length() > 0) {
      if(keyword==1)
      {
        STR.Storage += tmp; // if the keyword is triggered, copy the string
      }

      Serial.print(tmp);
      tmp = "";
    }
  }
}

