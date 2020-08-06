
#https://wiki.python.org/moin/TcpCommunication

import socket
 
 
TCP_IP = "10.0.0.18"
TCP_PORT = 5005
BUFFER_SIZE = 20
MESSAGE = "Hello, World!"

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((TCP_IP, TCP_PORT))
s.send(MESSAGE)
data = s.recv(BUFFER_SIZE)
s.close()

print ("received data:", data)
