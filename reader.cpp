#include "reader.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <sstream>
#include <lsl_c.h>
#include <iostream>

template <typename T>
double AsioReaderImpl<T>::read(std::string &buf, uint_least16_t nbytes, uint_least16_t timeout_ms) {
	/*bool readbytes = 0;
	port.async_read_some(boost::asio::mutable_buffer(buf, sizeof(buf)), [&readbytes](const boost::system::error_code &ec, std::size_t bytes){readbytes=bytes;});
	//boost::asio::async_read(port, boost::asio::mutable_buffer(buf), []() {});
	ctx.run_one_for(std::chrono::milliseconds(timeout_ms));
	return readbytes >= minbytes;*/
	return false;
}

template <typename T> double AsioReaderImpl<T>::readLine(std::string& buf, uint_least16_t timeout_ms) {
	boost::asio::read_until(port, buffer, '\n');
	auto ts = lsl_local_clock();
	std::ostringstream ss;
	ss << &buffer;
	buf = ss.str();
	return ts;
}

template class AsioReaderImpl<boost::asio::serial_port>;
template class AsioReaderImpl<boost::asio::ip::tcp::socket>;

NetPort::NetPort(boost::asio::io_context& ctx, uint16_t tcpport): AsioReaderImpl<Socket>(Socket(ctx)) {
	tcp::acceptor acc(ctx, tcp::endpoint(tcp::v4(), tcpport));
	acc.listen(1);
	acc.accept(port, peer_addr);
	std::cout << "Accepted." << peer_addr << std::endl;
}

AsioReader::~AsioReader() noexcept
{

}
