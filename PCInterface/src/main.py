# Filename: main.py
# Created: 12/11/2026
# Description: Main program for Arduino PC stat display
from ArduinoConnectionManager import ArduinoConnectionManager


def main():
    arduino = ArduinoConnectionManager("COM3")

    while True:
        arduino.read()

if __name__ == '__main__':
    main()