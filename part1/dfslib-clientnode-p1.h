#ifndef _DFSLIB_CLIENTNODE_H
#define _DFSLIB_CLIENTNODE_H

#include <map>
#include <string>
#include <vector>
#include <map>
#include <limits.h>
#include <chrono>

#include <grpcpp/grpcpp.h>
#include "src/dfslibx-clientnode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

class DFSClientNodeP1 : public DFSClientNode {

public:
        /**
         *
         * Constructor for the DFSClientNode class
         */
        DFSClientNodeP1();

        /**
         * Destructor for the DFSClientNode class
         */
        ~DFSClientNodeP1();

        /**
         * Store a file from the mount path on to the RPC server
         *
         * @param filename
         * @return grpc::StatusCode
         */
        grpc::StatusCode Store(const std::string& filename) override;

        /**
         * Fetch a file from the RPC server and put it in the mount path
         *
         * @param filename
         * @return grpc::StatusCode
         */
        grpc::StatusCode Fetch(const std::string& filename) override;

        /**
         * Delete a file from the RPC server
         *
         * @param filename
         * @return grpc::StatusCode
         */
        grpc::StatusCode Delete(const std::string& filename) override;

        /**
         * Get or print a list from the RPC server.
         *
         * The `file_map` parameter should be a pointer to a map
         * with the keys as string based file names and the values as
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
        grpc::StatusCode List(std::map<std::string,int>* file_map, bool display = false) override ;

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
        grpc::StatusCode Stat(const std::string& filename, void* file_status = NULL) override ;

        //
        // STUDENT INSTRUCTION:
        //
        // Add your additional declarations here
        //

};
#endif
