#include <core.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <Adafruit_Protomatter.h>
#include <Regexp.h>
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "secrets.h"

void sendWebsiteData(WiFiClient*);
void parseMatrixResponse(String*);
void sendPreset(WiFiClient*,bool);
int getSiteSize(bool);

///// Matrix status ///////////////////////////////////////////////////////////////
// off - display off
// still - nightlight - still image being dispayed
// gif - nightlight - gif image being displayed
// drawing - server - displaying image provided by web connection
/*

*/
enum mode
{
  off,
  still,
  gif,
  drawing
};

mode activeStatus = mode::off;

///// Generic debug functions - remove after debugging ///////////////////////////////////////////////////////////////
void print(String text)
{
  if (Serial)
    Serial.println(text);
}

void print(const char* text)
{
  if (Serial)
    Serial.println(text);
}

///// Set up buttons ///////////////////////////////////////////////////////////////
//voidFuncPtr downFunction = &testDown;
//voidFuncPtr upFunction = NULL;

void setupButtons()
{
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(UP, INPUT_PULLUP);
  setButtonFuncs(testDown, testUp);

  Serial.println("Buttons setup complete");
}

void setButtonFuncs(void (*downFunc)(), void (*upFunc)())
{
  attachInterrupt(digitalPinToInterrupt(DOWN), downFunc, FALLING);
  attachInterrupt(digitalPinToInterrupt(UP), upFunc, FALLING);
}

void testDown()
{
  Serial.println("DOWN PRESSED!");
}

void testUp()
{
  Serial.println("UP PRESSED!");
}

///// Set up server ///////////////////////////////////////////////////////////////
int wifi_status = WL_IDLE_STATUS;
WiFiServer server(80);

void connectToWifi(int retries = 10)
{
  do {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(SECRET_SSID);

    wifi_status = WiFi.begin(SECRET_SSID, SECRET_PASS);

    // wait 2 seconds for connection:
    delay(2000);
    retries--;
  } while (wifi_status != WL_CONNECTED && retries >= 0);

  if (wifi_status == WL_CONNECTED)
    Serial.println("Connected to wifi");
  else
    Serial.println("Failed to connect to wifi, continuing offline");
}

void setupWifi()
{
  WiFi.noLowPowerMode();

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed");
    return;
  }
  connectToWifi();

  if (wifi_status == WL_CONNECTED)
  {
    server.begin();
    Serial.print("Server started at IP ");
    Serial.println(WiFi.localIP());
  }
}

bool isWifi()
{
  return WiFi.status() == WL_CONNECTED;
}

void handleClient(WiFiClient* client)
{
  String currentLine = "";
  String getLine = "";

  int gotBytes = 0;

  while (client->connected()) 
  {
    if (client->available()) 
    {
      char c = client->read();
      gotBytes++;
      //Serial.write(c);

      if (c == '\n') 
      {
        // if the current line is blank, you got two newline characters in a row.
        // that's the end of the client HTTP request, so send a response:
        if (currentLine.length() == 0) 
        {
          // MIMETYPE header
          //client->println("HTTP/1.1 200 OK");
          //client->println("Content-type:text/html");
          //client->println();

          // HTML data
          //client->print("Click <a href=\"/H\">here</a> turn the LED on pin 9 on<br>");
          //client->print("Click <a href=\"/L\">here</a> turn the LED on pin 9 off<br>");
          //client->print("Click <a href=\"/matrix.matrix?data=fe12345d\">matrix data</a> send matrix data<br>");
          //sendWebsiteData(client);
          
          
  client->println("HTTP/1.1 200 OK");
  client->print("Content-Length: ");
  client->println(String(getSiteSize(true)));
  client->println("Content-type: text/html; charset=utf-8");
  client->println();
  sendPreset(client,true);
  client->println();
  client->flush();


          // Endline to indicate end of data stream
          client->println();
          break;
        }
        // Reset currentline
        else 
          currentLine = "";
      }
      // Non return carriage characters get appended to currentLine
      else if (c != '\r') 
        currentLine += c;

      // Check for GET request with matrix data
      if (currentLine.startsWith("GET /matrix."))
        getLine = currentLine;
    }
  }

  Serial.printf("Got %d bytes from client\n",gotBytes);

  if (getLine != "")
  {
    Serial.println("GOT DATA FROM CLIENT");
    //Serial.println("GET:");
    //Serial.println(getLine);
    parseMatrixResponse(&getLine);
  }

  // Terminate connection
  client->stop();
  Serial.println("Client connection terminated");
}

