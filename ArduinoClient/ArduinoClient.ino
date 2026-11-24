#include <avr/pgmspace.h>
#include "LedControl.h"

#include "font.h"

#define MAX_DATA_LENGTH 2
#define START_BYTE 0xAA

/* #region Display Variables */

enum Quadrant {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT
};

LedControl lc = LedControl(2, 4, 3, 1); // DIN -> pin 8, CLK -> pin 10, CS -> pin 9, 1 device

/* #endregion */

/* #region Comms Variables */

enum ReceiveState {
    WAIT_START,
    READ_COMMAND,
    READ_LENGTH,
    READ_DATA
};

// Packet reception
ReceiveState rxState = WAIT_START;
byte rxCommand;
byte rxLength;
byte rxData[MAX_DATA_LENGTH];
byte rxChecksum;
byte dataIndex = 0;

unsigned long lastByteTime = 0;
const unsigned long RX_TIMEOUT = 100;

bool acknowledged = false;
bool connected = false;

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

Stat* lastStat = nullptr;

/* #endregion */

/* #region States Variables */

const int muteLedPin = 6;
const int muteBtnPin = 7;

enum StateID {
    MUTE,
};

int muteBtnState = HIGH;
int lastMuteBtnState = HIGH;

int muteLedState = LOW;

unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

/* #endregion */

/* #region Display Functions */

/// @brief Displays a character in the specified quadrant of the LED matrix
/// @param charIndex The index of the character to display (see `font.h`)
/// @param quadrant Which quadrant of the LED matrix to display the character in
/// @param blankBefore If the blank column should be before the character instead of after
void displayChar(byte charIndex, Quadrant quadrant, bool blankBefore=false, bool animated=false) {
    int rowOffset;
    int colOffset;

    // Set offsets depending on which quadrant to display in
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

    // Loop through each pixel in the character and update the LED matrix
    for (int row = 0; row < 4; row++) {
        byte charRow = pgm_read_byte(&font[charIndex][row]);

        if (animated) {
            lc.setLed(0, rowOffset + row, colOffset, false);
            lc.setLed(0, rowOffset + row, colOffset + 1, false);
            lc.setLed(0, rowOffset + row, colOffset + 2, false);
            lc.setLed(0, rowOffset + row, colOffset + 3, false);
    
            delay(25);
        }
        
        for (int col = 0; col < 4; col++) {
            // Set blank column position
            int charCol = blankBefore ? col - 1 : col;

            bool pixelOn = false;

            if (charCol >= 0 && charCol < 3) {
                pixelOn = bitRead(charRow, 2 - charCol);
            }

            lc.setLed(0, rowOffset + row, colOffset + col, pixelOn);
        }

        if (animated) {
            delay(25);
        }
    }
}

/* #endregion */

/* #region Comms Functions */

/// @brief Receive serial messages from the Python script
void receiveSerial() {
    // Timeout if waiting too long for next byte of the command
    if (rxState != WAIT_START && millis() - lastByteTime > RX_TIMEOUT) {
        rxState = WAIT_START;
    }

    // Loop while bytes are available
    while (Serial.available() > 0) {
        byte incoming = Serial.read();
        lastByteTime = millis();

        switch (rxState) {
            case WAIT_START:
                // Process command if byte is START_BYTE
                if (incoming == START_BYTE) {
                    rxState = READ_COMMAND;
                }
                break;
            case READ_COMMAND:
                rxCommand = incoming;
                
                // Reset if unknown command
                if (rxCommand >= sizeof(commandLengths)) {
                    rxState = WAIT_START;
                } else {
                    rxState = READ_LENGTH;
                }
                break;
            case READ_LENGTH:
                rxLength = incoming;

                // Reset if length is incorrect for command or longer than max data length
                if (rxLength != commandLengths[rxCommand] || rxLength > MAX_DATA_LENGTH) {
                    rxState = WAIT_START;
                } else {
                    dataIndex = 0;
                    rxState = READ_DATA;
                }
                break;
            case READ_DATA:
                // If still reading data, add byte to data array
                if (dataIndex < rxLength) {
                    rxData[dataIndex++] = incoming;
                } else {
                    // When done reading data, get checksum byte
                    rxChecksum = incoming;

                    processPacket();

                    // Return to waiting state
                    rxState = WAIT_START;
                }
                break;
        }
    }
}

