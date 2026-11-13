#ifndef COMMANDS_H  
#define COMMANDS_H

#define START_BYTE 0xAA

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

void updateStat(byte statId, byte newValue);
void changeStat(byte statId, byte nameChar1, byte nameChar2, byte initialValue);
void setState(byte stateId, byte newValue);

#endif