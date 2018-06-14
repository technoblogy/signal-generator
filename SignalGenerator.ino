/* Programmable Signal Generator

   David Johnson-Davies - www.technoblogy.com - 14th June 2018
   ATtiny85 @ 1 MHz (internal oscillator; BOD disabled)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#include <Wire.h>

// Matrix keypad *******************************************************

const int Matrix = A2;
const int nButtons = 12;
const int SmallestGap = 40;
int AnalogVals[] = {1023, 680, 640, 590, 547, 507, 464, 411, 351, 273, 180, 133, 0};
int Buttons[] =    {-1,   11,  9,   6,   3,   0,   10,  8,   7,   5,   4,   2,   1};

// Returns the keypad character or -1 if no button pressed
int ReadKeypad() {
  int val, lastval=0, count = 0;
  do {
    val = analogRead(Matrix);
    if (abs(val-lastval)<2) count++;
    else { lastval = val; count = 0; }
  } while (count < 3);
  val = val + SmallestGap/2;
  int i = 0;
  while (val < AnalogVals[i]) { i++; }
  return Buttons[i];
}

// OLED I2C 128 x 32 monochrome display **********************************************

const int OLEDAddress = 0x3C;

// Initialisation sequence for OLED module
int const InitLen = 24;
const unsigned char Init[InitLen] PROGMEM = {
  0xAE, // Display off
  0xD5, // Set display clock
  0x80, // Recommended value
  0xA8, // Set multiplex
  0x1F,
  0xD3, // Set display offset
  0x00,
  0x40, // Zero start line
  0x8D, // Charge pump
  0x14,
  0x20, // Memory mode
  0x01, // Vertical addressing
  0xA1, // 0xA0/0xA1 flip horizontally
  0xC8, // 0xC0/0xC8 flip vertically
  0xDA, // Set comp ins
  0x02,
  0x81, // Set contrast
  0x7F, // 0x00 to 0xFF
  0xD9, // Set pre charge
  0xF1,
  0xDB, // Set vcom detect
  0x40,
  0xA6, // Normal (0xA7=Inverse)
  0xAF  // Display on
};

const int data = 0x40;
const int single = 0x80;
const int command = 0x00;

void InitDisplay () {
  Wire.beginTransmission(OLEDAddress);
  Wire.write(command);
  for (uint8_t c=0; c<InitLen; c++) Wire.write(pgm_read_byte(&Init[c]));
  Wire.endTransmission();
}

// Graphics **********************************************

int Scale = 2; // 2 for big characters
const int Space = 10;
const int Hz = 11;
const int Icons = 13;
const int UserIcon = 27;

// Character set for digits, "Hz", and waveform icons - stored in program memory
const uint8_t CharMap[][6] PROGMEM = {
{ 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 }, // 30
{ 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 }, 
{ 0x72, 0x49, 0x49, 0x49, 0x46, 0x00 }, 
{ 0x21, 0x41, 0x49, 0x4D, 0x33, 0x00 }, 
{ 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00 }, 
{ 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 }, 
{ 0x3C, 0x4A, 0x49, 0x49, 0x31, 0x00 }, 
{ 0x41, 0x21, 0x11, 0x09, 0x07, 0x00 }, 
{ 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 }, 
{ 0x46, 0x49, 0x49, 0x29, 0x1E, 0x00 },
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // Space
{ 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00 }, // H
{ 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00 }, // z
};

void ClearDisplay () {
  Wire.beginTransmission(OLEDAddress);
  Wire.write(command);
  // Set column address range
  Wire.write(0x21); Wire.write(0); Wire.write(127);
  // Set page address range
  Wire.write(0x22); Wire.write(0); Wire.write(3);
  Wire.endTransmission();
  // Write the data in 16 32-byte transmissions
  for (int i = 0 ; i < 32; i++) {
    Wire.beginTransmission(OLEDAddress);
    Wire.write(data);
    for (int i = 0 ; i < 32; i++) Wire.write(0);
    Wire.endTransmission();
  }
}

// Converts bit pattern abcdefgh into aabbccddeeffgghh
int Stretch (int x) {
  x = (x & 0xF0)<<4 | (x & 0x0F);
  x = (x<<2 | x) & 0x3333;
  x = (x<<1 | x) & 0x5555;
  return x | x<<1;
}

// Plots a character; line = 0 to 2; column = 0 to 21
void PlotChar(int c, int line, int column) {
  Wire.beginTransmission(OLEDAddress);
  Wire.write(command);
  // Set column address range
  Wire.write(0x21); Wire.write(column*6); Wire.write(column*6 + Scale*6 - 1);
  // Set page address range
  Wire.write(0x22); Wire.write(line); Wire.write(line + Scale - 1);
  Wire.endTransmission();
  Wire.beginTransmission(OLEDAddress);
  Wire.write(data);
  for (uint8_t col = 0 ; col < 6; col++) {
    int bits = pgm_read_byte(&CharMap[c][col]);
    if (Scale == 1) Wire.write(bits);
    else {
      bits = Stretch(bits);
      for (int i=2; i--;) { Wire.write(bits); Wire.write(bits>>8); }
    }
  }
  Wire.endTransmission();
}

uint8_t DigitChar (unsigned int number, unsigned int divisor) {
  return (number/divisor) % 10;
}

// Display a 8-digit frequency starting at line, column
void PlotFreq (long freq, int line, int column) {
  boolean dig = false;
  for (long d=10000000; d>0; d=d/10) {
    char c = freq/d % 10;
    if (c == 0 && !dig) c = Space; else dig = true;
    PlotChar(c, line, column);
    column = column + Scale;
  }
}

void PlotHz (int line, int column) { 
  PlotChar(Hz, line, column); column = column + Scale;
  PlotChar(Hz+1, line, column);
}

// Set frequency **********************************************

const int OscAddress = 0x17;
long Input;
const long Mult = (long)2078 * 1024;

int CalculateParameters (long target) {
  if (target < 1039) target = 1039;
  int oct = 0;
  while (target >= ((long)2078 * 1<<oct) && oct < 15) oct++;
  long factor = (long)1<<oct;
  long val = (target + factor/2) / factor;
  int frac = (Mult + val/2) / val;
  int dac = 2048 - frac;
  return oct<<10 | dac;
}

long GetFrequency (int parameters) {
  int dac = parameters & 0x3FF;
  int oct = parameters>>10 & 0x0F;
  long factor = (long)1<<oct;
  int frac = 2048 - dac;
  return ((Mult + frac/2)/ frac) * factor;
}

void SendFrequency (int parameters) {
  int cnf = 2; // Only CLK output enabled.
  int data = parameters<<2 | cnf;
  Wire.beginTransmission(OscAddress);
  Wire.write(data>>8 & 0xFF);
  Wire.write(data & 0xFF);
  Wire.endTransmission();
}

void SendSilence () {
  int cnf = 3; // Powered down
  Wire.beginTransmission(OscAddress);
  Wire.write(0);
  Wire.write(cnf);
  Wire.endTransmission();
}

// Setup **********************************************
  
int Mode = 0; // 0 = enter frequency, 1 = generate output
const int Star = 10;
const int Hash = 11;

void setup() {
  Wire.begin();
  InitDisplay();
  ClearDisplay();
  SendSilence();
  Input = 0;
}

void loop() {
  int key, parameters;
  // Wait for key
  do { key = ReadKeypad(); } while (key == -1);
  
  if (key <= 9 && Input < 9999999) {
    if (Mode) {
      PlotChar(Space, 0, 8*Scale);
      PlotChar(Space, 0, 9*Scale);
      Input = 0;
      Mode = 0;
      SendSilence();
    }
    Input = Input*10 + key;
  } else if (key == Hash) {
    PlotChar(Space, 0, 8*Scale);
    PlotChar(Space, 0, 9*Scale);
    Input = 0;
    Mode = 0;
    SendSilence();
  } else if (key == Star) {
    Mode = 1;
    parameters = CalculateParameters(Input);
    Input = GetFrequency(parameters);
    PlotHz(0, 8*Scale);
    SendFrequency(parameters);
  }
  
  PlotFreq(Input,0,0);
  // Wait for key up
  do { key = ReadKeypad(); } while (key != -1);
}
