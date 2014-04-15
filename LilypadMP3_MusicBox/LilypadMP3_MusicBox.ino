#include <SFEMP3Shield.h>
#include <SFEMP3ShieldConfig.h>
#include <SFEMP3Shieldmainpage.h>
/* 
 This code is based on "Trigger" example for Lilypad MP3 Player
 by Mike Grusin, SparkFun Electronics http://www.sparkfun.com
 
 Changes to code were made by 
 Keith Simpson and Liat Wassershtrom, Codasign http://codasign.com/
 for Pete Letanka's musical box
 April 2014
 
 * * *
 
 This sketch will play a specific audio file when a switch that attached to one of the five trigger
 inputs (labeled T1 - T5) is open. Audio file will stop playing when switch is closed. 
 
 You can place up to five audio files on the micro-SD card.
 These files should have the desired trigger number (1 to 5) as the first character in the filename. 
 
 
 // Uses the SdFat library by William Greiman, which is supplied
 // with this archive, or download from http://code.google.com/p/sdfatlib/
 
 // Uses the SFEMP3Shield library by Bill Porter, which is supplied
 // with this archive, or download from http://www.billporter.info/
 
 // We'll need a few libraries to access all this hardware!
 */

#include <SPI.h>            // To talk to the SD card and MP3 chip
#include <SdFat.h>          // SD card file system
#include <SFEMP3Shield.h>   // MP3 decoder chip

//Constants for the trigger input pins, which we'll place in an array.

const int TRIG1 = A0;
const int TRIG2 = A4;
const int TRIG3 = A5;
const int TRIG4 = 1;
const int TRIG5 = 0;
int trigger[5] = {
  TRIG1,TRIG2,TRIG3,TRIG4,TRIG5};
int triggerState[5] = {
  LOW,LOW,LOW,LOW,LOW};

// And a few outputs we'll be using:

const int ROT_LEDR = 10; // Red LED in rotary encoder (optional)
const int EN_GPIO1 = A2; // Amp enable + MIDI/MP3 mode select
const int SD_CS = 9;     // Chip Select for SD card

// Create library objects:

SFEMP3Shield MP3player;
SdFat sd;

// Set debugging = true if you'd like status messages sent 
// to the serial port. Note that this will take over trigger
// inputs 4 and 5. (You can leave triggers connected to 4 and 5
// and still use the serial port, as long as you're careful to
// NOT ground the triggers while you're using the serial port).

boolean debugging = false;

// Set interrupt = false if you would like a triggered file to
// play all the way to the end. If this is set to true, new
// triggers will stop the playing file and start a new one.

boolean interrupt = true;

// Set interruptself = true if you want the above rule to also
// apply to the same trigger. In other words, if interrupt = true
// and interruptself = false, subsequent triggers on the same
// file will NOT start the file over. However, a different trigger
// WILL stop the original file and start a new one.

boolean interruptself = true;

// We'll store the five filenames as arrays of characters.

char filename[5][13];


