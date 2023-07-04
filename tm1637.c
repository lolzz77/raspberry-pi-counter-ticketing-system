#include <stdio.h>
#include <unistd.h>
#include <lgpio.h>
#include "tm1637.h"
//
// Larry's TM1637 library
// Copyright (c) 2018 BitBank Software, Inc.
// Project started 3/17/2018
// written by Larry Bank (bitbank@pobox.com)
//
// The TM1637 from Titan Micro Electronics is a 7-segment LED controller
// It features a 2-wire interface (Clock + Data), but doesn't conform to I2C nor SPI standards
// It can control up to 6 7-segment digits along with decimal points or colons as well as has
// 16 button/keyboard inputs
// The datasheet says that it runs on 5V with 5V logic, but I've run it on 3.3V (both Vcc and signals)
// and it seems to behave correctly.
// Since the communication protocol is custom, it requires "bit-banging" of 2 IO pins to control it.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//
// table which translates a digit into the segments
//
const unsigned char cDigit2Seg[] = {0x3f, 0x6, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
static unsigned char bClockPin, bDataPin;
static int h = -1;

#define CLOCK_DELAY 5
#define LOW 0
#define HIGH 1
#define LFLAGS 0

//
// Initialize the tm1637 by specifying the digital pins used to communicate
// returns 0 for success, -1 for failure
// 
int tm1637Init(unsigned char bClock, unsigned char bData)
{

   /* get a handle to the gpiochip */
   h = lgGpiochipOpen(0);
   if (h >= 0)
   {
	   bClockPin = bClock;
	   bDataPin = bData;

	   lgGpioClaimOutput(h, LFLAGS, bClockPin, LOW);
	   lgGpioClaimOutput(h, LFLAGS, bDataPin, LOW);
	   return 0;
   }
   else
   {
	   printf("Error opening GPIO Chip!\n");
	   return -1;
   }

} /* tm1637Init() */


//
// Close the tm1637 port pins
//
void tm1637Close() {
   if (h >= 0)
   {
	  lgGpioFree(h, bClockPin);
	  lgGpioFree(h, bDataPin);
      lgGpiochipClose(h);
   }
} /* tm1637Close() */

//
// Start wire transaction
//
static void tm1637Start(void)
{
   lgGpioWrite(h, bDataPin, HIGH);
   lgGpioWrite(h, bClockPin, HIGH);
   usleep(CLOCK_DELAY);
   lgGpioWrite(h, bDataPin, LOW);
} /* tm1637Start() */
//
// Stop wire transaction
//
static void tm1637Stop(void)
{
  lgGpioWrite(h, bClockPin, LOW); // clock low
  usleep(CLOCK_DELAY);
  lgGpioWrite(h, bDataPin, LOW); // data low
  usleep(CLOCK_DELAY);
  lgGpioWrite(h, bClockPin, HIGH); // clock high
  usleep(CLOCK_DELAY);
  lgGpioWrite(h, bDataPin, HIGH); // data high
} /* tm1637Stop() */
//
// Get data ack
//
static unsigned char tm1637GetAck(void)
{
unsigned char bAck = 1;

//
// With the sysfs GPIO driver, ArmbianIO is not prepared to reverse
// the direction of an existing GPIO pin
// We can safely disable this code on Linux
//
 // read ack
   lgGpioWrite(h, bClockPin, LOW); // clock to low
//   lgGpioClaimInput(h, LFAGS, bDataPin); // data as input
   usleep(CLOCK_DELAY);
   
//   bAck = lgGpioRead(h, bDataPin);
   lgGpioWrite(h, bClockPin, HIGH); // clock high
   usleep(CLOCK_DELAY);
   lgGpioWrite(h, bClockPin, LOW); // clock to low
//   lgGpioClaimOutput(h, LFLAGS, bDataPin, LOW);

   
   return bAck;
} /* tm1637GetAck() */
//
// Write a unsigned char to the controller
//
static void tm1637WriteByte(unsigned char b)
{
unsigned char i;

   for (i=0; i<8; i++)
   {
	  lgGpioWrite(h, bClockPin, LOW); // clock low
      if (b & 1) // LSB to MSB
         lgGpioWrite(h, bDataPin, HIGH);
      else
    	 lgGpioWrite(h, bDataPin, LOW);
      usleep(CLOCK_DELAY);
      lgGpioWrite(h, bClockPin, HIGH); // clock high
      usleep(CLOCK_DELAY);
      b >>= 1;
   }
} /* tm1637Writeunsigned char() */
//
// Write a sequence of unsigned chars to the controller
//
static void tm1637Write(unsigned char *pData, unsigned char bLen)
{
unsigned char b, bAck;
   bAck = 1;
   tm1637Start();
   for (b=0; b<bLen; b++)
   {
      tm1637WriteByte(pData[b]);
      bAck &= tm1637GetAck();
   }
   tm1637Stop();
} /* tm1637Write() */
//
// Set brightness (0-8)
//
void tm1637SetBrightness(unsigned char b)
{
unsigned char bControl;
   if (b == 0) // display off
      bControl = 0x80; // display off
   else
   {
      if (b > 8) b = 8;
      bControl = 0x88 | (b - 1);
   }
   tm1637Write(&bControl, 1);
} /* tm1637SetBrightness() */
//
// Display a string of 4 digits and optional colon
// by passing a string such as "12:34" or "45 67"
//
void tm1637ShowDigits(char *pString)
{
unsigned char b, bTemp[16]; // commands and data to transmit
unsigned char i, j;
   
   j = 0;
   bTemp[0] = 0x40; // memory write command (auto increment mode)
   tm1637Write(bTemp, 1);

   bTemp[j++] = 0xc0; // set display address to first digit command
   for (i=0; i<5; i++)
   {
      if (i == 2) // position of the colon
      {
         if (pString[i] == ':') // turn on correct bit
            bTemp[2] |= 0x80; // second digit high bit controls colon LEDs
      }
      else
      {
         b = 0;
         if (pString[i] >= '0' && pString[i] <= '9')
         {
            b = cDigit2Seg[pString[i] & 0xf]; // segment data
         }
         bTemp[j++] = b;
      }
   }
   tm1637Write(bTemp, j); // send to the display
}
