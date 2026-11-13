# Filename: main.py
# Created: 12/11/2026
# Description:
from ConnManager import ConnManager, Command


def main():
    conn_man = ConnManager(True)

    packet = conn_man.create_packet(Command.ACK)
    conn_man.send_to_arduino(packet)

    packet = conn_man.create_packet(Command.PING)
    conn_man.send_to_arduino(packet)

    packet = conn_man.create_packet(Command.UPDATE_STAT, bytes([0x00, 0x25]))
    conn_man.send_to_arduino(packet)

    packet = conn_man.create_packet(Command.CHANGE_STAT, bytes([0x00, ord("C"), ord("U"), 0x0F]))
    conn_man.send_to_arduino(packet)

    packet = conn_man.create_packet(Command.BTN_PRESS, bytes([0x00]))
    conn_man.send_to_arduino(packet)

    packet = conn_man.create_packet(Command.BTN_RELEASE, bytes([0x00]))
    conn_man.send_to_arduino(packet)

    packet = conn_man.create_packet(Command.SET_STATE, bytes([0x00, 0x01]))
    conn_man.send_to_arduino(packet)

    packet = conn_man.create_packet(Command.REQUEST_STAT, bytes([0x01]))
    conn_man.send_to_arduino(packet)

if __name__ == '__main__':
    main()