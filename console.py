import socketserver
import socket
import threading
from pathlib import Path
from datetime import datetime

# ####################################################################
# Listen for messages coming from either the heater or the temperature station,
# and send commands to the heater.
######################################################################


# sync with 3way_controller/components/include/libconfig.h
temp_port = 3339
heater_control_port = 3337
heater_broadcast_port = 3341
ota_port = 3343

# TODO: listen for the heater's broadcast and remember its address instead of braodcasting?
heater_ip = '10.0.0.255'

currdir = Path(__file__).parent
heaterbinary = currdir/"3way_controller/build/3way_controller.bin"

# Listener
class MyUDPHandler(socketserver.BaseRequestHandler):
    def handle(self):
        data = self.request[0].decode().strip()
        print(f"{datetime.now():%X}: {self.client_address[0]} wrote: {data}")
        print(". ", end="", flush=True)

def monitor_port(portno):
    with socketserver.UDPServer(("", portno), MyUDPHandler) as server:
        server.serve_forever()

# uploader
def start_upload(path: Path):
    """Prepare connection and send file contents"""
    sender = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sender.bind(('', ota_port))
    sender.listen()
    with sender:
        conn, _ = sender.accept()
        print("Upload connection accepted.\n. ")
        with conn:
            with open(str(path), mode='rb') as f:
                try:
                    conn.sendfile(f)
                except ConnectionResetError:
                    # this happens when the device reboots.
                    pass
                print("Upload completed.\n. ")


if __name__ == "__main__":
    # Listen to data coming from the temperature station and heater
    t1 = threading.Thread(target=monitor_port, args=(temp_port,), daemon=True)
    t2 = threading.Thread(target=monitor_port, args=(heater_broadcast_port,), daemon=True)
    t1.start()
    t2.start()

    # channel we use to send commands to the heater
    broadcaster = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    while(1):
        cmd = input(". ")

        if cmd in ["?","help", "h"]:
            print("""
            hello:  heater responds with udp message
            version: respond with version id
            level off|low|medium|high|auto: set heater level
            bump amount, duration: increase/decrease the desired temperature by
                  amount degrees for duration hours
            schedule temp{1:24}  set an hourly schedule for desired temps.  If the
                  schedule is less than 24 hours long, the last value is repeated.
            update: upgrade to the current version in the build directory
            reboot: tell the heater to reboot itself [TODO]
            report: [TODO]
            """)
        elif cmd.startswith("up"):
            # Currently hardwiring the path.  If we need to handle multiple binaries, will have to modify.
            fp = Path(heaterbinary)
            if not fp.exists():
                print(f"File {fp} doesn't seem to exist")
            else:
                filelen = fp.stat().st_size
                t3 = threading.Thread(target=start_upload, args=(fp,), daemon=True)
                t3.start()

                # warning: this next line will not work on some unix systems.
                # in that case, replace with the code indicated here: https://stackoverflow.com/a/28950776
                myip = socket.gethostbyname(socket.gethostname())
                outcommand = f"update {myip} {filelen}"
                broadcaster.sendto(outcommand.encode(), (heater_ip, heater_control_port))
                print(f"sent {outcommand}")
        else:
            broadcaster.sendto(cmd.encode(), (heater_ip, heater_control_port))
            print(f"sent {cmd}")
 
