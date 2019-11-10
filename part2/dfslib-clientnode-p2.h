#ifndef PR4_DFSLIB_CLIENTNODE_H
#define PR4_DFSLIB_CLIENTNODE_H

#include <string>
#include <vector>
#include <map>
#include <limits.h>
#include <chrono>
#include <mutex>

#include <grpcpp/grpcpp.h>

#include "src/dfslibx-clientnode-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

class DFSClientNodeP2 : public DFSClientNode {

public:

    //
    // STUDENT INSTRUCTION:
    //
    // This class derives from the parent DFSClientNode so
    // that you can focus on the functions that you need to complete.
    //
    // You may add any additional declarations of methods or variables that you need here.
    //

    /**
     * Constructor for the DFSClientNode class
     */
    DFSClientNodeP2();

    /**
     * Destructor for the DFSClientNode class
     */
    ~DFSClientNodeP2();

    /**
     * Request write access to the server
     *
     * @param filename
     * @return bool
     */
    grpc::StatusCode RequestWriteAccess(const std::string& filename) override ;

    /**
     * Store a file from the mount path on to the RPC server
     *
     * @param filename
     * @return grpc::StatusCode
     */
    grpc::StatusCode Store(const std::string& filename) override ;

    /**
     * Fetch a file from the RPC server and put it in the mount path
     *
     * @param filename
     * @return grpc::StatusCode
     */
    grpc::StatusCode Fetch(const std::string& filename) override ;

    /**
     * Delete a file from the RPC server
     *
     * @param filename
     * @return grpc::StatusCode
     */
    grpc::StatusCode Delete(const std::string& filename) override ;

    /**
     * Get or print a list from the RPC server.
     *
     * The `file_map` parameter should be a pointer to a map
     * with the keys as string based file names as the values as
     * the last modified time (mtime) for a file on the server.
     * A correct listing will be tested against this map.
     *
     * The `display` parameter indicates if the command should print
     * the listing to stdout. This is a convenience parameter and won't
     * be tested. You may skip this or implement it however you see fit.
     * For example, you may want to print a listing of the files returned.
     *
     * @param file_map
     * @param display
     * @return grpc::StatusCode
     */
    grpc::StatusCode List(std::map<std::string,int>* file_map = NULL, bool display = false) override;

    /**
     * Get or print the status details for a given filename,
     *
     * You won't be tested on this method, but you will most likely find
     * that you need to implement it in order to communicate file status
     * between the server and the client.
     *
     * The void* file_status parameter is left to you to determine how to use.
     * One suggestion is to use it to fill a protobuf message type with status details
     * that you can then use elsewhere.
     *
     * @param filename
     * @param file_status
     * @return grpc::StatusCode
     */
    grpc::StatusCode Stat(const std::string& filename, void* file_status = NULL) override;

    /**
     * Handle the asynchronous callback list completion queue
     *
     * This method gets called whenever an asynchronous callback
     * message is received from the server.
     *
     * See the STUDENT INSTRUCTION hints for more details.
     */
    void HandleCallbackList();

    /**
     * Initialize the callback list
     */
     void InitCallbackList() override;

    /**
     * Watcher wrapper
     *
     * This method will get called whenever inotify signals
     * a change to a file. The callback parameter is a method
     * that should be called at the time this method is called.
     *
     * However, you may want to consider how the callback can be
     * surrounded to ensure proper coordination with the sync timer thread.
     *
     * See the STUDENT INSTRUCTION hints for more details.
     *
     * @param callback
     *
     */
    void InotifyWatcherCallback(std::function<void()> callback) override;

    //
    // STUDENT INSTRUCTION:
    //
    // You may add any additional declarations of methods or variables that you need here.
    //


};
#endif
