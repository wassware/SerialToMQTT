// listens to serial2 and sends / recieves to MQTT

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <PubSubClient.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>

// for json props
DynamicJsonDocument doc(200);
const int bsize = 250;
char b[bsize];

// for Serial2 in buffer..
const int bLen = 200;
char buff[bLen];
int bPtr = 0;

const int casize = 2000;
char cacert2[casize];


WiFiClientSecure espClient;
PubSubClient client(espClient);

void getCertificate(fs::FS &fs, const char * path)
{
  int spilled = 0;
  File file = fs.open(path);
  if(!file || file.isDirectory())
  {
     Serial.printf("− failed to open %s for reading\r\n", path);
     return;
  }
  int ptr = 0; 
  while(file.available())
  {
    char c = file.read();
    if (ptr < casize-1)
    {
      if (c != '\r')
      {
        cacert2[ptr++] = c;
        Serial.print(c);
      }
    }
    else
    {
      spilled++;
    }
  }
  cacert2[ptr]=0;
  Serial.println();
  if (spilled > 0)
  {
    Serial.printf("cacert too small - spilled = %d\r\n",spilled);
  }
  Serial.printf("cacert size = %d / %d\r\n",ptr,casize);
  return;
}

void callback(char* topic, byte* messageB, unsigned int length) 
{
  char message[length+1];
  for (int ix = 0; ix < length; ix++)
  {
    message[ix] = messageB[ix];
  }
  message[length] = 0;
  String topicS(topic);
  String messageS(message);
  Serial.println("MQTT in: l=" + String(length) + " '" + topicS + "' '" + messageS + "'");
  // add code to process
  
  if (topicS == "boiler/echo")
  {
    sendmqtt("boiler/reply",  "echo '" + messageS + "'");
  }
  else if (topicS == "boiler/input")
  {
    Serial2.println(messageS);
  }
}

void readProps(fs::FS &fs, const char * path)
{
   Serial.printf("Reading file: %s\r\n", path);   
   File file = fs.open(path);
   if(!file || file.isDirectory()){
       Serial.printf("− failed to open %s for reading\r\n", path);
       return;
   }
   int ptr = 0; 
   while(file.available() && ptr < bsize-1)
   {
      b[ptr++] = file.read();
   }
   b[ptr]=0;
   Serial.println(b);
   Serial.printf("Read buffer %d / %d \r\n", ptr, bsize);
   // Deserialize the JSON document
   DeserializationError error = deserializeJson(doc, b);
  // Test if parsing succeeds.
  if (error) 
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
}


bool connwifi()
{
  WiFi.mode(WIFI_STA);
  const char* wifissid = doc["wifissid"];
  const char* wifipwd = doc["wifipwd"];
  Serial.printf("Connecting to wifi: '%s' '%s'",wifissid, wifipwd);
  WiFi.begin(wifissid, wifipwd);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
    if (count++ > 60)
    {
      Serial.print("wifi connection timeout..");
      return false;
    }
  }

  Serial.println("");
  Serial.print("WiFi connected, IP= ");
  Serial.println(WiFi.localIP());
  setDateTime();
  return true;
}

void setDateTime() 
{
  configTime(0,0, "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) 
  {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

bool connmqtt()
{
  // Attempt to connect
  const char* mqttclient = doc["mqttclient"];
  const char* mqttuser = doc["mqttuser"];
  const char* mqttpwd = doc["mqttpwd"];
  Serial.printf("mqtt state: %d\r\n",client.state());
  Serial.printf("Attempting MQTT connection '%s' '%s' '%s'\r\n", mqttclient, mqttuser, mqttpwd);
  if (client.connect(mqttclient, mqttuser, mqttpwd)) 
  {
    Serial.println("connected() returned true");
    client.loop();
    delay(10);
    if (client.loop())
    {
      Serial.println("loop() returned true");
      client.subscribe("boiler/input");
      client.subscribe("boiler/echo");
      return true;
    }
  }
  Serial.printf("mqtt connect failed, state = %d\r\n", client.state());
  return false;
}

void sendmqtt(String topic, String message)
{
  if (client.state() != 0)
  {
    Serial.println("Can't send - mqtt not connected");
  }
  int len = topic.length();
  char topicB[len+1];
  topic.toCharArray(topicB,len+1);
  len = message.length();
  char messageB[len+1];
  message.toCharArray(messageB, len+1);
  Serial.println("MQTT Send '" + String(topicB) + "' '" + String(messageB));
  client.publish(topicB, messageB);
}

String getIsoTime()
{
  char b[20];
  struct tm ti;
  
  if(!getLocalTime(&ti)){
    Serial.println("Failed to obtain time");
    return "????-??-??T??:??:??";
  }
  int bLen = sprintf(b,"%d%d%d%d%d%d",ti.tm_year+1900,ti.tm_mon+101,ti.tm_mday+100,ti.tm_hour+100,ti.tm_min+100,ti.tm_sec+100);
  b[4] = '-';
  b[7] = '-';
  b[10] = 'T';
  b[13] = ':';
  b[16] = ':';
  return String(b);
}

void processSerial2()
{
  while (Serial2.available()) 
  {
    char c = Serial2.read();
    if (c == '\r')
    {
      // ignore
    }
    else if (c == '\n')
    {
      // end line..
      String topic ;
      buff[bPtr++] = 0;  // null terminate
      String message(buff);
      bool addtimestamp = false;
      if (bPtr > 4)
      {
        String key = message.substring(0,3);
        message = message.substring(4,bPtr);
        
        if (key == "f:0")
        {
          topic = "boiler/cmd";
        }
        else if (key == "f:1")
        {
          topic = "boiler/log";
          addtimestamp = true;
        }
        else if (key == "f:2")
        {
          topic = "boiler/cycle";
          addtimestamp = true;
        }
        else if (key == "f:3")
        {
          topic = "boiler/demand";
          addtimestamp = true;
        }
        else
        {
          topic = "boiler/" + key;
        }
      }
      if (addtimestamp)
      {
        message = getIsoTime() + "," + message;
      }
      sendmqtt(topic,message);
      bPtr = 0;
    }
    else if (bPtr < bLen-1)
    {
      buff[bPtr++] = c;
    }
  }
}

void setup() 
{
  Serial.begin(115200);
  Serial.println("Boiler MQQT interface - expect input / output to serial2 at 115200");
  Serial2.begin(115200);
  if(!SPIFFS.begin(false))
  {
      Serial.println("SPIFFS Mount Failed");
      return;
  }
  readProps(SPIFFS, "/properties.txt");
  getCertificate(SPIFFS, "/cacert.txt");
  //espClient.setCACert(ca_cert);
  espClient.setCACert(cacert2);
  //espClient.setInsecure();
  const char* mqttserver = doc["mqttserver"];
  int mqttport = 8883;
  Serial.printf("set mqttserver: %s:%d \r\n",mqttserver,mqttport);
  client.setServer(mqttserver, mqttport);
  client.setCallback(callback);
  //SerialBT.begin("MQTT", true);

}

unsigned long retryAt = 0;
unsigned long retryInterval = 30000;

void loop() 
{
    
  delay(10);
  processSerial2();
  if (!WiFi.isConnected() && millis())
  {
    if (millis() < retryAt)
    {
      return;
    }
    if (!connwifi() || !WiFi.isConnected())
    {
      retryAt = millis() + retryInterval;
      return;
    }
  }
  if (!client.loop())
  {
    if (millis() < retryAt)
    {
      return;
    }
    if (!connmqtt());
    {
      retryAt = millis() + retryInterval;
      return;
    }
  }
}
