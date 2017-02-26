#include <gtest/gtest.h>

#include "../src/utils/reusable_buffer.h"

namespace
{
	static constexpr int rbsize{1000};

	TEST(reusablebuffer, produces)
	{
		auto rb_half = rbsize/2;

		reusable_buffer<rbsize> rb;
		auto buf = rb.reserve();
		EXPECT_EQ( boost::asio::buffer_size(buf), rbsize);

		//Use half
		auto ptr = rb.produce(rb_half);
		EXPECT_TRUE(ptr);

		//Obtain remaing
		buf = rb.reserve();
		EXPECT_EQ( boost::asio::buffer_size(buf), rb_half);

		//Mark it as used
		EXPECT_EQ(ptr+rb_half, rb.produce(rb_half));

		//Attempt to recall any, should return empty
		buf = rb.reserve();
		EXPECT_EQ( boost::asio::buffer_size(buf), 0);

		//Use none
		EXPECT_EQ(ptr, rb.produce(0));
	}

	TEST(reusablebuffer, consumes)
	{
		reusable_buffer<rbsize> rb;
		auto buf = rb.reserve();
		EXPECT_EQ( boost::asio::buffer_size(buf), rbsize);

		//Use all
		auto ptr = rb.produce(rbsize);
		EXPECT_TRUE(ptr);

		//Free all
		auto consumed = rb.consume(ptr+rbsize);
		EXPECT_EQ( consumed, rbsize);
	}

	TEST(reusablebuffer, res_prod_cons)
	{
		size_t used{rbsize};

		//Recall whole buffer
		reusable_buffer<rbsize> rb;
		auto buf = rb.reserve();
		EXPECT_EQ( boost::asio::buffer_size(buf), rbsize);

		//Use half and give back the remaining
		auto ptr = boost::asio::buffer_cast<char*>(buf);
		EXPECT_EQ(ptr, rb.produce(used));

		//Free a portion
		used -= rb.consume(ptr+200);
		EXPECT_EQ(rbsize - used, 200);

		//Use some more
		EXPECT_EQ(ptr, rb.produce(100));
		used += 100;

		//Try to free a portion that's already been freed
		EXPECT_EQ(0, rb.consume(ptr+200));

		//Free some for real
		used -= rb.consume(ptr+400);
		EXPECT_EQ(rbsize - used, 300);

		//Check available mem
		buf = rb.reserve();
		EXPECT_EQ(rbsize - used, boost::asio::buffer_size(buf));

		//Use the remaining
		EXPECT_EQ(ptr+100, rb.produce(rbsize - used));
		used = rbsize;

		//Check nothing's left
		buf = rb.reserve();
		EXPECT_EQ( boost::asio::buffer_size(buf), 0);

		//Free all at once, shouldn't be possible
		//(two parts are not logically contiguous)
		EXPECT_EQ(600, rb.consume(ptr+rbsize));
		EXPECT_EQ(400, rb.consume(ptr+400));

		//Use all
		EXPECT_TRUE(rb.produce(rbsize));

		//Check buf is now full
		buf = rb.reserve();
		EXPECT_EQ( boost::asio::buffer_size(buf), 0);
	}
}
