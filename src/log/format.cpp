#include "format.h"
#include "../utils/date.h"

#include <ctime>
#include <cstdio>
#include <string>
#include <array>

namespace logging
{
namespace format
{
namespace
{

/**
 * @brief The gmt_offset_t class is an utility to compute
 * at startup the local GMT time zone.
 */
class gmt_offset_t
{
	// i keep the offset in case we have to print the time in localtime
	std::chrono::seconds tm_gmtoff;
	std::string str;
public:
	gmt_offset_t()
		: tm_gmtoff{}
		, str{}
	{
		std::time_t ct = std::time(nullptr);
		auto lct = std::localtime(&ct);
		tm_gmtoff = std::chrono::seconds{lct->tm_gmtoff};

		int m_off = tm_gmtoff.count() / 60;
		int h_off = m_off / 60;
		m_off -= 60 * h_off;

		std::array<char,6> a{};
		a[0] = lct->tm_gmtoff >= 0 ? '+' : '-';
		std::snprintf(a.data() + 1, 3, "%2.2d", h_off);
		std::snprintf(a.data() + 3, 3, "%2.2d", m_off);

		str.assign(a.begin(), a.end());
	}

	 std::chrono::seconds offset() const noexcept
	 {
		 return tm_gmtoff;
	 }

	 const std::string& to_string() const noexcept
	 {
		 return str;
	 }
};

// return a gmt_offset_t singleton
const gmt_offset_t& gmt_offset() noexcept
{
	static gmt_offset_t ret;
	return ret;
}

std::array<char,9> to_string(const date::time_of_day<std::chrono::seconds>& time) noexcept
{
	std::array<char,9> ret;
	std::snprintf(ret.data(), 3, "%2.2d", static_cast<int>(time.hours().count()));
	ret[2] = ':';
	std::snprintf(ret.data() + 3, 3, "%2.2d", static_cast<int>(time.minutes().count()));
	ret[5] = ':';
	std::snprintf(ret.data() + 6, 3, "%2.2d", static_cast<int>(time.seconds().count()));
	ret[8] = '\0';

	return ret;
}

std::array<char,16> to_string(const date::time_of_day<std::chrono::microseconds>& time) noexcept
{
	std::array<char,16> ret;
	std::snprintf(ret.data(), 3, "%2.2d", static_cast<int>(time.hours().count()));
	ret[2] = ':';
	std::snprintf(ret.data() + 3, 3, "%2.2d", static_cast<int>(time.minutes().count()));
	ret[5] = ':';
	std::snprintf(ret.data() + 6, 3, "%2.2d", static_cast<int>(time.seconds().count()));
	ret[8] = '.';
	std::snprintf(ret.data() + 9, 7, "%6.6d", static_cast<int>(time.subseconds().count()));
	ret[15] = '\0';

	return ret;
}

static const std::string inv_s = "xxx";

static const std::string sun_s = "Sun";
static const std::string mon_s = "Mon";
static const std::string tue_s = "Tue";
static const std::string wed_s = "Wed";
static const std::string thu_s = "Thu";
static const std::string fri_s = "Fri";
static const std::string sat_s = "Sat";

inline
const std::string& to_string(const date::weekday& wd) noexcept
{
	switch (static_cast<unsigned>(wd))
	{
	case 0:
		return sun_s;
	case 1:
		return mon_s;
	case 2:
		return tue_s;
	case 3:
		return wed_s;
	case 4:
		return thu_s;
	case 5:
		return fri_s;
	case 6:
		return sat_s;
	default:
		return inv_s;
	}
}

static const std::string jan_s = "Jan";
static const std::string feb_s = "Feb";
static const std::string mar_s = "Mar";
static const std::string apr_s = "Apr";
static const std::string may_s = "May";
static const std::string jun_s = "Jun";
static const std::string jul_s = "Jul";
static const std::string aug_s = "Aug";
static const std::string sep_s = "Sep";
static const std::string oct_s = "Oct";
static const std::string nov_s = "Nov";
static const std::string dec_s = "Dec";

inline
const std::string& to_string(const date::month& m) noexcept
{
	switch (static_cast<unsigned>(m))
	{
	case 1:
		return jan_s;
	case 2:
		return feb_s;
	case 3:
		return mar_s;
	case 4:
		return apr_s;
	case 5:
		return may_s;
	case 6:
		return jun_s;
	case 7:
		return jul_s;
	case 8:
		return aug_s;
	case 9:
		return sep_s;
	case 10:
		return oct_s;
	case 11:
		return nov_s;
	case 12:
		return dec_s;
	default:
		return inv_s;
	}
}

}


std::string apache_access_time(std::chrono::system_clock::time_point t)
{
	using namespace date;

	auto dd = floor<days>(t);
	auto today = year_month_day{dd};
	auto time = make_time(floor<std::chrono::seconds>(t) - dd);

	// 28/Jul/2006:10:27:10 -0300
	std::array<char,27> ret;
	std::snprintf(ret.data(), 3, "%2.2u", static_cast<unsigned>(today.day()));
	ret[2] = '/';
	std::snprintf(ret.data() + 3, 4, "%s", to_string(today.month()).data());
	ret[6] = '/';
	std::snprintf(ret.data() + 7, 5, "%4.4d", static_cast<int>(today.year()));
	ret[11] = ':';
	std::snprintf(ret.data() + 12, 9, "%s", to_string(time).data());
	ret[20] = ' ';
	std::snprintf(ret.data() + 21, 6, "%s", gmt_offset().to_string().data());
	ret.back() = '\0';

	return ret.data();
}


std::string apache_error_time(std::chrono::system_clock::time_point t)
{
	using namespace date;

	auto dd = floor<days>(t);
	auto today = year_month_weekday{dd};
	auto time = make_time(floor<std::chrono::microseconds>(t) - dd);

	// Fri Sep 09 10:42:29.902022 2011
	std::array<char,32> ret;
	std::snprintf(ret.data(), 4, "%s", to_string(today.weekday()).data());
	ret[3] = ' ';
	std::snprintf(ret.data() + 4, 4, "%s", to_string(today.month()).data());
	ret[7] = ' ';
	std::snprintf(ret.data() + 8, 3, "%2.2u", static_cast<unsigned>(year_month_day{today}.day()));
	ret[10] = ' ';
	std::snprintf(ret.data() + 11, 16, "%s", to_string(time).data());
	ret[26] = ' ';
	std::snprintf(ret.data() + 27, 5, "%4.4d", static_cast<int>(today.year()));
	ret.back() = '\0';

	return ret.data();
}


std::string http_header_date(std::chrono::system_clock::time_point t)
{
	using namespace date;

	auto dd = floor<days>(t);
	auto today = year_month_weekday{dd};
	auto time = make_time(floor<std::chrono::seconds>(t) - dd);

	// Fri Sep 09 10:42:29.902022 2011
	std::array<char,30> ret;
	std::snprintf(ret.data(), 4, "%s", to_string(today.weekday()).data());
	ret[3] = ',';
	ret[4] = ' ';
	std::snprintf(ret.data() + 5, 3, "%2.2u", static_cast<unsigned>(year_month_day{today}.day()));
	ret[7] = ' ';
	std::snprintf(ret.data() + 8, 4, "%s", to_string(today.month()).data());
	ret[11] = ' ';
	std::snprintf(ret.data() + 12, 5, "%4.4d", static_cast<int>(today.year()));
	ret[16] = ' ';
	std::snprintf(ret.data() + 17, 9, "%s", to_string(time).data());
	ret[25] = ' ';
	ret[26] = 'G';
	ret[27] = 'M';
	ret[28] = 'T';
	ret.back() = '\0';

	return ret.data();
}

}
}
