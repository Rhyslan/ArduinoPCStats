# Filename: main.py
# Created: 12/11/2026
# Description:

import serial
import time
import json


def main():
    arduino = serial.Serial(port="COM3", baudrate=115200, timeout=0.5)

    time.sleep(1)

    for x in range(37):
        jsonified = json.dumps({
            "charIndex": x
        })

        arduino.write(bytes(jsonified, "utf-8"))
        time.sleep((0.5))

'''
    while(True):
        charIndex = input("Char index: ")

        jsonified = json.dumps({
            "charIndex": int(charIndex)
        })

        arduino.write(bytes(jsonified, "utf-8"))
        print(bytes(jsonified, "utf-8"))

        time.sleep(0.05)
        '''

if __name__ == '__main__':
    main()