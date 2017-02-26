#include <gtest/gtest.h>

#ifndef DOORMAT_MEASUREMENTS_H
#define DOORMAT_MEASUREMENTS_H

class measurement
{
public:

	measurement(std::string name) : name{name} {};

	template<typename I>
	void put(I&& v)
	{
		++n;
		static_assert(std::is_convertible<I, double>::value, "invalid type");
		double value = static_cast<double>(v);
		double delta = value - current_average;
		current_average += delta/n;
		m2 += delta*(value - current_average);
		value_collection.push_back(value);
	}

	double get_average()
	{
		return current_average;
	}

	double get_stddev()
	{
		return m2/n;
	}

	virtual void push_average() const
	{
		std::cout << "[PERF]" << name << ": " << current_average << std::endl;
	}


protected:
	std::vector<double>(value_collection);
	int n = 0;
	double current_average = 0;
	double m2 = 0;

private:
	std::string name;
};

#endif //DOORMAT_MEASUREMENTS_H
