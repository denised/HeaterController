import socketserver
import socket
import threading
from datetime import datetime

# Listen for messages coming from either the heater or the temperature station,
# and send commands to the heater.

class MyUDPHandler(socketserver.BaseRequestHandler):
    def handle(self):
        data = self.request[0].decode().strip()
        print(f"{datetime.now():%X}: {self.client_address[0]} wrote: {data}")
        print(". ", end="", flush=True)

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

        if cmd in ["?","help", "h"]:
            print("""
            hello:  heater responds with udp message
            level off|low|medium|high|auto: set heater level
            bump amount, duration: increase/decrease the desired temperature by
                  amount degrees for duration hours
            schedule temp{1:24}  set an hourly schedule for desired temps.  If the
                  schedule is less than 24 hours long, the last value is repeated.
            reboot: tell the heater to reboot itself [TODO]
            update filename: upgrade the sofware [TODO]
            report: [TODO]
            """)
        else:
            broadcaster.sendto(cmd.encode(), ("10.0.0.255", 3337))
            print(f"sent {cmd}")

