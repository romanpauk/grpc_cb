#include <grpc_cb/steady_timer.h>

#include <grpcpp/alarm.h>
#include <grpcpp/completion_queue.h>

#if defined(GRPCCB_ASIO_ENABLED)
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#endif

#include <benchmark/benchmark.h>

#define benchmark_assert(...) do { if(!(__VA_ARGS__)) std::abort(); } while(0)

template< bool Cancel > static void grpc_alarm_impl(benchmark::State& state)
{
    grpc::CompletionQueue cq;
    
    for (auto _ : state)
    {
        grpc::Alarm alarm;
        // gpr_now(),
        // gpr_time_0(GPR_TIMESPAN)
        alarm.Set(&cq, gpr_time_from_seconds(0, GPR_TIMESPAN), &alarm);
        if(Cancel)
            alarm.Cancel();
        void* tag;
        bool ok;
        cq.Next(&tag, &ok);
    }
    
    state.SetItemsProcessed(state.iterations());
}

static void grpc_alarm(benchmark::State& state) { grpc_alarm_impl<false>(state); }
static void grpc_alarm_cancel(benchmark::State& state) { grpc_alarm_impl<true>(state); }

template < bool Cancel > static void grpc_cb_steady_timer_impl(benchmark::State& state)
{
    grpc_cb::io_context context;

    volatile int counter = 0;
    for (auto _ : state)
    {
        grpc_cb::steady_timer timer(context);
        timer.expires_from_now(std::chrono::seconds(0));
        timer.async_wait([&](bool result){ ++counter; });
        if(Cancel)
            timer.cancel();
        context.run_one();
    }

    state.SetItemsProcessed(state.iterations());
    benchmark_assert(counter == state.iterations());
}

static void grpc_cb_steady_timer(benchmark::State& state) { grpc_cb_steady_timer_impl<false>(state); }
static void grpc_cb_steady_timer_cancel(benchmark::State& state) { grpc_cb_steady_timer_impl<true>(state); }

static void grpc_cb_post(benchmark::State& state)
{
    grpc_cb::io_context context;

    int counter = 0;
    for (auto _ : state)
    {
        context.post([&] { ++counter; });
        context.run_one();
    }

    state.SetItemsProcessed(state.iterations());
    benchmark_assert(counter == state.iterations());
}

static void grpc_cb_post_thread(benchmark::State& state)
{
    grpc_cb::io_context context;
    std::atomic< bool > stop = false;
    std::thread thread([&]
    {
        while(!stop) context.poll(); // TODO: need run() that will respect work.
    });

    int counter = 0;
    for (auto _ : state)
    {
        context.post([&] { ++counter; });
    }

    stop = true;
    thread.join();

    state.SetItemsProcessed(counter);
}

template< bool Cancel > static void asio_steady_timer_impl(benchmark::State& state)
{
    asio::io_context context;
    
    int counter = 0;
    for (auto _ : state)
    {
        asio::steady_timer timer(context);
        timer.expires_from_now(std::chrono::seconds(0));
        timer.async_wait([&](const asio::error_code&) { ++counter; });
        context.run_one();
        context.reset();
    }

    state.SetItemsProcessed(state.iterations());
    benchmark_assert(counter == state.iterations());
}

static void asio_steady_timer(benchmark::State& state) { asio_steady_timer_impl<false>(state); }
static void asio_steady_timer_cancel(benchmark::State& state) { asio_steady_timer_impl<true>(state); }

static void asio_post(benchmark::State& state)
{
    asio::io_context context;

    int counter = 0;
    for (auto _ : state)
    {
        context.post([&]{ ++counter; });
        context.run_one();
        context.reset();
    }

    state.SetItemsProcessed(state.iterations());
    benchmark_assert(counter == state.iterations());
}

static void asio_post_thread(benchmark::State& state)
{
    asio::io_context context;
    std::thread thread([&]
    {
        asio::io_context::work work(context); // Keep run() running
        context.run();
    });

    volatile int counter = 0;
    for (auto _ : state)
    {
        context.post([&] { ++counter; });
    }

    context.stop();
    thread.join();

    state.SetItemsProcessed(counter);
}

const int PostIterations = 10000000;

BENCHMARK(grpc_alarm)->UseRealTime();
BENCHMARK(grpc_alarm_cancel)->UseRealTime();

BENCHMARK(grpc_cb_steady_timer)->UseRealTime();
BENCHMARK(grpc_cb_steady_timer_cancel)->UseRealTime();

BENCHMARK(grpc_cb_post)->UseRealTime();
BENCHMARK(grpc_cb_post_thread)->UseRealTime()->Iterations(PostIterations);

BENCHMARK(asio_steady_timer)->UseRealTime();
BENCHMARK(asio_steady_timer_cancel)->UseRealTime();

BENCHMARK(asio_post)->UseRealTime();
BENCHMARK(asio_post_thread)->UseRealTime()->Iterations(PostIterations);
