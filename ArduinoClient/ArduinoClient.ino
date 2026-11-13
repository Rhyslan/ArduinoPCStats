#include <avr/pgmspace.h>
#include "LedControl.h"

#include "font.h"

#define MAX_DATA_LENGTH 4
#define START_BYTE 0xAA

/* #region Display Variables */

LedControl lc = LedControl(8, 10, 9, 1); // DIN -> pin 8, CLK -> pin 10, CS -> pin 9

enum Quadrant
{
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT
};

/* #endregion */

/* #region Comms Variables */

enum ReceiveState {
    WAIT_START,
    READ_COMMAND,
    READ_LENGTH,
    READ_DATA
};

ReceiveState rxState = WAIT_START;
byte rxCommand;
byte rxLength;
byte rxData[MAX_DATA_LENGTH];
byte rxChecksum;
byte dataIndex = 0;

unsigned long lastByteTime = 0;
const unsigned long RX_TIMEOUT = 100;

/* #endregion */

/* #region Commands Variables */

enum Command {
    ACK,
    PING,
    UPDATE_STAT,
    CHANGE_STAT,
    BTN_PRESS,
    BTN_RELEASE,
    SET_STATE,
    REQEUST_STAT
};

const byte commandLengths[] = {
    0,  // ACK
    0,  // PING
    2,  // UPDATE_STAT
    4,  // CHANGE_STAT
    1,  // BTN_PRESS
    1,  // BTN_RELEASE
    2,  // SET_STATE
    1   // REQUEST_STAT
};

/* #endregion */

/* #region Stats Variables */

struct Stat {
    byte id;
    byte char1;
    byte char2;
};

Stat stats[] = {
    {0x00, _C, _U},   // CPU Usage
    {0x01, _C, _T},   // CPU Temp
    {0x02, _G, _U},   // GPU Usage
    {0x03, _G, _T},   // GPU Temp
    {0x04, _R, _U},   // RAM Usage
};

/* #endregion */

void setup()
{
    lc.shutdown(0, false);
    lc.setIntensity(0, 8);
    lc.clearDisplay(0);

    Serial.begin(115200);
    Serial.println(millis());
    spinDisplayChar(_SPACE, TOP_LEFT, false);
    spinDisplayChar(_SPACE, TOP_RIGHT, false);
    spinDisplayChar(_SPACE, BOTTOM_LEFT, true);
    spinDisplayChar(_SPACE, BOTTOM_RIGHT, true);
    Serial.println(millis());
}

void loop()
{
    receiveSerial();
}

/* #region Display Functions */

void displayChar(byte charIndex, Quadrant quadrant, bool blankBefore)
{
    int rowOffset;
    int colOffset;

    switch (quadrant)
    {
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

    for (int row = 0; row < 4; row++)
    {
        byte charRow = pgm_read_byte(&font[charIndex][row]);

        for (int col = 0; col < 4; col++)
        {
            int charCol = blankBefore ? col - 1 : col;

            bool pixelOn = false;

            if (charCol >= 0 && charCol < 3)
            {
                pixelOn = bitRead(charRow, 2 - charCol);
            }

            lc.setLed(0, rowOffset + row, colOffset + col, pixelOn);
        }
    }
}

void spinDisplayChar(byte charIndex, Quadrant quadrant, bool blankBefore)
{
    for (int i = 0; i < 37; i++)
    {
        displayChar(i, quadrant, blankBefore);
        delay(25);
        if (i == charIndex)
        {
            break;
        }
    }
}

/* #endregion */

/* #region Comms Functions */

void receiveSerial() {
    if (rxState != WAIT_START && millis() - lastByteTime > RX_TIMEOUT) {
        rxState = WAIT_START;
    }

    while (Serial.available() > 0) {
        byte incomming = Serial.read();
        lastByteTime = millis();

        switch (rxState) {
            case WAIT_START:
                if (incomming == START_BYTE) {
                    rxState = READ_COMMAND;
                }
                break;
            case READ_COMMAND:
                rxCommand = incomming;
                
                if (rxCommand >= sizeof(commandLengths)) {
                    rxState = WAIT_START;
                } else {
                    rxState = READ_LENGTH;
                }
                break;
            case READ_LENGTH:
                rxLength = incomming;

                if (rxLength != commandLengths[rxCommand] || rxLength > MAX_DATA_LENGTH) {
                    rxState = WAIT_START;
                } else {
                    dataIndex = 0;
                    rxState = READ_DATA;
                }
                break;
            case READ_DATA:
                if (dataIndex < rxLength) {
                    rxData[dataIndex++] = incomming;
                } else {
                    rxChecksum = incomming;

                    processPacket();

                    rxState = WAIT_START;
                }
                break;
        }
    }
}

bool verifyChecksum() {
    byte checksum = 0;
    
    checksum ^= rxCommand;
    checksum ^= rxLength;

    for (byte i = 0; i < rxLength; i++) {
        checksum ^= rxData[i];
    }

    return checksum == rxChecksum;
}

void processPacket() {
    if (!verifyChecksum()) {
        return;
    }

    switch (rxCommand)
    {
        case ACK:
            // handle acknowledgement
            break;
        case PING:
            // handle ping
            break;
        case UPDATE_STAT:
            updateStat(rxData[0], rxData[1]);
            break;
        case CHANGE_STAT:
            changeStat(rxData[0], rxData[1], rxData[2], rxData[3]);
            break;
        case SET_STATE:
            setState(rxData[0], rxData[1]);
            break;
    }
}

/* #endregion*/

/* #region Commands Functions */

void updateStat(byte statId, byte newValue) {
    Stat* stat = getStat(statId);

    if (stat == nullptr) {
        return;
    }
    
    spinDisplayChar(stat->char1, TOP_LEFT, false);
    spinDisplayChar(stat->char2, TOP_RIGHT, false);
    spinDisplayChar(newValue / 10, BOTTOM_LEFT, true);
    spinDisplayChar(newValue % 10, BOTTOM_RIGHT, true);
}

void changeStat(byte statId, byte nameChar1, byte nameChar2, byte newValue) {
    Stat* stat = getStat(statId);

    if (stat == nullptr) {
        return;
    }

    stat->char1 = nameChar1;
    stat->char2 = nameChar2;

    spinDisplayChar(nameChar1, TOP_LEFT, false);
    spinDisplayChar(nameChar2, TOP_RIGHT, false);
    spinDisplayChar(newValue / 10, BOTTOM_LEFT, true);
    spinDisplayChar(newValue % 10, BOTTOM_RIGHT, true);
}

void setState(byte stateId, byte newValue) {

}

/* #endregion */

/* #region Stat Functions */

Stat* getStat(byte id) {
    for (byte i = 0; i < sizeof(stats) / sizeof(stats[0]); i++) {
        if (stats[i].id == id) {
            return &stats[i];
        }
    }

    return nullptr;    // Unknown stat
}

/* #endregion */