IPAddress clientIP;
int clientPort;

void getClientInfo(WiFiClient* client)
{
  IPAddress clientIP = client->remoteIP();
  int clientPort = client->remotePort();
  Serial.printf("Client IP: %d %d %d %d \nClient Port: %d\n",clientIP[0],clientIP[1],clientIP[2],clientIP[3],clientPort);
}

void handle2(WiFiClient* client)
{
  bool endOfStream = false;
  
  String getReq = "";

  size_t bytesRead = 0;
  Serial.println("\n--Client connection started--\n");

  getClientInfo(client);

  client->println("HTTP/1.1 200 OK");

  while (client->connected()) 
  {
    // Looking for /r/n/r/n
    String fromClient = "";
    char c = '\0';
    do
    {
      c = client->read();
      bytesRead++;

      if (c == '\r' && fromClient == "")
        endOfStream = true;

      fromClient += c;
    } while (client->available() && c != '\n');

    Serial.printf("**%s\n",fromClient.c_str());


    if (fromClient.startsWith("GET"))
      getReq = fromClient;

    if (endOfStream)
      break;
  }

  // respond here based on above
  if (getReq.startsWith("GET / HTTP"))
  {
    //client->println("HTTP/1.1 200 OK");
    client->println("Content-type: text/html; charset=utf-8");
    client->println("Clear-Site-Data: \"*\"");
    //client->println("\r\n\r");
    client->println();
    client->print(WELCOME_SITE);
    client->println();
    client->flush();
  }
  else if (getReq.startsWith("GET /fake"))
  {
    Serial.println("Sending website");
    //client->println("HTTP/1.1 200 OK");
    //client->println("Connection: Keep-Alive");
    //client->println("Connection: Close");
    client->print("Content-Length: ");
    client->println(String(getSiteSize(false)));
    client->println("Content-type: text/html; charset=utf-8");
    //client->println("Expect: 100-continue");
    //client->println("Keep-Alive: timeout=1000, max=100");
    //client->println("\r\n\r");
    client->println();
    //client->println("Hello");
    //sendWebsiteData(client);
    sendPreset(client,false);
    //sendHello(client);
    client->println();
    client->flush();
  }
  else if (getReq.startsWith("GET /real"))
  {
    Serial.println("Sending website");
    //client->println("HTTP/1.1 200 OK");
    client->print("Content-Length: ");
    client->println(String(getSiteSize(true)));
    client->println("Content-type: text/html; charset=utf-8");
    client->println();
    sendPreset(client,true);
    client->println();
    client->flush();
  }
  else if (getReq.startsWith("GET /favicon.ico"))
  {
    Serial.println("Sending favicon");
    //client->println("HTTP/1.1 200 OK");
    client->println("Content-type:text/html");
    //client->println("Connection: Close");
    //client->println("\r\n\r");
    client->println();
    client->print("<link rel=\"icon\" href=\"data:;base64,iVBORw0KGgo=\">");
    client->println();
    client->flush();
  }
  else if (getReq.startsWith("GET /matrix"))
  {
    Serial.println("Getting matrix data");
    //client->println("HTTP/1.1 200 OK");
    client->println("Content-type:text/html");
    client->println("\n");
    client->flush();
  }

  Serial.printf("GET:\n%s\nTotal bytes read: %d\n",getReq.c_str(),bytesRead);

  //delay(8);
  client->stop();
  Serial.println("\n--Client connection terminated--\n");
}

int getSiteSize(bool real)
{
  int byteTotal = 0;
  char* ptr;
  if (real)
    ptr = SITE_CODE_2;
  else
    ptr = SITE_CODE;

  while (ptr[0] != '\0')
  {
    byteTotal++;
    ptr++;
  }
  Serial.printf("%s site size is %d bytes\n",(real ? "Real" : "Fake"),byteTotal);
  return byteTotal;
}

