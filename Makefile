CPP = g++
#static library use 'ar' command   
AR = ar

CPPFLAGS = -I. -lwiringPi -lpthread -fpermissive -Werror=implicit-function-declaration

TARGET = logger forward

all: $(TARGET) 

%.o: %.cpp
	$(CPP) -c -o $@ $< $(CPPFLAGS)

logger: logger.o ads129x.o 
	$(CPP) -o $@ $^ $(CPPFLAGS)

forward: forward.o ads129x.o 
	$(CPP) -o $@ $^ $(CPPFLAGS)

.PHONY: clean

clean:
	rm -f $(TARGET) *.o
