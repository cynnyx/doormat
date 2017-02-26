#include "../src/configuration/configuration_parser.h"
#include <gtest/gtest.h>

TEST(configuration_parser, asd)
{
	try
	{
		configuration_parser my_parser("./etc/doormat/doormat.test.config");
		auto cw = my_parser.parse();
	}
	catch(...)
	{
		FAIL();
	}
}
