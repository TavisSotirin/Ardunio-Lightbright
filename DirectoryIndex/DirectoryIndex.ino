#include <core.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <Adafruit_Protomatter.h>
#include <Regexp.h>
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "secrets.h"

struct siteData
{
  int size = 0;
  String data = "";
};

String activeIndexLoc = "/";
siteData buildDirectory(String root);

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

void handleClient(WiFiClient* client)
{
  String currentLine = "";
  String getLine = "";
  bool sent = false;
  bool updated = false;

  int gotBytes = 0;

  if (client->connected() && client->available())
  {
    Serial.println("\n\n-----------------------");
    currentLine = client->readStringUntil('\n');

    // Send icon - really does nothing but return response to browser
    if (currentLine.startsWith("GET /favicon.ico"))
    {
      Serial.println("SENDING ICON DATA");
      
      client->println();
      client->println();
      client->flush();

      Serial.println("-----------------------\n\n");
    }
    // Received file data to upload
    else if (currentLine.startsWith("GET /send"))
    {
      // ADD CODE TO PARSE PASSED FILE DATA
    }
    // Send index site
    else if (currentLine.startsWith("GET /"))
    {
      int endParseIndex = currentLine.lastIndexOf(" HTTP/");

      activeIndexLoc = currentLine.substring(4, (endParseIndex != -1) ? endParseIndex : currentLine.length());
      siteData site = buildDirectory(activeIndexLoc);

      client->println("HTTP/1.1 200 OK");
      client->print("Content-Length: ");
      client->println(site.size);
      client->println("Content-type: text/html; charset=utf-8");
      client->println();
      
      sendFileData(client, &site);

      client->println();
      client->flush();
      client->println();
    }
  }

  // Terminate connection
  client->stop();
  Serial.println("Client connection terminated\n\n");
}

void sendFileData(WiFiClient* client, siteData* site)
{
  char* ptr = (char*)site->data.c_str();
  int size = site->size;
  int byteTotal = 0;

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

IPAddress clientIP;
int clientPort;

void getClientInfo(WiFiClient* client)
{
  IPAddress clientIP = client->remoteIP();
  int clientPort = client->remotePort();
  Serial.printf("Client IP: %d %d %d %d \nClient Port: %d\n",clientIP[0],clientIP[1],clientIP[2],clientIP[3],clientPort);
}

//// Set up file system ///////////////////////////////////////////////////////////////
// Open reference to QSPI flash memory (2mb total size)
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
// File system reference and file to open
FatFileSystem fatfs;
File curFile;
int usedSize;

void showDirectory(File dir, int numTabs)
{
  const size_t bufsize = 1024;
  char buffer[bufsize];

  while(true)
  {
    File entry = dir.openNextFile();
    if(!entry) break;

    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }

    entry.getName(buffer,bufsize);
    Serial.print(buffer);

    if (entry.isDirectory()) {
      Serial.println("/");
      showDirectory(entry, numTabs + 1);

    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      usedSize += entry.size();
      Serial.println(entry.size(), DEC);
    }

    entry.close();
  }
}

siteData buildFile(File file)
{
  siteData outData;

  const size_t bufsize = 1024;
  char buffer[bufsize]; 

  file.getName(buffer,bufsize);
  String fileName = String(buffer);
  String fileExt = fileName.substring(fileName.lastIndexOf(".") + 1);

  outData.data.reserve(file.size());
  outData.size = file.size();
  Serial.println("Attempting to send data for " + fileName);

  int fileSize = 0;

  if (fileName.endsWith(".html"))
  {
    while (true)
    {
      int readBytes = file.readBytes(buffer,bufsize);
      Serial.printf("Read %d bytes from file\n",readBytes);

      if (readBytes <= 0) break;

      outData.data += String(buffer);
      fileSize += readBytes;
    }
  }
  else
  {
    outData.data = "UNIDENTIFIED FILE EXTENSION";
    outData.size = outData.data.length();
  }
  
  //Serial.printf("Calc size: %d\tFile size %d\tStr size: %d\n",fileSize,file.size(),fileData.length());
  return outData;
}

siteData buildDirectory(String root)
{
  Serial.println("Request to build index of " + root);
  siteData outData;

  File dir = fatfs.open(root, FILE_READ);

  // If root couldn't be opened return nothing
  if (!dir) return outData;

  if (dir.isDirectory())
    Serial.println("Request is a directory");
  else
    Serial.println("Request is a file");

  // If root is not a directory return file data based on type
  if (!dir.isDirectory()) return buildFile(dir);

  String site1 = "<!doctype html><html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><title>Index of ";
  String site2 = "</title></head><body><h1>Index of ";
  String site3 = "</h1><hr><ul>";

  // Build site links for given directory
  String links = "";
  { 
    const size_t bufsize = 1024;
    char buffer[bufsize];

    while(true)
    {
      File entry = dir.openNextFile();
      if(!entry) break;

      entry.getName(buffer,bufsize);

      if (entry.isDirectory())
        links += "<li><pre><a href=\"" + root + (root.endsWith("/") ? "" : String("/")) + (String)buffer + "\">" + (String)buffer + "/</a></pre></li>";
      else
      {
        int fileSize = entry.size();
        String fileSizeStr = String((int)(fileSize / 1024)) + " kb  " + String(fileSize % 1024) + " b";

        links += "<li><pre><a href=\"" + root + (root.endsWith("/") ? "" : String("/")) + (String)buffer + "\">" + (String)buffer + "</a>" + "     " + fileSizeStr + "</pre></li>";
      }
      
      entry.close();
    }
  }

  String site4 = "</ul><hr></body></html>";

  outData.data = site1 + root + site2 + root + site3 + links + site4;
  outData.size = outData.data.length();
  
  return outData;
}

void addFile(String path, String* pData, int fileSize)
{
  // Open directory to add file to and check if filename aleady exists
  File dir = fatfs.open(path.substring(0,path.lastIndexOf("/")));

  while (true)
  {
    File entry = dir.openNextFile();
    if(!entry) break;

    const int bufSize = 1024;
    char* buf[bufSize];

    entry.getName(buf,bufSize);

    if (!entry.compareTo(path.substring(path.lastIndexOf("/") + 1)))
    {
      path = path.substring();
    }

  }

  // ADD CODE TO CHECK IF FILE ALREADY EXISTS? OR JUST OVERWRITE?
  File newFile = fatfs.open(path, FILE_WRITE);

  newFile.write((char*)path.c_str(), fileSize);
}

void setupFileSystem()
{
  Serial.println("Initializing file system on external flash...");
  
  // Init external flash
  flash.begin();

  // Open file system on the flash
  if ( !fatfs.begin(&flash) )
  {
    Serial.println("Error: filesystem does not exist.");
    while(1) yield();
  }

  Serial.println("File system initialization done\n");

  Serial.print("Flash size: "); Serial.print(flash.size() / 1024); Serial.println(" KB\n");


  File root = fatfs.open("/", FILE_READ);
  usedSize = 0;
  showDirectory(root, 0);

  Serial.print("Flash used size: "); Serial.print(usedSize / 1024); Serial.println(" KB\n");
}

void setup() {
  Serial.begin(9600);
  delay(2000);

  Serial.print("About to start file setup\n");
  setupFileSystem();
  setupWifi();

  Serial.println("Setup complete, moving to loop");
}

void loop() {
  WiFiClient client = server.available();

  if (client)
  {
    Serial.println("Handling client");
    handleClient(&client);
  }

}
