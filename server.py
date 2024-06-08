from socketserver import BaseRequestHandler, TCPServer, ThreadingMixIn
import threading
from queue import Queue
from queue import Empty as QueueEmpty

# Outgoing/Incoming commands
POSITION = "P"
ROOM = "R"
TRAP = "T"
GAMESTART = "S"
GAMEOVER = "O"
KICK = "K"
FACING = "F"
TRAPACTIVATED = "A"
ITEMTAKEN = "I"
ATTACK = "C"

# Lobby commands
JOINLOBBY = "J"

ROOM_N = 6


class Client(BaseRequestHandler):
    def setup(self):
        self.running = True
        self.outqueue = Queue()
        self.ip, self.port = "Unknown address", "Unknown port"
        self.start()

    def start(self):
        run_thread = threading.Thread(target=self.run)
        run_thread.daemon = True
        run_thread.start()

    def run(self):
        while self.running:
            try:
                buffer = bytearray()

                try:
                    message = self.outqueue.get(timeout=5)
                    buffer.extend(message)
                    try:
                        while True:
                            message = self.outqueue.get(block=False)
                            buffer.extend(message)
                    except QueueEmpty:
                        pass
                except QueueEmpty:
                    continue

                data = bytes(buffer)
                self.request.sendall(data)
            except (ConnectionResetError, OSError):
                break
            except Exception as e:
                print("Error sending message:", repr(e))
                self.request.close()

    def handle(self):
        self.ip, self.port = self.client_address
        model: "Model" = self.server.model
        buffer = bytearray()
        model.enqueue(model.on_connect, self)

        while True:
            try:
                data = self.request.recv(4096)
            except ConnectionResetError:
                break

            if not data:
                break

            buffer.extend(data)

            while b"\n" in buffer:
                args, buffer = buffer.split(b"\n", 1)
                args = args.decode("utf-8").strip("\x00").split(",")

                model.enqueue(model.on_data, self, (args,))

        model.enqueue(model.on_disconnect, self)

    def send(self, *args):
        self.outqueue.put((",".join(args) + "\n").encode("utf-8"))


class Trap:
    def __init__(self, owner, room, x, y):
        self.room = room
        self.x = x
        self.y = y
        self.owner = owner

class Lobby:
    def __init__(self, model, lobbyN):
        self.clients = []
        self.positions = [(0, 0), (0, 0)]
        self.rooms = [0, 0]
        self.model = model
        self.lobbyN = lobbyN
        self.running = True
        self.game_started = False
        self.traps = []
        self.fnmap = {
            POSITION: self.on_position,
            ROOM: self.on_room_change,
            TRAP: self.on_trap,
            TRAPACTIVATED: self.on_trap_activated,
            GAMEOVER: self.on_game_over,
            FACING: self.on_facing,
            ITEMTAKEN: self.on_item_taken,
            ATTACK: self.on_attack,
        }
        thread = threading.Thread(target=self.run)
        thread.daemon = True
        thread.start()

    def run(self):
        pass

    def on_item_taken(self, client, playerN, furnitureN, itemN):
        try:
            playerN = int(playerN)
            furnitureN = int(furnitureN)
            itemN = int(itemN)
        except ValueError:
            print(f"Invalid item data: {furnitureN}, {itemN}")
            return
        
        itemN = max(0, itemN - 1)

        for other in self.clients:
            other.send(ITEMTAKEN, str(playerN), str(furnitureN), str(itemN))
    
    def on_attack(self, client, targetN, damage):
        try:
            targetN = int(targetN)
            damage = float(damage)
        except ValueError:
            print(f"Invalid attack data: {targetN}, {damage}")
            return

        for other in self.clients:
            if hasattr(other, "playerN") and other.playerN == targetN:
                other.send(ATTACK, str(damage))
                break


    def on_facing(self, client, playerN, facing):
        try:
            facing = int(facing)
        except ValueError:
            print(f"Invalid facing data: {facing}")
            return

        if not hasattr(client, "playerN"):
            client.playerN = playerN

        for other in self.clients:
            if not hasattr(other, "playerN") or other.playerN == client.playerN:
                continue

            other.send(FACING, str(client.playerN), str(facing))

    def on_connect(self, client):
        if self.game_started:
            client.send(KICK, "The lobby is full.")
        
        self.clients.append(client)

        if len(self.clients) == 2:
            self.game_started = True
    
    def on_disconnect(self, client):
        try:
            if client in self.clients:
                self.clients.remove(client)
                if len(self.clients) == 0:
                    self.model.on_deletelobby(self.lobbyN)
        except ValueError:
            print("Client not in lobby")

    def on_room_change(self, client, playerN, roomN):
        try:
            playerN = int(playerN)
            roomN = int(roomN)
        except ValueError:
            print(f"Invalid room data: {playerN}, {roomN}")
            return

        if not hasattr(client, "playerN"):
            client.playerN = playerN

        self.rooms[playerN] = roomN
        for other in self.clients:
            if not hasattr(other, "playerN") or other.playerN == client.playerN:
                continue

            other.send(ROOM, str(client.playerN), str(self.rooms[playerN]))

    def on_game_over(self, client, winner):
        try:
            winner = int(winner)
        except ValueError:
            print(f"Invalid winner data: {winner}")
            return

        for other in self.clients:
            other.send(GAMEOVER, str(winner))

    def on_trap(self, client, playerN, trapN, roomN, x, y):
        try:
            playerN = int(playerN)
            trapN = int(trapN)
            roomN = int(roomN)
            x = float(x)
            y = float(y)
        except ValueError:
            print(f"Invalid trap data: {playerN}, {trapN}, {roomN}, {x}, {y}")
            return
        
        if not hasattr(client, "playerN"):
            client.playerN = playerN

        self.traps.append(Trap(playerN, roomN, x, y))

        for other in self.clients:
            # if not hasattr(other, "playerN") or other.playerN == client.playerN:
                # continue

            other.send(TRAP, str(playerN), str(trapN), str(roomN), str(x), str(y))

    def on_trap_activated(self, client, trapN):
        try:
            trapN = int(trapN)
        except ValueError:
            print(f"Invalid trap activation data: {trapN}")
            return

        if trapN < 0 or trapN >= len(self.traps):
            return

        trap = self.traps[trapN]
        for other in self.clients:
            other.send(TRAPACTIVATED, str(trapN))

    def on_position(self, client, playerN, x, y):
        try:
            playerN = int(playerN)
            x = float(x)
            y = float(y)
        except ValueError:
            print(f"Invalid position data: {playerN}, {x}, {y}")
            return

        if playerN < 0 or playerN >= len(self.positions):
            return

        if not hasattr(client, "playerN"):
            client.playerN = playerN

        self.positions[playerN] = (x, y)
        for other in self.clients:
            if not hasattr(other, "playerN") or other.playerN == client.playerN:
                continue

            otherPos = self.positions[other.playerN]
            client.send(
                POSITION, str(other.playerN), str(otherPos[0]), str(otherPos[1])
            )


