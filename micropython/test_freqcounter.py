import time
import utime
import machine
import gc

gc.disable()


# PWM values for speed 0 and speed 1 (maximum is 1024)
OUTPUT_LIMITS = (1024., 100.)

# Basis for frequency measurement in milliseconds
TIME_SCALE_MS = 100

"""
Output setup
"""
pwm = machine.PWM(machine.Pin(12)) # D6
pwm.duty(1024)

def set_speed(speed):
    speed = min(1., max(0., speed))
    pwm.duty(int(OUTPUT_LIMITS[0] + speed*(OUTPUT_LIMITS[1] - OUTPUT_LIMITS[0])))

"""
Input setup
"""
pin = machine.Pin(14, machine.Pin.IN) # D5

class FrequencyCounter:
    def __init__(self, timescale_ms):
        self.callback = lambda freq: None
        self.ts_us = int(1000 * timescale_ms)

        self._c = 0
        self._reset = None
        self._skip = False

    def count(self, *args):
        self._c += 1
        self.verify()

    def verify(self):
        t = utime.ticks_us()
        d = utime.ticks_diff(t, self._reset)
        if self._reset is None:
            self._reset = t
        elif d > self.ts_us:
            freq = 1000000 * self._c / d

            self._c = 0
            self._reset = t

            self.callback(freq)

    def start(self, pin):
        pin.irq(trigger=machine.Pin.IRQ_RISING, handler=self.count)


counter = FrequencyCounter(TIME_SCALE_MS)


if __name__ == '__main__':
    t0 = utime.ticks_ms()
    def update(freq):
        print("%.3f, %.2f" % (utime.ticks_diff(utime.ticks_ms(), t0)/1000., freq))

    counter.callback = update

    set_speed(.8)
    print("0")

    utime.sleep_ms(1000)
    print("0")
    counter.start(pin)

    i = 0
    while True:
        utime.sleep_ms(TIME_SCALE_MS)
        counter.verify()
        gc.collect()
        

