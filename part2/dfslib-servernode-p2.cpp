#include <map>
#include <mutex>
#include <shared_mutex>
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
#include <utime.h>

#include "proto-src/dfs-service.grpc.pb.h"
#include "src/dfslibx-call-data.h"
#include "src/dfslibx-service-runner.h"
#include "dfslib-shared-p2.h"
#include "dfslib-servernode-p2.h"
#include <google/protobuf/util/time_util.h>

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;
using grpc::string_ref;

using namespace dfs_service;

using google::protobuf::util::TimeUtil;
using google::protobuf::Timestamp;
using google::protobuf::uint64;

using namespace std;

//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using in your `dfs-service.proto` file
// to indicate a file request and a listing of files from the server
//
using FileRequestType = dfs_service::File;
using FileListResponseType = dfs_service::Files;

using FileName = string;
using ClientId = string;

extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// As with Part 1, the DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in your `dfs-service.proto` file.
//
// You may start with your Part 1 implementations of each service method.
//
// Elements to consider for Part 2:
//
// - How will you implement the write lock at the server level?
// - How will you keep track of which client has a write lock for a file?
//      - Note that we've provided a preset client_id in DFSClientNode that generates
//        a client id for you. You can pass that to the server to identify the current client.
// - How will you release the write lock?
// - How will you handle a store request for a client that doesn't have a write lock?
// - When matching files to determine similarity, you should use the `file_checksum` method we've provided.
//      - Both the client and server have a pre-made `crc_table` variable to speed things up.
//      - Use the `file_checksum` method to compare two files, similar to the following:
//
//          std::uint32_t server_crc = dfs_file_checksum(filepath, &this->crc_table);
//
//      - Hint: as the crc checksum is a simple integer, you can pass it around inside your message types.
//
class DFSServiceImpl final :
    public DFSService::WithAsyncMethod_CallbackList<DFSService::Service>,
        public DFSCallDataManager<FileRequestType , FileListResponseType> {

private:

    /** The runner service used to start the service and manage asynchronicity **/
    DFSServiceRunner<FileRequestType, FileListResponseType> runner;

    /** The mount path for the server **/
    std::string mount_path;

    /** Mutex for managing the queue requests **/
    std::mutex queue_mutex;

    /** The vector of queued tags used to manage asynchronous requests **/
    std::vector<QueueRequest<FileRequestType, FileListResponseType>> queued_tags;


    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }

    /** CRC Table kept in memory for faster calculations **/
    CRC::Table<std::uint32_t, 32> crc_table;

    shared_timed_mutex filesMutex;
    map<FileName, ClientId> fileNameToClientId;

    shared_timed_mutex inProgressMutex;
    map<FileName, unique_ptr<shared_timed_mutex>> inProgressFileNameToMutex;

    shared_timed_mutex dirMutex;

    void ReleaseWriteLock(string fileName) {
        filesMutex.lock();
        fileNameToClientId.erase(fileName);
        filesMutex.unlock();
    }

    // client and server checksum should not match
    Status verifyChecksum(const multimap<string_ref, string_ref>& metadata, string& filePath) {
        auto clientCheckSumV = metadata.find(CheckSumMetadataKey);
        if (clientCheckSumV == metadata.end()){
            stringstream ss;
            ss << "Missing " << CheckSumMetadataKey << " in client metadata" << endl;
            return Status(StatusCode::INTERNAL, ss.str());
        }
        unsigned long clientCheckSum = stoul(string(clientCheckSumV->second.begin(), clientCheckSumV->second.end()));
        unsigned long serverCheckSum = dfs_file_checksum(filePath, &crc_table);
        dfs_log(LL_DEBUG2) << "File path: " << filePath << "Client file checksum: " << clientCheckSum << " Server file checksum: " << serverCheckSum;
        if (clientCheckSum == serverCheckSum) {
            stringstream ss;
            ss << "File " << filePath << " contents are the same on client and server" << endl;
            return Status(StatusCode::ALREADY_EXISTS, ss.str());
        }
        return Status::OK;
    }

