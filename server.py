import socket
import msgpack


def server_program():
    # get the hostname
    host = "127.0.0.1"
    port = 8080

    server_socket = socket.socket()  # get instance
    server_socket.bind((host, port))  # bind host address and port together

    # configure how many clients the server can listen simultaneously
    server_socket.listen(1)
    conn, address = server_socket.accept()  # accept new connection
    print("Connection from: " + str(address))

    unpacker = msgpack.Unpacker()
    while True:
        # receive data stream. it won't accept data packet greater than 1024 bytes
        buf = conn.recv(1024)
        if not buf:
            break  # if data is not received break
        unpacker.feed(buf)
        for obj in unpacker:
            print("from connected user: " + str(obj))
            conn.send(msgpack.packb("Hi from server!"))  # send data to the client

    conn.close()  # close the connection


if __name__ == '__main__':
    server_program()
