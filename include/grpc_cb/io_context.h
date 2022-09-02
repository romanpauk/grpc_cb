#pragma once

#include <grpc_cb/io_handler.h>

#include <grpcpp/completion_queue.h>
#include <grpcpp/alarm.h>

#include <memory>

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
                auto handler = handler_cast(tag);
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
                auto handler = handler_cast(tag);
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

        template < typename Handler > auto make_handler(Handler&& handler)
        {
            // Return handler as pointer to base class so no casting is required before calling process().
            // TODO: reuse memory for the handlers that are of some reasonable size.
            return std::unique_ptr< io_handler_base >(std::make_unique< io_handler< Handler > >(std::forward< Handler >(handler)));
        }

        // TODO
        template < typename Handler > void post(Handler&& handler);

    private:
        std::unique_ptr< io_handler_base > handler_cast(void* tag)
        {
            return std::unique_ptr< io_handler_base >(reinterpret_cast<io_handler_base*>(tag));
        }

        grpc::CompletionQueue cq_;
    };
}
