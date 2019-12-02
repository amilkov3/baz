#include <map>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>

#include "src/dfs-utils.h"
#include "dfslib-shared-p1.h"
#include "dfslib-servernode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include <google/protobuf/util/time_util.h>

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;
using grpc::string_ref;

using dfs_service::DFSService;
using dfs_service::FileChunk;
using dfs_service::FileAck;
using dfs_service::File;
using dfs_service::Files;
using dfs_service::Empty;
using dfs_service::FileStatus;

using google::protobuf::util::TimeUtil;
using google::protobuf::Timestamp;
using google::protobuf::uint64;

//using std::multimap;
//using std::stringstream;
//using std::endl;
//using std::string;
using namespace std;


//
// STUDENT INSTRUCTION:
//
// DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in the `dfs-service.proto` file.
//
// You should add your definition overrides here for the specific
// methods that you created for your GRPC service protocol. The
// gRPC tutorial described in the readme is a good place to get started
// when trying to understand how to implement this class.
//
// The method signatures generated can be found in `proto-src/dfs-service.grpc.pb.h` file.
//
// Look for the following section:
//
//      class Service : public Service {
//
// The methods returning grpc::Status are the methods you'll want to override.
//
// In C++, you'll want to use the `override` directive as well. For example,
// if you have a service method named MyMethod that takes a MyMessageType
// and a ServerWriter, you'll want to override it similar to the following:
//
//      Status MyMethod(ServerContext* context,
//                      const MyMessageType* request,
//                      ServerWriter<MySegmentType> *writer) override {
//
//          /** code implementation here **/
//      }
//
class DFSServiceImpl final : public DFSService::Service {

private:

    /** The mount path for the server **/
    std::string mount_path;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }


