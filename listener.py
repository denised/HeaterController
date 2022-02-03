import socketserver
from datetime import datetime

class MyUDPHandler(socketserver.BaseRequestHandler):

    def handle(self):
        data = self.request[0].strip()
        print(f"{datetime.now():%X}: {self.client_address[0]} wrote:\n{data}")

if __name__ == "__main__":
    with socketserver.UDPServer(("", 3339), MyUDPHandler) as server:
        print("Ready to go!")
        server.serve_forever()
