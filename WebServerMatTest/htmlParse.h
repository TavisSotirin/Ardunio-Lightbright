#pragma once
#include <coreMatrix.h>
#include <Regexp.h>

void parseGET(String line)
{
  if (line.startsWith("GET /matrix.matrix"))
  {
    Serial.println("GET matrix data");

    MatchState ms;
    ms.Target((char*)line.c_str());
    char result = ms.Match("(%a+)=([%a%d]+)");

    if (result > 0)
    {
      Serial.println("Found matrix data in string");

      char buf[line.length()];
      ms.GetCapture(buf,0);
      ms.Target(buf);
      
      result = ms.Match("=[%a%d]+");

      if (result > 0)
      {
        ms.GetCapture(buf,0);
        uint16_t* matBuffer = matrix.getBuffer();
        String capture = String(buf).substring(1);

        for (int i=0; i < capture.length(); i+=4)
        {
          if (capture[i] == '\0')
            break;

          int index = int(i / 4);
          int value = (int)strtol(capture.substring(i,i+4).c_str(), NULL, 16);
          matBuffer[index] = value;
          IMAGE_MATRIX[index] = value;
        }

        matrix.show();
      }
    }
  }
}

void sendHTMLData(const char* filename)
{
  curFile = fatfs.open(filename, FILE_READ);

  // if file pointer is valid and is an open file
  if (curFile)
  {
    String str = "";
    char* ptr;
    uint16_t color = 0;
    int i = 0;

    while(curFile.available() && i < IMAGE_SIZE)
    {
      // read a line
      str = curFile.readStringUntil('\n');

      // convert text to int for color value, or set to 0 if failed
      color = (uint16_t)strtol(str.c_str(), &ptr, 10);

      // set color in matrix and increment index
      IMAGE_MATRIX[i] = color;
      i++;
    }

    curFile.close();
  }
  else 
  {
    if (Serial)
      Serial.printf("Error opening %s",filename);
  }
}