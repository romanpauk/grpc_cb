//
// This file is part of smart_ptr project <https://github.com/romanpauk/grpc_cb>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <grpc_cb/steady_timer.h>

#include <gtest/gtest.h>
#include <optional>
#include <thread>

TEST(deadline_timer, dtor_cancel)
{
    grpc_cb::io_context context;
    std::optional< bool > result;
    {
        grpc_cb::steady_timer timer(context);
        timer.expires_from_now(std::chrono::seconds(10));
        timer.async_wait([&](bool res) { result = res; });
    }
    EXPECT_EQ(context.run_one(), 1);
    EXPECT_EQ(result, false);
}

TEST(deadline_timer, async_wait)
{
    grpc_cb::io_context context;
    grpc_cb::steady_timer timer(context);

    std::optional< bool > result = true;
    timer.expires_from_now(std::chrono::seconds(10));
    timer.async_wait([&](bool res) { result = res; });

    // TODO: poll_one is a bit racy, it could return 0 because it would
    // not yet see async_wait().
    //EXPECT_EQ(context.poll_one(), 0);
    timer.cancel();
    EXPECT_EQ(context.run_one(), 1);
    EXPECT_EQ(result, false);

    timer.expires_from_now(std::chrono::seconds(0));
    timer.async_wait([&](bool res) { result = res; });
    EXPECT_EQ(context.run_one(), 1);
    EXPECT_EQ(result, true);
}

TEST(io_context, post)
{
    grpc_cb::io_context context;
    int counter = 0;
    context.post([&]{ ++counter; });
    context.post([&]{ ++counter; });
    EXPECT_EQ(context.run_one(), 1);
    EXPECT_EQ(context.run_one(), 1);
    EXPECT_EQ(counter, 2);
}

TEST(io_context, post_cancel)
{
    struct Dtor
    {
        Dtor(int& dtor): dtor_(dtor) {}
        ~Dtor() { ++dtor_; }
        int& dtor_;
    };

    int dtor = 0;
    int counter = 0;
    {
        grpc_cb::io_context context;

        // Check that handler capture gets freed
        auto d = std::make_shared< Dtor >(dtor); // TODO: should work with unique
        context.post([&, d=std::move(d)] { ++counter; });
        EXPECT_EQ(counter, 0);
        EXPECT_EQ(dtor, 0);
    }

    EXPECT_EQ(counter, 0);
    EXPECT_EQ(dtor, 1);
}

TEST(io_context, stop)
{
    grpc_cb::io_context context;
    std::thread thread([&]{ context.run(); });
    context.stop();
    thread.join();
}
