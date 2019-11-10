#include <getopt.h>
#include <string>
#include <iostream>
#include <fstream>
#include <csignal>

#include "dfs-utils.h"
#include "../dfslib-servernode-p1.h"

void HandleSignal(int signum) {
    exit(0);
}

void Usage() {
    std::cout <<
        "\nUSAGE: dfs-server-p1 [OPTIONS]\n"
        "-a, --address <address>:    The server address to connect to (default: 0.0.0.0:42001)\n"
        "-d, --debug_level <level>:  The debug level to use: 0, 1, 2, 3 (default: 0 = no debug, higher numbers increase verbosity)\n"
        "-m, --mount_path <path>:    The mount storage path (default: mnt/server)\n"
        "-h, --help:                 Show help\n\n";
    exit(1);
}

int main(int argc, char** argv) {

    const char* const short_opts = "a:d:m:h";

    const option long_opts[] = {
        {"address", optional_argument, nullptr, 'a'},
        {"debug_level", optional_argument, nullptr, 'd'},
        {"mount_path", optional_argument, nullptr, 'm'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, no_argument, nullptr, 0}
    };

    char option_char;
    int debug_level = static_cast<int>(LL_ERROR);
    std::string mount_path = "mnt/server/";
    std::string server_address = "0.0.0.0:42001";

    while((option_char = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch(option_char) {
            case 'a':
                server_address = std::string(optarg);
                break;
            case 'd':
                debug_level = std::stoi(optarg);
                break;
            case 'm':
                mount_path = std::string(optarg);
                break;
            case 'h':
            case '?':
            default:
                Usage();
                break;
        }
    }

    if (debug_level > 0 && debug_level <= 3) {
        DFS_LOG_LEVEL = static_cast<dfs_log_level_e>(debug_level + 1);
    }

    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    DFSServerNode server_node(server_address, dfs_clean_path(mount_path), [&]{ return; });
    server_node.Start();

    return 0;

}