void setup()
{
  int x, index;
  SdFile file;
  byte result;
  char tempfilename[13];

  // Set the five trigger pins as inputs, and turn on the 
  // internal pullup resistors:

  for (x = 0; x <= 4; x++)
  {
    pinMode(trigger[x],INPUT);
    digitalWrite(trigger[x],HIGH);
  }

  // The board uses a single I/O pin to select the
  // mode the MP3 chip will start up in (MP3 or MIDI),
  // and to enable/disable the amplifier chip:

  pinMode(EN_GPIO1,OUTPUT);
  digitalWrite(EN_GPIO1,LOW);  // MP3 mode / amp off

  // If debugging is true, initialize the serial port:
  // (The 'F' stores constant strings in flash memory to save RAM)

  if (debugging)
  {
    Serial.begin(9600);
    Serial.println(F("Lilypad MP3 Player trigger sketch"));
  }

  // Initialize the SD card; SS = pin 9, half speed at first

  if (debugging) Serial.print(F("initialize SD card... "));

  result = sd.begin(SD_CS, SPI_HALF_SPEED); // 1 for success

  if (result != 1) // Problem initializing the SD card
  {
    if (debugging) Serial.print(F("error, halting"));

  }
  else
    if (debugging) Serial.println(F("success!"));

  // Start up the MP3 library

  if (debugging) Serial.print(F("initialize MP3 chip... "));

  result = MP3player.begin(); // 0 or 6 for success

  // Check the result, see the library readme for error codes.

  if ((result != 0) && (result != 6)) // Problem starting up
  {
    if (debugging)
    {
      Serial.print(F("error code "));
      Serial.print(result);
      Serial.print(F(", halting."));
    }
  }
  else
    if (debugging) Serial.println(F("success!"));

  // Now we'll access the SD card to look for any (audio) files

  if (debugging) Serial.println(F("reading root directory"));

  sd.chdir("/",true);
  while (file.openNext(sd.vwd(),O_READ))
  {
    // get filename

    file.getFilename(tempfilename);

    // Does the filename start with char '1' through '5'?      

    if (tempfilename[0] >= '1' && tempfilename[0] <= '5')
    {
      // Yes! subtract char '1' to get an index of 0 through 4.

      index = tempfilename[0] - '1';

      // Copy the data to our filename array.

      strcpy(filename[index],tempfilename);

      if (debugging) // Print out file number and name
      {
        Serial.print(F("found a file with a leading "));
        Serial.print(index+1);
        Serial.print(F(": "));
        Serial.println(filename[index]);
      }
    }
    else
      if (debugging)
      {
        Serial.print(F("found a file w/o a leading number: "));
        Serial.println(tempfilename);
      }

    file.close();
  }

  if (debugging)
    Serial.println(F("done reading root directory"));

  if (debugging) // List all the files we saved:
  {
    for(x = 0; x <= 4; x++)
    {
      Serial.print(F("trigger "));
      Serial.print(x+1);
      Serial.print(F(": "));
      Serial.println(filename[x]);
    }
  }

  // Set the VS1053 volume. 0 is loudest, 255 is lowest (off):

  MP3player.setVolume(10,10);

  // Turn on the amplifier chip:

  digitalWrite(EN_GPIO1,HIGH);
  delay(2);

  for (x = 0; x <= 4; x++)
  {
    triggerState[x] = digitalRead(trigger[x]);
  }
}


void loop()
{
  int t;              // current trigger
  static int last_t;  // previous (playing) trigger
  int countIfNewState, countIfSameState;
  byte result;

  // Step through the trigger inputs, looking for LOW signals.

  for(t = 1; t <= (debugging ? 3 : 5); t++)
  {
    // The trigger pins are stored in the inputs[] array.
    // Read the pin and check if it is LOW (triggered).

    if (digitalRead(trigger[t-1]) != triggerState[t-1])
    {
      // Wait for trigger to return high for a solid 50ms
      // (necessary to avoid switch bounce on T2 and T3
      // since we need those free for I2C control of the
      // amplifier)

      countIfNewState = 0;
      countIfSameState = 0;
      while((countIfNewState < 50) && (countIfSameState < 50))
      {
        if (digitalRead(trigger[t-1]) != triggerState[t-1]){
          countIfNewState++;
          countIfSameState=0;
        }
        else{
          countIfNewState = 0;
          countIfSameState++;
        }
        delay(1);
      } 
      if (countIfNewState>0){
        triggerState[t-1] = digitalRead(trigger[t-1]);

        if (debugging)
        {
          Serial.print(F("got trigger "));
          Serial.println(t);
        }

        // Do we have a valid filename for this trigger?

        if (filename[t-1][0] == 0)
        {
          if (debugging)
            Serial.println(F("no file with that number"));
        }
        else // We do have a filename for this trigger!
        {
          // If a file is already playing, and we've chosen to
          // allow playback to be interrupted by a new trigger,
          // stop the playback before playing the new file.

          if(triggerState[t-1] == LOW){ // switch is closed
            MP3player.stopTrack();

          }
          else{//switch is open
            result = MP3player.playMP3(filename[t-1]);
          }

          // Play the filename associated with the trigger number.
          // (If a file is already playing, this command will fail
          //  with error #2).

          if (result == 0) last_t = t;  // Save playing trigger

          if(debugging)
          {
            if(result != 0)
            {
              Serial.print(F("error "));
              Serial.print(result);
              Serial.print(F(" when trying to play track "));
            }
            else
            {
              Serial.print(F("playing "));
            }
            Serial.println(filename[t-1]);
          }
        }
      }
    }
  }
}
