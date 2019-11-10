#ifndef PR4_DFSCALLDATAMANAGER_H
#define PR4_DFSCALLDATAMANAGER_H

#include <grpcpp/grpcpp.h>
#include "dfs-utils.h"
#include "../proto-src/dfs-service.grpc.pb.h"

/**
 * Virtual class meant to be inherited by the DFSServiceImpl class. It is used
 * solely to abstract certain callback features and make them available in the
 * student server node.
 *
 * @tparam RequestT
 * @tparam ResponseT
 */
template <typename RequestT, typename ResponseT>
class DFSCallDataManager {
public:

    virtual void RequestCallback(grpc::ServerContext* context,
                                 RequestT* request,
                                 grpc::ServerAsyncResponseWriter<ResponseT>* responder,
                                 grpc::ServerCompletionQueue* cq,
                                 void* tag) {}
    virtual void ProcessCallback(grpc::ServerContext* context, RequestT* request, ResponseT* response) {}

};

/**
 * This class handles asynchronous data calls between the client and the server.
 *
 * It was inspired and uses some of the structural elements of the async C++
 * example in the GRPC source. It was changed to suit the purposes of this assignment.
 *
 * @tparam RequestT
 * @tparam ResponseT
 */
template <typename RequestT, typename ResponseT>
class DFSCallData {

private:

    // The means of communication with the gRPC runtime for an asynchronous
    // server.
    dfs_service::DFSService::AsyncService* service;

    DFSCallDataManager<RequestT, ResponseT>* manager;

    // The producer-consumer queue where for asynchronous server notifications.
    grpc::ServerCompletionQueue* cq;

    // Context for the rpc, allowing to tweak aspects of it such as the use
    // of compression, authentication, as well as to send metadata back to the
    // client.
    grpc::ServerContext ctx_;

    // What we get from the client.
    RequestT request_;

    // What we send back to the client.
    ResponseT reply_;

    // The means to get back to the client.
    grpc::ServerAsyncResponseWriter<ResponseT> responder;

    // Let's implement a tiny state machine with the following states.
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status;  // The current serving state.

public:
    // Take in the "service" instance (in this case representing an asynchronous
    // server) and the completion queue "cq" used for asynchronous communication
    // with the gRPC runtime.
    DFSCallData(dfs_service::DFSService::AsyncService* service,
        DFSCallDataManager<RequestT, ResponseT>* manager, grpc::ServerCompletionQueue* cq) :
        service(service), manager(manager), cq(cq), responder(&ctx_), status(CREATE) {

        dfs_log(LL_DEBUG3) << "DFSCallDataManager[constructor]";
        // Invoke the serving logic right away.
        Proceed();

    }

    /**
     * Proceed starts the asynchronous callback process
     */
    void Proceed() {
        if (status == CREATE) {
            // Make this instance progress to the PROCESS state.
            status = PROCESS;

            dfs_log(LL_DEBUG3) << "Proceed[CREATE]";
            // As part of the initial CREATE state, we *request* that the system
            // start processing SayHello requests. In this request, "this" acts are
            // the tag uniquely identifying the request (so that different CallData
            // instances can serve different requests concurrently), in this case
            // the memory address of this CallData instance.

            manager->RequestCallback(&ctx_, &request_, &responder, cq, this);

        } else if (status == PROCESS) {
            dfs_log(LL_DEBUG3) << "Proceed[PROCESS]";
            // Spawn a new CallData instance to serve new clients while we process
            // the one for this CallData. The instance will deallocate itself as
            // part of its FINISH state.
            new DFSCallData<RequestT, ResponseT>(service, manager, cq);

            manager->ProcessCallback(&ctx_, &request_, &reply_);

            // And we are done! Let the gRPC runtime know we've finished, using the
            // memory address of this instance as the uniquely identifying tag for
            // the event.
            status = FINISH;
            responder.Finish(reply_, grpc::Status::OK, this);
        } else {
            dfs_log(LL_DEBUG3) << "Proceed[Finish]";
            // GPR_ASSERT(status == FINISH);
            if (status != FINISH) {
                dfs_log(LL_ERROR) << "HandleAsyncRPC finish status was not correct.";
            }
            // Once in the FINISH state, deallocate ourselves (CallData).
            delete this;
        }
    }
};

#endif //PR4_DFSCALLDATAMANAGER_H
