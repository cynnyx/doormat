#include <gtest/gtest.h>

#include "../../src/cache/ttl_cleaner.h"

#include <chrono>

using namespace std::chrono;

static const auto epoch = ttl_cleaner<0>::clock::time_point{};

TEST(ttl_cleaner_test, insord)
{
	ttl_cleaner<> t;
	auto now = std::chrono::system_clock::now();
	auto time = now + std::chrono::seconds(10);
	t.insert(1, time);
	t.insert(2, time);
	t.insert(0, time);
	t.lazy_remove_element(2, time);
	t.print(now);
	t.insert(2, time);
	t.insert(2, time+std::chrono::seconds(1));
	t.insert(8, time+std::chrono::seconds(1));

	t.print(now);
}


TEST(ttl_cleaner_test, heap)
{
	ttl_cleaner<1024, 0> t;
	auto now = std::chrono::system_clock::now();
	auto time = now + std::chrono::seconds(10);
	t.insert(1, time); t.print_heap(now);
	t.insert(2, time); t.print_heap(now);
	t.insert(0, time); t.print_heap(now);
	t.insert(2, time+std::chrono::seconds(1));t.print_heap(now);
	t.insert(8, time+std::chrono::seconds(1));t.print_heap(now);
	t.insert(2, time); t.print_heap(now); t.merge();
	t.print(now);
}


TEST(ttl_cleaner_test, merge)
{
	ttl_cleaner<1024, 8> t;
	auto now = std::chrono::system_clock::now();
	auto time = now + std::chrono::seconds(10);
	for(int i = 0; i < 20; i+=2)
	{
		t.insert(i, time); t.print(now);
	}

	//now we insert into the heap!
	for(int i = 1; i < 20; i+=2)
	{
		t.insert(i, time);
	}
	t.print_heap(now);
	t.merge();
	t.print(now);
}


TEST(ttl_cleaner_test, huge_size)
{
	ttl_cleaner<1024*1024, 32> t;
	auto starting_time = std::chrono::system_clock::now();
	int id = std::rand() % 2048;
	int s = std::rand() % 100;
	for(int i = 0; i < 500000; ++i)
	{
		id = id * 9785468 % 7897898;
		s = s * 3456 % 100;
		t.insert(i, starting_time + std::chrono::seconds(s));
	}
	auto merge_begin = std::chrono::system_clock::now();
	t.merge();
	auto merge_end = std::chrono::system_clock::now();
	std::cout << "inserting time: " << ((double) std::chrono::duration_cast<std::chrono::milliseconds>(merge_begin - starting_time).count())/500000 << " ms" << std::endl;
	std::cout << "merging time: " << std::chrono::duration_cast<std::chrono::microseconds>(merge_end - merge_begin).count() << " us" << std::endl;

}


TEST(ttl_cleaner_test, remove_expired)
{
	const auto tp = epoch + hours{300} + minutes{2} + seconds{1} + milliseconds{64};
	std::vector<std::pair<unsigned int, decltype(tp)>> in{
		std::make_pair(1, tp + hours{3}),
		std::make_pair(-13, tp + hours{538}),
		std::make_pair(-7, tp + seconds{1})
	};
	std::vector<std::pair<unsigned int, decltype(tp)>> out{
		std::make_pair(0, tp - minutes{1}),
		std::make_pair(std::numeric_limits<unsigned int>::max(), tp)
	};

	size_t count{};
	ttl_cleaner<> ttl;
	ttl.set_remove_callback([&](const unsigned int id)
	{
		++count;
		EXPECT_TRUE(std::find_if(out.begin(), out.end(), [id](const std::pair<unsigned int, decltype(tp)> p) { return p.first == id; }) != out.end());
	});

	// interleaved insertion
	for(size_t i = 0; i/2 < std::min(in.size(), out.size()); ++i)
	{
		if(i % 2 && i/2 < out.size())
			ttl.insert(out[i/2].first, out[i/2].second);
		else
			ttl.insert(in[i/2].first, in[i/2].second);
	}
	auto& v = in.size() < out.size() ? out : in;
	for(size_t i = std::min(in.size(), out.size()); i < v.size(); ++i)
		ttl.insert(v[i].first, v[i].second);

	ttl.remove_expired(tp);
	EXPECT_EQ(count, out.size());
}
