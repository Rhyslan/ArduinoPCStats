#include <avr/pgmspace.h>
#include <ArduinoJson.h>
#include "LedControl.h"
#include "font.h"

LedControl lc = LedControl(8, 10, 9, 1);    // DIN -> pin 8, CLK -> pin 10, CS -> pin 9

enum Quadrant {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT
};

void setup() {
    lc.shutdown(0, false);
    lc.setIntensity(0, 8);
    lc.clearDisplay(0);

    Serial.begin(115200);
}

void loop() {
    if (Serial.available() > 0) {
        String input = Serial.readString();
    
        JsonDocument doc;
    
        deserializeJson(doc, input);
    
        byte charToDisp = doc["charIndex"];
        Serial.print(charToDisp);
    
        displayChar(charToDisp, TOP_LEFT, false);
    
        delay(10);
    }


    // for (int i = 0; i < 4; i++) {
    //     for (int j = 0; j < 36; j++) {
    //         displayChar(j, (enum Quadrant)i, false);
    //         delay(200);
    //     }
    // }

    // spinDisplayChar(_C, TOP_LEFT, false);
    // spinDisplayChar(_U, TOP_RIGHT, false);
    // spinDisplayChar(2, BOTTOM_LEFT, true);
    // spinDisplayChar(5, BOTTOM_RIGHT, true);

    // delay(2000);

    // spinDisplayChar(_C, TOP_LEFT, false);
    // spinDisplayChar(_T, TOP_RIGHT, false);
    // spinDisplayChar(7, BOTTOM_LEFT, true);
    // spinDisplayChar(0, BOTTOM_RIGHT, true);
    
    // delay(2000);
}

void displayChar(byte charIndex, Quadrant quadrant, bool blankBefore) {
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

void spinDisplayChar(byte charIndex, Quadrant quadrant, bool blankBefore) {
    for (int i = 0; i < 37; i++) {
        displayChar(i, quadrant, blankBefore);
        delay(25);
        if (i == charIndex) {
            break;
        }
    }
}