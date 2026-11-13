#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include "LedControl.h"

enum Quadrant {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT
};

void displayChar(LedControl lc, byte charIndex, Quadrant quadrant, bool blankBefore);
void spinDisplayChar(LedControl lc, byte charIndex, Quadrant quadrant, bool blankBefore);

#endif