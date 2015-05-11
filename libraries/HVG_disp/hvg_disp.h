// hvg_comms.h
// Author: Rowan Gifford
// Date: 23/06/2014
// 20x4 Character Display Driver
// Raystar RX2004A-BIW
// Controller SSD1803a
// Bias 1/6, Duty 1/33, Vo 7.8V, I2C

#ifndef hvg_disp_h
#define hvg_disp_h

#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <wiring.h>
#include <pins_arduino.h>
#endif

#include <Print.h>
#include <Wire.h>

#define WIDTH 20           // Display width (20 characters)
#define HEIGHT 4           // Display height (4 lines) 
#define ADDRESS 0x3C       // Display I2C address
#define INIT_SIZE 30       // Number of elements in init sequence

class HVGDISP : public Print
{
  private:

    // Put a character on the display
    uint8_t drawChar(uint8_t c);          

  public:
  
    // Constructor
    HVGDISP( void );

    // Send initialisation sequence to display
    uint8_t initDisplay( void );

    //  Set write position on the display (in DDRAM)
    uint8_t setPrintPos(uint8_t x, uint8_t y);

    // Clear the display RAM contents and reset position to 0,0
    uint8_t clearDisplay ( void );

    // Return the x position to centre and string on a line
    uint8_t centerString(char * buffer);

	  // Overload write in Print class
	  #if defined(ARDUINO) && ARDUINO >= 100
	    size_t write(uint8_t c) { drawChar(c); return 1; };
	  #else
	    void write(uint8_t c) { drawChar(c); };
	  #endif
};

// Set DDRAM Address
static uint8_t SSD1803a_setPrintPos_seq[] = {
  
  0x80,  // Control Byte
  0x38,
  0x80,  // Control Byte
  0x80   // Data Byte
};

// Write a single character to DDRAM
static uint8_t SSD1803a_writeChar_seq[] = {
  
  0x40,  // Control Byte
  0x00   // Data Byte
};

// * Clear display
// Sets all DDRAM to ascii 0x20 (space), sets address to 0x00 and sets increment counter
static uint8_t SSD1803a_clearDisplay_seq[] = {
 
  0x80,   // Control Byte
  0x01   // Data Byte 
};

// Initialisation sequence for Solomon Systech SSD1803a COG
const static uint8_t SSD1803a_init_seq[] = {  

  // * Alt Function set, RE = 1, IS = 0
  // DL = 1, 8 bit mode
  // N = 1, 4 line display mode
  // BE = 0 Blink enabled disabled
  // RE = 1 Extended functions enabled
  // REV = 0 Reverse display disabled
  0x80,   // Control Byte
  0x39,   // Data Byte

  // Double height/Bias & Display dot shift, Requires IS = 0, RE = 1
  // UD2:1 = 00, Height mode
  // BS1 = 1 for 1/6 Bias
  // DH' = 0 For smooth dot scroll
  0x80,   // Control Byte
  0x06,   // Data Byte

  // Extended function set, Requires RE = 1
  // FW = 0 5-dot font width
  // B/W = 0 Black/White inversion disabled
  // NW = 1 for 4 line mode
  0x80,   // Control Byte
  0x0C,   // Data Byte

  // * Function set, RE = 0, IS = 1
  // DL = 1, 8 bit mode
  // N = 1, 4 line display mode
  // DH = 0 Disable double height control
  // RE = 0 Extended functions enabled
  // IS = 1 Special registers enabled
  0x80,   // Control Byte
  0x1B,   // Data Byte

  // * Clear display
  // Sets all DDRAM to ascii 0x20 (space), sets address to 0x00 and sets increment counter
  0x80,   // Control Byte
  0x55,   // Data Byte

  // Entery Mode Set, Requires RE = 0
  // I/D = 1 , cursor moves right and DDRAM increments
  // S = 0, Data is not shfited
  0x80,   // Control Byte
  0x6D,   // Data Byte

  // * Contrast set (low byte), Requires IS = 1, RE = 0
  // Sets C0:3 contrast level = 0011.b for Contrast value of 0x23 (~35)
  0x80,   // Control Byte
  0x7F,   // Data Byte

  // * Contrast set (high byte) & Power/Icon Control, Requires IS = 1, RE = 0
  // Sets C5:4 contrast level = 10.b for Contrast value of 0x23 (~35)
  // Ion = 0, Set ICON display off
  // Bon = 1, Turns DC-DC converter & regulator on
  0x80,   // Control Byte
  0x38,   // Data Byte

  // * Internal divider/OSC freq Requires IS = 1, RE = 0
  // BS0 = 1 (Required for 1/6bias)
  // F2:0 = 011, 540kHz Oscillator Frequency (POR)
  0x80,   // Control Byte
  0x3E,   // Data Byte

  // Follower control, Requires IS = 1, RE = 0
  // Don = 1 Turn internal divider circuit on
  // Rab2:0 = 101.b, IR5 resistor ratio set for V0 for 7.8V
  0x80,   // Control Byte
  0x02,   // Data Byte

  // Display ON/OFF Control
  // D = 1 Entire display is on
  // C = 0 Cursor is off
  // B = 0 Cursor blink is off
  0x80,   // Control Byte (last control byte)
  0x05,   // Data Byte

  0x80,   // Control Byte (last control byte)
  0x09,   // Data Byte

  0x80,   // Control Byte (last control byte)
  0x1E,   // Data Byte

  0x80,   // Control Byte (last control byte)
  0x80,   // Data Byte

  0x80,   // Control Byte (last control byte)
  0x01   // Data Byte

  // Com/Seg Data shift direction, requires RE = 1
  // BDC = ?
  // BDS = ?
  //0x80,
  //0x??,

};

#endif