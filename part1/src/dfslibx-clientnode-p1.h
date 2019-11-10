#ifndef PR4_DFS_CLIENTNODE_H
#define PR4_DFS_CLIENTNODE_H

#include <string>
#include <vector>
#include <map>
#include <limits.h>
#include <chrono>
#include <mutex>

#include <grpcpp/grpcpp.h>
#include "../proto-src/dfs-service.grpc.pb.h"

class DFSClientNode {

protected:

    /** The deadline timeout in milliseconds **/
    int deadline_timeout;

    /** The unique client id **/
    std::string client_id;

    /** The mount path **/
    std::string mount_path;

    /** The service stub **/
    std::unique_ptr<dfs_service::DFSService::Stub> service_stub;

    /**
     * Utility function to wrap a filename with the mount path.
     *
     * @param filepath
     * @return
     */
    std::string WrapPath(const std::string& filepath);

public:
    /**
     * Constructor for the DFSClientNode class
     */
    DFSClientNode();

    /**
     * Destructor for the DFSClientNode class
     */
    ~DFSClientNode();

    /**
     * Sets the mount path on the client node
     * @param path
     */
    void SetMountPath(const std::string& path);

    /**
     * Sets the deadline timeout in milliseconds
     * @param deadline
     */
    void SetDeadlineTimeout(int deadline);

    /**
     * Gets the current mount path for the client node
     * @return std::string
     */
    const std::string MountPath();

    /**
     * Retrieve the unique client id
     */
    const std::string ClientId();

    /**
     * Creates the RPC channel to be used by the library
     * to connect to the remote GRPC service.
     *
     * @param channel
     */
    void CreateStub(std::shared_ptr<grpc::Channel> channel);

    /**
     * Store a file from the mount path on to the RPC server
     * @param filename
     * @return grpc::StatusCode
     */
    virtual grpc::StatusCode Store(const std::string& filename) = 0;

    /**
     * Fetch a file from the RPC server and put it in the mount path
     * @param filename
     * @return grpc::StatusCode
     */
    virtual grpc::StatusCode Fetch(const std::string& filename) = 0;

    /**
     * Delete a file from the RPC server
     * @param filename
     * @return grpc::StatusCode
     */
    virtual grpc::StatusCode Delete(const std::string& filename) = 0;

    /**
     * Get or print a list from the RPC server.
     *
     * @param file_map - A map of filenames as keys and sizes as values. If NULL, the map can be ignored.
     *                   If not NULL, then the map should be filled in with the filenames and sizes.
     * @param display - If true, the system should print a list of files to stdout
     * @return grpc::StatusCode
     */
    virtual grpc::StatusCode List(std::map<std::string,int>* file_map = NULL, bool display = false) = 0;

    /**
     * Get or print the stat details for a given filename,
     *
     * If the provided `file_status` parameter is NULL, this function
     * should print the status details to stdout.
     *
     * Otherwise, the file_status parameter should contain a pointer to the
     * protobuf status object returned by the GRPC service.
     *
     * @param filename
     * @param file_status
     * @return grpc::StatusCode
     */
    virtual grpc::StatusCode Stat(const std::string& filename, void* file_status = NULL) = 0;

};
#endif
