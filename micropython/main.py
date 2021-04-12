import time
import utime
import machine

from PID import PID

# PWM values for speed 0 and speed 1 (maximum is 1024)
OUTPUT_LIMITS = (1024., 100.)

# Basis for frequency measurement in milliseconds
TIME_SCALE_MS = 200

# Measurement lowpass
MEASURE_LP = 10
MEASURE_LP_INERTIA = 0.9

# Hz control per rpm output
REFERENCE = 13.26

# State change time constants
MANUAL_TIMEOUT_S = 10

# Aimed for accuracy in percent
TARGET_ACCURACY = 2 # Should be < 0.2

# PID constants
Kp = 0.0005
Ti = 2
Td = 0.1

"""
Output setup
"""
pwm = machine.PWM(machine.Pin(12)) # D6
pwm.duty(1024)

pstatus0 = machine.Pin(5, machine.Pin.OUT) # D1
pstatus1 = machine.Pin(4, machine.Pin.OUT) # D2
pstatus2 = machine.Pin(0, machine.Pin.OUT) # D3
pstatus0.value(0)
pstatus1.value(0)
pstatus2.value(0)

def set_speed(speed):
    speed = min(1., max(0., speed))
    pwm.duty(int(OUTPUT_LIMITS[0] + speed*(OUTPUT_LIMITS[1] - OUTPUT_LIMITS[0])))

"""
Input setup
"""
pin = machine.Pin(14, machine.Pin.IN) # D5

class FrequencyCounter:
    def __init__(self, timescale_us):
        self.callback = lambda freq: None
        self.ts_us = timescale_us

        self._c = 0
        self._reset = utime.ticks_us()

    def count(self):
        self._c += 1
        self.verify()

    def verify(self):
        t = utime.ticks_us()
        if utime.ticks_diff(t, self._reset) > self.ts_us:
            freq = self._c * 1000000. / utime.ticks_diff(t, self._reset)
            self._c = 0
            self._reset = t
            self.callback(freq)

class Lowpass:
    def __init__(self, inertia, update_every):
        self.inertia = inertia
        self.update_every = update_every
        self.callback = lambda val: None
        self.fine_callback = lambda val: None

        self._v = 0
        self._c = 0

    def update(self, val):
        self._v = self._v * self.inertia + val * (1. - self.inertia)
        self._c += 1

        if self._c == self.update_every:
            self.callback(self._v)
            self._c = 0
        else:
            self.fine_callback(self._v)


counter = FrequencyCounter(TIME_SCALE_MS * 1000 / MEASURE_LP)
pin.irq(trigger=machine.Pin.IRQ_RISING, handler=lambda pin: counter.count())

freq = Lowpass(MEASURE_LP_INERTIA, MEASURE_LP)
counter.callback = freq.update

manual_values = {
    33: 0.72,
    45: 0.90
}

def load_manual():
    global manual_values
    with open('values', 'r') as inp:
        for r in inp:
            r = r.split(":")
            manual_values[int(r[0])] = float(r[1])

def save_manual(speed, ctl):
    global manual_values
    manual_values[speed] = ctl

    with open('values', 'w') as outp:
        for k in manual_values:
            outp.write("%d: %.10f" % (k, manual_values[k]))

try:
    load_manual()
except:
    print("Warning! Loading manual values failed")

"""
Main
"""
class Main:
    def __init__(self, speed, plot=True):
        self.t0 = utime.ticks_ms()
        self.plot = True

        self.speed = speed

        self.state = "manual"
        self.state_since = utime.ticks_ms()

        self.target = self.speed * REFERENCE
        self.pid = PID(Kp, Kp / Ti if Ti > 0 else 0, Kp * Td, self.target, TIME_SCALE_MS / 1000., (0., 1.))

        self.hit_since = None
        self.hit_saved = False

        self.switched_off_since = None
        self.switch_off_detected = False


    def switch_state(self, to_state):
        if to_state == self.state:
            return

        self.state = to_state
        self.state_since = utime.ticks_ms()

        if "pid" in to_state:
            self.pid.set_auto_mode(True, manual_values[self.speed])
        else:
            self.pid.set_auto_mode(False)

    def switch_speed(self, speed):
        if speed == self.speed:
            return

        self.speed = speed
        self.target = self.speed * REFERENCE
        self.pid = PID(Kp, Kp / Ti if Ti > 0 else 0, Kp * Td, self.target, TIME_SCALE_MS / 1000., (0., 1.))

        self.hit_saved = False

        # Switching speed => Manual
        self.switch_state("manual")

    def update(self, freq):
        t = utime.ticks_diff(utime.ticks_ms(), self.t0) / 1000.
        hit = abs(freq-self.target) / self.target < TARGET_ACCURACY * 0.01

        pstatus0.value(hit)
        pstatus1.value(self.state == "manual")
        pstatus2.value(self.switch_off_detected)

        if self.state == "manual":
            self.ctl = 0

            if abs(self.target - freq) / self.target < 0.05:
                # Reached target area => Hand over fine-tuning to PID
                self.switch_state("pid")
                self.update(freq)
                return

            if not self.switch_off_detected and utime.ticks_diff(utime.ticks_ms(), self.state_since) > 1000 * MANUAL_TIMEOUT_S:
                # Too long in state manual => Assume manual_values are invalid (this transition should not happen)
                self.switch_state("pid")
                self.update(freq)
                return

            elif freq < 0.5 * self.target:
                self.ctl = 1.0
            else:
                self.ctl = manual_values[self.speed]

            set_speed(self.ctl)
            if self.plot:
                print("%f,%f,%f,manual" % (t, self.ctl, freq))

            if freq < 0.1 * self.target:
                if self.switched_off_since is None:
                    self.switched_off_since = utime.ticks_ms()

                if utime.ticks_diff(utime.ticks_ms(), self.switched_off_since) > 1000:
                    self.switch_off_detected = True
            else:
                if self.switch_off_detected:
                    # We're starting again => Reset manual state
                    self.state_since = utime.ticks_ms()

                self.switched_off_since = None
                self.switch_off_detected = False

        else:
            if freq < 0.9 * self.target:
                # Drop detected => Hand over control to manual
                self.switch_state("manual")
                self.update(freq)
                return

            self.ctl = self.pid(freq)
            set_speed(self.ctl)

            if self.plot:
                print("%f,%f,%f,pid,%s" %
                      (t, self.ctl, freq, ",".join([str(s) for s in self.pid.components])))

            if hit:
                if self.hit_since is None:
                    self.hit_since = utime.ticks_ms()

                if utime.ticks_diff(utime.ticks_ms(), self.hit_since) > 1000 * 2 and not self.hit_saved:
                    save_manual(self.speed, self.ctl)
                    self.hit_saved = True
            else:
                self.hit_since = None


    def main(self):
        freq.callback = self.update
        if self.plot:
            freq.fine_callback = lambda freq: print("%f,%f,%f,measure" % (
                utime.ticks_diff(utime.ticks_ms(), self.t0) / 1000., self.ctl, freq))

        self.update(0)
        while True:
            time.sleep(TIME_SCALE_MS / MEASURE_LP / 1000.)
            counter.verify()


if __name__ == '__main__':
    # Regular operation
    Main(33, True).main()
    
    # Constant output
    # set_speed(0.7)
    # counter.callback = lambda freq: print(freq)
    # while True:
    #     time.sleep(1.)
