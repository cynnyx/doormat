#include "../src/utils/log_wrapper.h"

#include <gtest/gtest.h>

class DoormatEnv : public testing::Environment
{
public:
	void SetUp() override
	{
		log_wrapper::init(true);
	}

	void TearDown() override
	{
		boost::log::core::get()->remove_all_sinks();
	}
};

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	testing::AddGlobalTestEnvironment(new DoormatEnv());
	return RUN_ALL_TESTS();
}
