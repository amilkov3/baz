#include <string>
#include <iostream>
#include <fstream>
#include <cstddef>
#include <sys/stat.h>

#include "dfslib-shared-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

using dfs_service::FileStatus;
using google::protobuf::util::TimeUtil;
using google::protobuf::Timestamp;

using std::string;

// Global log level used throughout the system
// Note: this may be adjusted from the CLI in
// both the client and server executables.
// This shouldn't be changed at the file level.
dfs_log_level_e DFS_LOG_LEVEL = LL_ERROR;

//
// STUDENT INSTRUCTION:
//
// Add your additional code here. You may use
// the shared files for anything you need to add outside
// of the main structure, but you don't have to use them.
//
// Just be aware they are always submitted, so they should
// be compilable.
//
const char* ClientIdMetadataKey = "client_id";
const char* FileNameMetadataKey = "file_name";
const char* CheckSumMetadataKey = "checksum";
const char* MtimeMetadataKey = "mtime";

int ChunkSize = 5120;

/* getStat wraps the `stat` method returning only certain fields in a `FileStatus` type */
int getStat(string path, FileStatus* fs) {
    struct stat result;
    if (stat(path.c_str(), &result) != 0){
        return -1;
    }
    Timestamp* modified = new Timestamp(TimeUtil::TimeTToTimestamp(result.st_mtime));
    Timestamp* created = new Timestamp(TimeUtil::TimeTToTimestamp(result.st_ctime));
    fs->set_allocated_modified(modified);
    fs->set_allocated_created(created);
    fs->set_size(result.st_size);
    fs->set_name(path);
    return 0;
}