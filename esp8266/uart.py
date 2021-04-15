from threading import Thread
import time
import serial
import matplotlib.pyplot as plt

class Runner(Thread):
    def __init__(self):
        super().__init__()
        self.t0 = time.time()
        self.data = {
            'delta_t': [],
            'avg_delta_t': [],
            'control': []
        }

        self.pending_cmd = None

    def run(self):
        with serial.Serial('/dev/ttyUSB0', 74880) as ser:
            while True:
                line = ser.readline()
                if len(line) == 4:
                    if line[0] == ord('d'):
                        self.data['delta_t'] += [
                            (time.time() - self.t0, int(line[1]) * 256 + int(line[2]))
                        ]
                    elif line[0] == ord('a'):
                        self.data['avg_delta_t'] += [
                            (time.time() - self.t0, int(line[1]) * 256 + int(line[2]))
                        ]
                    elif line[0] == ord('f'):
                        self.data['control'] += [
                            (time.time() - self.t0, int(line[1]) * 256 + int(line[2]))
                        ]
                if self.pending_cmd is not None:
                    ser.write((self.pending_cmd + "\n").encode("ascii"))
                    self.pending_cmd = None


runner = Runner()
runner.start()

time.sleep(2)
fig = plt.figure()
ax = fig.add_subplot(111)

plt.ion()
plt.show()

while True:
    ax.clear()
    ax.plot(
        [d[0] for d in runner.data['delta_t']],
        [d[1] for d in runner.data['delta_t']], 'r+')
    ax.plot(
        [d[0] for d in runner.data['avg_delta_t']],
        [d[1] for d in runner.data['avg_delta_t']], 'r-')
    ax.plot(
        [d[0] for d in runner.data['control']],
        [d[1] for d in runner.data['control']], 'b-')

    plt.draw()
    cmd = input("Update? ")
    if "=" in cmd:
        runner.pending_cmd = cmd