class Model:
    def __init__(self):
        self.running = True
        self.inqueue = Queue()
        self.outqueue = Queue()
        self.clients = []
        self.lobbies: dict[int, Lobby] = {}
        self.fnmap = {
            JOINLOBBY: self.on_joinlobby,
        }

    def on_joinlobby(self, client, lobbyN):
        try:
            lobbyN = int(lobbyN)
        except ValueError:
            print(f"Invalid lobby number: {lobbyN}")
            return

        if lobbyN not in self.lobbies:
            print(f"Creating lobby {lobbyN}")
            new_lobby = Lobby(self, lobbyN)
            self.lobbies[lobbyN] = new_lobby

        lobby = self.lobbies[lobbyN]
        client.lobby = lobby
        client.lobby.on_connect(client)
    
    def on_deletelobby(self, lobbyN):
        print(f"Deleting lobby {lobbyN}")
        if lobbyN in self.lobbies:
            del self.lobbies[lobbyN]

    def on_connect(self, client):
        print(f"Client at {client.ip}:{client.port} connected")
        client.position = (0, 0)
        self.clients.append(client)

    def on_disconnect(self, client):
        print(f"Client {client.ip}:{client.port} disconnected")
        if hasattr(client, "lobby"):
            client.lobby.on_disconnect(client)
        
        self.clients.remove(client)

    def on_data(self, client, message):
        try:
            if message[0] in self.fnmap:
                self.fnmap[message[0]](client, *message[1:])
                return
        except KeyError:
            print(f"Unknown lobby message type: {message[0]}")
            return
        except Exception as e:
            print(f"Error processing lobby message: {e}")
            return

        try:
            lobby = self.lobbies[int(message[0])]
        except ValueError:
            print(f"Invalid lobby number: {message[0]}")
            return
        except KeyError:
            print(f"Unknown lobby: {message[0]}")
            return

        try:
            lobby.fnmap[message[1]](client, *message[2:])
        except KeyError:
            print(f"Unknown message type: {message[0]}")
        except Exception as e:
            print(f"Error processing message: {e}")

    def enqueue(self, func, socket, args=()):
        self.inqueue.put((func, socket, args))

    def run(self):
        while self.running:
            buffer = []

            try:
                item = self.inqueue.get(timeout=5)
                buffer.append(item)

                try:
                    while True:
                        item = self.inqueue.get(block=False)
                        buffer.append(item)
                except QueueEmpty:
                    pass

                for func, socket, args in buffer:
                    try:
                        func(socket, *args)
                    except Exception as e:
                        print(f"Error calling function {func}: {repr(e)}")

            except QueueEmpty:
                continue

    def start(self):
        self.running = True
        thread = threading.Thread(target=self.run)
        thread.daemon = True
        thread.start()


class Server(ThreadingMixIn, TCPServer):
    pass


if __name__ == "__main__":
    HOST, PORT = "0.0.0.0", 3490

    model = Model()
    model.start()
    server = Server((HOST, PORT), Client)
    server.model = model

    print(f"Server loop running on {HOST}:{PORT}")
    server.serve_forever()
