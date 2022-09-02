#pragma once

#include <grpc_cb/io_context.h>
#include <grpc_cb/io_handler.h>

#include <chrono>

#include <grpcpp/alarm.h>

namespace grpc_cb
{
    class steady_timer
    {
    public:
        steady_timer(io_context& context)
            : context_(context)
        {}

        ~steady_timer()
        {
            cancel();
        }

        template < typename Duration > void expires_from_now(Duration duration)
        {
            // There is both expires_from_now and async_wait to mimic asio contract.
            // yet it requires two member variables which is unfortunate.
            cancel();
            deadline_ = gpr_time_from_nanos(
                std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(),
                GPR_TIMESPAN);
        }

        template< typename WaitHandler > void async_wait(WaitHandler&& handler)
        {
            // Cancels are async, so to support cancel/async_wait/cancel/async_wait,
            // handlers have to be owned by io_context and submitted under different tags.

            // TODO: now how to get rid of the new() here... we need to accomodate for different
            // types of handlers, but most of them will have the same size, so we can just ugly-cast
            // for some common size and cache the underlying memory allocations in stack<>.

            auto tag = context_.make_handler(std::forward< WaitHandler >(handler));
            alarm_.Set(context_, deadline_, tag.get());
            tag.release();
        }

        // TODO: cancel should return if handler was cancelled or not. But there is no way
        // to find out.
        void cancel()
        {
            alarm_.Cancel();
        }

    private:
        io_context& context_;
        grpc::Alarm alarm_;
        gpr_timespec deadline_;
    };
}
