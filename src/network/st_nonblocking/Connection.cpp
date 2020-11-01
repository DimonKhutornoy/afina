#include "Connection.h"

#include <iostream>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _logger->debug("Connection on {} socket started", _socket);
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    _event.data.fd = _socket;
    running = true;
}
// See Connection.h
void Connection::OnError() {
    running = false;
    _logger->error("Error on socket {}", _socket);
}

// See Connection.h
void Connection::OnClose() {
    running = false;
    _logger->debug("Closed connection on socket {}", _socket);
}

// See Connection.h
void Connection::DoRead() {
        std::size_t arg_remains;
        Protocol::Parser parser;
        std::string argument_for_command;
        try {
            int readed_bytes = -1;
            while ((readed_bytes = read(_socket, client_buffer + now_pos, sizeof(client_buffer) - now_pos)) > 0) {
                _logger->debug("Got {} bytes from socket", readed_bytes);
                now_pos += readed_bytes;
            while (now_pos > 0) {
                _logger->debug("Process {} bytes", now_pos);
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(client_buffer, now_pos, parsed)) {
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(client_buffer, client_buffer + parsed, now_pos - parsed);
                        now_pos -= parsed;
                    }
                }

                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", now_pos, arg_remains);
                    std::size_t to_read = std::min(arg_remains, std::size_t(now_pos));
                    argument_for_command.append(client_buffer, to_read);

                    std::memmove(client_buffer, client_buffer + to_read, now_pos - to_read);
                    arg_remains -= to_read;
                    now_pos -= to_read;
                }

                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    result += "\r\n";
                    buffer.push_back(result);

                    if (buffer.size() == 1) {
                        _event.events |= EPOLLOUT;
                    }
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while end
        }

        running = false;
        if (readed_bytes == 0) {
            _logger->debug("Connection closed");
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
    }
}

void Connection::DoWrite() {
    _logger->debug("Writing on socket {}", _socket);
    try {
        while (!buffer.empty()) {
            auto result = buffer.front();
            if (send(_socket, result.data(), result.size(), 0) <= 0) {
                throw std::runtime_error("Failed to send response");
            }
            buffer.pop_front();
        }
        if (buffer.empty()) {
            _event.events &= !EPOLLOUT;
            running = false;
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to write connection on descriptor {}: {}", _socket, ex.what());
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
