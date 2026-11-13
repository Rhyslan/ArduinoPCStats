#include "comms_manager.h"

#include "commands.h"

void receiveSerial() {
    if (rxState != WAIT_START & millis() - lastByteTime > RX_TIMEOUT) {
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