CXX      = g++
CXXFLAGS = -Wall -pthread -std=c++11

TARGET   = cse4001_sync
OBJS     = main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp semaphore_class.h
	$(CXX) $(CXXFLAGS) -c main.cpp

clean:
	rm -f $(TARGET) $(OBJS)
