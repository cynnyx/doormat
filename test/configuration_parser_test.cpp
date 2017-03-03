#include "../src/configuration/configuration_parser.h"
#include <gtest/gtest.h>

TEST(configuration_parser, loading_and_parsing)
{
	try
	{
		configuration_parser my_parser("./etc/doormat/doormat.test.config");
		auto cw = my_parser.parse();
	}
	catch (const std::invalid_argument& invarg)
	{
		FAIL() << "invalid argument:" << invarg.what();
	} catch (const std::logic_error &logicerror) {
        FAIL() << "logic error: " << logicerror.what();   
    } catch (...) {
		FAIL() << "an unknown error occurred!";
	}
}
