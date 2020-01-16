CFLAGS = -O2 -Wall -std=c++14
CC = g++

rainfall: rainfall_seq.cpp rainfall_pt.cpp
	$(CC) $(CFLAGS) -o rainfall_seq rainfall_seq.cpp
	$(CC) $(CFLAGS) -o rainfall_pt rainfall_pt.cpp -lpthread
clean:
	rm rainfall_seq 

