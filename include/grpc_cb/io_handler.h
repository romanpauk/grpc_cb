#pragma once

namespace grpc_cb
{
    class io_handler_base
    {
    public:
        virtual ~io_handler_base() {}
        virtual void process(bool) = 0;
    };

    template< typename Handler > class io_handler
        : public io_handler_base
    {
    public:
        template < typename HandlerT > io_handler(HandlerT&& handler) : handler_(std::forward< HandlerT >(handler)) {}
        virtual void process(bool result) override { handler_(result); }

    private:
        Handler handler_;
    };
}
