#include <avr/pgmspace.h>
#include "LedControl.h"

#include "font.h"

#define MAX_DATA_LENGTH 4
#define START_BYTE 0xAA

bool connected = false;

/* #region Display Variables */

LedControl lc = LedControl(2, 4, 3, 1); // DIN -> pin 8, CLK -> pin 10, CS -> pin 9, 1 device

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

bool acknowledged = false;

/* #endregion */

/* #region Commands Variables */

enum Command {
    ACK,
    PING,
    UPDATE_STAT,
    BTN_PRESS,
    SET_STATE,
    COMMAND_COUNT
};

const byte commandLengths[] = {
    0,  // ACK
    0,  // PING
    2,  // UPDATE_STAT
    1,  // BTN_PRESS
    2,  // SET_STATE
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

/* #region States Variables */

const int muteLedPin = 6;
const int muteBtnPin = 7;

int muteBtnState = HIGH;
int lastMuteBtnState = HIGH;

int muteLedState = LOW;

unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

enum StateID {
    MUTE,
};

/* #endregion */

void setup()
{
    // Setup IO pins
    pinMode(muteLedPin, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(muteBtnPin, INPUT_PULLUP);

    // Setup LED Matrix
    lc.shutdown(0, false);
    lc.setIntensity(0, 8);
    lc.clearDisplay(0);

    // Setup mute LED
    digitalWrite(muteLedPin, muteLedState);

    // Start serial connection
    Serial.begin(115200);

    // Do boot animation
    //spinDisplayChar(_SPACE, TOP_LEFT, false);
    //spinDisplayChar(_SPACE, TOP_RIGHT, false);
    //spinDisplayChar(_SPACE, BOTTOM_LEFT, true);
    //spinDisplayChar(_SPACE, BOTTOM_RIGHT, true);
}

void loop()
{
    digitalWrite(LED_BUILTIN, acknowledged);

    receiveSerial();

    if (connected) {
        toggleMute();
    }
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
        byte incoming = Serial.read();
        lastByteTime = millis();

        switch (rxState) {
            case WAIT_START:
                if (incoming == START_BYTE) {
                    rxState = READ_COMMAND;
                }
                break;
            case READ_COMMAND:
                rxCommand = incoming;
                
                if (rxCommand >= sizeof(commandLengths)) {
                    rxState = WAIT_START;
                } else {
                    rxState = READ_LENGTH;
                }
                break;
            case READ_LENGTH:
                rxLength = incoming;

                if (rxLength != commandLengths[rxCommand] || rxLength > MAX_DATA_LENGTH) {
                    rxState = WAIT_START;
                } else {
                    dataIndex = 0;
                    rxState = READ_DATA;
                }
                break;
            case READ_DATA:
                if (dataIndex < rxLength) {
                    rxData[dataIndex++] = incoming;
                } else {
                    rxChecksum = incoming;

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
            acknowledged = true;
            break;
        case PING:
            Serial.println("ping received, sending acknowledge");
            sendToPC(ACK, nullptr, 0);
            break;
        case UPDATE_STAT:
            updateStat(rxData[0], rxData[1]);
            break;
        case SET_STATE:
            setState(rxData[0], rxData[1]);
            break;
    }
}

bool checkAcknowledged() {
    if (acknowledged) {
        acknowledged = false;
        return true;
    }

    return false;
}

void sendToPC(Command command, byte *data, byte dataLength) {
    // Confirm command is valid
    if (command < 0 || command >= COMMAND_COUNT) {
        Serial.print("Error: Unknown command received while creating packet: ");
        Serial.println(command);
        return;
    }

    // Confirm data length is valid for command specified
    if (dataLength != commandLengths[command]) {
        Serial.print("Error: Invalid payload for command ");
        Serial.print(command);
        Serial.print(" expected ");
        Serial.print(commandLengths[command]);
        Serial.print(", got ");
        Serial.println(dataLength);
        return;
    }

    // Create packet array
    int packetLength = 4 + dataLength;
    byte packet[packetLength] = {START_BYTE, command, dataLength};

    // Add data and calc checksum
    byte checksum = 0;
    checksum ^= command;
    checksum ^= dataLength;

    for (byte i = 0; i < dataLength; i++) {
        packet[i + 3] = data[i];
        checksum ^= data[i];
    }

    packet[packetLength - 1] = checksum;

    Serial.write(packet, packetLength);
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

void setState(byte stateId, byte newValue) {
    switch (stateId) {
        case MUTE:
            muteLedState = newValue;
            digitalWrite(muteLedPin, muteLedState);
            break;
    }
}

/* #endregion */

/* #region Stat Functions */

Stat* getStat(byte id) {
    for (byte i = 0; i < sizeof(stats) / sizeof(byte); i++) {
        if (stats[i].id == id) {
            return &stats[i];
        }
    }

    return nullptr;    // Unknown stat
}

/* #endregion */

/* #region State Functions */

void toggleMute() {
    int reading = digitalRead(muteBtnPin);

    if (reading != lastMuteBtnState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != muteBtnState) {
            muteBtnState = reading;

            if (muteBtnState == LOW) {
                // Send mute change command
                byte data[1] = {0x00};
                sendToPC(BTN_PRESS, data, 1);
            }
        }
    }
    lastMuteBtnState = reading;
}

/* #endregion */
