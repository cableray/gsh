
CC = g++

CFLAGS = -O2

IFLAGS = 

LFLAGS = 

LIBS = -lreadline

COMPILE = $(CC) $(CFLAGS) -c



%.out: %.o
	$(CC) -o $@ $(IFLAGS) $(LFLAGS) $(LIBS) $<
	

%.o: %.cpp
	$(COMPILE) -o $@ $(IFLAGS) $<
	