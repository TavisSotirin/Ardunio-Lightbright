#pragma once
#include <SPI.h>
#include "SdFat.h"  
#include "Adafruit_SPIFlash.h"

// Open reference to QSPI flash memory (2mb total size)
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

// File system reference and file to open
FatFileSystem fatfs;
File curFile;

void setupFileSystem()
{
  if (Serial)
    Serial.print("Initializing Filesystem on external flash...");
  
  // Init external flash
  flash.begin();

  // Open file system on the flash
  if ( !fatfs.begin(&flash) ) {
    if (Serial)
      Serial.println("Error: filesystem is not existed. Please try SdFat_format example to make one.");
    while(1) yield();
  }

  if (Serial)
    Serial.println("initialization done.");
}