# Compiler settings
CXX=g++
CXXFLAGS=-g -Wall -Wextra -std=c++20

# Build settings
TARGET=ipk24chat-client
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)

# Link the target with all objects files
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Compile each source file to an object
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up the build
clean:
	rm -f $(TARGET) $(OBJECTS)

# Phony targets for commands that do not represent files
.PHONY: clean
