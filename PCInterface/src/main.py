# Filename: main.py
# Created: 12/11/2026
# Description: Main program for Arduino PC stat display
from ArduinoConnectionManager import ArduinoConnectionManager, Command, StatID
from Timer import Timer

def main():
    arduino = ArduinoConnectionManager("COM3", True)
    stat_change_timer = Timer(5000)
    curr_stat = 1

    arduino.send_to_arduino(arduino.create_packet(Command.UPDATE_STAT, bytes([StatID.CPU_TEMP.value, 10])))

    while True:
        arduino.read()

        if stat_change_timer.is_done():
            match curr_stat:
                case 0:
                    arduino.send_to_arduino(arduino.create_packet(Command.UPDATE_STAT, bytes([StatID.CPU_TEMP.value, 10])))
                case 1:
                    arduino.send_to_arduino(arduino.create_packet(Command.UPDATE_STAT, bytes([StatID.CPU_USAGE.value, 20])))
                case 2:
                    arduino.send_to_arduino(arduino.create_packet(Command.UPDATE_STAT, bytes([StatID.GPU_USAGE.value, 30])))
                case 3:
                    arduino.send_to_arduino(arduino.create_packet(Command.UPDATE_STAT, bytes([StatID.GPU_TEMP.value, 40])))
                case 4:
                    arduino.send_to_arduino(arduino.create_packet(Command.UPDATE_STAT, bytes([StatID.RAM_USAGE.value, 50])))

            curr_stat += 1
            if curr_stat >= len(StatID):
                curr_stat = 0

if __name__ == '__main__':
    main()