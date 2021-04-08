import time
import machine

pwm = machine.PWM(machine.Pin(12))
pin = machine.Pin(14, machine.Pin.IN)

# 33rpm ~ 520Hz
counter = 0
def handle_interrupt(pin):
    global counter
    counter += 1

pin.irq(trigger=machine.Pin.IRQ_RISING, handler=handle_interrupt)

def set_speed(perc):
    pwm.duty(650 - int(250 * min(1, max(0, perc))))

p = 0.50
while True:
    counter = 0
    set_speed(p)
    time.sleep(5.)
    print(p, " -> ", counter/5., "Hz")
