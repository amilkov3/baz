#ifndef _DFSLIB_SHARED_H
#define _DFSLIB_SHARED_H

#include <algorithm>
#include <cctype>
#include <locale>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

#include "src/dfs-utils.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include <google/protobuf/util/time_util.h>

#define DFS_RESET_TIMEOUT 3000

//
// STUDENT INSTRUCTION:
//
// Add your additional code here
//


// const char* const  
extern const char *FileNameMetadataKey; //= "file_name";
//extern const string& FileNameMetadataKey;
//extern const int ChunkSize;
extern int ChunkSize;

// path
int getStat(std::string path, dfs_service::FileStatus* fs);


#endif

