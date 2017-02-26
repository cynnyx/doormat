#include <gtest/gtest.h>
#include "../../src/cache/translator/translation_unit.h"
#include "../../src/cache/cache_element.h"

TEST(translation_unit_test, insert)
{

	translation_unit<std::string, cache_element, std::hash<std::string>> t{};
	std::string insert_key = "ciaone";
	auto time = std::chrono::system_clock::now();
	auto id = t.insert(insert_key, "nada", time, time, 100, 100, "");
	ASSERT_TRUE(id >= 0);
}
