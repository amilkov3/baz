#include <regex>
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
#include <google/protobuf/util/time_util.h>

#include "dfslib-shared-p1.h"
#include "dfslib-clientnode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;

using dfs_service::File;
using dfs_service::FileStatus;
using dfs_service::Files;
using dfs_service::FileChunk;
using dfs_service::FileAck;
using dfs_service::Empty;

using google::protobuf::RepeatedPtrField;
using google::protobuf::util::TimeUtil;

using std::chrono::system_clock;
using std::chrono::milliseconds;
using namespace std;

//
// STUDENT INSTRUCTION:
//
// You may want to add aliases to your namespaced service methods here.
// All of the methods will be under the `dfs_service` namespace.
//
// For example, if you have a method named MyMethod, add
// the following:
//
//      using dfs_service::MyMethod
//


DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}

StatusCode DFSClientNodeP1::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept and store a file.
    //
    // When working with files in gRPC you'll need to stream
    // the file contents, so consider the use of gRPC's ClientWriter.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //
    FileAck response;

    ClientContext context;
    context.AddMetadata(FileNameMetadataKey, filename);
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    const string& filePath = WrapPath(filename);
    struct stat fs;
    if (stat(filePath.c_str(), &fs) != 0){
        dfs_log(LL_ERROR) << "File " << filePath << " does not exist";
        return StatusCode::NOT_FOUND;
    }

    int fileSize = fs.st_size;

    unique_ptr<ClientWriter<FileChunk>> resp = service_stub->WriteFile(&context, &response);
    dfs_log(LL_SYSINFO) << "Storing file " << filePath << "of size " << fileSize;
    ifstream ifs(filePath);
    FileChunk chunk;

    int bytesSent = 0;
    try {
        while(!ifs.eof() && bytesSent < fileSize){
            char buffer[ChunkSize];
            int bytesToSend = min(fileSize - bytesSent, ChunkSize);
            ifs.read(buffer, bytesToSend);
            chunk.set_contents(buffer, bytesToSend);
            resp->Write(chunk);
            bytesSent += bytesToSend;
            dfs_log(LL_SYSINFO) << "Stored " << bytesSent << " of " << fileSize << " bytes";
        }
        ifs.close();
        if (bytesSent != fileSize) {
            stringstream ss;
            ss << "The impossible happened" << endl;
            return StatusCode::CANCELLED;
        }
    } catch (exception const& e) {
        dfs_log(LL_ERROR) << "Error while sending file: " << e.what();
        resp.release();
        return StatusCode::CANCELLED;
    }
    resp->WritesDone();
    Status status = resp->Finish();
    if (!status.ok()) {
        dfs_log(LL_ERROR) << "Store response message: " << status.error_message() << " code: " << status.error_code();
        if (status.error_code() == StatusCode::INTERNAL) {
            return StatusCode::CANCELLED;
        }
    }
    dfs_log(LL_SYSINFO) << "Successfully finished storing: " << response.DebugString();
    return status.error_code();
}


StatusCode DFSClientNodeP1::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept a file request and return the contents
    // of a file from the service.
    //
    // As with the store function, you'll need to stream the
    // contents, so consider the use of gRPC's ClientReader.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
    ClientContext context;
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    File request;
    request.set_file_name(filename);

    const string& filePath = WrapPath(filename);
    unique_ptr<ClientReader<FileChunk>> response = service_stub->GetFile(&context, request);
    ofstream ofs;
    FileChunk chunk;
    try {
        while (response->Read(&chunk)) {
            if (!ofs.is_open()){
                ofs.open(filePath, ios::trunc);
            }
            const string& str = chunk.contents();
            dfs_log(LL_SYSINFO) << "Writing chunk of size " << str.length() << " bytes";
            ofs << str;
        }
        ofs.close();
    } catch (exception const& e) {
        dfs_log(LL_ERROR) << "Error writing to file: " << e.what();
        response.release();
        return StatusCode::CANCELLED;
    }
    Status status = response->Finish();
    if (!status.ok()) {
        dfs_log(LL_ERROR) << "Fetch response message: " << status.error_message() << " code: " << status.error_code();
        if (status.error_code() == StatusCode::INTERNAL) {
            return StatusCode::CANCELLED;
        }
    }
    return status.error_code();
}

StatusCode DFSClientNodeP1::Delete(const std::string& filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You will also need to add a request for a write lock before attempting to delete.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
    ClientContext context;
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    File request;
    request.set_file_name(filename);

    FileAck response;

    Status status = service_stub->DeleteFile(&context, request, &response);
    if (!status.ok()) {
        dfs_log(LL_ERROR) << "Delete failed - message: " << status.error_message() << ", code: " << status.error_code();
        if (status.error_code() == StatusCode::INTERNAL) {
            return StatusCode::CANCELLED;
        }
    }
    dfs_log(LL_SYSINFO) << "Success - response: " << response.DebugString();
    return status.error_code();
    
}

StatusCode DFSClientNodeP1::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list all files here. This method
    // should connect to your service's list method and return
    // a list of files using the message type you created.
    //
    // The file_map parameter is a simple map of files. You should fill
    // the file_map with the list of files you receive with keys as the
    // file name and values as the modified time (mtime) of the file
    // received from the server.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //
    ClientContext context;
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    Empty request;
    Files response;

    Status status = service_stub->ListFiles(&context, request, &response);
    if (!status.ok()) {
        dfs_log(LL_ERROR) << "List files failed - message: " << status.error_message() << ", code: " << status.error_code();
        if (status.error_code() == StatusCode::INTERNAL) {
            return StatusCode::CANCELLED;
        }
    }
    dfs_log(LL_SYSINFO) << "Success - response: " << response.DebugString();
   
    for (const FileAck& ack : response.file()) {
        int seconds = TimeUtil::TimestampToSeconds(ack.modified());
        file_map->insert(pair<string,int>(ack.file_name(), seconds));
        dfs_log(LL_SYSINFO) << "Adding " << ack.file_name() << " to file map";
    }
    return status.error_code();
}

StatusCode DFSClientNodeP1::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. This method should
    // retrieve the status of a file on the server. Note that you won't be
    // tested on this method, but you will most likely find that you need
    // a way to get the status of a file in order to synchronize later.
    //
    // The status might include data such as name, size, mtime, crc, etc.
    //
    // The file_status is left as a void* so that you can use it to pass
    // a message type that you defined. For instance, may want to use that message
    // type after calling Stat from another method.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //

    ClientContext context;
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    File request;
    request.set_file_name(filename);
    FileStatus response;

    Status status = service_stub->GetFileStatus(&context, request, &response);
    if (!status.ok()) {
        dfs_log(LL_ERROR) << "Delete failed - message: " << status.error_message() << ", code: " << status.error_code();
        if (status.error_code() == StatusCode::INTERNAL) {
            return StatusCode::CANCELLED;
        }
    }
    dfs_log(LL_SYSINFO) << "Success - response: " << response.DebugString();

    file_status = &response;

    return status.error_code();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional code here, including
// implementations of your client methods
//