void sendPreset(WiFiClient* client, bool real)
{
  //client->setTimeout(1000);
  int size = getSiteSize(real);

  char* ptr;
  if (real)
    ptr = SITE_CODE_2;
  else
    ptr = SITE_CODE;

  int byteTotal = 0;

if (!real)
{
  client->write(ptr,36);
  ptr += 36;
  byteTotal += 36;

  int lineSize = 120;
  while (ptr[0] != '<')
  {
    if (client->connected())
    { 
      client->write(ptr, lineSize);
      ptr += lineSize;
      byteTotal += lineSize;
      delay(1);
    }
    else
    {
      Serial.println("\nBREAKING EARLY");
      break;
    }
  }

  client->write(ptr,14);
  byteTotal += 14;
}
else
{
  int lineSize = 120;
  while (size > 0)
  {
    if (size < lineSize)
      lineSize = size;
    if (client->connected())
    { 
      client->write(ptr, lineSize);
      ptr += lineSize;
      byteTotal += lineSize;
      size -= lineSize;
      delay(1);
    }
    else
    {
      Serial.println("\nBREAKING EARLY");
      break;
    }
  }
}

  Serial.printf("Wrote %d bytes\n",byteTotal);
}

void sendHello(WiFiClient* client)
{
  client->print("<HTML><BODY>");
  for (int i = 0; i < 10000; i++)
  {
    if (i %2) client->print("HIDEY ");
    else client->print("HO! ");
  }
  client->print("</BODY></HTML>");
}

//// Set up file system ///////////////////////////////////////////////////////////////
// Open reference to QSPI flash memory (2mb total size)
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
// File system reference and file to open
FatFileSystem fatfs;
File curFile;

void setupFileSystem()
{
  print("Initializing file system on external flash...");
  
  // Init external flash
  flash.begin();

  // Open file system on the flash
  if ( !fatfs.begin(&flash) ) 
  {
    print("Error: filesystem is not existed. Please try SdFat_format example to make one.");
    while(1) yield();
  }

  print("File system initialization done");
}

///// Set up matrix ///////////////////////////////////////////////////////////////
// UPDATE - use new?
uint16_t IMAGE_MATRIX[MATRIX_SIZE];
uint16_t WEB_MATRIX[MATRIX_SIZE];
float brightness = 1.0;
bool updatingImage = false;

