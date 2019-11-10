#include <map>
#include <regex>
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
#include "dfs-client-p2.h"
#include "dfslibx-clientnode-p2.h"
#include "../dfslib-shared-p2.h"
#include "../dfslib-clientnode-p2.h"

DFSClient::DFSClient() {}

DFSClient::~DFSClient() noexcept { this->Unmount(); }

void DFSClient::ProcessCommand(const std::string &command, const std::string &filename) {

    if (command == "mount") {

        Mount(this->mount_path);

    }
    if (command == "fetch") {

        client_node.Fetch(filename);

    } else if (command == "store") {

        client_node.Store(filename);

    } else if (command == "delete") {

        client_node.Delete(filename);

    } else if (command == "list") {

        std::map<std::string,int> file_map;
        client_node.List(&file_map, true);

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

void DFSClient::Mount(const std::string &filepath) {

    this->mount_path = filepath;
    if (this->mount_path.back() != '/') {
        this->mount_path.append("/");
    }

    dfs_log(LL_SYSINFO) << "Mounting on " << this->mount_path;

    std::vector <std::thread> threads;
    //    uint event_flags = IN_CLOSE_WRITE | IN_OPEN;
    uint event_flags = IN_CREATE | IN_MODIFY | IN_DELETE;

    const FileDescriptor fd = inotify_init();

    if (fd < 0) {
        std::cerr << "Error during inotify_init: " << strerror(errno) << std::endl;
        exit(-1);
    }

    const WatchDescriptor wd = inotify_add_watch(fd, filepath.c_str(), event_flags | IN_ONLYDIR);

    std::thread thread_watcher(DFSClient::InotifyWatcher, DFSClient::InotifyEventCallback, event_flags, fd, &this->client_node);
    NotifyStruct n_event = {fd, wd, event_flags, &thread_watcher, DFSClient::InotifyEventCallback};
    events.emplace_back(n_event);
    threads.push_back(std::move(thread_watcher));

    thread_async = std::thread(&DFSClientNodeP2::HandleCallbackList, &this->client_node);
    threads.push_back(std::move(thread_async));

    // Initialize the callback list
    this->client_node.InitCallbackList();

    for (std::thread &t : threads) {
        if (t.joinable()) { t.join(); }
    }
}

void DFSClient::Unmount() {
    std::vector <FileDescriptor> descriptors;

    this->client_node.Unmount();
    for (NotifyStruct &e: events) {
        if (e.thread->joinable()) { e.thread->detach(); }
        e.thread->~thread();
        inotify_rm_watch(e.wd, e.fd);
        descriptors.push_back(e.fd);
    }

    std::unique(descriptors.begin(), descriptors.end());

    for (FileDescriptor fd: descriptors) {
        if (close(fd) != 0) {
            std::cerr << "Unable to close file descriptor" << std::endl;
        }
    }

    events.clear();

    if (thread_async.joinable()) {
        thread_async.detach();
        thread_async.~thread();
    }
}

void DFSClient::InotifyWatcher(InotifyCallback callback,
                                   uint event_type,
                                   FileDescriptor fd,
                                   DFSClientNode *node) {
    int len;
    std::allocator<char> allocator;
    std::unique_ptr<char> handle(allocator.allocate(DFS_I_BUFFER_SIZE));
    char *events_buffer = handle.get();

    while (true) {

        // Read the next inotify event as it becomes available
        len = read(fd, events_buffer, DFS_I_BUFFER_SIZE);

        int index = 0;

        node->InotifyWatcherCallback([&]{
            // This loop handles each of the inotify events as they come through
            while (index < len) {

                inotify_event *event = reinterpret_cast<inotify_event *>(&(events_buffer[index]));

                EventStruct event_data;
                event_data.event = event;
                event_data.instance = node;

                if ((event_type & event->mask) && event->name[0] != '.') {
                    callback(event_type, std::string{node->MountPath() + event->name}, &event_data);
                }

                size_t used = DFS_I_EVENT_SIZE + event->len;
                index += (used / sizeof(char));
            }

            if (errno == EINTR) {
                dfs_log(LL_ERROR) << "inotify system call invalid";
            }
        });

    }

}

void DFSClient::InotifyEventCallback(uint event_type, const std::string &filename, void *data) {

    // For the purposes of this assignment we will ignore files that do not
    // match the following set of extensions (e.g. temporary files)
    try {
        if (!std::regex_match(filename, std::regex(".*\\.(jpg|png|gif|txt|xlsx|docx|md|psd)$"))) {
            dfs_log(LL_ERROR) << "Ignored file type used for " << filename;
            return;
        }
    }
    catch (std::regex_error& ex) {
        dfs_log(LL_ERROR) << ex.what();
        return;
    }

    // Get the basename for the file
    std::string basename = filename.substr(filename.find_last_of("/") + 1);

    auto event_data = reinterpret_cast<EventStruct *>(data);
    inotify_event *event = reinterpret_cast<inotify_event *>(event_data->event);
    DFSClientNode *node = reinterpret_cast<DFSClientNode *>(event_data->instance);

    // Handle a new file that was created by storing
    // this file on the server
    if (event->mask & IN_CREATE) {
        dfs_log(LL_DEBUG2) << "inotify IN_CREATE event occurred";
        node->Store(basename);
    }

    // Handle a new file that was modified by storing
    // this file on the server
    if (event->mask & IN_MODIFY) {
        dfs_log(LL_DEBUG2) << "inotify IN_MODIFY event occurred";
        node->Store(basename);
    }

    // Handle a deleted file
    if (event->mask & IN_DELETE) {
        dfs_log(LL_DEBUG2) << "inotify IN_DELETE event occurred";
        node->Delete(basename);
    }

}

#ifdef DFS_MAIN

DFSClient client;
void HandleSignal(int signum) {
    client.Unmount();
    exit(0);
}

void Usage() {
    std::cout <<
        "\nUSAGE: dfs-client [OPTIONS] COMMAND [FILENAME]\n"
        "-a, --address <address>:  The server address to connect to (default: 0.0.0.0:42001)\n"
        "-d, --debug_level <level>:  The debug level to use: 0, 1, 2, 3 (default: 0 = no debug, higher numbers increase verbosity)\n"
        "-m, --mount_path <path>:  The mount path this client attaches to\n"
        "-t, --deadline_timeout <int>:  The deadline timeout in milliseconds (default: 10000)\n"
        "-h, --help:               Show help\n"
        "\n"
        "COMMAND is one of mount|fetch|store|delete|list|stat.\n"
        "FILENAME is the filename to fetch, store, delete, or stat. The mount and list commands do not require a filename.\n\n";
    exit(1);
}

int main(int argc, char** argv) {

    const char* const short_opts = "a:d:m:r:t:h";

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
    std::string mount_path = "";
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

    for(int i = optind; i < argc; i++) {
        if (command.empty()) { command = argv[i]; }
        else if (filename.empty()) { filename = argv[i]; }
    }

    if (command.empty()) {
        std::cerr << "\nMissing command!\n";
        Usage();
        return -1;
    }

    std::string commands("fetch store delete list stat mount sync");
    if (commands.find(command) == std::string::npos ) {
        std::cerr << "\nUnknown command!\n";
        Usage();
        return -1;
    }

    std::string nonpath_commands("list mount sync");
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
