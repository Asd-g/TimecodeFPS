CFLAGS = -Wall -O2

all: timecodefps64.dll

timecodefps.o: timecodefps.c
	gcc -c timecodefps.c $(CFLAGS) -o timecodefps.o

timecodefps64.dll: timecodefps.o lib/x64/AviSynth.lib
	gcc -shared timecodefps.o lib/x64/AviSynth.lib $(CFLAGS) -o timecodefps64.dll

clean:
	rm -f timecodefps.o timecodefps64.dll

