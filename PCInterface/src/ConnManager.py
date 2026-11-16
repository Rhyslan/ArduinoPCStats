# Filename: ConnManager.py
# Created: 13/11/2026
# Description:
import serial
import time
from enum import Enum, auto

# Constants
START_BYTE = 0xAA
MAX_DATA_LENGTH = 4
RX_TIMEOUT = 100


def calc_checksum(data):
    """

    :param data:
    :return:
    """
    checksum: int = 0

    for byte in data:
        checksum ^= byte

    return  checksum

def millis():
    return time.monotonic_ns() // 1_000_000


# Enums
class Command(Enum):
    ACK = 0
    PING = auto()
    UPDATE_STAT = auto()
    BTN_PRESS = auto()
    SET_STATE = auto()


class ReceiveState(Enum):
    WAIT_START = 0
    READ_COMMAND = auto()
    READ_LENGTH = auto()
    READ_DATA = auto()

class ButtonID(Enum):
    MUTE = 0


# Main class
class ConnManager:
    def __init__(self, debug_mode=False):
        """

        :param debug_mode:
        """
        self.debug_mode = debug_mode

        if not self.debug_mode:
            self.arduino = serial.Serial(port="COM3", baudrate=115200, timeout=1, dsrdtr=False)

        self.command_lengths = {
            Command.ACK: 0,
            Command.PING: 0,
            Command.UPDATE_STAT: 2,
            Command.BTN_PRESS: 1,
            Command.SET_STATE: 2,
        }

        # Receive vars
        self.rx_state = ReceiveState.WAIT_START
        self.rx_command = 0
        self.rx_length = 0
        self.rx_data = [0] * MAX_DATA_LENGTH
        self.rx_checksum = 0
        self.data_index = 0
        self.last_byte_time = 0

        self.acknowledged = False

        # States
        self.muted = False

        # Wait for connection acknowledge from Arduino
        self.connected = False
        print("Waiting for Arduino connection...")
        self.send_to_arduino(self.create_packet(Command.PING))
        last_ping = millis()
        while not self.connected:
            if millis() - last_ping > 1000:
                self.send_to_arduino(self.create_packet(Command.PING))
                last_ping = millis()

            self.read()

            if self.check_acknowledged():
                self.connected = True
                print("Arduino connected")

    def read(self):
        """

        :return:
        """
        if self.debug_mode:
            return

        # Timeout if waiting too long for next byte of command
        if self.rx_state != ReceiveState.WAIT_START and millis() - self.last_byte_time > RX_TIMEOUT:
            self.rx_state = ReceiveState.WAIT_START

        # Loop while bytes are available
        while self.arduino.in_waiting > 0:
            incoming = self.arduino.read(1)
            self.last_byte_time = millis()

            match self.rx_state:
                case ReceiveState.WAIT_START:
                    if incoming[0] == START_BYTE:
                        self.rx_state = ReceiveState.READ_COMMAND
                    else:
                        debug_data = incoming + self.arduino.readline()
                        text = debug_data.decode("utf-8", errors="ignore").strip()

                        if text:
                            print(f"Arduino debug: {text}")
                case ReceiveState.READ_COMMAND:
                    self.rx_command = incoming[0]

                    if Command(self.rx_command) not in self.command_lengths:
                        self.rx_state = ReceiveState.WAIT_START
                    else:
                        self.rx_state = ReceiveState.READ_LENGTH
                case ReceiveState.READ_LENGTH:
                    self.rx_length = incoming[0]

                    if self.rx_length != self.command_lengths[Command(self.rx_command)] or self.rx_length > MAX_DATA_LENGTH:
                        self.rx_state = ReceiveState.WAIT_START
                    else:
                        self.data_index = 0
                        self.rx_state = ReceiveState.READ_DATA
                case ReceiveState.READ_DATA:
                    if self.data_index < self.rx_length:
                        self.rx_data[self.data_index] = incoming[0]
                        self.data_index += 1
                    else:
                        self.rx_checksum = incoming[0]

                        self.process_packet()

                        self.rx_state = ReceiveState.WAIT_START

    def process_packet(self):
        # Verify checksum
        checksum = 0
        checksum ^= self.rx_command
        checksum ^= self.rx_length

        for x in range(self.rx_length):
            checksum ^= self.rx_data[x]

        if checksum != self.rx_checksum:
            print("Checksum failed")
            return

        match Command(self.rx_command):
            case Command.ACK:
                self.acknowledged = True
            case Command.PING:
                self.send_to_arduino(self.create_packet(Command.ACK))
            case Command.BTN_PRESS:
                self.handle_btn_press(self.rx_data[0])

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

    def handle_btn_press(self, btn_id):
        match ButtonID(btn_id):
            case ButtonID.MUTE:
                #do muting
                self.muted = not self.muted
                self.send_to_arduino(self.create_packet(Command.SET_STATE, bytes([0x00, self.muted])))




    def check_acknowledged(self):
        if self.acknowledged:
            self.acknowledged = False
            return True
        return False