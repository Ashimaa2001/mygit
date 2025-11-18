CXX = g++
CXXFLAGS = -std=c++17 -O2
LDFLAGS = -lcrypto -lz

all: mygit

mygit: src/mygit.cpp src/commands.cpp src/git_utils.cpp
	$(CXX) $(CXXFLAGS) -o mygit src/mygit.cpp src/commands.cpp src/git_utils.cpp $(LDFLAGS)

clean:
	rm -f mygit
