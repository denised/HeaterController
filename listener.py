import socketserver
import socket
import threading
from datetime import datetime


class MyUDPHandler(socketserver.BaseRequestHandler):
    def handle(self):
        data = self.request[0].decode().strip()
        print(f"{datetime.now():%X}: {self.client_address[0]} wrote:\n{data}")
        print(". ")

def monitor_port(portno):
    with socketserver.UDPServer(("", portno), MyUDPHandler) as server:
        server.serve_forever()

if __name__ == "__main__":
    t1 = threading.Thread(target=monitor_port, args=(3339,), daemon=True)
    t2 = threading.Thread(target=monitor_port, args=(3341,), daemon=True)
    t1.start()
    t2.start()

    broadcaster = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    while(1):
        cmd = input(". ")
        broadcaster.sendto(cmd.encode(), ("10.0.0.255", 3337))
        print(f"sent {cmd}")

