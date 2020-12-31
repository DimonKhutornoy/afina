#include "Connection.h"

#include <iostream>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _logger->debug("Connection on {} socket started", _socket);
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    running = true;
	shift = 0;
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
	if (buffer.size() > N){
		_event.events = ~EPOLLIN;
	}
	std::size_t arg_remains=0;
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

					if (buffer.size() > 0) {
						_event.events |= EPOLLOUT;
					}
					command_to_execute.reset();
					argument_for_command.resize(0);
					parser.Reset();
				}
			} // while end
		}
		if (readed_bytes == 0) {
			_logger->debug("Connection closed");
			running = false;
		} else {
			throw std::runtime_error(std::string(strerror(errno)));
		}
    } catch (std::runtime_error &ex) {
        if (errno != EAGAIN) {
            _logger->error("Failed to read connection on descriptor {}: {}", _socket, ex.what());
			running = false;
        }
	}
}

		
void Connection::DoWrite() {
    _logger->debug("Writing on socket {}", _socket);
	static constexpr size_t max_buffer = 64;
	iovec write_vec[max_buffer];
    size_t write_vec_v = 0;
    try {
		auto it = buffer.begin();
        write_vec[write_vec_v].iov_base = &((*it)[0]) + shift;
		write_vec[write_vec_v].iov_len = it->size() - shift;
        it++;
        write_vec_v++;
		for (; it != buffer.end(); it++) {
            write_vec[write_vec_v].iov_base = &((*it)[0]);
            write_vec[write_vec_v].iov_len = it->size();
            if (++write_vec_v > max_buffer) {
                break;
            }
        }
		int writed = 0;
        if ((writed = writev(_socket, write_vec, write_vec_v)) >= 0) {
			size_t i = 0;
			while (i < write_vec_v && writed >= write_vec[i].iov_len) {

				buffer.pop_front();
				writed -= write_vec[i].iov_len;
				i++;
			}
			shift = writed;
		} else {
			throw std::runtime_error("Failed to send response");
		}
        if (buffer.empty()) {
            _event.events &= ~EPOLLOUT;
        }
		if (buffer.size() <= N){
			_event.events |= EPOLLIN;
		}
    } catch (std::runtime_error &ex) {
        if (errno != EAGAIN) {
            _logger->error("Failed to write connection on descriptor {}: {}", _socket, ex.what());
			running = false;
        }
	}
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
