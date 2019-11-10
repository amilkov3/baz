#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <errno.h>
#include <csignal>
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <algorithm>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>

#include "dfs-utils.h"
#include "dfs-client-p1.h"
#include "../dfslib-shared-p1.h"
#include "../dfslib-clientnode-p1.h"

using grpc::Status;
using grpc::StatusCode;

DFSClient::DFSClient() {}

DFSClient::~DFSClient() noexcept {}


void DFSClient::ProcessCommand(const std::string &command, const std::string &filename) {

    if (command == "fetch") {

        client_node.Fetch(filename);

    } else if (command == "store") {

        client_node.Store(filename);

    } else if (command == "list") {

        std::map<std::string,int> file_map;
        client_node.List(&file_map, true);

    } else if (command == "delete") {

        client_node.Delete(filename);

    } else if (command == "stat") {

        client_node.Stat(filename);

    } else {

        dfs_log(LL_ERROR) << "Unknown command";

    }

}

void DFSClient::InitializeClientNode(const std::string &server_address) {
    this->client_node.CreateStub(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
}

void DFSClient::SetMountPath(const std::string &path) {
    this->mount_path = dfs_clean_path(path);
    this->client_node.SetMountPath(this->mount_path);
}

void DFSClient::SetDeadlineTimeout(int deadline) {
    this->deadline_timeout = deadline;
    this->client_node.SetDeadlineTimeout(deadline);
}

#ifdef DFS_MAIN

DFSClient client;
void HandleSignal(int signum) {
    exit(0);
}

void Usage() {
    std::cout <<
        "\nUSAGE: dfs-client [OPTIONS] COMMAND [FILENAME]\n"
        "-a, --address <address>:  The rpc server address to connect to (default: 0.0.0.0:42001)\n"
        "-d, --debug_level <level>:  The debug level to use: 0, 1, 2, 3 (default: 0 = no debug, higher numbers increase verbosity)\n"
        "-m, --mount_path <path>:  The mount path this client attaches to\n"
        "-t, --deadline_timeout <int>:  The deadline timeout in milliseconds (default: 10000)\n"
        "-h, --help:               Show help\n"
        "\n"
        "COMMAND is one of fetch|store|delete|list|stat.\n"
        "FILENAME is the filename to fetch, store, delete, or stat. The list command does not require a filename.\n\n";
    exit(1);
}

int main(int argc, char** argv) {

    const char* const short_opts = "a:d:m:t:h";

    const option long_opts[] = {
        {"address", optional_argument, nullptr, 'a'},
        {"debug_level", optional_argument, nullptr, 'd'},
        {"mount_path", optional_argument, nullptr, 'm'},
        {"deadline_timeout", optional_argument, nullptr, 't'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, no_argument, nullptr, 0}
    };

    char option_char;
    int deadline_timeout = 10000;
    int debug_level = static_cast<int>(LL_ERROR);
    std::string command = "";
    std::string filename = "";
    std::string mount_path = "mnt/client";
    std::string server_address = "0.0.0.0:42001";

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd error:");
        return 1;
    }
    std::string working_directory(cwd);

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
            case 't':
                deadline_timeout = std::stoi(optarg);
                break;
            case 'h':
                Usage();
                break;
            case '?':
                if (optopt == 'n' || optopt == 'a') {
                    std::cerr << "Option '" << optopt << "' requires an argument." << std::endl;
                }
                else if (isprint(optopt)) {
                    std::cerr << "Unknown option:" << optopt << std::endl;
                }
                else {
                    std::cerr << "Unknown option characteri:" << optopt << std::endl;
                }
                return 1;
            default:
                abort();
        }
    }

    if (debug_level > 0 && debug_level <= 3) {
        DFS_LOG_LEVEL = static_cast<dfs_log_level_e>(debug_level + 1);
    }

    if (mount_path.empty()) {
        mount_path = working_directory + "/mnt/client";
    }

    struct stat st;
    // check for directory existence
    if (!(stat(mount_path.c_str(), &st) == 0)) {
        std::cerr << "Mount path not found at: " << mount_path << std::endl;
        return 1;
    }

    // Get command and filename
    for(int i = optind; i < argc; i++) {
        if (command.empty()) { command = argv[i]; }
        else if (filename.empty()) { filename = argv[i]; }
    }

    if (command.empty()) {
        std::cerr << "\nMissing command!\n";
        Usage();
        return -1;
    }

    std::string commands("fetch store delete list stat");
    if (commands.find(command) == std::string::npos ) {
        std::cerr << "\nUnknown command!\n";
        Usage();
        return -1;
    }

    std::string nonpath_commands("list");
    if (filename.empty() && nonpath_commands.find(command) == std::string::npos ) {
        std::cerr << "\nMissing filename!\n";
        Usage();
        return -1;
    }

    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    client.SetMountPath(mount_path);
    client.SetDeadlineTimeout(deadline_timeout);
    client.InitializeClientNode(server_address);
    client.ProcessCommand(command, filename);

    return 0;
}

#endif
