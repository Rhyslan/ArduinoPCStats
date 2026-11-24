# Filename: Timer.py
# Created: 24/11/2026
# Description: Simple timer
import time


class Timer:
    def __init__(self, duration):
        """
        A simple timer used to check and run things at intervals
        :param duration: Duration/interval of the timer
        """
        self.duration = duration
        self.elapsed = monotonic_ms()

    def reset(self) -> None:
        """
        Resets the timer's elapsed time
        """
        self.elapsed = monotonic_ms()

    def is_done(self) -> bool:
        """
        Checks if the timer is done and has reached the interval duration. Resets the timer if it is done
        :return: Done state of the timer
        """
        if monotonic_ms() - self.elapsed > self.duration:
            self.reset()
            return True
        return False

def monotonic_ms():
    """
    Get monotonic time in milliseconds
    :return: Monotonic time in milliseconds
    """
    return time.monotonic_ns() // 1_000_000