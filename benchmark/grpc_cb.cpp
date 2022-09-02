#include <grpc_cb/steady_timer.h>

#include <grpcpp/alarm.h>
#include <grpcpp/completion_queue.h>

#if defined(GRPCCB_ASIO_ENABLED)
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#endif

#include <benchmark/benchmark.h>

static void grpc_alarm(benchmark::State& state)
{
    grpc::CompletionQueue cq;
    grpc::Alarm alarm;

    for (auto _ : state)
    {
        // gpr_now(),
        // gpr_time_0(GPR_TIMESPAN)
        alarm.Set(&cq, gpr_time_from_seconds(0, GPR_TIMESPAN), &alarm);
        void* tag;
        bool ok;
        cq.Next(&tag, &ok);
    }
    
    state.SetItemsProcessed(state.iterations());
}

static void grpc_cb_steady_timer(benchmark::State& state)
{
    grpc_cb::io_context context;
    grpc_cb::steady_timer timer(context);

    for (auto _ : state)
    {
        timer.expires_from_now(std::chrono::seconds(0));
        timer.async_wait([](bool result){});
        context.run_one();
    }

    state.SetItemsProcessed(state.iterations());
}

static void asio_steady_timer(benchmark::State& state)
{
    asio::io_context context;
    asio::steady_timer timer(context);

    volatile int counter = 0;
    for (auto _ : state)
    {
        timer.expires_from_now(std::chrono::seconds(0));
        timer.async_wait([&](const asio::error_code&) { ++counter; });
        context.run_one();
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(grpc_alarm)->UseRealTime();
BENCHMARK(grpc_cb_steady_timer)->UseRealTime();
BENCHMARK(asio_steady_timer)->UseRealTime();
