// NOTE - BUTTONS ARE [NOT] INTERRUPTING

#define UP 2
#define DOWN 3

extern void displayImage();
extern void changeBrightness(bool);

void testDown()
{
  if (Serial)
    Serial.println("DOWN INTERUPT!!!!");

  changeBrightness(false);
}
void testUp()
{
  if (Serial)
    Serial.println("UP INTERUPT!!!!");

  changeBrightness(true);
}

void setupButtons()
{
  #if defined(DOWN)
    pinMode(DOWN, INPUT_PULLUP);
  #endif
  #if defined(UP)
    pinMode(UP, INPUT_PULLUP);
  #endif

  attachInterrupt(digitalPinToInterrupt(DOWN), testDown, FALLING);
  attachInterrupt(digitalPinToInterrupt(UP), testUp, FALLING);
}

void buttonDownCheck(void func(), bool continuous = false, unsigned long interval = 0)
{
  #if defined(DOWN)
    if(!digitalRead(DOWN)) {
      if (Serial)
        Serial.println("DOWN HAS BEEN PRESSED");

      unsigned long previousMillis = 0;

      // Wait for release
      while(!digitalRead(DOWN))
      {
        unsigned long currentMillis = millis();

        if (continuous && (currentMillis - previousMillis >= interval) )
        {
          func();
          previousMillis = currentMillis;
        }
      } 

      if (!continuous)
        func();
    }
  #endif
}

void buttonUpCheck(void func(), bool continuous = false, unsigned long interval = 0)
{
  #if defined(UP)
    if(!digitalRead(UP)) {
      if (Serial)
        Serial.println("UP HAS BEEN PRESSED");
      
      unsigned long previousMillis = 0;
      
      // Wait for release
      while(!digitalRead(UP))
      {
        unsigned long currentMillis = millis();

        if (continuous && (currentMillis - previousMillis >= interval) )
        {
          func();
          previousMillis = currentMillis;
        }
      } 

      if (!continuous)
        func();
    }
  #endif
}