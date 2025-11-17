# to make, `mingw32-make -f  Makefile`

# Define the compiler (use g++ from MSYS2 MinGW 64-bit)
CXX = g++

# Define the C++ standard and basic flags
# -std=c++17: Use C++17 standard
# -Wall -Wextra: Enable common warning messages
# -O2: Enable optimizations for release build
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Define include paths using -I flags
# Point to the directory containing zlib.h, zconf.h
# Point to the directory containing the openssl/ directory
CXXFLAGS += -Isrc/External
CXXFLAGS += -Isrc/External/openssl

# Define the name of the final executable
TARGET = silt_cpp

# Define the source files to compile
# List all .cpp files here
SOURCES = src/Main/main.cpp \
          src/Main/CLI.cpp \
          src/Main/Repository.cpp \
          src/Main/Objects.cpp \
          src/Main/Index.cpp \
          src/Main/Refs.cpp \
          src/Main/Commands.cpp \
          src/Main/Utils.cpp

# Define libraries to link using -l flags
# -lz: Link against the zlib library (provides zlib functions)
# -lcrypto: Link against the OpenSSL crypto library (provides SHA1 functions)
# The linker needs to find these libraries, typically in the MSYS2 lib directory.
# The -L flag tells the linker where to look (using the MSYS2 standard path).
LIBS = -lz -lcrypto
LDFLAGS = -L/mingw64/lib

# Default target
all: $(TARGET)

# Rule to build the TARGET executable
# $(CXX): The compiler (g++)
# $(CXXFLAGS): The compiler flags (standard, warnings, includes)
# -o $(TARGET): Specify the output executable name
# $(SOURCES): The list of source files to compile and link
# $(LDFLAGS): Linker flags (library search path)
# $(LIBS): The libraries to link
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS) $(LIBS)

# Optional: Rule to clean up built files
clean:
	rm -f $(TARGET)  # Removes the executable

# Optional: Rule to mark 'clean' as phony (not a file)
.PHONY: all clean