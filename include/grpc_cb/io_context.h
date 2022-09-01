#pragma once

#include <grpc_cb/io_handler.h>

#include <grpcpp/completion_queue.h>
#include <grpcpp/alarm.h>

namespace grpc_cb
{
    class io_context
    {
    public:
        ~io_context()
        {
            cq_.Shutdown();
            poll();
        }

        operator grpc::CompletionQueue* () { return &cq_; }

        size_t run_one()
        {
            void* tag = 0;
            bool ok = false;
            if (cq_.Next(&tag, &ok))
            {
                auto handler = std::unique_ptr< io_handler_base >(reinterpret_cast<io_handler_base*>(tag));
                handler->process(ok);
                return 1;
            }

            return 0;
        }

        size_t poll_one()
        {
            void* tag = 0;
            bool ok = false;
            const auto status = cq_.AsyncNext(&tag, &ok, gpr_now(GPR_CLOCK_PRECISE));
            if (status == grpc::CompletionQueue::GOT_EVENT)
            {
                auto handler = std::unique_ptr< io_handler_base >(reinterpret_cast<io_handler_base*>(tag));
                handler->process(ok);
                return 1;
            }

            return 0;
        }

        size_t poll()
        {
            size_t count = 0;
            while (poll_one())
                ++count;

            return count;
        }

    private:
        grpc::CompletionQueue cq_;
    };
}
