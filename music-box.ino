/*
 *  Author: Jaicob Schott
 *  Date: 29 Sept 2020
 *  Using an analogue input on pin A0,
 *    outputs a song with two channels to 8ohm speakers
 *    using pins 8 and 9
 */
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "tone.h"
#include "music-box.h"

Tone chan[CHANNEL_N];      // create the tones
int sensorPin = INPUT_PIN; // select the input pin for the potentiometer
float sensorValue = 0;     // variable to store the value coming from the sensor
float multiplier = 0;      //Multiplier for altering the note durations
File songDir;
File chanFile[CHANNEL_N];
String curSong;
char artist[NAME_SIZE];
char track[NAME_SIZE];
int bpm = 60;
unsigned int chanData[2][2][LINE_SIZE];
int noteCount[CHANNEL_N] = {0, 0};
char chanID[CHANNEL_N] = {'1', '2'};
volatile bool interrupted = false;

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("//init...");
#endif

  SD.begin(10);
  chan[0].begin(CHANNEL_1);
  chan[1].begin(CHANNEL_2);
  songDir = SD.open(MUSIC_DIR);
  pinMode(INTER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INTER_PIN), intChangeSong, CHANGE);
  nextSong();

#ifdef DEBUG
  Serial.println("//setup complete...");
#endif
}

void intChangeSong()
{
  interrupted = true;
#ifdef DEBUG
  Serial.println("INTERRUPT");
#endif
}

void nextSong()
{
  if (chanFile[0])
    chanFile[0].close();
  if (chanFile[1])
    chanFile[1].close();
  File entry = songDir.openNextFile();
  if (!entry)
  {
    songDir.rewindDirectory();
    entry = songDir.openNextFile();
  }

  //Create filename of next file
  curSong = MUSIC_DIR;
  curSong.concat(entry.name());

  //Get details from the song using opened file
  initSong(entry);
  entry.close();

#ifdef DEBUG
  Serial.println("Loaded the next song!");
  Serial.println(curSong);
  Serial.println(track);
  Serial.println(artist);
  Serial.println("BPM to be implemented");
#endif
}

void openSong() //Used for opening the current song for first or replay
{
  delay(50);
  //Open the current file for each channel
  chanFile[0] = SD.open(curSong.c_str());
  chanFile[1] = SD.open(curSong.c_str());
  //Seek the opened file pointer to the beginning of the channels note sections
  initChannel(chanFile[0], chanID[0]);
  initChannel(chanFile[1], chanID[1]);
}

void initSong(File readDetail)
{
  //Get track name from file
  char temp[NAME_SIZE];
  char curChar = readDetail.read();
  int ii = 0;
  while ((curChar != '-1') && (curChar != ',') && (ii < (NAME_SIZE - 1)))
  {
    temp[ii] = curChar;
    curChar = readDetail.read();
    ii++;
  }
  temp[ii] = '\0';
  strncpy(track, temp, NAME_SIZE);

  //Get artist name from file
  curChar = readDetail.read();
  ii = 0;
  while ((curChar != '-1') && (curChar != ',') && (curChar != '\n') && (ii < (NAME_SIZE - 1)))
  {
    temp[ii] = curChar;
    curChar = readDetail.read();
    ii++;
  }
  temp[ii] = '\0';
  strncpy(artist, temp, NAME_SIZE);

  openSong();
}

void initChannel(File chan, char number)
{
  char curChar = chan.read();
  bool repeat = true;
  while ((curChar != '-1') && repeat)
  {
    if (curChar == '[')
    {
      curChar = chan.read();
      if (curChar == number)
      {
        repeat = false;
        while ((curChar != '-1') && (curChar != ']'))
        {
          curChar = chan.read();
        }
      }
    }
    curChar = chan.read();
  }
}

bool nextLine()
{
  delay(50);

  bool cont = true;
  for (int ii = 0; ii < CHANNEL_N; ii++)
  {
    if (chanFile[ii].available())
    {
      char curChar = chanFile[ii].read();
      char curLine[LINE_SIZE * 16];
      int jj = 0;
      if ((curChar != '['))
      {
        while ((curChar != '-1') && (curChar != '\n'))
        {
          curLine[jj] = curChar;
          curChar = chanFile[ii].read();
          jj++;
        }
        curLine[jj] = '\0';
      }
      if (jj == 0)
      {
        cont *= false;
      }
      else
      {
        int kk = 0;
        int noNotes = 0;
        int pos = 0;
        char *curTok = strtok(curLine, "|");
        sscanf(curTok, "%d", &noNotes);
        while (kk < noNotes)
        {
          curTok = strtok(NULL, ",");
          sscanf(curTok, " %d{%d} ", &chanData[ii][0][kk], &chanData[ii][1][kk]);
          kk++;
        }
        noteCount[ii] = kk;
      }
    }
    else
    {
      cont *= false;
    }
  }
  return cont;
}

void playData()
{
#ifdef DEBUG
  Serial.println("playData()");
#endif
  int curNote[2] = {0, 0};
  bool finish[2] = {0, 0};

  while (!(finish[0] && finish[1]) && !interrupted)
  {
    sensorValue = analogRead(sensorPin);
    float nextMult = constrain((3 * pow(0.85, sensorValue)), 0.1, 5);
    float delta = (nextMult - multiplier) / 5; // used to make sure multiplier smoothly changes
    if (delta < 0)
    {
      multiplier = constrain(multiplier + delta, nextMult, 5);
    }
    else if (delta > 0)
    {
      multiplier = constrain(multiplier + delta, 0.1, nextMult);
    }

    if ((sensorValue > 0) && !interrupted)
    {
      for (int ii = 0; ii < 2; ii++)
      {
        if (!chan[ii].isPlaying() && !finish[ii])
        {
          if (curNote[ii] < noteCount[ii])
          {
            chan[ii].play(chanData[ii][0][curNote[ii]], multiplier * 60000 / (bpm * chanData[ii][1][curNote[ii]]));
            curNote[ii]++;
          }
          else
          {
            finish[ii] = 1;
          }
        }
      }
    }
  }
#ifdef DEBUG
  Serial.println("END playData()");
#endif
}

void loop()
{
  while (!interrupted)
  {
    if (nextLine())
    {
      playData();
    }
    else
    {
      openSong();
    }
  }

  nextSong();
  interrupted = false;
}
