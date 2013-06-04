#!/bin/sh

gcc -c -fpic runCAEN.c
gcc -shared -o librunCAEN.so.1.0 runCAEN.o -L. -lCAENDigitizer
sudo rm /usr/lib/librunCAEN*
sudo mv librunCAEN.so.1.0 /usr/lib
sudo ln -s /usr/lib/librunCAEN.so.1.0 /usr/lib/librunCAEN.so