/// @brief Verify that the calculated checksum is the same as the checksum byte
/// @return Bool indicating if checksum is valid
bool verifyChecksum() {
    byte checksum = 0;
    
    checksum ^= rxCommand;
    checksum ^= rxLength;

    for (byte i = 0; i < rxLength; i++) {
        checksum ^= rxData[i];
    }

    return checksum == rxChecksum;
}

/// @brief Process received packet. Validates checksum then handles the command
void processPacket() {
    if (!verifyChecksum()) {
        return;
    }

    // Handle command
    switch (rxCommand) {
        case ACK:
            acknowledged = true;
            break;
        case PING:
            // Send acknowledgement to ping
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

/// @brief Checks if Python has acknowledged a command since the last check
/// @return Bool indicating acknowledgement
bool checkAcknowledged() {
    if (acknowledged) {
        acknowledged = false;
        return true;
    }

    return false;
}

/// @brief Sends a command to the Python script
/// @param command Comand ID
/// @param data Data array payload to be sent
/// @param dataLength Length of the data array
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

    // Send the packet
    Serial.write(packet, packetLength);
}

/* #endregion*/

/* #region Commands Functions */

/// @brief Handle updating the display with a new stat value
/// @param statId ID of the stat to display
/// @param newValue New value fo the stat to display
void updateStat(byte statId, byte newValue) {
    // Get stat info
    Stat* stat = getStat(statId);

    if (stat == nullptr) {
        return;
    }
    
    if (stat != lastStat) {
        // Display the chars for the stat
        displayChar(stat->char1, TOP_LEFT, false, true);
        displayChar(stat->char2, TOP_RIGHT, false, true);

        // Display the digits for the value
        displayChar(newValue / 10, BOTTOM_LEFT, true, true);
        displayChar(newValue % 10, BOTTOM_RIGHT, true, true);
    }
    
    // Display the digits for the value
    displayChar(newValue / 10, BOTTOM_LEFT, true);
    displayChar(newValue % 10, BOTTOM_RIGHT, true);

    lastStat = stat;
}

/// @brief Handle updating a state value
/// @param stateId ID of the state to change
/// @param newValue New value for the state
void setState(byte stateId, byte newValue) {
    switch (stateId) {
        case MUTE:
            // Save the new mute state and update the LED
            muteLedState = newValue;
            digitalWrite(muteLedPin, muteLedState);
            break;
    }
}

/* #endregion */

/* #region Stat Functions */

/// @brief Get a stat from its ID
/// @param id ID of the stat to look for
/// @return Pointer to Stat struct with matching ID. Returns `nullptr` if stat is unknown
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

/// @brief Handle mute button press
void handleMuteBtn() {
    // Read the button state
    int reading = digitalRead(muteBtnPin);
    
    // Reset debounce timer if state changed
    if (reading != lastMuteBtnState) {
        lastDebounceTime = millis();
    }
    
    // If state remains the same after `debounceDelay`, use the new value
    if ((millis() - lastDebounceTime) > debounceDelay) {
        // If the state has changed, toggle the mute state
        if (reading != muteBtnState) {
            muteBtnState = reading;
            
            // Only process if button is pressed
            if (muteBtnState == LOW) {
                // Send mute change command
                byte data[1] = {MUTE};
                sendToPC(BTN_PRESS, data, sizeof(data) / sizeof(byte));
            }
        }
    }
    
    // Save new state
    lastMuteBtnState = reading;
}

/* #endregion */

void setup() {
    // Setup IO pins
    pinMode(muteLedPin, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(muteBtnPin, INPUT_PULLUP);

    // Setup LED Matrix
    lc.shutdown(0, false);
    lc.setIntensity(0, 3);
    lc.clearDisplay(0);

    // Setup LEDs
    digitalWrite(muteLedPin, muteLedState);
    digitalWrite(LED_BUILTIN, acknowledged);

    // Start serial connection
    Serial.begin(115200);

    // Do boot animation
    // displayChar(_A, TOP_LEFT);
    // displayChar(_A, TOP_RIGHT);
    // displayChar(_A, BOTTOM_LEFT, true);
    // displayChar(_A, BOTTOM_RIGHT, true);
}

void loop() {
    // Use built-in LED to show acknowledged state
    digitalWrite(LED_BUILTIN, acknowledged);

    receiveSerial();

    // If connected, allow stuff to happen, otherwise wait for connection acknowledgement
    if (connected) {
        handleMuteBtn();
    } else {
        connected = checkAcknowledged();
    }
}
