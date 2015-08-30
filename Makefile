# Roborally Makefile


# Build tool
CC = g++

# Include directories
INC_DIRS = ../../.. ..

# Include flags for .cpp files
INC = $(INC_DIRS:%=-I%)

# Compile flags
CXXFLAGS := -Wall -g $(INC)

# Include directories for .h files
VPATH = $(INC_DIRS)

# Objects to build
WRAP_OBJS = Arduino.o SPI.o
RC_OBJS = MFRC522.o

# Combine objects
OBJS = main.o $(WRAP_OBJS) $(RC_OBJS)

# Compile and link main
roborally : $(OBJS)
	$(CC) -lpthread -o  roborally $(OBJS)

# Target dependencies
main.o : Arduino.h SPI.h MFRC522.h
Arduino.o : Arduino.h
SPI.o : SPI.h
MFRC522.o : MFRC522.h SPI.h Arduino.h

# Cleanup
.PHONY : clean
clean :
	rm -f main *.o