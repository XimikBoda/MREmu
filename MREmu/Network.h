#pragma once
#include <memory>

namespace asio {
    class io_context;
}

class Network {
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    static Network* ptr;
public:
    Network();
    ~Network();

    asio::io_context& get_context();

    void stop();

    static Network* getPtr();
};