public:

    DFSServiceImpl(const std::string& mount_path, const std::string& server_address, int num_async_threads):
        mount_path(mount_path), crc_table(CRC::CRC_32()) {

        this->runner.SetService(this);
        this->runner.SetAddress(server_address);
        this->runner.SetNumThreads(num_async_threads);
        this->runner.SetQueuedRequestsCallback([&]{ this->ProcessQueuedRequests(); });

        // populate existing files in inProgressFileNameToMutex
        DIR *dir;
        if ((dir = opendir(mount_path.c_str())) == NULL) {
            // could not open directory 
            dfs_log(LL_ERROR) << "Failed to open directory at mount path " << mount_path;
            exit(EXIT_FAILURE);
        }
        // traverse everything in directory 
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            struct stat path_stat;
            string dirEntry(ent->d_name);
            string path = WrapPath(dirEntry);
            stat(path.c_str(), &path_stat);
            // if dir item is a file
            if (!S_ISREG(path_stat.st_mode)){
                dfs_log(LL_SYSINFO) << "Found dir at " << path << " - Skipping";
                continue;
            }
            dfs_log(LL_SYSINFO) << "Found file at " << path;
            inProgressFileNameToMutex[dirEntry] = make_unique<shared_timed_mutex>();
        }
        closedir(dir);
    }

    ~DFSServiceImpl() {
        this->runner.Shutdown();
    }

    void Run() {
        this->runner.Run();
    }

    /**
     * Request callback for asynchronous requests
     *
     * This method is called by the DFSCallData class during
     * an asynchronous request call from the client.
     *
     * Students should not need to adjust this.
     *
     * @param context
     * @param request
     * @param response
     * @param cq
     * @param tag
     */
    void RequestCallback(grpc::ServerContext* context,
                         FileRequestType* request,
                         grpc::ServerAsyncResponseWriter<FileListResponseType>* response,
                         grpc::ServerCompletionQueue* cq,
                         void* tag) {

        std::lock_guard<std::mutex> lock(queue_mutex);
        this->queued_tags.emplace_back(context, request, response, cq, tag);

    }

    /**
     * Process a callback request
     *
     * This method is called by the DFSCallData class when
     * a requested callback can be processed. You should use this method
     * to manage the CallbackList RPC call and respond as needed.
     *
     * See the STUDENT INSTRUCTION for more details.
     *
     * @param context
     * @param request
     * @param response
     */
    void ProcessCallback(ServerContext* context, FileRequestType* request, FileListResponseType* response) {

        //
        // STUDENT INSTRUCTION:
        //
        // You should add your code here to respond to any CallbackList requests from a client.
        // This function is called each time an asynchronous request is made from the client.
        //
        // The client should receive a list of files or modifications that represent the changes this service
        // is aware of. The client will then need to make the appropriate calls based on those changes.
        //
        dfs_log(LL_DEBUG2) << "Handling ProcessCallback call. Filename: " << request->name();
        Status status = this->CallbackList(context, request, response);

        if (!status.ok()) {
            dfs_log(LL_ERROR) << "CallbackList via ProcessCallback failed - message: " << status.error_message() << ", code: " << status_code_str(status.error_code());
            return;
        }
        dfs_log(LL_DEBUG2) << "ProcessCallback success - response: " << response->DebugString();

    }

    /**
     * Processes the queued requests in the queue thread
     */
    void ProcessQueuedRequests() {
        while(true) {

            //
            // STUDENT INSTRUCTION:
            //
            // You should add any synchronization mechanisms you may need here in
            // addition to the queue management. For example, modified files checks.
            //
            // Note: you will need to leave the basic queue structure as-is, but you
            // may add any additional code you feel is necessary.
            //


            // Guarded section for queue
            {
                dfs_log(LL_DEBUG2) << "Waiting for queue guard";
                std::lock_guard<std::mutex> lock(queue_mutex);


                for(QueueRequest<FileRequestType, FileListResponseType>& queue_request : this->queued_tags) {
                    this->RequestCallbackList(queue_request.context, queue_request.request,
                        queue_request.response, queue_request.cq, queue_request.cq, queue_request.tag);
                    queue_request.finished = true;
                }

                // any finished tags first
                this->queued_tags.erase(std::remove_if(
                    this->queued_tags.begin(),
                    this->queued_tags.end(),
                    [](QueueRequest<FileRequestType, FileListResponseType>& queue_request) { return queue_request.finished; }
                ), this->queued_tags.end());

            }
        }
    }

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // the implementations of your rpc protocol methods.
    //

    Status AcquireWriteLock(
        ServerContext* context,
        const File* request,
        WriteLock* response
    ) override {
        const multimap<string_ref, string_ref>& metadata = context->client_metadata();
        auto clientIdV = metadata.find(ClientIdMetadataKey);
        // File name is missing
        if (clientIdV == metadata.end()){
            stringstream ss;
            ss << "Missing " << ClientIdMetadataKey << " in client metadata" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        }
        auto clientId = string(clientIdV->second.begin(), clientIdV->second.end());

        filesMutex.lock();
        auto lockClientIdV = fileNameToClientId.find(request->name());
        string lockClientId;
        if (lockClientIdV != fileNameToClientId.end() && (lockClientId = lockClientIdV->second).compare(clientId) != 0){
            filesMutex.unlock();

            stringstream ss;
            ss << "Acquiring lock failed. File " << request->name() << " already has a write lock from client " << lockClientId << " Your id: " << clientId;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::RESOURCE_EXHAUSTED, ss.str());
        } else if (lockClientId.compare(clientId) == 0) {
            filesMutex.unlock();

            dfs_log(LL_SYSINFO) << "Your client id " << clientId << " already has a lock on " << request->name();
            return Status::OK;
        }
        fileNameToClientId[request->name()] = clientId;
        if (inProgressFileNameToMutex.find(request->name()) == inProgressFileNameToMutex.end()){
            inProgressFileNameToMutex[request->name()] =  make_unique<shared_timed_mutex>();
        }
        filesMutex.unlock();

        return Status::OK;
    }

    Status WriteFile(
        ServerContext* context,
        ServerReader<FileChunk>* reader,
        FileAck* response
    ) override {
        const multimap<string_ref, string_ref>& metadata = context->client_metadata();
        auto fileNameV = metadata.find(FileNameMetadataKey);
        auto clientIdV = metadata.find(ClientIdMetadataKey);
        auto mtimeV = metadata.find(MtimeMetadataKey);

        // File name is missing
        if (fileNameV == metadata.end()){
            stringstream ss;
            ss << "Missing " << FileNameMetadataKey << " in client metadata" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
            // Client id is missing
        } else if (clientIdV == metadata.end()){
            stringstream ss;
            ss << "Missing " << ClientIdMetadataKey << " in client metadata" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        } else if (mtimeV == metadata.end()){
            stringstream ss;
            ss << "Missing " << MtimeMetadataKey << " in client metadata" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        }
        auto fileName = string(fileNameV->second.begin(), fileNameV->second.end());
        auto clientId = string(clientIdV->second.begin(), clientIdV->second.end());
        long mtime = stol(string(mtimeV->second.begin(), mtimeV->second.end()));

        string filePath = WrapPath(fileName);

        filesMutex.lock_shared();
        auto lockClientIdV = fileNameToClientId.find(fileName);
        auto fileAccessMutexV = inProgressFileNameToMutex.find(fileName);
        // TODO: Skipping check whether it exists
        shared_timed_mutex* fileAccessMutex = fileAccessMutexV->second.get();
        string lockClientId;
        if (lockClientIdV == fileNameToClientId.end()){
            filesMutex.unlock_shared();

            stringstream ss;
            ss << "Your client id " << clientId << " doesn't have a write lock for file " << fileName;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        } else if ((lockClientId = lockClientIdV->second).compare(clientId) != 0) {
            filesMutex.unlock_shared();

            stringstream ss;
            ss << "File " << fileName << " already has a lock from client " << lockClientId << " Your id: " << clientId;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        }
        filesMutex.unlock_shared();

        dirMutex.lock();
        fileAccessMutex->lock();

        Status checkSumResult = verifyChecksum(metadata, filePath);
        if (!checkSumResult.ok()){
            struct stat fs;
            if ((stat(filePath.c_str(), &fs) == 0) && (mtime > fs.st_mtime)){
                dfs_log(LL_SYSINFO) << "Client mtime " << mtime << " greater than server mtime" << fs.st_mtime << " but contents are the same. Updating";
                struct utimbuf ub;
                ub.modtime = mtime;
                ub.actime = mtime;
                if (!utime(filePath.c_str(), &ub)) {
                    dfs_log(LL_SYSINFO) << "Updated " << filePath << " mtime to " << mtime;
                }
            }
            ReleaseWriteLock(fileName);
            fileAccessMutex->unlock();
            dirMutex.unlock();

            dfs_log(LL_ERROR) << checkSumResult.error_message();
            return checkSumResult;
        }

        dfs_log(LL_SYSINFO) << "Writing file " << filePath << "Client id: " << clientId;

        FileChunk chunk;
        ofstream ofs;
        
        try {
            while (reader->Read(&chunk)) {
                if (!ofs.is_open()) {
                    ofs.open(filePath, ios::trunc);
                }

                if (context->IsCancelled()){
                    ReleaseWriteLock(fileName);
                    fileAccessMutex->unlock();
                    dirMutex.unlock();

                    const string& err = "Request deadline has expired";
                    dfs_log(LL_ERROR) << err;
                    return Status(StatusCode::DEADLINE_EXCEEDED, err);
                }

                const string& chunkStr = chunk.contents();
                ofs << chunkStr;
                dfs_log(LL_SYSINFO) << "Wrote chunk of size " << chunkStr.length();
            }
            ofs.close();
            ReleaseWriteLock(fileName);
        } catch (exception const& e) {
            ReleaseWriteLock(fileName);
            fileAccessMutex->unlock();
            dirMutex.unlock();

            stringstream ss;
            ss << "Error writing to file " << e.what() << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        }
        FileStatus fs;
        if (getStat(filePath, &fs) != 0) {
            fileAccessMutex->unlock();
            dirMutex.unlock();

            stringstream ss;
            ss << "Getting file info for file " << filePath << " failed with: " << strerror(errno) << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::NOT_FOUND, ss.str());
        }
        fileAccessMutex->unlock();
        dirMutex.unlock();

        response->set_name(fileName);
        Timestamp* modified = new Timestamp(fs.modified());
        response->set_allocated_modified(modified);
        return Status::OK;
    }

    Status GetFile(
        ServerContext* context, 
        const File* request,
        ServerWriter<FileChunk>* writer
    ) override {
        string filePath = WrapPath(request->name());
        
        filesMutex.lock();
        if (inProgressFileNameToMutex.find(request->name()) == inProgressFileNameToMutex.end()){
            inProgressFileNameToMutex[request->name()] =  make_unique<shared_timed_mutex>();
        }         
        shared_timed_mutex* fileAccessMutex = inProgressFileNameToMutex.find(request->name())->second.get();
        filesMutex.unlock();

        fileAccessMutex->lock_shared();

        struct stat fs;
        if (stat(filePath.c_str(), &fs) != 0){
            fileAccessMutex->unlock_shared();

            stringstream ss;
            ss << "File " << filePath << " does not exist" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::NOT_FOUND, ss.str());
        }

        Status checkSumResult = verifyChecksum(context->client_metadata(), filePath);
        if (!checkSumResult.ok()){
            fileAccessMutex->unlock_shared();

            const multimap<string_ref, string_ref>& metadata = context->client_metadata();
            auto mtimeV = metadata.find(MtimeMetadataKey);
            if (mtimeV == metadata.end()){
                stringstream ss;
                ss << "Missing " << MtimeMetadataKey << " in client metadata" << endl;
                dfs_log(LL_ERROR) << ss.str();
                return Status(StatusCode::INTERNAL, ss.str());
            }
            long mtime = stol(string(mtimeV->second.begin(), mtimeV->second.end()));

            if (mtime > fs.st_mtime){
                dfs_log(LL_SYSINFO) << "Client mtime " << mtime << " greater than server mtime" << fs.st_mtime << " but contents are the same. Updating";
                struct utimbuf ub;
                ub.modtime = mtime;
                ub.actime = mtime;
                if (!utime(filePath.c_str(), &ub)) {
                    dfs_log(LL_SYSINFO) << "Updated " << filePath << " mtime to " << mtime;
                }
            }
            dfs_log(LL_ERROR) << checkSumResult.error_message();
            return checkSumResult;
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
                    fileAccessMutex->unlock_shared();
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
            fileAccessMutex->unlock_shared();
            if (bytesSent != fileSize) {
                stringstream ss;
                ss << "The impossible happened" << endl;
                return Status(StatusCode::INTERNAL, ss.str());
            }
        } catch (exception const& e) {
            fileAccessMutex->unlock_shared();
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
        const string& filePath = WrapPath(request->name());

        const multimap<string_ref, string_ref>& metadata = context->client_metadata();
        // Retrieve client id
        auto clientIdV = metadata.find(ClientIdMetadataKey);
        if (clientIdV == metadata.end()){
            stringstream ss;
            ss << "Missing " << ClientIdMetadataKey << " in client metadata" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        }
        auto clientId = string(clientIdV->second.begin(), clientIdV->second.end());
        
        filesMutex.lock_shared();
        auto lockClientIdV = fileNameToClientId.find(request->name());
        if (inProgressFileNameToMutex.find(request->name()) == inProgressFileNameToMutex.end()){
            inProgressFileNameToMutex[request->name()] =  make_unique<shared_timed_mutex>();
        }         
        shared_timed_mutex* fileAccessMutex = inProgressFileNameToMutex.find(request->name())->second.get();
        string lockClientId;
        if (lockClientIdV == fileNameToClientId.end()){
            filesMutex.unlock_shared();
            stringstream ss;
            ss << "Your client id " << clientId << " doesn't have a write lock for file " << request->name();
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        } else if ((lockClientId = lockClientIdV->second).compare(clientId) != 0) {
            filesMutex.unlock_shared();
            stringstream ss;
            ss << "File " << request->name() << " already has a lock from client " << lockClientId << ". Your id: " << clientId;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::INTERNAL, ss.str());
        }
        filesMutex.unlock_shared();


        dfs_log(LL_SYSINFO) << "Deleting file " << filePath;
        dirMutex.lock();
        fileAccessMutex->lock();
        // File doesnt exist
        FileStatus fs;
        if (getStat(filePath, &fs) != 0) {
            ReleaseWriteLock(request->name());
            fileAccessMutex->unlock();
            dirMutex.unlock();

            stringstream ss;
            ss << "File " << filePath << " doesn't exist" << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::NOT_FOUND, ss.str());
        }
        if (context->IsCancelled()){
            ReleaseWriteLock(request->name());
            fileAccessMutex->unlock();
            dirMutex.unlock();

            const string& err = "Request deadline has expired";
            dfs_log(LL_ERROR) << err;
            return Status(StatusCode::DEADLINE_EXCEEDED, err);
        }
        // Delete file
        if (remove(filePath.c_str()) != 0) {
            ReleaseWriteLock(request->name());
            fileAccessMutex->unlock();
            dirMutex.unlock();

            stringstream ss;
            ss << "Removing file " << filePath << " failed with: " << strerror(errno) << endl;
            return Status(StatusCode::INTERNAL, ss.str());
        }
        ReleaseWriteLock(request->name());
        fileAccessMutex->unlock();
        dirMutex.unlock();

        response->set_name(request->name());
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
        dirMutex.lock_shared();
        DIR *dir;
        /*if (context->IsCancelled()){
            dfs_log(LL_SYSINFO) << "we fucked";
            dirMutex.unlock_shared();

            const string& err = "Request deadline has expired";
            dfs_log(LL_ERROR) << err;
        }*/
        if ((dir = opendir(mount_path.c_str())) == NULL) {
            /* could not open directory */
            dfs_log(LL_ERROR) << "Failed to open directory at mount path " << mount_path;
            return Status::OK;
        }
        /* traverse everything in directory */
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            /*if (context->IsCancelled()){
                dirMutex.unlock_shared();

                const string& err = "Request deadline has expired";
                dfs_log(LL_ERROR) << err;
                return Status(StatusCode::DEADLINE_EXCEEDED, err);
            }*/
            struct stat path_stat;
            string dirEntry(ent->d_name);
            string path = WrapPath(dirEntry);
            stat(path.c_str(), &path_stat);
            /* if dir item is a file */
            if (!S_ISREG(path_stat.st_mode)){
                dfs_log(LL_DEBUG2) << "Found dir at " << path << " - Skipping";
                continue;
            }
            dfs_log(LL_DEBUG2) << "Found file at " << path;
            FileStatus* ack = response->add_file();
            FileStatus status;
            if (getStat(path, &status) != 0) {
                dirMutex.unlock_shared();

                stringstream ss;
                ss << "Getting file info for file " << path << " failed with: " << strerror(errno) << endl;
                dfs_log(LL_ERROR) << ss.str();
                return Status(StatusCode::NOT_FOUND, ss.str());
            }
            /*
            ack->set_name(dirEntry);
            Timestamp* modified = new Timestamp(status.modified());
            ack->set_allocated_modified(modified);
            */
           ack->set_name(dirEntry);
           Timestamp* modified = new Timestamp(status.modified());
           Timestamp* created = new Timestamp(status.created());
           ack->set_allocated_created(created);
           ack->set_allocated_modified(modified);
           ack->set_size(status.size());
        }
        closedir(dir);
        dirMutex.unlock_shared();

        return Status::OK;
    }

    Status GetFileStatus(
        ServerContext* context,
        const File* request,
        FileStatus* response
    ) override {
        cout << "\n";
        if (context->IsCancelled()){
            const string& err = "Request deadline has expired";
            dfs_log(LL_ERROR) << err;
            return Status(StatusCode::DEADLINE_EXCEEDED, err);
        }

        string filePath = WrapPath(request->name());

        filesMutex.lock();
        if (inProgressFileNameToMutex.find(request->name()) == inProgressFileNameToMutex.end()){
            inProgressFileNameToMutex[request->name()] =  make_unique<shared_timed_mutex>();
        }         
        shared_timed_mutex* fileAccessMutex = inProgressFileNameToMutex.find(request->name())->second.get();
        filesMutex.unlock();

        fileAccessMutex->lock_shared();

        /* Get FileStatus of file */
        if (getStat(filePath, response) != 0) {
            fileAccessMutex->unlock_shared();

            stringstream ss;
            ss << "Getting file info for file " << filePath << " failed with: " << strerror(errno) << endl;
            dfs_log(LL_ERROR) << ss.str();
            return Status(StatusCode::NOT_FOUND, ss.str());
        }
        fileAccessMutex->unlock_shared();

        return Status::OK;
    }

    Status CallbackList(
        ServerContext* context,
        const File* request,
        Files* response
    ) override {
        dfs_log(LL_DEBUG2) << "Handling CallbackList call. Filename: " << request->name();
        Empty request1;
        return this->ListFiles(context, &request1, response);
    }



};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly
// to add additional startup/shutdown routines inside, but be aware that
// the basic structure should stay the same as the testing environment
// will be expected this structure.
//
/**
 * The main server node constructor
 *
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        int num_async_threads,
        std::function<void()> callback) :
        server_address(server_address),
        mount_path(mount_path),
        num_async_threads(num_async_threads),
        grader_callback(callback) {}
/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
}

/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path, this->server_address, this->num_async_threads);


    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    service.Run();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//
