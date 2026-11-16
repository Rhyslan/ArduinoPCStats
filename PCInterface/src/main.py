# Filename: main.py
# Created: 12/11/2026
# Description:
import time

from ConnManager import ConnManager, Command


def main():
    conn_man = ConnManager()

    while True:
        conn_man.read()


    packet = conn_man.create_packet(Command.SET_STATE, bytes([0x00, 0x01]))
    conn_man.send_to_arduino(packet)

    time.sleep(1)

    packet = conn_man.create_packet(Command.SET_STATE, bytes([0x00, 0x00]))
    conn_man.send_to_arduino(packet)

    time.sleep(1)

    packet = conn_man.create_packet(Command.SET_STATE, bytes([0x00, 0x01]))
    conn_man.send_to_arduino(packet)

    time.sleep(1)

    packet = conn_man.create_packet(Command.SET_STATE, bytes([0x00, 0x00]))
    conn_man.send_to_arduino(packet)

if __name__ == '__main__':
    main()