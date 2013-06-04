#!/usr/bin/python

import sys
from ctypes import CDLL,pointer,POINTER,c_int,c_char,c_uint16,byref,addressof,cast,Structure
import time
import matplotlib.pyplot as plt
import scipy.signal as sig

caenlib = CDLL("librunCAEN.so")


#This must match the structure declaration in runCAEN.h
class eventNode(Structure):
    pass    
eventNode._fields_ = [("channels", c_int),
                ("samples", c_int),
                ("event",POINTER(POINTER(c_uint16))),
                ("nxt",POINTER(eventNode)),
                ("prv",POINTER(eventNode))]

#Initialize variables and open connection to digitizer
handle = c_int()
buff = POINTER(c_char)()
thresh = c_int(50)
num = c_int()

queueHead = POINTER(eventNode)() 
queueTail = POINTER(eventNode)()

caenlib.setupCAEN(byref(handle),byref(buff),thresh)
caenlib.startCAEN(handle)

caenlib.getFromCAEN.argtypes = [c_int, POINTER(c_char),
                                POINTER(POINTER(eventNode)),
                                POINTER(POINTER(eventNode))]
#The purpose of this section is to discard the bogus events created upon
#starting acquisition
time.sleep(4)
num=caenlib.getFromCAEN(handle,buff,byref(queueHead),byref(queueTail))
while queueHead:
    caenlib.freeEvent(queueHead.contents.prv)
    queueHead = queueHead.contents.nxt

#Now begin acquisition
queueHead = POINTER(eventNode)() 
queueTail = POINTER(eventNode)()


start = time.time()
while time.time()-start<5:
    num=caenlib.getFromCAEN(handle,buff,byref(queueHead),byref(queueTail))

plt.hold(True)
#Do we want to step through every event and look at it?
examine = input("Examine waveforms?Y/n")
inspect = True
if examine == 'n' or examine == 'N': 
   inspect = False

i=0 #index to count events
xpoints = list()
ypoints = list()
while queueHead:
    i+=1
    print("event!",i)
    if inspect:
        for chan in range(4):
            plt.figure(chan)
            plt.clf()
            plt.plot(queueHead.contents.event[chan][0:150])       #plot original and smoothed
            plt.plot(sig.convolve(queueHead.contents.event[chan][0:150],
                                    [.1,.1,.1,.1,.1,.1,.1,.1,.1,.1],mode='same'))
        plt.show(block=False)
        command = input("Continue? Y/n")
        if command == 'n' or command == 'N':
            inspect = False
    peaks=[]
    for chan in range(4):
        peaks.append(max(sig.convolve(queueHead.contents.event[chan][0:150], [.1,.1,.1,.1,.1,.1,.1,.1,.1,.1],mode='same')))
    x = (peaks[0]-peaks[1])/(peaks[0]+peaks[1])
    y = (peaks[2]-peaks[3])/(peaks[2]+peaks[3])
    energy = sum(peaks)
    xpoints.append(x)
    ypoints.append(y)
    queueHead = queueHead.contents.nxt

while queueTail:
    caenlib.freeEvent(queueTail.contents.nxt)
    queueTail = queueTail.contents.prv

plt.scatter(xpoints,ypoints)
plt.show()

caenlib.stopCAEN(handle)

caenlib.closeCAEN(handle,byref(buff))
