# Filename: arduino_connection_manager.py
# Created: 13/11/2026
# Description: Communication manager for messages between Arduino and Python
import threading
import keyboard

import _thread
import serial
from enum import Enum, auto

#from arduino_stats import States
from timer import Timer, monotonic_ms

# Constants
START_BYTE = 0xAA
MAX_DATA_LENGTH = 2
RX_TIMEOUT = 100

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


class StatID(Enum):
    CPU_USAGE = 0
    CPU_TEMP = auto()
    GPU_USAGE = auto()
    GPU_TEMP = auto()
    RAM_USAGE = auto()


class ButtonID(Enum):
    MUTE = 0


class StateID(Enum):
    MUTE = 0


# Main class
class ArduinoConnectionManager:
    def __init__(self, com_port, states, lock, debug_mode=False):
        """
        Connection manager for Arduino and Python
        :param str com_port: COM port for the Arduino
        :param bool debug_mode: Enable debug mode to disable serial connection
        :param States states:
        :param _thread.RLock lock:
        """

        self.debug_mode = debug_mode

        self.arduino = serial.Serial(port=com_port, baudrate=115200, timeout=1)

        # Length of data array for each command
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

        # States
        self.acknowledged = False
        #self.muted = False
        self.states = states
        self.lock = lock

        # Wait for connection acknowledge from Arduino
        self.connected = False
        if debug_mode:
            print("Waiting for Arduino connection...")
        self.send_to_arduino(self.create_packet(Command.PING))
        ping_timer = Timer(1000)
        while not self.connected:
            # Ping Arduino every second
            if ping_timer.is_done():
                self.send_to_arduino(self.create_packet(Command.PING))

            self.read()

            if self.check_acknowledged():
                # Inform Arduino of connection success
                self.send_to_arduino(self.create_packet(Command.ACK))
                self.connected = True
                if self.debug_mode:
                    print("Arduino connected")

    def read(self) -> None:
        """
        Read serial messages from the Arduino
        """

        # Timeout if waiting too long for next byte of the command
        if self.rx_state != ReceiveState.WAIT_START and monotonic_ms() - self.last_byte_time > RX_TIMEOUT:
            self.rx_state = ReceiveState.WAIT_START

        # Loop while bytes are available
        while self.arduino.in_waiting > 0:
            # Get next byte
            incoming = self.arduino.read(1)
            self.last_byte_time = monotonic_ms()

            match self.rx_state:
                case ReceiveState.WAIT_START:
                    # Process command if byte is START_BYTE, otherwise treat as debug string
                    if incoming[0] == START_BYTE:
                        self.rx_state = ReceiveState.READ_COMMAND
                    else:
                        debug_data = incoming + self.arduino.readline()
                        text = debug_data.decode("utf-8", errors="ignore").strip()
                        if self.debug_mode:
                            print(f"Arduino debug: {text}")
                case ReceiveState.READ_COMMAND:
                    self.rx_command = incoming[0]

                    # Reset if unknown command
                    if Command(self.rx_command) not in self.command_lengths:
                        self.rx_state = ReceiveState.WAIT_START
                    else:
                        self.rx_state = ReceiveState.READ_LENGTH
                case ReceiveState.READ_LENGTH:
                    self.rx_length = incoming[0]

                    # Reset if length is incorrect for command or longer than max data length
                    if self.rx_length != self.command_lengths[Command(self.rx_command)] or self.rx_length > MAX_DATA_LENGTH:
                        self.rx_state = ReceiveState.WAIT_START
                    else:
                        self.data_index = 0
                        self.rx_state = ReceiveState.READ_DATA
                case ReceiveState.READ_DATA:
                    # If still reading data, add byte to data array
                    if self.data_index < self.rx_length:
                        self.rx_data[self.data_index] = incoming[0]
                        self.data_index += 1
                    else:
                        # When done reading data, get checksum byte
                        self.rx_checksum = incoming[0]

                        self._process_packet()

                        # Return to waiting state
                        self.rx_state = ReceiveState.WAIT_START

    def create_packet(self, command, data=b'') -> bytearray:
        """
        Create a new packet to send to the Arduino
        :param Command command: Command ID
        :param bytes data: Array of bytes to send as the payload
        :return: New packet bytearray
        """
        # Ensure command is known
        if command not in self.command_lengths:
            raise ValueError(f"Unknown command: {command}")

        # Ensure data is the correct length for the command
        if len(data) != self.command_lengths[command]:
            raise ValueError(f"Invalid paylad for command {command}: expected {self.command_lengths[command]}, got {len(data)}")

        # Create packet bytearray
        packet = bytearray([
            START_BYTE,
            command.value,
            len(data)
        ])

        packet.extend(data)

        # Calculate checksum
        checksum: int = 0
        for byte in packet[1:]:
            checksum ^= byte

        packet.append(checksum)

        return packet

    def send_to_arduino(self, packet) -> None:
        """
        Send a packet to the Arduino. Only prints packet if in debug mode
        :param bytearray packet: Packet to send
        """
        if self.debug_mode:
            print(f"Sending packet: {packet.hex()}...")
        self.arduino.write(packet)

    def check_acknowledged(self) -> bool:
        """
        Checks if the Arduino has acknowledged a command since the last check
        :return: Bool indicating acknowledgement
        """
        if self.acknowledged:
            self.acknowledged = False
            return True
        return False

    def _process_packet(self) -> None:
        """
        Process received packet. Validates checksum then handles the command
        """
        # Verify checksum
        checksum = 0

        checksum ^= self.rx_command
        checksum ^= self.rx_length

        for x in range(self.rx_length):
            checksum ^= self.rx_data[x]

        if checksum != self.rx_checksum:
            if self.debug_mode:
                print("Checksum failed")
            return

        # Handle command
        match Command(self.rx_command):
            case Command.ACK:
                self.acknowledged = True
            case Command.PING:
                # Send ping acknowledgement to Arduino
                self.send_to_arduino(self.create_packet(Command.ACK))
            case Command.BTN_PRESS:
                threading.Thread(target=self._handle_btn_press, args=(self.rx_data[0],)).start()
                #self._handle_btn_press(self.rx_data[0])

    def _handle_btn_press(self, btn_id) -> None:
        """
        Handles button press command
        :param int btn_id:
        """
        match ButtonID(btn_id):
            case ButtonID.MUTE:
                # Toggle Discord mute
                keyboard.press_and_release("ctrl+shift+alt+F12")

                with self.lock:
                    self.states.muted = not self.states.muted
                    # Change Arduino muted state
                    self.send_to_arduino(self.create_packet(Command.SET_STATE, bytes([StateID.MUTE.value, self.states.muted])))
