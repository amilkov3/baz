#ifndef PR4_DFSLIB_SERVERNODE_H
#define PR4_DFSLIB_SERVERNODE_H

#include <string>
#include <iostream>
#include <thread>
#include <grpcpp/grpcpp.h>

/**
 * DFSService is used to start up and run your DFSServiceImpl
 * based on the protobuf service you created in `proto-service.proto`.
 */
class DFSServerNode {

private:
    /** The server address information **/
    std::string server_address;

    /** The mount path for the server **/
    std::string mount_path;

    /** The pointer to the grpc server instance **/
    std::unique_ptr<grpc::Server> server;

    /** Number of asynchronous threads to use **/
    int num_async_threads;

    /** Server callback **/
    std::function<void()> grader_callback;

public:
    DFSServerNode(const std::string& server_address,
        const std::string& mount_path,
        int num_async_threads,
        std::function<void()> callback);
    ~DFSServerNode();
    void Shutdown();
    void Start();
};

#endif
