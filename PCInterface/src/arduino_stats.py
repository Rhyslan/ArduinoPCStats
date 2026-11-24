# Filename: arduino_stats.py
# Created: 24/11/2026
# Description:
import threading
import time

import WinTmp
import psutil
from GPUtil import GPUtil
from pywinauto.application import Application

from arduino_connection_manager import ArduinoConnectionManager, Command, StatID, StateID
from timer import Timer

class States:
    def __init__(self):
        self.muted = False
        self.update_mute = False

class ArduinoStats:
    def __init__(self, debug_mode=False):
        """

        :param debug_mode:
        """
        self.debug_mode = debug_mode

        self.stat_values = {
            "CPU Usage": 10,
            "CPU Temp": 20,
            "GPU Usage": 30,
            "GPU Temp": 40,
            "RAM Usage": 50,
        }

        self.states = States()

        self.lastValue = None

        self.running = True
        self.lock = threading.RLock()

        self.stat_updater = threading.Thread(target=self.update_stats)
        self.comms_thread = threading.Thread(target=self.arduino_comms)
        self.discord_sync = threading.Thread(target=self.sync_discord_mute)

    def start(self) -> None:
        """

        """
        print("Starting...")
        self.stat_updater.start()
        self.comms_thread.start()
        self.discord_sync.start()

        try:
            self.comms_thread.join()
            self.stat_updater.join()
            self.discord_sync.join()
        except RuntimeError as e:
            print(e)
            with self.lock:
                self.running = False
        except KeyboardInterrupt:
            print("Program terminated")
            with self.lock:
                self.running = False

    def stop(self) -> None:
        """

        """
        print("Stopping")
        with self.lock:
            self.running = False

    def is_running(self) -> bool:
        """

        :return:
        """
        with self.lock:
            return self.running

    def arduino_comms(self) -> None:
        """

        """
        arduino = ArduinoConnectionManager("COM3", self.states, self.lock, self.debug_mode)
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
                value_to_send = list(self.stat_values.values())[curr_stat]
                if value_to_send != self.lastValue:
                    arduino.send_to_arduino(arduino.create_packet(
                        Command.UPDATE_STAT,
                        bytes([
                            curr_stat,
                            value_to_send
                        ])
                    ))

                    self.lastValue = value_to_send

            with self.lock:
                if self.states.update_mute:
                    arduino.send_to_arduino(arduino.create_packet(
                        Command.SET_STATE,
                        bytes([
                            StateID.MUTE.value,
                            self.states.muted
                        ])
                    ))

                    self.states.update_mute = False

    def update_stats(self) -> None:
        """

        """
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

    def sync_discord_mute(self) -> None:
        """

        """
        discord = Application(backend="uia").connect(title_re=".* - Discord").window()

        while self.running:
            is_muted = discord.child_window(title="Unmute", control_type="Text").exists()
            with self.lock:
                if self.states.muted != is_muted:
                    self.states.muted = is_muted
                    self.states.update_mute = True

            time.sleep(1)