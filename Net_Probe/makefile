# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
LDFLAGS = -lpthread -lssl -lcrypto

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

# Source files
COMMON_SRCS = $(SRCDIR)/thread_pool.cpp $(SRCDIR)/net_probe.cpp $(SRCDIR)/http_server.cpp $(SRCDIR)/http_client.cpp
SERVER_SRCS = $(SRCDIR)/net_probe_server.cpp
CLIENT_SRCS = $(SRCDIR)/net_probe_client.cpp

# Object files
COMMON_OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(COMMON_SRCS))
SERVER_OBJS = $(OBJDIR)/net_probe_server.o
CLIENT_OBJS = $(OBJDIR)/net_probe_client.o

# Targets
SERVER = $(BINDIR)/netprobe_server
CLIENT = $(BINDIR)/netprobe_client

# Detect OS and set OpenSSL paths accordingly
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS - typically installed via Homebrew
    OPENSSL_INCLUDE = -I/usr/local/opt/openssl/include -I/opt/homebrew/opt/openssl/include
    OPENSSL_LIBS = -L/usr/local/opt/openssl/lib -L/opt/homebrew/opt/openssl/lib
else ifeq ($(UNAME_S),Linux)
    # Most Linux distributions
    OPENSSL_INCLUDE = -I/usr/include/openssl
    OPENSSL_LIBS = -L/usr/lib
endif

# Update compiler flags
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 $(OPENSSL_INCLUDE)
LDFLAGS = -lpthread -lssl -lcrypto $(OPENSSL_LIBS)

# Default target
all: dirs $(SERVER) $(CLIENT)

# Create necessary directories
dirs:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Build server
$(SERVER): $(COMMON_OBJS) $(SERVER_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Build client
$(CLIENT): $(COMMON_OBJS) $(CLIENT_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c -o $@ $<

# Individual targets
server: dirs $(SERVER)
client: dirs $(CLIENT)

# Clean build files
clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all dirs server client clean