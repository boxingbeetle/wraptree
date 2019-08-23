.PHONY: all clean

CXX := x86_64-w64-mingw32-g++
CXXFLAGS := -Os -s

all: WrapTree.exe

WrapTree.exe: WrapTree.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f WrapTree.exe
