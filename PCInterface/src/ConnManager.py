# Filename: ConnManager.py
# Created: 13/11/2026
# Description:
import time
from enum import Enum

import serial

START_BYTE = 0xAA


def calc_checksum(data):
    checksum: int = 0

    for byte in data:
        checksum ^= byte

    return  checksum


class Command(Enum):
    ACK = 0x00
    PING = 0x01
    UPDATE_STAT = 0x02
    CHANGE_STAT = 0x03
    BTN_PRESS = 0x04
    BTN_RELEASE = 0x05
    SET_STATE = 0x06
    REQUEST_STAT = 0x07


class ConnManager:
    def __init__(self, debug_mode=False):
        self.debug_mode = debug_mode
        if not self.debug_mode:
            self.arduino = serial.Serial(port="COM3", baudrate=115200, timeout=1, dsrdtr=False)
        time.sleep(5)

        self.command_lengths = {
            Command.ACK: 0,
            Command.PING: 0,
            Command.UPDATE_STAT: 2,
            Command.CHANGE_STAT: 4,
            Command.BTN_PRESS: 1,
            Command.BTN_RELEASE: 1,
            Command.SET_STATE: 2,
            Command.REQUEST_STAT: 1
        }

    def create_packet(self, command, data=b''):
        if command not in self.command_lengths:
            raise ValueError(f"Unknown command: {command}")

        if len(data) != self.command_lengths[command]:
            raise ValueError(f"Invalid paylad for command {command}: expected {self.command_lengths[command]}, got {len(data)}")

        packet = bytearray([
            START_BYTE,
            command.value,
            len(data)
        ])

        packet.extend(data)

        checksum = calc_checksum(packet[1:])
        packet.append(checksum)

        return packet

    def send_to_arduino(self, packet: bytearray):
        print(f"Sending packet: {packet.hex()}...")
        if not self.debug_mode:
            self.arduino.write(packet)
        time.sleep(0.05)