#ifndef PR4_DFSLIB_SHARED_H
#define PR4_DFSLIB_SHARED_H

#include <algorithm>
#include <cctype>
#include <locale>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <thread>
#include <sys/stat.h>

#include "src/dfs-utils.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include <google/protobuf/util/time_util.h>


//
// STUDENT INSTRUCTION
//
// The following defines and methods must be left as-is for the
// structural components provided to work.
//
#define DFS_RESET_TIMEOUT 3000
#define DFS_I_EVENT_SIZE (sizeof(struct inotify_event))
#define DFS_I_BUFFER_SIZE (1024 * (DFS_I_EVENT_SIZE + 16))

/** A file descriptor type **/
typedef int FileDescriptor;

/** A watch descriptor type **/
typedef int WatchDescriptor;

/** An inotify callback method **/
typedef void (*InotifyCallback)(uint, const std::string&, void*);

/** The expected struct for the inotify setup in this project **/
struct NotifyStruct {
    FileDescriptor fd;
    WatchDescriptor wd;
    uint event_type;
    std::thread * thread;
    InotifyCallback callback;
};

/** The expected event type for the event method in this project **/
struct EventStruct {
    void* event;
    void* instance;
};

//
// STUDENT INSTRUCTION:
//
// Add any additional shared code here
//

extern const char* ClientIdMetadataKey;
extern const char* FileNameMetadataKey;
extern const char* CheckSumMetadataKey;
extern const char* MtimeMetadataKey;

extern int ChunkSize;

int getStat(std::string path, dfs_service::FileStatus* fs);

inline std::string status_code_str(grpc::StatusCode code) {
    switch (code) {
        case grpc::StatusCode::OK: return "OK";
        case grpc::StatusCode::INTERNAL: return "INTERNAL";
        case grpc::StatusCode::DEADLINE_EXCEEDED: return "DEADLINE_EXCEEDED";
        case grpc::StatusCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
        case grpc::StatusCode::CANCELLED: return "CANCELLED";
        case grpc::StatusCode::NOT_FOUND: return "NOT_FOUND";
        case grpc::StatusCode::ALREADY_EXISTS: return "ALREADY_EXISTS";
        default: 
            std::stringstream ss;
            ss << code; 
            return ss.str();
    }
}

#endif

