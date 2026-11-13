#ifndef COMMS_MANAGER_H
#define COMMS_MANAGER_H

#define MAX_DATA_LENGTH 4

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

// RX
void receiveSerial();
bool verifyChecksum();
void processPacket();

// TX
void createPacket();
void sendPacket();

#endif