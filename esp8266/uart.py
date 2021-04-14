from threading import Thread
import time
import serial
import matplotlib.pyplot as plt

t0 = time.time()
data = {
    'delta_t': [],
    'avg_delta_t': [],
    'control': []
}

def run():
    with serial.Serial('/dev/ttyUSB0', 74880) as ser:
        while True:
            line = ser.readline()
            if len(line) == 4:
                if line[0] == ord('d'):
                    data['delta_t'] += [(time.time() - t0, int(line[1]) * 256 + int(line[2]))]
                elif line[0] == ord('a'):
                    data['avg_delta_t'] += [(time.time() - t0, int(line[1]) * 256 + int(line[2]))]

                elif line[0] == ord('f'):
                    data['control'] += [(time.time() - t0, int(line[1]) * 256 + int(line[2]))]
                print(".")
Thread(target=run).start()


fig = plt.figure()
ax = fig.add_subplot(111)

plt.ion()
plt.show()

while True:
    ax.clear()
    ax.plot([d[0] for d in data['delta_t']], [d[1] for d in data['delta_t']], 'r+')
    ax.plot([d[0] for d in data['avg_delta_t']], [d[1] for d in data['avg_delta_t']], 'r-')
    ax.plot([d[0] for d in data['control']], [d[1] for d in data['control']], 'b-')

    plt.draw()
    input("Update? ")




