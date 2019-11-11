#ifndef PR4_DFS_SERVICE_RUNNER_H
#define PR4_DFS_SERVICE_RUNNER_H

#include <map>
#include <mutex>
#include <shared_mutex>
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
#include <utime.h>

#include "dfs-utils.h"
#include "dfslibx-call-data.h"
#include "../proto-src/dfs-service.grpc.pb.h"

/**
 * The QueueRequest is a container for managing the asynchronous callbacks
 */
template <typename RequestT, typename ResponseT>
struct QueueRequest {
    grpc::ServerContext* context;
    RequestT* request;
    grpc::ServerAsyncResponseWriter<ResponseT>* response;
    grpc::ServerCompletionQueue* cq;
    void* tag;
    bool finished;
    QueueRequest(grpc::ServerContext* context,
                 RequestT* request,
                 grpc::ServerAsyncResponseWriter<ResponseT>* response,
                 grpc::ServerCompletionQueue* cq,
                 void* tag) :
        context(context), request(request), response(response), cq(cq), tag(tag), finished(false) {}
};

/**
 * Static callback for handling asynchronous requests to the service protocol.
 *
 * @tparam RequestT
 * @tparam ResponseT
 * @param service
 * @param manager
 * @param cq
 */
template <typename RequestT, typename ResponseT>
static void HandleAsyncRPC(dfs_service::DFSService::AsyncService* service,
                           DFSCallDataManager<RequestT, ResponseT>* manager,
                           std::shared_ptr<grpc::ServerCompletionQueue> cq) {

    // Spawn a new CallData instance to serve new clients.
    new DFSCallData<RequestT, ResponseT>(service, manager, cq.get());

    void* tag;  // uniquely identifies a request.

    bool ok;

    while (true) {

        // Block waiting to read the next event from the completion queue. The
        // event is uniquely identified by its tag, which in this case is the
        // memory address of a CallData instance.
        // The return value of Next should always be checked. This return value
        // tells us whether there is any kind of event or cq is shutting down.
        // GPR_ASSERT(cq->Next(&tag, &ok));
        // GPR_ASSERT(ok);
        dfs_log(LL_DEBUG3) << "HandleAsyncRPC[Next]";
        if (!cq->Next(&tag, &ok) || !ok) {
            dfs_log(LL_ERROR) << "HandleAsyncRPC failed to get an ok from completion queue. Did the client crash?";
            continue;
        }
        static_cast<DFSCallData<RequestT, ResponseT>*>(tag)->Proceed();
    }
}

/**
 * Static callback for handling synchronous requests to the service protocol.
 *
 * @tparam RequestT
 * @tparam ResponseT
 * @param server
 */
template <typename RequestT, typename ResponseT>
static void HandleSyncRPC(std::shared_ptr<grpc::Server> server) {
    server->Wait();
}

/**
 * The DFSServiceRunner has been abstracted out of the DFSServiceImpl
 * in order to make it easier for students to focus on the specifics of the assignment.
 *
 * This class manages starting the DFSService, processing queued requests, etc.
 *
 * @tparam RequestT
 * @tparam ResponseT
 */
template <typename RequestT, typename ResponseT>
class DFSServiceRunner {

private:

protected:

    /** The server address **/
    std::string server_address;

    /** The number of asynchronous threads to use **/
    int num_async_threads;

    /** The grpc service object **/
    grpc::Service* service;

    /** The server instance **/
    std::shared_ptr<grpc::Server> server;

    /** The completion queue for async calls **/
    std::shared_ptr<grpc::ServerCompletionQueue> completion_queue;

    /** The async service object **/
    dfs_service::DFSService::AsyncService async_service;

    /** Queued requests callback **/
    std::function<void()> queued_requests_callback;
public:

    DFSServiceRunner() {}

    void SetService(grpc::Service* service) {
        this->service = service;
    }

    void SetQueuedRequestsCallback(std::function<void()> queued_requests_callback) {
        this->queued_requests_callback = queued_requests_callback;
    }

    void SetAddress(const std::string& server_address) {
        this->server_address = server_address;
    }

    void SetNumThreads(int num_async_threads) {
        this->num_async_threads = num_async_threads;
    }

    void Shutdown() noexcept {
        this->server->Shutdown();
    }

    /**
     * Run the service
     */
    void Run() {
        grpc::ServerBuilder builder;
        builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(this->service);
        this->completion_queue = builder.AddCompletionQueue();
        this->server = builder.BuildAndStart();
        dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;

        std::vector <std::thread> threads;

        // Send async methods to separate threads
        for (int i = this->num_async_threads; i > 0; i--) {
            std::thread thread_async(HandleAsyncRPC<RequestT, ResponseT>,
                                     &this->async_service,
                                     dynamic_cast<DFSCallDataManager<RequestT, ResponseT> *>(this->service),
                                     this->completion_queue);
            dfs_log(LL_SYSINFO) << "Async thread " << i << " started";
            threads.push_back(std::move(thread_async));
        }

        // Start the synchronous server on a separate thread
        std::thread thread_server(HandleSyncRPC<RequestT, ResponseT>, this->server);
        dfs_log(LL_SYSINFO) << "Server thread " << " started";
        threads.push_back(std::move(thread_server));

        // Start the queue processor
        std::thread thread_queue(queued_requests_callback);
        dfs_log(LL_SYSINFO) << "Queue thread " << " started";
        threads.push_back(std::move(thread_queue));

        for (std::thread &t : threads) {
            if (t.joinable()) { t.join(); }
        }

    }

};

#endif //PR4_DFS_SERVICE_RUNNER_H