// REMOVE
const uint16_t unicorn[MATRIX_SIZE] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,59164,65503,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,40052,6473,21068,61278,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,42164,8781,13916,11316,18923,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48568,8813,16094,13591,27374,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65503,63390,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,59132,8488,8586,8651,37971,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,59164,25229,10406,10405,12551,33681,63390,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61245,63390,59132,0,0,0,0,0,0,0,0,0,0,0,61277,46455,37971,37939,44342,59165,0,61245,12519,25353,38155,38187,36043,21064,18890,63390,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,54874,23116,6181,8330,10413,10445,8330,6181,25261,25229,23240,38155,38187,38187,38187,38187,18983,29520,0,0,61277,61245,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,57019,61245,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,35858,6181,10479,16792,16825,16825,16825,16825,16792,10413,6212,36043,38187,38187,38187,38187,38187,36074,8357,50648,63390,8293,8325,21036,59165,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,40116,6375,8391,54906,0,0,0,0,0,0,0,0,0,0,0,0,0,0,35794,6215,14710,16825,16825,16825,16825,16825,16825,18873,16792,10437,38155,31722,23272,21128,23304,33866,38155,27529,12551,57019,18923,23240,21096,25229,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,57019,10406,13787,11316,21036,61277,0,0,0,0,0,0,0,0,0,0,0,0,46455,6182,16758,16825,18873,16825,16825,16825,16825,12561,12561,16791,6213,10469,12488,20875,22989,18795,8326,16839,38091,25320,8325,10438,25353,38123,8293,63390,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63390,18890,6440,8976,16094,13949,8716,6343,31600,0,0,0,0,0,0,0,0,0,0,61310,10438,14643,16825,16825,12594,10413,8331,10445,12562,6246,16974,6246,4132,25070,33489,33490,33490,33490,33489,16649,16839,38187,36043,36011,38187,38187,8325,65471,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,54939,6376,14014,16094,16094,16094,16094,13754,10406,0,0,0,0,0,0,0,0,0,0,33713,8330,16825,14643,6214,14601,22989,27182,22989,10407,16975,42558,36220,16974,12455,31408,25069,27182,33489,33489,33489,10374,33866,38155,38123,31721,21128,6180,33713,23181,21068,12552,12552,23116,0,0,0,0,0,0,0,0,0,0,0,0,23116,8749,16094,16094,16094,13884,6408,40052,0,0,0,0,0,0,0,0,0,63358,8326,14709,12527,8294,29263,33489,33489,25101,18795,10374,25591,61375,65503,44638,16942,6213,6212,8293,20843,33489,33489,18795,12614,10469,8325,8683,6635,9041,8944,11447,11317,13917,13949,8878,0,0,0,0,0,0,0,0,0,0,0,0,48535,6538,16094,16094,16094,13916,6278,61310,0,0,0,0,0,59165,33745,27374,40116,40084,8298,14643,8294,31376,33489,25102,8326,8781,11219,6343,29882,0,0,65503,29848,8294,31376,22988,14569,33489,33489,18794,6570,13852,13819,14046,13722,14046,11414,14014,11414,13949,13560,8391,0,0,0,0,0,0,0,0,0,0,0,0,38003,11057,13624,8781,8976,13787,6538,54906,0,0,0,0,52826,8358,10412,12528,6182,4132,12561,6215,27182,33490,27150,6311,13625,13884,6571,4133,25525,61343,0,65503,29849,8294,33489,33489,33489,33489,33489,8294,11251,14046,13624,14046,11414,13949,13819,13625,13981,8748,14665,54906,0,61277,27374,37939,0,0,0,0,0,0,0,0,54874,12552,18890,42197,35826,14697,18923,63390,0,0,0,63423,12584,12528,16825,12594,4100,4133,12595,12487,33489,33489,8326,13592,14046,6571,10479,12594,8424,34074,48831,40446,34174,14862,16649,31409,33489,31408,14569,6278,9073,16095,11414,13949,13982,13592,16094,11349,6408,33745,65503,0,54939,21036,8911,6440,35826,63390,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,42229,8297,16825,16825,16758,8331,12562,10446,20908,33489,23021,8781,14046,13624,8297,16825,6215,6248,8392,34141,34206,34206,34141,17007,8326,10407,8326,19121,27736,6538,14046,13981,11252,14046,13916,8813,12584,57020,0,0,0,8358,11089,13916,13657,8748,31600,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,25261,12561,18873,16825,16825,16825,16825,8330,27183,33489,16681,11382,14046,11316,10446,16792,4132,16758,8331,25558,34174,34206,34206,34174,34141,32028,34173,34174,34174,10570,13494,14046,13981,11154,6473,33745,63423,0,0,0,0,21036,9041,16094,16094,6440,42197,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63390,14697,14676,16825,16825,16825,16825,16825,6216,31376,33489,12520,13657,14046,11511,8330,16825,14643,16791,8364,25558,34174,34206,34206,34206,34206,34206,34206,34206,34206,29850,8359,8976,6603,6213,40084,0,0,0,0,0,0,33777,8976,11316,13560,6440,54939,0,0,0,0,0,0,0,0,0,0,0,0,65503,46423,25229,12519,12519,6213,14676,16825,16825,16825,16825,16825,6215,33457,33490,14601,13624,16094,14014,6473,10446,14677,12528,8359,32061,34206,34206,34206,34206,34206,34206,34206,34206,34206,34174,31996,21332,25590,19120,48568,0,0,0,0,0,0,50681,14697,25261,21003,18923,63390,0,0,0,0,0,0,0,0,0,0,0,56987,16777,23116,48568,63422,0,23181,12561,16825,16825,16793,12562,12560,6182,33457,33489,18794,11252,16094,16095,13917,9041,8748,6343,25590,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34174,34174,16942,52826,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,44342,25293,27407,56987,0,0,52793,10439,46422,0,0,0,0,40084,8330,16825,16825,8297,14568,10375,4100,31376,33489,25102,8781,16094,16094,16094,14046,14046,11219,19088,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,12650,61278,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65503,25229,6248,12594,12527,12552,37939,16777,6213,6181,18890,44310,0,59165,44342,31632,4100,14676,12594,12520,33489,12488,8294,33489,33489,31376,6246,13917,16094,16094,16094,16094,13852,8359,34141,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,32093,14697,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,33681,8330,16793,16825,8330,4100,10412,10447,10446,10412,8298,4133,12584,12650,21266,25558,19121,6214,8330,25069,33489,33489,33457,33489,25102,6246,4132,11284,16094,16094,16094,16094,16094,8846,23412,34174,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,29850,27374,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61278,6214,16792,16825,16825,16758,12594,6216,6538,11154,11186,6441,8359,27704,34174,34174,34206,34174,19055,4100,27183,33490,33490,33490,33489,8326,13690,6506,11414,16095,16094,16094,16094,16094,11511,14829,34174,34206,34206,34206,34206,34174,34174,32093,34206,34206,34206,34174,29850,25261,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48600,8330,16825,16825,16825,14677,6214,11349,13657,8651,6180,6278,29915,32029,32061,32029,34206,34206,34174,14861,22989,33489,33490,33490,33489,6311,16094,16094,14015,16094,16094,16094,16094,16094,13884,10537,34174,34206,34206,34206,21267,25623,14829,19121,34174,34206,34174,40478,48798,18891,42165,0,0,0,0,0,0,0,0,0,0,0,0,0,48535,8363,18873,16825,16792,6215,11317,11446,6277,25416,18983,23380,34174,21114,18937,18970,34206,34206,34206,25623,12487,33489,33490,33490,33489,8294,13852,16094,16094,16094,16094,16094,16094,16094,13819,10537,34206,34206,34206,34174,23380,16942,23412,34174,34206,40446,59263,0,0,63390,21036,48568,0,0,0,0,0,0,0,0,0,0,0,0,57052,6214,10446,10412,10412,8781,13949,6310,31753,38187,10470,34141,34174,27676,18905,27580,34206,34206,34206,34174,10570,22989,33489,33490,33489,18794,8911,14046,16094,16094,16094,16094,16094,16094,11187,17007,34174,34206,34206,34206,34206,34174,34174,34206,40478,65503,0,0,57019,61310,46422,35890,0,0,0,0,0,0,0,0,0,0,0,0,61278,8326,8976,8814,4132,13754,11284,16806,38187,38155,10537,34174,34206,34206,34142,34174,34206,34206,34206,34206,29882,8326,22989,33489,33489,31409,10375,11122,14014,16094,16095,16094,16094,11414,8391,29915,34174,34206,34206,34206,34206,34206,34206,36286,63423,0,0,0,0,0,35890,44277,0,0,0,0,0,0,0,0,0,0,0,0,33713,8911,14046,11154,8944,13722,8748,25416,38187,38155,10537,34174,34206,34206,34206,34206,34206,34206,34206,34206,34174,29850,10537,12488,22989,29296,27215,8294,6441,9041,11316,9073,6506,4132,23412,34174,34174,34206,34206,34206,34206,34206,34174,48831,0,54906,0,0,0,63358,10471,29487,0,0,0,0,0,0,0,0,0,0,0,0,16777,13754,16094,13917,6440,10469,4100,31818,38187,38187,10470,10505,31996,34206,34206,34206,34206,34206,34206,34206,34206,34206,34174,25591,16942,12651,10570,12716,21266,21331,19121,21331,23509,21233,12683,8359,14796,23445,32061,34174,34206,34206,34174,59263,0,23149,54906,35826,61277,21003,42165,12551,54939,0,0,0,0,0,0,0,0,0,0,0,12551,13884,14046,11154,16871,31786,10437,38123,38187,23240,8359,25591,34174,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34174,34206,34174,34206,34206,34174,34174,34206,34206,34206,34174,34206,34141,25591,14894,8359,16942,29947,34174,34206,0,0,59165,29487,18891,14665,46422,0,40052,29487,0,0,0,0,0,0,0,0,0,0,0,21003,11511,16094,8813,27496,38187,38187,31786,12582,14796,31995,34174,34206,34174,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34174,32060,6246,8295,12683,12715,23116,18890,10408,8293,42197,63390,0,0,61277,10439,0,0,0,0,0,0,0,0,0,0,0,29519,6570,16062,8878,25416,31753,14694,6246,25558,34174,34206,34206,34206,34206,34174,27704,10505,34141,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34206,34141,29915,34174,34206,34206,34206,34206,25590,12526,18839,6215,8330,18839,18872,10446,37939,0,0,0,0,0,16777,57052,0,0,0,0,0,0,0,0,0,0,25294,10406,8814,11154,12549,6246,14642,8362,31995,34206,34206,34206,34206,32028,16975,4132,19088,32093,34206,34174,34174,34206,34206,34206,34206,34206,34206,34174,34206,34174,31996,19056,6180,10472,16942,25558,32093,34174,12684,16791,18905,6215,33713,8361,10411,31600,0,0,0,0,0,0,25261,50680,0,0,0,0,0,0,0,0,0,0,27406,46423,29519,6245,4100,8330,18873,14676,17007,34174,34174,32028,19120,10439,37938,52793,33713,18923,12650,17007,21299,25590,29817,29883,29915,29915,27769,23445,17040,12618,18955,40084,61277,63390,46422,27374,10439,14796,10411,18905,14675,18955,0,59164,54939,0,0,0,0,0,0,0,25294,48568,0,0,0,0,0,0,0,0,0,0,33777,38003,0,59132,40116,8326,16790,18905,8363,23444,16942,10471,35858,63358,0,0,0,0,63422,52826,44309,35858,29519,25294,25294,25294,29551,38003,48600,63390,0,0,0,0,0,0,63422,42196,12552,8330,8327,54874,0,0,0,0,0,0,0,0,0,0,23116,52793,0,0,0,0,0,0,0,0,0,0,50648,21036,0,0,0,44277,8296,12560,6216,16810,42229,63423,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65503,50713,59164,0,0,0,0,0,0,0,0,0,0,0,10471,61310,0,0,0,0,0,0,0,0,0,0,65503,12551,57019,0,0,0,46422,35826,54906,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,54874,16778,0,0,0,0,0,0,0,0,0,0,0,0,40084,23116,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,27374,40084,0,0,0,0,0,0,0,0,0,0,0,0,65503,18923,37939,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,54874,12551,63390,0,0,0,0,0,0,0,0,0,0,0,0,0,61277,16777,37939,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61310,14665,46455,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,61277,21003,23148,59164,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63390,21003,33745,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65503,40052,10471,27407,52761,65503,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,57052,16777,31600,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63390,42197,21003,10406,18890,25229,27406,35858,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63422,61245,0,0,0,0,0,0,0,0,0,0,0,63390,37939,10471,44310,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65503,57019,50713,50649,23116,27374,63390,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,59164,16810,8358,25261,46422,61310,0,0,0,0,0,65471,50713,31632,10439,27407,59197,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65471,31568,14664,50680,0,0,0,0,0,0,0,0,0,0,0,0,0,42197,10439,42197,65471,46422,25294,12519,12552,18955,21036,21035,14664,10471,23116,40084,61277,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,50648,16777,21003,44342,63390,0,0,0,0,0,0,0,61245,40051,14664,23149,57052,0,0,0,0,0,63358,57019,54939,56987,61277,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,48536,25261,10439,16810,29455,33777,35858,33745,25294,14665,12584,31600,54939,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,65503,57019,46487,42197,40052,42229,48600,59165,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,};

