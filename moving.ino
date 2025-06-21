//For more information on FastLED, see http://fastled.io/docs/index.html 
#include <FastLED.h>


// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define DATA_PIN 3

// Params for width and height of the LED panel
const uint8_t kMatrixWidth = 32;
const uint8_t kMatrixHeight = 8;
 
// Param for different pixel layouts
const bool    kMatrixSerpentineLayout = true;
const bool    kMatrixVertical = true;

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
CRGB leds_plus_safety_pixel[ NUM_LEDS + 1];
CRGB* const leds( leds_plus_safety_pixel + 1);

//How slow the color changes when printing "ICHTHUS". The higher the number the slower the color change. Range from 1-255
#define COLOR_RATE 32
//Brightness range: 0-255
#define BRIGHTNESS 200

// Placeholder string buffer for printing debug message.
String str;

//Height of a character from ICHTHUS in pixel
#define HEIGHT 8
//Width of a character from ICHTHUS in pixel
#define WIDTH 32
//Number of characters in ICHTHUS
#define NUM_CHARS 7

#define NUM_FRAME_WORD 120
#define NUM_FRAME_FISH 128

/*
 * Pixel art of characther I
 * Element [i] refers to the pixel pattern on row i, with row 0 being at the top.
 * The most significant bit of each element maps to the left-most pixel, and the most least significant bit maps to the right-most pixel.
 * Bit 1 means the pixel at the position needs to be colored, otherwise it should be left as blank
*/
const uint32_t charWord[HEIGHT] = {
	0x00044101,
	0x00044101,
	0x00044101,
	0x649DE731,
	0x14A44909,
	0x64A44909,
	0x84A54949,
	0x77248932
};

const uint32_t charGoFish[HEIGHT] = {
	0x8028038E,
	0x40820451,
	0x22048441,
	0x180E0441,
	0x1804445D,
	0x22040451,
	0x40808451,
	0x802A039E
};

typedef enum {
  STATE_WORD = 0x0,
  STATE_MOVE = 0x1
} STATE;

// Helper functions for an two-dimensional XY matrix of pixels.
// Adapted from http://fastled.io/docs/_x_y_matrix_8ino-example.html
//
//     XY(x,y) takes x and y coordinates and returns an LED index number,
//             for use like this:  leds[ XY(x,y) ] == CRGB::Red;
//             No error checking is performed on the ranges of x and y.
//
//     XYsafe(x,y) takes x and y coordinates and returns an LED index number,
//             for use like this:  leds[ XYsafe(x,y) ] == CRGB::Red;
//             Error checking IS performed on the ranges of x and y, and an
//             index of "-1" is returned.  Special instructions below
//             explain how to use this without having to do your own error
//             checking every time you use this function.  
//             This is a slightly more advanced technique, and 
//             it REQUIRES SPECIAL ADDITIONAL setup, described below.
// Set 'kMatrixSerpentineLayout' to false if your pixels are 
// laid out all running the same way, like this:
//
//     0 >  1 >  2 >  3 >  4
//                         |
//     .----<----<----<----'
//     |
//     5 >  6 >  7 >  8 >  9
//                         |
//     .----<----<----<----'
//     |
//    10 > 11 > 12 > 13 > 14
//                         |
//     .----<----<----<----'
//     |
//    15 > 16 > 17 > 18 > 19
//
// Set 'kMatrixSerpentineLayout' to true if your pixels are 
// laid out back-and-forth, like this:
//
//     0 >  1 >  2 >  3 >  4
//                         |
//                         |
//     9 <  8 <  7 <  6 <  5
//     |
//     |
//    10 > 11 > 12 > 13 > 14
//                        |
//                        |
//    19 < 18 < 17 < 16 < 15
//
// Bonus vocabulary word: anything that goes one way 
// in one row, and then backwards in the next row, and so on
// is call "boustrophedon", meaning "as the ox plows."
 
 
// This function will return the right 'led index number' for 
// a given set of X and Y coordinates on your matrix.  
// IT DOES NOT CHECK THE COORDINATE BOUNDARIES.  
// That's up to you.  Don't pass it bogus values.
//
// Use the "XY" function like this:
//
//    for( uint8_t x = 0; x < kMatrixWidth; x++) {
//      for( uint8_t y = 0; y < kMatrixHeight; y++) {
//      
//        // Here's the x, y to 'led index' in action: 
//        leds[ XY( x, y) ] = CHSV( random8(), 255, 255);
//      
//      }
//    }
//
//
uint16_t XY( int16_t x, int16_t y)
{
  uint16_t i;
  
  if( kMatrixSerpentineLayout == false) {
    if (kMatrixVertical == false) {
      i = (y * kMatrixWidth) + x;
    } else {
      i = kMatrixHeight * x + y;	
    }
  }
 
  if( kMatrixSerpentineLayout == true) {
    if (kMatrixVertical == false) {
      if( y & 0x01) {
        // Odd rows run backwards
        int16_t reverseX = (kMatrixWidth - 1) - x;
        i = (y * kMatrixWidth) + reverseX;
      } else {
        // Even rows run forwards
        i = (y * kMatrixWidth) + x;
      }
    } else { // vertical positioning
      if ( x & 0x01) {
      	int16_t reverseY = (kMatrixHeight - 1) - y;
        i = kMatrixHeight * x + reverseY;
      } else {
        i = kMatrixHeight * x + y;
      }
    }
  }
  
  return i;
}


