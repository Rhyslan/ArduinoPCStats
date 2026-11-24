# Filename: main.py
# Created: 12/11/2026
# Description: Main program for Arduino PC stat display
import threading
import time
import WinTmp
import psutil
from GPUtil import GPUtil

from ArduinoConnectionManager import ArduinoConnectionManager, Command, StatID
from Timer import Timer

class ArduinoStats:
    def __init__(self, debug_mode=False):
        self.debug_mode = debug_mode

        self.stat_values = {
            "CPU Usage": 10,
            "CPU Temp": 20,
            "GPU Usage": 30,
            "GPU Temp": 40,
            "RAM Usage": 50,
        }

        self.running = True
        self.lock = threading.Lock()

        self.stat_updater = threading.Thread(target=self.update_stats)
        self.comms_thread = threading.Thread(target=self.arduino_comms)
        self.ender = threading.Thread(target=self.stopper)

    def start(self) -> None:
        print("Starting...")
        self.stat_updater.start()
        self.comms_thread.start()
        self.ender.start()

        try:
            self.comms_thread.join()
            self.stat_updater.join()
            self.ender.join()
        except RuntimeError as e:
            print(e)
            with self.lock:
                self.running = False
        except KeyboardInterrupt:
            print("Program terminated")
            with self.lock:
                self.running = False

    def stopper(self):
        end_timer = Timer(100000, True)

        while self.running:
            if end_timer.is_done():
                self.stop()

    def stop(self):
        with self.lock:
            self.running = False

    def arduino_comms(self) -> None:
        arduino = ArduinoConnectionManager("COM3", self.debug_mode)
        stat_change_timer = Timer(5000, True)
        stat_update_timer = Timer(500)
        curr_stat = 0

        while self.is_running():
            arduino.read()

            if stat_change_timer.is_done():
                curr_stat += 1
                if curr_stat >= len(StatID):
                    curr_stat = 0

            if stat_update_timer.is_done():
                arduino.send_to_arduino(arduino.create_packet(
                    Command.UPDATE_STAT,
                    bytes([
                        curr_stat,
                        list(self.stat_values.values())[curr_stat]
                    ])
                ))

    def update_stats(self) -> None:
        get_stat_timer = Timer(1000)
        while self.is_running():
            if get_stat_timer.is_done():
                with self.lock:
                    self.stat_values["CPU Usage"] = int(psutil.cpu_percent())

                with self.lock:
                    self.stat_values["CPU Temp"] = int(WinTmp.CPU_Temp())

                with self.lock:
                    gpu: GPUtil.GPU = GPUtil.getGPUs()[0]
                    self.stat_values["GPU Usage"] = min(int(gpu.load * 100), 99)

                with self.lock:
                    self.stat_values["GPU Temp"] = int(WinTmp.GPU_Temp())

                with self.lock:
                    self.stat_values["RAM Usage"] = int(psutil.virtual_memory().percent)

    def is_running(self) -> bool:
        with self.lock:
            return self.running


if __name__ == '__main__':
    thing = ArduinoStats()

    thing.start()