// Define pins for rgb matrix
// UPDATE - hard code to matrix constructor?
uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};  
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;

Adafruit_Protomatter matrix(
  64,          // Width of matrix (or matrix chain) in pixels
  4,           // Bit depth, 1-6
  1, rgbPins,  // # of matrix chains, array of 6 RGB pins for each
  5, addrPins, // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin, // Other matrix control pins
  false);      // No double-buffering here (see "doublebuffer" example)

void setupMatrix()
{
  ProtomatterStatus mat_status = matrix.begin();
  print("Initializing matrix...");
  if (mat_status != PROTOMATTER_OK)
  {
    print("Error setting up matrix");
    while(1) yield();
  }
  print("Matrix setup complete");
}

///// Global functions - Must be located down here to have access to all vars above ///////////////////////////////////////////////////////////////
void sendWebsiteData(WiFiClient* client)
{
  curFile = fatfs.open(WEBSITE, FILE_READ);

  if (curFile)
  {
    int fileSize = curFile.size();

    Serial.printf("Filesize: %d\n",fileSize);

    char* megaBuf = new char[fileSize];
    curFile.rewind();

    curFile.read(megaBuf, fileSize);
    int outBytes = 0;
    int sentBytes = 0;
    char* ptr = megaBuf;
    while (outBytes < fileSize)
    {
      if (outBytes + BUFFER_SIZE > fileSize)
        sentBytes = fileSize - outBytes;
      else
        sentBytes = BUFFER_SIZE;

      client->write(ptr,sentBytes);
      //client->flush();
      delay(1);
      
      outBytes += sentBytes;
      ptr += sentBytes;
    }

/*
    char buffer[BUFFER_SIZE];
    size_t totalBytesRead = 0;
    size_t lastBytesRead = 0;
    size_t lastBytesWritten = 0;
    size_t totalBytesWritten = 0;

    while(curFile.available())
    {
      lastBytesRead = curFile.readBytes(buffer, BUFFER_SIZE);
      // UPDATE - REMOVE AFTER CONFIRM DATA IS ALL GETTING SENT
      totalBytesRead += lastBytesRead;

      //buffer[lastBytesRead] = '\0';

      if (client->available())
      {
        //lastBytesWritten = client->print(buffer);
        totalBytesWritten += lastBytesRead;

        client->write(buffer, lastBytesRead);
        client->flush();
      }
      else
      {
        Serial.println("LEAVING SEND DATA LOOP EARLY!");
        break;
      }
    }

    Serial.printf("Bytes read to send to client: %d\nBytes written to client: %d\n",totalBytesRead,totalBytesWritten);
*/
    curFile.close();
    delete megaBuf;
  }
  else 
    print("Error opening website file\n");
}

