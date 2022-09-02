#pragma once

#include <grpc_cb/io_handler.h>

#include <grpcpp/completion_queue.h>
#include <grpcpp/alarm.h>

#include <memory>
#include <deque>

namespace grpc_cb
{
    class io_context
    {
    public:
        ~io_context()
        {
            handlers_alarm_.Cancel();
            cq_.Shutdown();

            while (true)
            {
                void* tag;
                bool ok;
                const auto status = cq_.AsyncNext(&tag, &ok, gpr_now(GPR_CLOCK_PRECISE));
                if (status == grpc::CompletionQueue::GOT_EVENT)
                    dispatch(tag, ok);
                else if(status == grpc::CompletionQueue::SHUTDOWN)
                    break;
            }
        }

        operator grpc::CompletionQueue* () { return &cq_; }

        size_t run_one()
        {
            void* tag = 0;
            bool ok = false;
            if (cq_.Next(&tag, &ok))
                return dispatch(tag, ok);

            return 0;
        }
        
        size_t poll_one()
        {
            void* tag = 0;
            bool ok = false;
            const auto status = cq_.AsyncNext(&tag, &ok, gpr_now(GPR_CLOCK_PRECISE));
            if (status == grpc::CompletionQueue::GOT_EVENT)
                return dispatch(tag, ok);

            return 0;
        }

        size_t poll()
        {
            size_t count = 0;
            while (size_t n = poll_one())
                count += n;

            return count;
        }

        template < typename Handler > auto make_handler(Handler&& handler)
        {
            // Return handler as pointer to base class so no casting is required before calling process().
            // TODO: reuse memory for the handlers that are of some reasonable size.
            return std::unique_ptr< io_handler_base >(std::make_unique< io_handler< Handler > >(std::forward< Handler >(handler)));
        }

        template< typename Handler > void post(Handler&& handler)
        {
            bool empty = true;
            {
                std::lock_guard lock(mutex_);
                empty = handlers_.empty();
                handlers_.emplace_back(std::forward< Handler >(handler));
            }
            if (empty)
                handlers_alarm_.Set(&cq_, gpr_now(GPR_CLOCK_PRECISE), &handlers_alarm_);
        }

    private:
        std::unique_ptr< io_handler_base > handler_cast(void* tag)
        {
            return std::unique_ptr< io_handler_base >(reinterpret_cast<io_handler_base*>(tag));
        }

        size_t dispatch(void* tag, bool ok)
        {
            if (tag == &handlers_alarm_)
            {
                if (ok)
                {
                    std::deque< std::function< void() > > handlers;
                    {
                        std::lock_guard lock(mutex_);
                        handlers = std::move(handlers_);
                    }

                    for (auto& handler : handlers)
                        handler();

                    return handlers.size();
                }
            }
            else
            {
                auto handler = handler_cast(tag);
                handler->process(ok);
                return 1;
            }

            return 0;
        }

        grpc::CompletionQueue cq_;

        // Members to support post()
        std::mutex mutex_;
        std::deque< std::function< void() > > handlers_;
        grpc::Alarm handlers_alarm_;
    };
}
