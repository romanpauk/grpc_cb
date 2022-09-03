#pragma once

#include <grpc_cb/io_handler.h>

#include <grpcpp/completion_queue.h>
#include <grpcpp/alarm.h>

#include <memory>
#include <deque>
#include <type_traits>

namespace grpc_cb
{
    bool operator == (gpr_timespec lhs, gpr_timespec rhs)
    {
        static_assert(std::is_pod_v< gpr_timespec >);
        return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
    }

    class io_context
    {
    public:
        ~io_context()
        {
            stop();
        }

        operator grpc::CompletionQueue* () { return &cq_; }

        size_t run_one() { return dispatch_handlers(infinite_timespec(), 1); }
        size_t run() { return dispatch_handlers(infinite_timespec(), -1); }

        // TODO: can't get those correct due to race to be usable in tests
        //size_t poll_one() { return dispatch_handlers(zero_timespec(), 1); }
        //size_t poll() { return dispatch_handlers(zero_timespec(), -1); }
        
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
                handlers_alarm_.Set(&cq_, zero_timespec(), &handlers_alarm_);
        }

        void stop()
        {
            handlers_alarm_.Cancel();
            cq_.Shutdown();
            dispatch_handlers(infinite_timespec(), -1);
        }

    private:
        size_t dispatch_handlers(gpr_timespec deadline, size_t limit)
        {
            // There is a race between posting to grpc queue and it being able to react to it.
            // Happens with a timer that is set to be expired, yet immediate AsyncNext will timeout
            // without event. Using limit and deadline to provide behavior that is needed to implement
            // stop(), run_one() and poll_one().
            
            assert(limit != 0);

            void* tag;
            bool ok;
            size_t dispatched = 0;
            bool remaining_handlers = false;
            
            while (true)
            {
                switch (cq_.AsyncNext(&tag, &ok, deadline))
                {
                case grpc::CompletionQueue::GOT_EVENT:
                    if (tag != &handlers_alarm_)
                    {
                        if((dispatched += dispatch_tagged_handler(tag, ok)) == limit)
                            return dispatched;
                    }
                    break;
                case grpc::CompletionQueue::TIMEOUT:
                    return dispatched;
                case grpc::CompletionQueue::SHUTDOWN:
                    return dispatched;
                default:
                    std::abort();
                }

                if (ok)
                {
                    // Drain as many posted handlers as limit allows, for rest, schedule a new timer.
                    // (as scheduling timers is expensive).
                    while (true)
                    {
                        if (dispatch_untagged_handler(remaining_handlers))
                        {
                            if (++dispatched == limit)
                            {
                                if (remaining_handlers)
                                    handlers_alarm_.Set(&cq_, zero_timespec(), &handlers_alarm_);
                                return dispatched;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }

        size_t dispatch_tagged_handler(void* tag, bool ok)
        {
            auto handler = handler_cast(tag);
            handler->process(ok);
            return 1;
        }

        size_t dispatch_untagged_handler(bool& remaining)
        {
            std::function< void() > handler;
            {
                std::lock_guard lock(mutex_);
                if (!handlers_.empty())
                {
                    handler = std::move(handlers_.front());
                    handlers_.pop_front();
                    remaining = !handlers_.empty();
                }
            }

            if (handler)
            {
                handler();
                return 1;
            }
            else
            {
                return 0;
            }
        }

        std::unique_ptr< io_handler_base > handler_cast(void* tag)
        {
            return std::unique_ptr< io_handler_base >(reinterpret_cast<io_handler_base*>(tag));
        }

        gpr_timespec zero_timespec() { return gpr_timespec{ 0, 0, GPR_TIMESPAN }; }
        gpr_timespec infinite_timespec() { return gpr_timespec{ std::numeric_limits< int64_t >::max(), 0, GPR_TIMESPAN}; }

        grpc::CompletionQueue cq_;

        // Members to support post()
        std::mutex mutex_;
        std::deque< std::function< void() > > handlers_;
        grpc::Alarm handlers_alarm_;
    };
}