void parseMatrixResponse(String* fromWebsite)
{
  char* data = (char*)fromWebsite->c_str();

  MatchState ms;
  ms.Target(data);
  char result = ms.Match ("data=([%a%d]+)", 0);

  if (result == REGEXP_MATCHED)
  {
    print("Found data from client\n");
    matrix.fillScreen(0);

    data += ms.MatchStart + 5;
    uint16_t* internalPtr = matrix.getBuffer();
    char val[5];
    val[4] = '\0';

    //Serial.println("DATA:");

    for (int i = 0; i < MATRIX_SIZE * 4; i += 4)
    {
      memcpy(val, data + i, 4);

      //Serial.printf("%s",val);

      /// UPDATE - STORE IN WEB_MATRIX AND MEMCPY TO INTERNALPTR
      int index = int(i / 4);
      uint16_t color = (uint16_t)strtol(val, NULL, 16);
      internalPtr[index] = color;
      //WEB_MATRIX[index] = color;
    }

    Serial.println("\nShowing matrix");
    matrix.show();
  }
  else
    print("Error parsing regex for matrix data from website\n");
}

void setup()
{
  // Setup serial connection if needed
  Serial.begin(9600);
  delay(1000);

  setupMatrix();
  setupButtons();
  setupFileSystem();
  setupWifi();

  matrix.fillScreen(0xD3);
  matrix.show();

  delay(1000);

  matrix.fillScreen(0);
  matrix.show();

  Serial.println("Setup complete, moving to loop");
}

void loop()
{
  WiFiClient client = server.available();

  if (client)
  {
    Serial.println("Handling client");
    handleClient(&client);
    //handle2(&client);
  }
}