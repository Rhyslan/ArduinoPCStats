# Filename: main.py
# Created: 12/11/2026
# Description: Main program for Arduino PC stat display
from arduino_stats import ArduinoStats

if __name__ == '__main__':
    thing = ArduinoStats()
    thing.start()
