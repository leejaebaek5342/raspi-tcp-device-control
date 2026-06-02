CC = gcc
CFLAG = -fPIC -c
SHARE = -shared -lwiringPi

LIBLED = libled.so
LIBBUZZER = libbuzzer.so
LIBCDS = libcds.so
LIBSEG = libseg.so
LIBTEMP = libtemp.so


all: $(LIBLED) $(LIBBUZZER) $(LIBCDS) $(LIBSEG) $(LIBTEMP) test_device

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

test_device: test_device.c 
	$(CC) $< -o $@ -lwiringPi -ldl
	scp test_device jambaek@100.113.15.114:~/fortest

clean:
	rm -f *.o *.so test_device