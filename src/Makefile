###############################################################################
# Makefile
###############################################################################


TARGET = sorter1
CC = g++

DEFINES =

COMMON = -std=gnu++11 -O3

LDFLAGS = $(COMMON)
LDFLAGS += -lpthread

CFLAGS = $(COMMON)
CFLAGS += -MD -MP
CFLAGS += $(DEFINES)


## Objects that must be built in order to link
OBJECTS = data_load.o thr.o sorter.o



## Build
all: $(TARGET)

## Compile

%.o:	%.c Makefile
	$(CC) $(INCLUDES) $(CFLAGS) -c $<


%.o:	%.cpp Makefile
	$(CC) $(INCLUDES) $(CFLAGS) -c $<




##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) -o $(TARGET)


.PHONY: clean include

clean:
	-rm -rf $(OBJECTS) $(OBJECTS:.o=.d) $(TARGET)


-include $(OBJECTS:.o=.d)
