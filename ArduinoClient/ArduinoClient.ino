#include <ArduinoJson.h>
#include "LedControl.h"
#include "display_handler.h"
#include "comms_manager.h"

LedControl lc = LedControl(8, 10, 9, 1);    // DIN -> pin 8, CLK -> pin 10, CS -> pin 9

void setup() {
    lc.shutdown(0, false);
    lc.setIntensity(0, 8);
    lc.clearDisplay(0);

    Serial.begin(115200);
}

void loop() {
    receiveSerial();


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
