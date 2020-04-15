CFLAGS = -Wall -O2

all: timecodefps.dll

timecodefps.o: timecodefps.c
	gcc -c timecodefps.c $(CFLAGS) -o timecodefps.o

timecodefps.dll: timecodefps.o lib/x86/AviSynth.lib
	gcc -shared timecodefps.o lib/x86/AviSynth.lib $(CFLAGS) -o timecodefps.dll

clean:
	rm -f timecodefps.o timecodefps.dll

