#ifndef _DFS_CLIENT_H
#define _DFS_CLIENT_H

#include <tuple>
#include <string>
#include <vector>

#include "../dfslib-shared-p1.h"
#include "../dfslib-clientnode-p1.h"

class DFSClient {

protected:

        int deadline_timeout;
        std::string mount_path;
        DFSClientNodeP1 client_node;

public:
        DFSClient();
        ~DFSClient();

        /**
         * Initializes the client node library.
         *
         * @param server_address
         */
        void InitializeClientNode(const std::string& server_address);

        /**
         * Handles the requested command from the user
         *
         * @param command
         * @param working_directory
         * @param filename
         */
        void ProcessCommand(const std::string& command, const std::string& filename);

        /**
         * Sets the mount path on the client node. This is the path
         * where files will be synced/cached with the server.
         *
         * @param path
         */
        void SetMountPath(const std::string& path);

        /**
         * Sets the deadline timeout
         *
         * @param deadline
         */
        void SetDeadlineTimeout(int deadline);

};
#endif