public:

    DFSServiceImpl(const std::string &mount_path): mount_path(mount_path) {
    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // implementations of your protocol service methods
    //

    Status WriteFile(
        ServerContext* context,
        ServerReader<FileChunk>* reader,
        FileAck* response
    ) override {
        const multimap<string_ref, string_ref>& metadata = context->client_metadata();
        auto fileNameV = metadata.find(FileNameMetadataKey);
        // File name is missing 
        if (fileNameV == metadata.end()){
            stringstream ss;
            ss << "Missing " << FileNameMetadataKey << " in client metadata" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        }

        auto fileName = string(fileNameV->second.begin(), fileNameV->second.end());

        const string& filePath = WrapPath(fileName);

        dfs_log(LL_SYSINFO) << "Writing file " << filePath;

        FileChunk chunk;
        ofstream ofs;
        try {
            while (reader->Read(&chunk)) {
                if (!ofs.is_open()) {
                    ofs.open(filePath, ios::trunc);
                }

                if (context->IsCancelled()){
                    const string& err = "Request deadline has expired";
                    dfs_log(LL_ERROR) << err;
                    return Status(StatusCode::DEADLINE_EXCEEDED, err);
                }

                const string& chunkStr = chunk.contents();
                ofs << chunkStr;
                dfs_log(LL_SYSINFO) << "Wrote chunk of size " << chunkStr.length();
            }
            ofs.close();
        } catch (exception const& e) {
            stringstream ss;
            ss << "Error writing to file " << e.what() << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        }
        FileStatus fs;
        if (getStat(filePath, &fs) != 0) {
            stringstream ss;
            ss << "Getting file info for file " << filePath << " failed with: " << strerror(errno) << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::NOT_FOUND, ss.str());
        }
        response->set_file_name(fileName);
        Timestamp* modified = new Timestamp(fs.modified());
        response->set_allocated_modified(modified);
        return Status::OK;
    }

    Status GetFile(
        ServerContext* context, 
        const File* request,
        ServerWriter<FileChunk>* writer
    ) override {
        const string& filePath = WrapPath(request->file_name());
        struct stat fs;
        if (stat(filePath.c_str(), &fs) != 0){
            stringstream ss;
            ss << "File " << filePath << " does not exist" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::NOT_FOUND, ss.str());
        }

        dfs_log(LL_SYSINFO) << "Retrieving file " << filePath;

        int fileSize = fs.st_size;
        
        ifstream ifs(filePath);
        FileChunk chunk;
        try {
            int bytesSent = 0;
            while(!ifs.eof() && bytesSent < fileSize){
                int bytesToSend = min(fileSize - bytesSent, ChunkSize);
                char buffer[ChunkSize];
                if (context->IsCancelled()){
                    const string& err = "Request deadline has expired";
                    dfs_log(LL_ERROR) << err;
                    return Status(StatusCode::DEADLINE_EXCEEDED, err);
                }
                ifs.read(buffer, bytesToSend);
                chunk.set_contents(buffer, bytesToSend);
                writer->Write(chunk);
                dfs_log(LL_SYSINFO) << "Returned chunk of size " << bytesToSend << " bytes";
                bytesSent += bytesToSend;
            }
            ifs.close();
            if (bytesSent != fileSize) {
                stringstream ss;
                ss << "The impossible happened" << endl;
                return Status(StatusCode::INTERNAL, ss.str());
            }
        } catch (exception const& e) {
            stringstream ss;
            ss << "Error reading file " << e.what() << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        }

        dfs_log(LL_SYSINFO) << "Finished retrieving file " << filePath;

        return Status::OK;
    }

    Status DeleteFile(
        ServerContext* context, 
        const File* request,
        FileAck* response
    ) override {
        const string& filePath = WrapPath(request->file_name());
        dfs_log(LL_SYSINFO) << "Deleting file " << filePath;
        /* File doesnt exist */
        FileStatus fs;
        if (getStat(filePath, &fs) != 0) {
            stringstream ss;
            ss << "File " << filePath << " doesn't exist" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::NOT_FOUND, ss.str());
        }
        if (context->IsCancelled()){
            const string& err = "Request deadline has expired";
            dfs_log(LL_ERROR) << err;
            return Status(StatusCode::DEADLINE_EXCEEDED, err);
        }
        /* Delete file */
        if (remove(filePath.c_str()) != 0) {
            stringstream ss;
            ss << "Removing file " << filePath << " failed with: " << strerror(errno) << endl;
            return Status(StatusCode::INTERNAL, ss.str());
        }
        response->set_file_name(request->file_name());
        Timestamp* modified = new Timestamp(fs.modified());
        response->set_allocated_modified(modified);
        dfs_log(LL_SYSINFO) << "Deleted file successfully";
        return Status::OK;
    }

    Status ListFiles(
        ServerContext* context,
        const Empty* request,
        Files* response
    ) override {
        DIR *dir;
        if ((dir = opendir(mount_path.c_str())) == NULL) {
            /* could not open directory */
            dfs_log(LL_ERROR) << "Failed to open directory at mount path " << mount_path;
            return Status::OK;
        }
        /* traverse everything in directory */
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (context->IsCancelled()){
                const string& err = "Request deadline has expired";
                dfs_log(LL_ERROR) << err;
                return Status(StatusCode::DEADLINE_EXCEEDED, err);
            }
            struct stat path_stat;
            string dirEntry(ent->d_name);
            string path = WrapPath(dirEntry);
            stat(path.c_str(), &path_stat);
            /* if dir item is a file */
            if (!S_ISREG(path_stat.st_mode)){
                dfs_log(LL_SYSINFO) << "Found dir at " << path << " - Skipping";
                continue;
            }
            dfs_log(LL_SYSINFO) << "Found file at " << path;
            FileAck* ack = response->add_file();
            FileStatus status;
            if (getStat(path, &status) != 0) {
                stringstream ss;
                ss << "Getting file info for file " << path << " failed with: " << strerror(errno) << endl;
                dfs_log(LL_ERROR) << ss.str();
                return Status(StatusCode::NOT_FOUND, ss.str());
            }
            ack->set_file_name(dirEntry);
            //Timestamp* modified = status.mod
            Timestamp* modified = new Timestamp(status.modified());
            ack->set_allocated_modified(modified);
        }
        closedir(dir);
        return Status::OK;
    }

    Status GetFileStatus(
        ServerContext* context,
        const File* request,
        FileStatus* response
    ) override {
        if (context->IsCancelled()){
            const string& err = "Request deadline has expired";
            dfs_log(LL_ERROR) << err;
            return Status(StatusCode::DEADLINE_EXCEEDED, err);
        }

        string filePath = WrapPath(request->file_name());
        /* Get FileStatus of file */
        if (getStat(filePath, response) != 0) {
            stringstream ss;
            ss << "Getting file info for file " << filePath << " failed with: " << strerror(errno) << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::NOT_FOUND, ss.str());
        }
        return Status::OK;
    }

};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly,
// but be aware that the testing environment is expecting these three
// methods as-is.
//
/**
 * The main server node constructor
 *
 * @param server_address
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        std::function<void()> callback) :
    server_address(server_address), mount_path(mount_path), grader_callback(callback) {}

/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
    this->server->Shutdown();
}

/** Server start **/
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path);
    ServerBuilder builder;
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional DFSServerNode definitions here
//

