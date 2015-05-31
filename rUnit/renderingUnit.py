#!/usr/bin/python

from socket import *
from sys import *
from glob import *
from os import *
from string import Template
import time

LOCAL_ADDR = gethostbyname(gethostname())
SCHED_SERVER_PORT = 5168
RENDERING_UNIT_PORT = 5169
BUFSIZ = 1024

if len(argv) < 2:
    print 'Schedule server IP should specified!'
    exit()

##############################
# init message
##############################
print 'server IP is ', argv[1]
ADDR = (argv[1], SCHED_SERVER_PORT)
sockfd = socket(AF_INET, SOCK_STREAM, 0)
sockfd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
sockfd.connect(ADDR)

# recv header message, send init, recv tail
print 'Received header message ', sockfd.recv(BUFSIZ)
sockfd.send('#initMessage|end')
print 'Received tail message ', sockfd.recv(BUFSIZ)

sockfd.close()

##############################
# waiting for task
##############################
ADDR = (LOCAL_ADDR, RENDERING_UNIT_PORT)
sockfd = socket(AF_INET, SOCK_STREAM)
sockfd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
sockfd.bind(ADDR)
sockfd.listen(5)
clifd, addr = sockfd.accept()

# send header, recv task, send tail
clifd.send('#helloClient|end')
print 'Received init message ', clifd.recv(BUFSIZ)
clifd.send('#serverProcessing|end')

sockfd.close()
clifd.close()

##############################
# send progress
##############################
# sleep for 3 seconds until sched server is ready to be connected
time.sleep(3)

ADDR = (argv[1], SCHED_SERVER_PORT)
sockfd = socket(AF_INET, SOCK_STREAM, 0)
sockfd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
sockfd.connect(ADDR)

# recv header message, send init, recv tail
print 'Received header message ', sockfd.recv(BUFSIZ)
sockfd.send('#taskstatus#building-01#2#rendering_frameprogress#building_perspShape_defaultRenderLayer.0002.rib#1#0#0##0|progress#finish|end')
print 'Received tail message ', sockfd.recv(BUFSIZ)

sockfd.close()
clifd.close()
