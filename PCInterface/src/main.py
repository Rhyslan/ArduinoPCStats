# Filename: main.py
# Created: 12/11/2026
# Description:
import time

from ConnManager import ConnManager, Command


def main():
    conn_man = ConnManager()

    packet = conn_man.create_packet(Command.UPDATE_STAT, bytes([0x00, 0x25]))
    conn_man.send_to_arduino(packet)

    time.sleep(5)

    packet = conn_man.create_packet(Command.UPDATE_STAT, bytes([0x01, 0x17]))
    conn_man.send_to_arduino(packet)

    time.sleep(5)

    packet = conn_man.create_packet(Command.CHANGE_STAT, bytes([0x00, ord("T"), ord("M"), 0x0F]))
    conn_man.send_to_arduino(packet)

    time.sleep(5)

    packet = conn_man.create_packet(Command.UPDATE_STAT, bytes([0x00, 0x05]))
    conn_man.send_to_arduino(packet)

if __name__ == '__main__':
    main()