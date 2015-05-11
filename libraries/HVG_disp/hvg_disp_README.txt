HarvestGeek
Engineer: Rowan Gifford
Date: 11/05/2015
Revision 1.1

Description:

Driver for RX2004-BIW 20x4 Character display with SSD1803a COG controller, I2C interface.

Files:

hvg_disp.cpp  - Driver source file
hvg_disp.h    - Driver header file

Functional Description:

Class based driver for setting up and sending data to the 20x4 display.

Include hvg_disp.h in the main source file.

When an instance is created by passing the LCD Reset pin the ATMEGA I2C interrupt driven interface is set up and a string of initialisation commands is sent to the display. Once this is done the display is ready to recieve data.

e.g HVGDISP hvg_disp(LCD_RST);

Class functions:
.print(char * string) - Puts the data contained in string on the display
.setPrintPos(uint8_t x, uint8_t y) - Sets the position on the display to write to, x is the position along the line and y is the row
.centerString(char * string) - Returns the x position required to center 'string' on display
.clearDisplay( void ) - Clears the display and sets the position to 0,0 (First character, first row, top left)