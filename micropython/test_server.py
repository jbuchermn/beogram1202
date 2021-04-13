import utime
import usocket
import uselect
import network

ssid = None
password = None

def load_network():
    global ssid
    global password
    with open('network', 'r') as inp:
        for r in inp:
            r = r.split(":")
            if r[0].lower() == "ssid":
                ssid = r[1].lstrip()
            elif r[0].lower() == "password":
                password = r[1].lstrip()

try:
    load_network()
except:
    print("Warning! Loading network info failed")

class Server:
    def __init__(self):
        self.connected = False

        ap_if = network.WLAN(network.AP_IF)
        ap_if.active(False)

        self.sta_if = network.WLAN(network.STA_IF)
        if not self.sta_if.isconnected():
            print('Connecting to network...')
            self.sta_if.active(True)
            self.sta_if.connect(ssid, password)

        self.socket = None

    def is_connected(self):
        return self.sta_if.isconnected()

    def main(self, timeout):
        if not self.sta_if.isconnected():
            utime.sleep_ms(timeout)
            return

        elif self.socket is None:
            addr = usocket.getaddrinfo('0.0.0.0', 80)[0][-1]
            print("Socket listening on port 80")

            self.socket = usocket.socket()
            self.socket.bind(addr)
            self.socket.listen(1)

        poller = uselect.poll()
        poller.register(self.socket, uselect.POLLIN)
        res = poller.poll(timeout)
        if res:
            cl, addr = self.socket.accept()
            print("Client connected from", addr)
            cl_file = cl.makefile('rwb', 0)
            while True:
                line = cl_file.readline()
                if line is None:
                    break
                print(line)
            cl.send('HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n')
            cl.send('Test')
            cl.close()

if __name__ == '__main__':
    server = Server()
    while True:
        server.main(500)

