# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11 -Wall -Wextra

# Client and server executable names
CLIENT = tftp-client
SERVER = tftp-server

# Source directories
CLIENT_SRC_DIR = client_src
SERVER_SRC_DIR = server_src

# Include directories
INCLUDE_DIR = include

# Object files directory
OBJ_DIR = obj

# Output directory for executables
BIN_DIR = bin

# Source files
CLIENT_SRCS = $(wildcard $(CLIENT_SRC_DIR)/*.cpp)
SERVER_SRCS = $(wildcard $(SERVER_SRC_DIR)/*.cpp)

# Object files
CLIENT_OBJS = $(patsubst $(CLIENT_SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CLIENT_SRCS))
SERVER_OBJS = $(patsubst $(SERVER_SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SERVER_SRCS))

# Targets
all: $(CLIENT) $(SERVER)

$(CLIENT): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/$(CLIENT) $(CLIENT_OBJS)

$(SERVER): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/$(SERVER) $(SERVER_OBJS)

$(OBJ_DIR)/%.o: $(CLIENT_SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(OBJ_DIR)/%.o: $(SERVER_SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

clean:
	rm -f $(CLIENT_OBJS) $(SERVER_OBJS)
	rm -f $(BIN_DIR)/$(CLIENT) $(BIN_DIR)/$(SERVER)

.PHONY: all clean
