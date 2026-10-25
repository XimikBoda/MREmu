#include "Network.h"
#include <asio/io_context.hpp>
#include <thread>

struct Network::Impl {
    asio::io_context io_ctx;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
    std::thread asio_thread;

    Impl() : work_guard(asio::make_work_guard(io_ctx)) {}
};

Network* Network::ptr = nullptr;

Network::Network() : pImpl(std::make_unique<Impl>()) {
    if (ptr != nullptr)
        throw std::runtime_error("Network already exists!");
    
    ptr = this;

    pImpl->asio_thread = std::thread([this]() {
        pImpl->io_ctx.run();
        });
}

Network::~Network() {
    stop();
    ptr = nullptr;
}

asio::io_context& Network::get_context() {
    return pImpl->io_ctx;
}

void Network::stop() {
    if (pImpl->asio_thread.joinable()) {
        pImpl->work_guard.reset();
        pImpl->io_ctx.stop();
        pImpl->asio_thread.join();
    }
}

Network* Network::getPtr() {
    return ptr;
}