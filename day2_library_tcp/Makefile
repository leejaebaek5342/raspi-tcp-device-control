CC = gcc
CFLAG = -fPIC -c
SHARE = -shared -lwiringPi
OPT = -lwiringPi -ldl -lpthread

LIBLED = libled.so
LIBBUZZER = libbuzzer.so
LIBCDS = libcds.so
LIBSEG = libseg.so
LIBTEMP = libtemp.so


all: $(LIBLED) $(LIBBUZZER) $(LIBCDS) $(LIBSEG) $(LIBTEMP)	server client

$(LIBLED): led.o
	$(CC) -o $@ $< $(SHARE)
	scp $(LIBLED) jambaek@100.113.15.114:~/fortest

$(LIBBUZZER): buzzer.o
	$(CC) -o $@ $< $(SHARE)
	scp $(LIBBUZZER) jambaek@100.113.15.114:~/fortest
	
$(LIBCDS): cds.o
	$(CC) -o $@ $< $(SHARE)
	scp $(LIBCDS) jambaek@100.113.15.114:~/fortest

$(LIBSEG): segment.o
	$(CC) -o $@ $< $(SHARE)
	scp $(LIBSEG) jambaek@100.113.15.114:~/fortest

$(LIBTEMP): temperature.o
	$(CC) -o $@ $< $(SHARE)
	scp $(LIBTEMP) jambaek@100.113.15.114:~/fortest

%.o: %.c
	$(CC) $(CFLAG) $< -o $@

server: server.c
	$(CC) server.c -o $@ $(OPT)
	scp $@ jambaek@100.113.15.114:~/fortest

client: client.c
	$(CC) client.c -o $@

clean:
	rm -f *.o *.so server client