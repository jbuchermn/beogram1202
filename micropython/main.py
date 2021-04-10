import time
import utime
import machine

from PID import PID

OUTPUT_LIMITS = (250., 750.)

TIME_SCALE_MS = 100

# 33rpm ~ 520Hz
# 45rpm ~ 710Hz
TARGET = 520

pwm = machine.PWM(machine.Pin(12)) # D6
pin = machine.Pin(14, machine.Pin.IN) # D5


def set_speed(speed):
    speed = min(1., max(0., speed))
    pwm.duty(int(OUTPUT_LIMITS[1] - speed*(OUTPUT_LIMITS[1] - OUTPUT_LIMITS[0])))


class FrequencyCounter:
    def __init__(self, timescale_us):
        self.callback = lambda freq: print(freq)
        self.ts_us = timescale_us

        self._c = 0
        self._reset = utime.ticks_us()

    def count(self):
        self._c += 1
        self.verify()

    def verify(self):
        t = utime.ticks_us()
        if t > self._reset + self.ts_us:
            freq = self._c * 1000000. / (t - self._reset)
            self._c = 0
            self._reset = t
            self.callback(freq)


counter = FrequencyCounter(TIME_SCALE_MS * 1000)
pin.irq(trigger=machine.Pin.IRQ_RISING, handler=lambda pin: counter.count())

def main():
    def update(freq):
        speed = pid(freq)
        print("%f,%f" % (speed, freq))
        set_speed(speed)
    counter.callback = update

    Kp = 0.001
    Ki = 0.004
    Kd = 0.0

    pid = PID(Kp, Ki, Kd, TARGET, TIME_SCALE_MS / 1000., (0., 1.))

    update(0)

    while True:
        time.sleep(TIME_SCALE_MS / 1000.)
        counter.verify()

def main_step(height):
    t0 = utime.ticks_us()
    initial = True

    def update(freq):
        t = utime.ticks_us() - t0
        print("%f,%f,%f" % (t, 0 if initial else height, freq))

    counter.callback = update
    set_speed(0.)
    update(0)

    time.sleep(5.)
    
    initial = False
    update(0)
    set_speed(height)

    while True:
        time.sleep(TIME_SCALE_MS / 1000.)
        counter.verify()

main()
# main_step(0.7)
