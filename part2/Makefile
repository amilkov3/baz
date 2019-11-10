CXX = g++ -Wall -g3 -fPIC
CPPFLAGS += `pkg-config --cflags protobuf grpc`
CXXFLAGS += -std=c++14
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer
ASAN_LIBS = -static-libasan
LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++ grpc`\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
           -ldl
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PROTOS_DIR = ./
PROTOS_SRC = ./proto-src
SRC_DIR = ./src
LIB_DIR = ./
BIN_DIR = ../bin
OBJ_DIR = ../tmp
SRC_FILES = $(wildcard $(SRC_DIR)/dfs-*.cpp)
SRC_LIB_FILES = $(wildcard $(LIB_DIR)dfslib-*.cpp)
SRC_LIBX_FILES = $(wildcard $(SRC_DIR)/dfslibx-*.cpp)
SRC_PROTO_FILES = $(wildcard $(PROTOS_SRC)/*.pb.cc)
OBJ_LIB_FILES = $(patsubst $(LIB_DIR)%.o, $(OBJ_DIR)/%.o, $(patsubst %.cpp, %.o, $(SRC_LIB_FILES)))
OBJ_LIBX_FILES = $(patsubst $(SRC_DIR)/%.o, $(OBJ_DIR)/%.o, $(patsubst %.cpp, %.o, $(SRC_LIBX_FILES)))
OBJ_PROTO_FILES = $(patsubst $(PROTOS_SRC)/%-p2.o, $(OBJ_DIR)/%-p2.o, $(patsubst %.pb.cc, %.pb-p2.o, $(SRC_PROTO_FILES)))
OBJ_SERVERNODE_FILES = $(filter $(OBJ_DIR)/dfs-service%.o, $(OBJ_PROTO_FILES))

#$(info $$OBJ_PROTO_FILES is [${OBJ_PROTO_FILES}])

vpath %.proto $(PROTOS_DIR)
vpath %.cpp $(SRC_DIR)
vpath %.cc $(PROTOS_SRC)
vpath %.o $(OBJ_DIR)

all: system-check \
	$(BIN_DIR)/dfs-client-p2 \
	$(BIN_DIR)/dfs-server-p2

protos: $(PROTOS_SRC)/dfs-service.grpc.pb.cc \
	$(PROTOS_SRC)/dfs-service.pb.cc

$(OBJ_DIR)/dfslib-%.o: $(LIB_DIR)dfslib-%.cpp
	$(CXX) $^ -c $(CPPFLAGS) -o $@

$(OBJ_DIR)/dfslibx-%.o: $(SRC_DIR)/dfslibx-%.cpp
	$(CXX) $^ -c $(CPPFLAGS) -o $@

$(OBJ_DIR)/%.pb-p2.o: $(PROTOS_SRC)/%.pb.cc
	$(CXX) $^ -c $(CPPFLAGS) -o $@

$(BIN_DIR)/dfs-client-p2: $(OBJ_SERVERNODE_FILES) $(OBJ_LIBX_FILES) $(OBJ_LIB_FILES) $(SRC_DIR)/dfs-client-p2.cpp
	$(CXX) $^ $(CPPFLAGS) $(ASAN_FLAGS) -DDFS_MAIN $(LDFLAGS) $(ASAN_LIBS) -o $@

$(BIN_DIR)/dfs-server-p2: $(OBJ_PROTO_FILES) $(OBJ_LIBX_FILES) $(OBJ_LIB_FILES) $(SRC_DIR)/dfs-server-p2.cpp
	$(CXX) $^ $(CPPFLAGS) $(ASAN_FLAGS) -DDFS_MAIN $(LDFLAGS) $(ASAN_LIBS) -o $@

.PRECIOUS: %.grpc.pb.cc
$(PROTOS_SRC)/%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_DIR) --grpc_out=$(PROTOS_SRC) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

.PRECIOUS: %.pb.cc
$(PROTOS_SRC)/%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_DIR) --cpp_out=$(PROTOS_SRC) $<

.PHONY: clean clean_protos clean_all

clean:
	rm -r -f $(BIN_DIR)/*-p2
	rm -r -f $(OBJ_DIR)/*-p2.o

clean_protos:
	rm -f $(PROTOS_SRC)/*.pb.cc $(PROTOS_SRC)/*.pb.h
	rm -r -f $(OBJ_DIR)/*.pb.o

clean_all: clean clean_protos

# The following is to test your system and ensure GRPC and Protobuffers are avialable.

PROTOBUF_CHECK_CMD = ls -l ./proto-src | grep ".*\.pb\.cc"
HAS_PROTOBUF_FILES = $(shell $(PROTOBUF_CHECK_CMD) > /dev/null && echo true || echo false)

PROTOC_CMD = which $(PROTOC)
PROTOC_CHECK_CMD = $(PROTOC) --version | grep -q libprotoc.3
PLUGIN_CHECK_CMD = which $(GRPC_CPP_PLUGIN)
HAS_PROTOC = $(shell $(PROTOC_CMD) > /dev/null && echo true || echo false)
ifeq ($(HAS_PROTOC),true)
HAS_VALID_PROTOC = $(shell $(PROTOC_CHECK_CMD) 2> /dev/null && echo true || echo false)
endif
HAS_PLUGIN = $(shell $(PLUGIN_CHECK_CMD) > /dev/null && echo true || echo false)

SYSTEM_OK = false
ifeq ($(HAS_VALID_PROTOC),true)
ifeq ($(HAS_PLUGIN),true)
ifeq ($(HAS_PROTOBUF_FILES),true)
SYSTEM_OK = true
endif
endif
endif

system-check:
ifneq ($(HAS_VALID_PROTOC),true)
	@echo
	@echo "DEPENDENCY ERROR"
	@echo
	@echo "You don't have protoc 3.0.0 installed in your path."
	@echo "Please install Google protocol buffers 3.0.0 and its compiler."
	@echo "You can find it here:"
	@echo
	@echo "   https://github.com/google/protobuf/releases/tag/v3.0.0"
	@echo
	@echo "Here is what I get when trying to evaluate your version of protoc:"
	@echo
	-$(PROTOC) --version
	@echo
	@echo
endif
ifneq ($(HAS_PLUGIN),true)
	@echo
	@echo "DEPENDENCY ERROR"
	@echo
	@echo "You don't have the grpc c++ protobuf plugin installed in your path."
	@echo "Please install grpc. You can find it here:"
	@echo
	@echo "   https://github.com/grpc/grpc"
	@echo
	@echo "Here is what I get when trying to detect if you have the plugin:"
	@echo
	-which $(GRPC_CPP_PLUGIN)
	@echo
	@echo
endif
ifneq ($(HAS_PROTOBUF_FILES),true)
	@echo
	@echo "YOUR PROTOBUF FILES HAVE NOT BEEN GENERATED"
	@echo
	@echo "You don't have any protobuf files generated in ./proto-src/."
	@echo
	@echo "Make sure you run the following once you have added your service "
	@echo "details in the ./dfs-service.proto file."
	@echo
	@echo "    make protos"
	@echo
	@echo
endif
ifneq ($(SYSTEM_OK),true)
	@false
endif
