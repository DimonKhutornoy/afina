#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <afina/logging/Service.h>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <memory>
#include "protocol/Parser.h"
#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <deque>
#include <spdlog/logger.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <iostream>

namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> &logger_, std::shared_ptr<Afina::Storage> &pStorage_)
        : _socket(s), _logger{logger_}, pStorage{pStorage_} {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        now_pos = 0;
    }

    inline bool isAlive() const { return running.load(); }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;
	friend class Worker;

    int _socket;
    struct epoll_event _event;
    std::atomic<bool> running;

    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;

    std::unique_ptr<Execute::Command> command_to_execute;
    char client_buffer[4096];
    std::deque<std::string> buffer;
    bool _eof{false};
    int now_pos;
	size_t shift;
	size_t N = 64;
	std::mutex _mutex;
	
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
