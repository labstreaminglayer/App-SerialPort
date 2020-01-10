#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/streambuf.hpp>
#include <cstdint>
#include <string>

class AsioReader {
public:
	virtual double read(std::string &buf, uint_least16_t minbytes, uint_least16_t timeout_ms) = 0;
	virtual double readLine(std::string &buf, uint_least16_t timeout_ms) = 0;
	virtual ~AsioReader() noexcept;
};

template <typename T> class AsioReaderImpl : public AsioReader {
protected:
	T port;
	boost::asio::streambuf buffer{16384};
public:
	AsioReaderImpl(T &&port_) : port(std::move(port_)) {}
	AsioReaderImpl(AsioReaderImpl&&)=default;

	double read(std::string &buf, uint_least16_t nbytes, uint_least16_t timeout_ms) override;
	double readLine(std::string &buf, uint_least16_t timeout_ms) override;
};

using SP = boost::asio::serial_port;
class SerialPort : public AsioReaderImpl<SP> {
public:
	SerialPort(boost::asio::io_context &ctx, std::string &&name, int baudrate, int databits,
			   int parity, int stopbits) : AsioReaderImpl<SP>(SP(ctx, name)) {
		port.set_option(SP::baud_rate(baudrate));
		port.set_option(SP::character_size(databits));
		port.set_option(SP::parity(static_cast<SP::parity::type>(parity)));
		port.set_option(SP::stop_bits(static_cast<SP::stop_bits::type>(stopbits)));
	}
};

using boost::asio::ip::tcp;

using Socket = boost::asio::ip::tcp::socket;
class NetPort : public AsioReaderImpl<Socket> {
public:
	tcp::endpoint peer_addr;
	NetPort(boost::asio::io_context &ctx, uint16_t tcpport);
};

extern template class AsioReaderImpl<boost::asio::serial_port>;
extern template class AsioReaderImpl<boost::asio::ip::tcp::socket>;
