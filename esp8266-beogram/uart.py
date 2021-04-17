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
            'delta_t_control': [],
        }

        self.pending_cmd = None
        self.path_base = 'output_%d' % (int(time.time()) % 1000)

    def save(self):
        with open(self.path_base + '_delta_t.csv', 'w') as outp:
            for t, v in self.data['delta_t']:
                outp.write("%f,%f\n" % (t, v))
        with open(self.path_base + '_delta_t_control.csv', 'w') as outp:
            for t, dt, c in self.data['delta_t_control']:
                outp.write("%f,%f,%f\n" % (t, c, dt))

    def run(self):
        with serial.Serial('/dev/ttyUSB0', 74880) as ser:
            pending_dt = None
            while True:
                line = ser.readline()
                while len(line) < 4:
                    line += ser.readline()

                if len(line) == 4:
                    if line[0] == ord('d'):
                        self.data['delta_t'] += [
                            (time.time() - self.t0, int(line[1]) * 256 + int(line[2]))
                        ]
                    elif line[0] == ord('a'):
                        pending_dt = int(line[1]) * 256 + int(line[2])
                    elif line[0] == ord('f'):
                        if pending_dt is not None:
                            self.data['delta_t_control'] += [
                                (time.time() - self.t0, pending_dt, int(line[1]) * 256 + int(line[2]))
                            ]
                            pending_dt = None
                        else:
                            print("Warning! Did not receive delta_t")

                    self.save()
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
        [d[1] for d in runner.data['delta_t']], 'r+', alpha=.1)
    ax.plot(
        [d[0] for d in runner.data['delta_t_control']],
        [d[1] for d in runner.data['delta_t_control']], 'r-')
    ax.plot(
        [d[0] for d in runner.data['delta_t_control']],
        [d[2] / 3. for d in runner.data['delta_t_control']], 'b-')

    plt.draw()
    cmd = input("Update? ")
    if len(cmd.strip()) > 0:
        runner.pending_cmd = cmd.strip()
