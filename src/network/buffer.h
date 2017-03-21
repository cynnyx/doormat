#ifndef BUFFER_H
#define BUFFER_H

#include "communicator.h"
#include "../utils/reusable_buffer.h"
#include "../utils/dstring.h"

namespace network
{

template<class T = boost::asio::ip::tcp::socket>
class buffer final
{
	std::unique_ptr<communicator<T>> _communicator{nullptr};
	dstring temporary;
public:
	buffer( std::unique_ptr<communicator<T>> comm ) noexcept: _communicator{ comm } {}
	buffer() noexcept = default;
	void set_communicator( std::unique_ptr<communicator<T>>&& comm ) noexcept
	{
		assert(!_communicator);
		_communicator = std::move(comm);
		_communicator->start();
		if ( temporary.size() )
			_communicator->write( std::move(temporary) );
	}
	void write( dstring&& segment ) noexcept
	{
		if ( _communicator )
			_communicator->write( std::move( segment ) );
		else
			temporary.append( std::move( segment ) );
	}
	~buffer()
	{
		assert( _communicator );
		_communicator->stop();
	}
};


}
#endif // BUFFER_H
