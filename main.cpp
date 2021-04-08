
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <base64.h>

WiFiMulti wifiMulti;

//how many clients should be able to telnet to this ESP32
#define MAX_SRV_CLIENTS 3
#define CTRLLED 2
#define ARRAYSIZE 4
#define HISTORY 10
#define RETRY 4

const char *ssid = "ShadesInside";
const char *password = "broodje5";
int clientNumber[ARRAYSIZE];
String commandHistory[ARRAYSIZE];
String ipHistory[ARRAYSIZE];
bool isConnected[ARRAYSIZE];

WiFiServer server(23);
WiFiClient serverClients[MAX_SRV_CLIENTS];
HTTPClient http;
String encoded = "";
String dBuff = "";
HTTPClient httpWS;

void setup()
{

  pinMode(CTRLLED, OUTPUT);
  digitalWrite(CTRLLED, HIGH);

  Serial.begin(9600);
  Serial.println("\nConnecting");

  wifiMulti.addAP(ssid, password);

  Serial.println("Connecting Wifi ");
  for (int loops = 10; loops > 0; loops--)
  {
    if (wifiMulti.run() == WL_CONNECTED)
    {
      digitalWrite(CTRLLED, LOW);
      Serial.println("");
      Serial.print("WiFi connected ");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      break;
    }
    else
    {
      Serial.println(loops);
      delay(1000);
    }
  }
  if (wifiMulti.run() != WL_CONNECTED)
  {
    Serial.println("WiFi connect failed");
    delay(1000);
    ESP.restart();
  }

  server.begin();
  server.setNoDelay(true);

  Serial.print("Ready! Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println(" 23' to connect");
}

void loop()
{
  uint8_t i;
  if (wifiMulti.run() == WL_CONNECTED)
  {

    //check if there are any new clients
    if (server.hasClient())
    {
      for (i = 0; i < MAX_SRV_CLIENTS; i++)
      {
        //find free/disconnected spot
        if (!serverClients[i] || !serverClients[i].connected())
        {
          if (serverClients[i])
          {
            serverClients[i].stop();
            Serial.println("Left?");
          }
          serverClients[i] = server.available();
          if (!serverClients[i])
            Serial.println("available broken");

          Serial.print("New client: ");
          Serial.print(i);
          Serial.print(' ');
          Serial.println(serverClients[i].remoteIP());
          serverClients[i].write("Welcome! Telnet server ...V0.012.198...  \r\n  LoRa Wan SusKey 0.2.9 (c) 1999-2004 \r\n -*-contact CTO-Dianatec@7ol.eu for further information!-*-\r\n Busybox V1.12.4 (2020-04-13) Built-in shell \r\n Enter 'help for a list of built in commands. \r\n tty id \"/dev/pts/0 \"\r\n\r\n");

          http.begin("https://XXXXXXX.restdb.io/rest/intruders");                                                                     //Specify destination
          http.addHeader("cache-control", "no-cache");                                                                                       //Specify content-type header
          http.addHeader("x-apikey", "XXXXXXXXXXXXXX");                                                               //Specify content-type header
          http.addHeader("Content-Type", "application/json");                                                                                //Specify content-type header
          int httpResponseCode = http.POST("{\"ip\":\"" + serverClients[i].remoteIP().toString() + "\",\"command\":\"New connection...\"}"); //Send the actual POST request
          http.end();

          Serial.println("Write to Server : " + String(httpResponseCode));

          for (int ij = 0; ij < RETRY; ij++)
          {
            if (httpResponseCode != 201)
            {
              Serial.println("Write to server : FAILED RETRYING. (" + String(ij) + ")");
              delay(250);
              httpResponseCode = http.POST("{\"ip\":\"" + serverClients[i].remoteIP().toString() + "\",\"command\":\"New connection...\"}");
            }
            else{
            Serial.println("Write to server : SUCCESS.");
            break;
            }
          }

          ipHistory[i] = serverClients[i].remoteIP().toString();

          break;
        }
      }
      if (i >= MAX_SRV_CLIENTS)
      {
        //no free/disconnected spot so reject
        server.available().stop();
      }
    }
    //check clients for data
    for (i = 0; i < MAX_SRV_CLIENTS; i++)
    {
      if (serverClients[i] && serverClients[i].connected())
      {
        if (serverClients[i].available())
        {
          //get data from the telnet client and push it to the UART
          dBuff = "";
          while (serverClients[i].available())
          {
            dBuff = serverClients[i].readString();
          }
          if (dBuff.indexOf("bin") >= 0)
          {
            serverClients[i].write("Running in background!\r\n");
          }
          Serial.print(serverClients[i].remoteIP());
          Serial.print(" : ");
          Serial.print(i);
          Serial.print(" : ");
          Serial.println("" + dBuff);
          isConnected[i] = true;
          commandHistory[i] += "" + dBuff;

          /*
//Schrijven van data

  String encoded = base64::encode(dBuff);
  String valueToWrite = "{\"ip\":\""+ serverClients[i].remoteIP().toString() +"\",\"command\":\"Interaction: "+ encoded+"\"}";

      httpWS.begin("https://XXXXXXX.restdb.io/rest/intruders"); //Specify destination
      httpWS.addHeader("cache-control", "no-cache"); //Specify content-type header
      httpWS.addHeader("x-apikey", "XXXXXXXXXXXXXX"); //Specify content-type header
      httpWS.addHeader("Content-Type", "application/json"); //Specify content-type header
      int httpResponseCode = httpWS.POST(valueToWrite); //Send the actual POST request
        
          if(httpResponseCode != 201){
          delay(250);
          httpWS.POST(valueToWrite);
          }

        
          http.end();
         

          Serial.println("Write to server : "+String(httpResponseCode) +" : "+ dBuff);
 */

          clientNumber[i]++;
          if (clientNumber[i] == 1)
          {
            serverClients[i].write("\r\nusername: ");
          }
          else if (clientNumber[i] == 2)
          {
            serverClients[i].write("password: ");
          }

          else
          {
            serverClients[i].write("#> ");
          }
          dBuff = "";
        }
      }
      else
      {
        if (isConnected[i])
        {
          Serial.println(ipHistory[i] + " " + String(i) + " " + "left");
          isConnected[i] = false;
          Serial.println(commandHistory[i]);

          String encoded = base64::encode(commandHistory[i]);
          String valueToWrite = "{\"ip\":\"" + ipHistory[i] + "\",\"command\":\"Interaction: " + encoded + "\"}";
          Serial.println(valueToWrite);
          delay(150);
          httpWS.begin("https://XXXXXXX.restdb.io/rest/intruders");       //Specify destination
          httpWS.addHeader("cache-control", "no-cache");                         //Specify content-type header
          httpWS.addHeader("x-apikey", "XXXXXXXXXXXXXX"); //Specify content-type header
          httpWS.addHeader("Content-Type", "application/json");                  //Specify content-type header
          int httpResponseCode = httpWS.POST(valueToWrite);                      //Send the actual POST request

          for (int ii = 0; ii < RETRY; ii++)
          {
            if (httpResponseCode != 201)
            {
              Serial.println("Write to server : FAILED RETRYING. (" + String(ii) + ")");
              delay(250);
              httpResponseCode = httpWS.POST(valueToWrite);
            }
            else{
            Serial.println("Write to server : SUCCESS.");
            break;
            }
          }

          http.end();

          Serial.println("Write to server : " + String(httpResponseCode));

          ipHistory[i] = "";
          commandHistory[i] = "";
        }
        if (serverClients[i])
        {
          serverClients[i].stop();
          //Serial.println(String(i)+" "+" left");
        }
      }
    }
  }
  else
  {
    Serial.println("WiFi not connected!");
    for (i = 0; i < MAX_SRV_CLIENTS; i++)
    {
      if (serverClients[i])
        serverClients[i].stop();
    }
    delay(150);
  }
}
