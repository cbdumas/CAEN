#!/usr/bin/python

import sys, getopt
import matplotlib.pyplot as plt
import numpy
import scipy

fhandle = open("events.dat","r")
eventData = fhandle.readlines()
fhandle.close()

#eventData = [int(val.rstrip()) for val in eventData if "#" not in val]
xcoords = [float(val.split(":")[1].split()[0]) for val in eventData if 'X Y:' in val]
ycoords = [float(val.split(":")[1].split()[1]) for val in eventData if 'X Y:' in val]
sums = [int(val.split(":")[1]) for val in eventData if 'Sum:' in val]
sums = [val for val in sums if val<4000]
print(len(xcoords))
#H,xedge,yedge= numpy.histogram2d(ycoords,xcoords,bins=256)

#extent = [yedge[0], yedge[-1], xedge[-1], xedge[0]]


plt.figure(1)
plt.plot(xcoords,ycoords,'r+')
plt.figure(2)
n, bins, pathces = plt.hist(sums,bins=(max(sums)-min(sums))/10)
plt.grid(True)
#plt.xticks(numpy.arange(200,400,20))
#plt.figure(3)
#plt.plot(sums,'r+')
#plt.figure(4)
#plt.imshow(H,interpolation='nearest',extent=extent)
#plt.colorbar()
plt.show()

