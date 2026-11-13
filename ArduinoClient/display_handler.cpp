#include "display_handler.h"

#include <avr/pgmspace.h>
#include "font.h"

void displayChar(LedControl lc, byte charIndex, Quadrant quadrant, bool blankBefore) {
    int rowOffset;
    int colOffset;

    switch (quadrant) {
        case TOP_LEFT:
            rowOffset = 0;
            colOffset = 0;
            break;
        case TOP_RIGHT:
            rowOffset = 0;
            colOffset = 4;
            break;
        case BOTTOM_LEFT:
            rowOffset = 4;
            colOffset = 0;
            break;
        case BOTTOM_RIGHT:
            rowOffset = 4;
            colOffset = 4;
            break;
    }

    for (int row = 0; row < 4; row++) {
        byte charRow = pgm_read_byte(&font[charIndex][row]);

        for (int col = 0; col < 4; col++) {
            int charCol = blankBefore ? col - 1 : col;

            bool pixelOn = false;

            if (charCol >= 0 && charCol < 3) {
                pixelOn = bitRead(charRow, 2 - charCol);
            }

            lc.setLed(0, rowOffset + row, colOffset + col, pixelOn);
        }
    }
}

void spinDisplayChar(LedControl lc, byte charIndex, Quadrant quadrant, bool blankBefore) {
    for (int i = 0; i < 37; i++) {
        displayChar(lc, i, quadrant, blankBefore);
        delay(25);
        if (i == charIndex) {
            break;
        }
    }
}