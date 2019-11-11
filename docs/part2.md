# Part 2: Completing the Distributed File System (DFS)

Now that you have a working gRPC service, we will turn our focus towards completing a rudimentary DFS. For this assignment, we’ll apply a weakly consistent cache strategy to the RPC calls you already created in Part 1. This is similar to the approach used by the [Andrew File System (AFS)](https://en.wikipedia.org/wiki/Andrew_File_System). To keep things simple, we’ll focus on whole-file caching for the client-side and a simple lock strategy on the server-side to ensure that only one client may write to the server at any given time.

In addition to the synchronous gRPC RPC calls you completed for Part 1, you will also make an asynchronous gRPC call for Part 2. This asynchronous call will be used to allow the server to communicate to connected clients whenever a change is registered in the service.

We've provided the basic structure of the asynchronous infrastructure, but you will need to manage the callbacks, the DFS logic, and any synchronization issues you find necessary.

## Part 2 Goals

For this assignment, your consistency model should adhere to the following expectations:

* **Whole-file caching**. The client should cache whole files on the client-side (i.e., do not concern yourself with partial file caches). Read and write operations should be applied to local files and only update the server when a file has been modified or created on the client.

* **One Creator/Writer per File (i.e., writer locks)**. Clients should request a file write lock from the server before pushing. If they are not able to obtain a lock, the attempt should fail. The server should keep track of which client holds a lock to a particular file, then release that lock after the client has successfully stored a file to the server.

* **Client initiated Deletions**. Deletion requests should come from a client, but be broadcast to other clients attached to the server. In other words, if Client1 deletes a file locally, it should send a deletion request to the server as well. The server should then delete it from its own cache and then notify all clients that the file has been deleted.

* **Synchronized Clients and Server**. For the purposes of this project, you'll need to ensure that the folders for the server and all clients stay in sync. In other words, the server's storage and all of the clients' mount storage directories should be equal.

* **CRC Checksums**. To determine if a file has changed between the server and the client, you may use a CRC checksum function we have provided for you. Any differences in the checksums between the server and the client constitute a change. The CRC function requires a file path and a table that we’ve already set up for you. The return value will be a `uint32_t` value that you can use in your gRPC message types. _CRC32 is not perfect, but for the purposes of this assignment it will suffice._ An example of how to obtain the checksum follows:

```cpp
std::uint32_t crc = file_checksum(filepath, this->crc_table);
```

* **Date based Sequences**. If a file has changed between the server and the client, then the last modified timestamp should win. In other words, if the server has a more recent timestamp, then the client should fetch it from the server. If the client has a more recent timestamp, then the client should store it on the server. If there is no change in the modified time for a file, the client should do nothing. Keep in mind, when storing data, you must first request a write lock as described earlier.

#### Using code from Part 1

> Note that you can copy the code from part1 for your Store, Fetch, Delete, List, and Stat methods to part2. However, please note that you will most likely need to adjust those methods to meet the requirements of the DFS implementation.

## Part 2 Structure

The file structure for Part 2 is similar to Part 1. As with Part 1, you are only responsible for adjusting the `dfslib-*` files in the `part2` directory.

However, in addition to the gRPC calls, the Part 2 infrastructure includes a file watcher and an asynchronous construct for gRPC. This enables the clients to mount a directory and watch for local `inotify` events in its mount. When a file is changed, added, or deleted, it will send a gRPC request to the server.

In addition, each client will make an asynchronous callback request to the server requesting that the server notify the client of changes that the server knows about. The asynchronous infrastructure has been set up for you. However, you will need to fill in the callback functions and enable the synchronization on your own.

Two threads on the client side will run concurrently. You will need to synchronize these threads and their access to the server.

The **watcher thread** uses the `inotify` system commands to monitor client directories for you. We’ve already provided the structural components to manage the events. However, you will need to make the appropriate changes to ensure that file notification events coordinate with the synchronization timer described next. An event callback function is provided in the `dfslib-clientnode-p2.cpp` file to assist you with that; read the `STUDENT INSTRUCTION` comments carefully as they provide some hints on how to manage that process.

The **async thread** connects to the server with an asynchronous gRPC request. The client should wait for these notifications from the server in the `HandleCallbackList` method of your `DFSClientNodeP2` implementation.

When the client receives a notification, it should then fetch, store, delete, or do nothing based on the goals discussed earlier.

The **server** also uses multiple threads to manage gRPC asynchronous callbacks and requests. The `ProcessCallback` method in `dfslib-servernode-p2.cpp` is triggered when a callback request is available. You will need to manage that request and return a listing of current files, along with any additional information you feel is necessary to communicate the changes that occurred.  You may need to adjust your implementation to ensure these threads are synchronizing as well.

### Part 2 Sequence and Class Diagrams

A high-level sequence diagram of the expected interactions for part 2 is available in the [docs/part2-sequence.pdf](part2-sequence.pdf) file of this repository.

A high-level class diagram is avaliable in [docs/part2-class-diagram.jpg](part2-class-diagram.jpg). However, you are only required to understand and work with the student code sections indicated.

## Part 2 Compiling

To compile the source code in Part 2, you may use the Makefile in the root of the repository and run:

```
make part2
```

Or, you may change to the `part2` directory and run `make`.

> For a list of all make commands available, run `make` in the root of the repository.

To run the executables, see the usage instructions in their respective files.

In most cases, you'll start the server with something similar to the following:

```
./bin/dfs-server-p2
```

The client should then mount to the client path. For example:

```
./bin/dfs-client-p2 mount
```

The above command will mount the client to the `mnt/client` directory, then start the watcher and sync timer threads. Changes to the mount path should then synchronize to the server and any other clients that have mounted.

As in Part 1, the client should also continue to accept individual commands, such as fetch, store, list, and stat.

## Part 2 Submitting

To submit part 2, run the following from the root of the repository:

```
python submit.py part2
```