int16_t XYsafe( int16_t x, int16_t y)
{
  if( (x >= kMatrixWidth) || x < 0) return -1;
  if( (y >= kMatrixHeight) || y < 0) return -1;
  return XY(x,y);
}
// End of helper functions adapted from http://fastled.io/docs/_x_y_matrix_8ino-example.html

/* draw_character(const uint8_t** pixels, int16_t y, int16_t x, CRGB* colorPixels, CRGB color)
 * Draws the characters in ICHTHUS on the LED strip at the desginated (x, y) location
 * pixels: 	pointer to an array of char pointers that house the pixel pattern of characters
 * y: 		upper-left y coordinate of the word
 * x: 		uplller-left x coordinate of the word
 * colorPixels: pointer to the color pixel buffer
 * color:       foreground color of the letters
*/
void draw_frame(const uint32_t* pixels, int16_t y, int16_t x, CRGB* colorPixels, CRGB color) {
	uint16_t num_pixels = (uint8_t) NUM_CHARS * (uint8_t) HEIGHT;
	for (uint8_t charY=0; charY<HEIGHT; charY++) {
		for (uint8_t charX=0; charX<WIDTH; charX++) {
			int16_t paintX = charX + x;
			int16_t paintY = charY + y;

			uint32_t drawPixel = pixels[charY] & (((uint32_t) 1) << charX);
			CRGB paintColor = drawPixel ? color : (CRGB::Black);

			colorPixels[XYsafe(paintX, paintY)] = paintColor;
		}
	}
}

// Paint everything black
void clear_screen (CRGB* colorPixels) {
  for (uint8_t charY=0; charY<HEIGHT; charY++) {
    for (uint8_t charX=0; charX<WIDTH; charX++) {
      int16_t paintX = charX;
      int16_t paintY = charY;
      CRGB paintColor = CRGB::Black;

      colorPixels[XYsafe(paintX, paintY)] = paintColor;
    }
  }
}


int8_t calculateHue(uint32_t ms) {
	return ((ms / COLOR_RATE) % 256);
}

uint32_t timer;
STATE currState;
int16_t move_x;
int16_t move_y;

void setup() {
  // put your setup code here, to run once:
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is assumed
  Serial.begin(9600);
  timer = 0;
  currState = STATE_WORD;
  move_x = 32;
  move_y = 0;
}

void loop() {
  // Progression of frame
  // STATE_WORD: draw ichthus and display statically, except with changing color for a while
  // STATE_MOVE: move "GO [fish]" from left to right and scroll over repeated for a while 
  // STATE_WORD --> STATE_MOVE --> STATE_WORD --> etc
  // set up ms
  uint32_t ms = millis();
  int8_t hue = calculateHue(ms);
  //Serial.println(str+"Hi");

  // put your main code here, to run repeatedly:
  CRGB wordColor = CHSV(hue, 255, 255);
  
  FastLED.setBrightness( BRIGHTNESS );

  //delay(2000);
  clear_screen(leds);
  STATE nextState = currState;
  if (currState == STATE_WORD) {
    //draw_frame(const uint32_t* pixels, int16_t y, int16_t x, CRGB* colorPixels, CRGB color)
    draw_frame(charWord, 0, 0, leds, wordColor);
    timer += 1;
    if (timer == NUM_FRAME_WORD) {
      timer = 0;
      nextState = STATE_MOVE;
      move_y = 0;
      move_x = 32;
    }
  } else {
    draw_frame(charGoFish, move_y, move_x, leds, CRGB::White);
    timer += 1;
    move_x -= 1;
    move_y = 0;

    if (move_x == -32) {
      move_x = 32;
    }
    if (timer == NUM_FRAME_FISH) {
      timer = 0;
      nextState = STATE_WORD;
    }
  }
  //About 12 FPS
  delay(80);
  FastLED.show();
  currState = nextState;
}