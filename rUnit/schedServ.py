#!/usr/bin/python

from socket import *
from sys import *
from glob import *
from os import *
from string import Template
import time

# Get task list
if len(argv) < 3:
    print 'Len path and number of frames should be specified..'
    exit()

# change to argv[1]
chdir(argv[1] + str(1))

##############################
# get all files named *.rib
##############################
fileFilter = '*.rib'
# print fileFilter, getcwd()
files = glob(fileFilter);
fileNames = ''
index = int(argv[2]) # control how many frames will be rendered
for fName in files:
    if(index > 0):
        fileNames += fName + '#'
        index -= 1
    else:
        break
fileNames = fileNames[:-1]
# exit()

# revert working dir
CWD = getcwd()
chdir(CWD)

##############################
# generate job message
##############################
jobid = 'building-01'
cameraPath = argv[1]
frameNum = int(argv[2])
neededMem = 10240
prerenderTag = 0
resWidth = 1024
resHeight = 768
sampleRate = 1



LOCAL_ADDR = gethostbyname(gethostname())
SCHED_SERVER_PORT = 5168
RENDERING_UNIT_PORT = 5169
BUFSIZ = 1024

# set socket TIME_WAIT

##############################
# recv init message
##############################
print 'Local IP is ', LOCAL_ADDR

ADDR = (LOCAL_ADDR, SCHED_SERVER_PORT)
sockfd = socket(AF_INET, SOCK_STREAM)
sockfd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
sockfd.bind(ADDR);
sockfd.listen(5)

count = 1
while True:
    clifd, addr = sockfd.accept()
    print 'rendering unit address is ', addr
    # send header message
    clifd.send("#helloClient|end")
    # recv init message
    print 'recved init message ', clifd.recv(BUFSIZ)
    # send tail message
    clifd.send("#serverProcessing|end")
    clifd.close()

    #IPData = IPData[:-4]# trim |end
    #RENDERING_UNIT_IP = [x for x in IPData.split('#')][2];
    #print 'rendering unit IP is', RENDERING_UNIT_IP

    ##############################
    # send task to rendering unit
    ##############################
    # send to render unit
    # wait for 3 second until rendering unit is ready to be connected
    time.sleep(3)

    ADDR = (addr[0], RENDERING_UNIT_PORT)
    clifd = socket(AF_INET, SOCK_STREAM, 0)
    clifd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    clifd.connect(ADDR)

    # recv header message
    print 'header message ', clifd.recv(BUFSIZ)

    # send job message
    cameraPath = argv[1] + str(count)
    jobMsg = '#taskdiscription#' + jobid +\
             '#' + cameraPath +\
             '#' + str(frameNum) +\
             '#' + fileNames +\
             '#' + str(neededMem) +\
             '#' + str(prerenderTag) +\
             '#' + str(resWidth) +\
             '#' + str(resHeight) +\
             '#' + str(sampleRate) + '|end'

    clifd.send(jobMsg)
    print 'Sended : ', jobMsg

    # recv tail message
    print 'tail message ', clifd.recv(BUFSIZ)
    count++
    clifd.close()

sockfd.close()

# sleep for a while to wait server recv message
#sleep(20)
# print 'Waiting progress....'
# ##############################
# # recv progress from r unit
# ##############################
# # wait for progress from rendering unit
# ADDR = (LOCAL_ADDR, SCHED_SERVER_PORT)
# sockfd = socket(AF_INET, SOCK_STREAM)
# sockfd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
# sockfd.bind(ADDR);
# sockfd.listen(5)
# clifd, addr = sockfd.accept()
# 
# # send header message
# clifd.send("#helloClient|end")
# print 'recv conn from:', addr
# 
# # recv init message
# progressData = clifd.recv(BUFSIZ)
# print 'Recved :', progressData
# 
# # send tail message
# clifd.send("#serverProcessing|end")
# 
# clifd.close()
# sockfd.close()


