//
// This file is part of smart_ptr project <https://github.com/romanpauk/grpc_cb>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <gtest/gtest.h>
#include <optional>

#include <grpc_cb/deadline_timer.h>

TEST(deadline_timer, dtor_cancel)
{
    grpc_cb::io_context context;
    std::optional< bool > result;
    {
        grpc_cb::deadline_timer timer(context);
        timer.expires_from_now(std::chrono::seconds(10));
        timer.async_wait([&](bool res) { result = res; });
    }
    EXPECT_EQ(context.poll_one(), 1);
    EXPECT_EQ(result, false);
}

TEST(deadline_timer, async_wait)
{
    grpc_cb::io_context context;
    grpc_cb::deadline_timer timer(context);

    std::optional< bool > result = true;
    timer.expires_from_now(std::chrono::seconds(10));
    timer.async_wait([&](bool res) { result = res; });
    
    EXPECT_EQ(context.poll_one(), 0);
    timer.cancel();
    EXPECT_EQ(context.poll_one(), 1);
    EXPECT_EQ(result, false);

    timer.expires_from_now(std::chrono::seconds(0));
    timer.async_wait([&](bool res) { result = res; });

    // TODO: does not work with poll_one(). Yet queuing of expired handler makes it ready by definition (in asio, it does)
    EXPECT_EQ(context.run_one(), 1);
    EXPECT_EQ(result, true);
}
