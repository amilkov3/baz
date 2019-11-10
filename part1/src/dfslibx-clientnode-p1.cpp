#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>
#include <utime.h>

#include "dfs-utils.h"
#include "dfslibx-clientnode-p1.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;

extern dfs_log_level_e DFS_LOG_LEVEL;

DFSClientNode::DFSClientNode() : mount_path("mnt/client/") {
    char host[HOST_NAME_MAX];
    std::ostringstream ss_id;
    gethostname(host, HOST_NAME_MAX);
    auto t_id = std::this_thread::get_id();
    ss_id << "T" << t_id;
    client_id = std::string(host + ss_id.str());
}

DFSClientNode::~DFSClientNode() noexcept {}

const std::string DFSClientNode::ClientId() {
    return this->client_id;
}

void DFSClientNode::CreateStub(std::shared_ptr <Channel> channel) {
    this->service_stub = dfs_service::DFSService::NewStub(channel);
}

void DFSClientNode::SetMountPath(const std::string &path) {
    this->mount_path = path;
}

void DFSClientNode::SetDeadlineTimeout(int deadline) {
    this->deadline_timeout = deadline;
}

const std::string DFSClientNode::MountPath() {
    return this->mount_path;
};

std::string DFSClientNode::WrapPath(const std::string &filepath) {
    return this->mount_path + filepath;
}


