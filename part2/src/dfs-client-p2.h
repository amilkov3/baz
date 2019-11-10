#ifndef _DFS_CLIENT_H
#define _DFS_CLIENT_H

#include <tuple>
#include <string>
#include <vector>

#include "../dfslib-shared-p2.h"
#include "../dfslib-clientnode-p2.h"

class DFSClient {

    protected:

        // The deadline timeout in milliseconds
        int deadline_timeout;

        // The mount path
        std::string mount_path;

        // The inotify callback method
        InotifyCallback callback;

        // The current client node
        DFSClientNodeP2 client_node;

        // The inotify events
        std::vector<NotifyStruct> events;

        // The sync thread
        std::thread thread_async;

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
         * Sets the deadline timeout in milliseconds
         *
         * @param deadline
         */
        void SetDeadlineTimeout(int deadline);

        /**
         * Mounts the client to the specified file path.
         *
         * Be default, it is mounted to the mnt/client path within the PR4 repo.
         *
         * @param filepath
         */
        void Mount(const std::string& filepath);

        /**
         * Unmounts the client from the path it was mounted on previously
         */
        void Unmount();

        /**
         * Handle the iNotify events
         *
         * @param event_type
         * @param filename
         * @param instance
         */
        static void InotifyEventCallback(uint event_type, const std::string& filename, void* instance);

        /**
         * Handle watch events from iNotify
         *
         * @param callback
         * @param event_type
         * @param fd
         * @param node
         */
        static void InotifyWatcher(InotifyCallback callback,
                                   uint event_type,
                                   FileDescriptor fd,
                                   DFSClientNode* node);

};
#endif
