# tcp_rpc
 A simple tcp client/server app using msgpack. Client is in C and server is in Python.

 Client can accept incomplete messages and waits till more data comes, so you can send big structured messages. This is possible by a streaming feature of msgpack.

Install msgpack
```bash
pip install msgpack

git clone https://github.com/msgpack/msgpack-c.git
cd msgpack-c
git checkout c_master
cmake .
make
sudo make install
```

Run server
```bash
python server.py
```

Run client
```bash
mkdir build
cd build
cmake ..
make
./client
```


```
$ python server.py 
Connection from: ('127.0.0.1', 41192)
from connected user: [42, b'Hello', b'World!']

$ ./client 
Socket successfully created..
connected to the server..
"Hi from server!"
```
