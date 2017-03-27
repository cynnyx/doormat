#include <gtest/gtest.h>

#include <string>
#include <chrono>
#include <fstream>
#include <algorithm>

#include "cynnypp/async_fs.hpp"

#include "../src/log/log.h"
#include "../src/log/format.h"
#include "../src/utils/date.h"

using days = std::chrono::duration
	<int, std::ratio_multiply<std::ratio<24>, std::chrono::hours::period>>;

using weeks = std::chrono::duration
	<int, std::ratio_multiply<std::ratio<7>, days::period>>;

using years = std::chrono::duration
	<int, std::ratio_multiply<std::ratio<146097, 400>, days::period>>;

using namespace date;
using namespace logging::format;

using std::chrono::hours;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::microseconds;
namespace fs = cynny::cynnypp::filesystem;

namespace
{

// I've been forced to implement this factory method due to a bug in gcc 6.2.1 (only under Arch!!!)
// aggregate initialization was not working on access_record with -std=c++11 flag (working with c++14)
logging::access_record make_access_record(dstring remote_addr,
										  dstring remote_user,
										  dstring req,
										  uint16_t status,
										  size_t body_bytes_sent,
										  dstring referer,
										  dstring user_agent,
										  logging::access_record::clock::time_point start,
										  logging::access_record::clock::time_point end,
										  bool pipe,
										  size_t req_length,
										  dstring x_debug_cyn_ip,
										  dstring host)
{
	logging::access_record ret;
	ret.remote_addr = remote_addr;
	ret.remote_user = remote_user;
	ret.req = req;
	ret.status = status;
	ret.body_bytes_sent = body_bytes_sent;
	ret.referer = referer;
	ret.user_agent = user_agent;
	ret.start = start;
	ret.end = end;
	ret.pipe = pipe;
	ret.req_length = req_length;
	ret.x_debug_cyn_ip = x_debug_cyn_ip;
	ret.host = host;
	return ret;
}

}


std::chrono::system_clock::time_point make_time_point(int y, unsigned m, unsigned d, unsigned hh = 0, unsigned mm = 0, unsigned ss = 0, unsigned us = 0)
{
	year_month_day date{year{y}, month{m}, day{d}};
	std::chrono::microseconds time = hours{hh} + minutes{mm} + seconds{ss} + microseconds{us};
	return sys_days(date) + time;
}

static const auto time1 = make_time_point(1987, 12, 16,  1, 46, 23, 12345);
static const auto time2 = make_time_point(2005, 12, 16,  1, 54, 34, 323423);
static const auto time3 = make_time_point(2005, 12, 16,  2, 25, 55, 993939);
static const auto time4 = make_time_point(2005, 12, 19, 23, 02, 01, 999999);
static const auto time5 = make_time_point(2016,  5, 26, 13, 28, 30, 636624);
static const auto time6 = make_time_point(2017, 12, 31, 23, 26, 21, 9939);
static const auto time7 = make_time_point(2020,  2, 29,  1,  1,  1, 111);
static const auto time8 = make_time_point(2027,  7,  1, 23, 59, 59, 999999);
static const auto time9 = make_time_point(2034,  1,  1,  0,  0,  0, 0);
static const std::string acc1 = "16/Dec/1987:01:46:23 +0200";
static const std::string acc2 = "16/Dec/2005:01:54:34 +0200";
static const std::string acc3 = "16/Dec/2005:02:25:55 +0200";
static const std::string acc4 = "19/Dec/2005:23:02:01 +0200";
static const std::string acc5 = "26/May/2016:13:28:30 +0200";
static const std::string acc6 = "31/Dec/2017:23:26:21 +0200";
static const std::string acc7 = "29/Feb/2020:01:01:01 +0200";
static const std::string acc8 = "01/Jul/2027:23:59:59 +0200";
static const std::string acc9 = "01/Jan/2034:00:00:00 +0200";
static const std::string err1 = "Wed Dec 16 01:46:23.012345 1987";
static const std::string err2 = "Fri Dec 16 01:54:34.323423 2005";
static const std::string err3 = "Fri Dec 16 02:25:55.993939 2005";
static const std::string err4 = "Mon Dec 19 23:02:01.999999 2005";
static const std::string err5 = "Thu May 26 13:28:30.636624 2016";
static const std::string err6 = "Sun Dec 31 23:26:21.009939 2017";
static const std::string err7 = "Sat Feb 29 01:01:01.000111 2020";
static const std::string err8 = "Thu Jul 01 23:59:59.999999 2027";
static const std::string err9 = "Sun Jan 01 00:00:00.000000 2034";
static const std::string http1 = "Wed, 16 Dec 1987 01:46:23 GMT";
static const std::string http2 = "Fri, 16 Dec 2005 01:54:34 GMT";
static const std::string http3 = "Fri, 16 Dec 2005 02:25:55 GMT";
static const std::string http4 = "Mon, 19 Dec 2005 23:02:01 GMT";
static const std::string http5 = "Thu, 26 May 2016 13:28:30 GMT";
static const std::string http6 = "Sun, 31 Dec 2017 23:26:21 GMT";
static const std::string http7 = "Sat, 29 Feb 2020 01:01:01 GMT";
static const std::string http8 = "Thu, 01 Jul 2027 23:59:59 GMT";
static const std::string http9 = "Sun, 01 Jan 2034 00:00:00 GMT";


TEST(DateFormat, AccessTime)
{
	EXPECT_EQ(acc1, apache_access_time(time1));
	EXPECT_EQ(acc2, apache_access_time(time2));
	EXPECT_EQ(acc3, apache_access_time(time3));
	EXPECT_EQ(acc4, apache_access_time(time4));
	EXPECT_EQ(acc5, apache_access_time(time5));
	EXPECT_EQ(acc6, apache_access_time(time6));
	EXPECT_EQ(acc7, apache_access_time(time7));
	EXPECT_EQ(acc8, apache_access_time(time8));
	EXPECT_EQ(acc9, apache_access_time(time9));
}

TEST(DateFormat, ErrorTime)
{
	EXPECT_EQ(err1, apache_error_time(time1));
	EXPECT_EQ(err2, apache_error_time(time2));
	EXPECT_EQ(err3, apache_error_time(time3));
	EXPECT_EQ(err4, apache_error_time(time4));
	EXPECT_EQ(err5, apache_error_time(time5));
	EXPECT_EQ(err6, apache_error_time(time6));
	EXPECT_EQ(err7, apache_error_time(time7));
	EXPECT_EQ(err8, apache_error_time(time8));
	EXPECT_EQ(err9, apache_error_time(time9));
}

TEST(DateFormat, HttpDate)
{
	EXPECT_EQ(http1, http_header_date(time1));
	EXPECT_EQ(http2, http_header_date(time2));
	EXPECT_EQ(http3, http_header_date(time3));
	EXPECT_EQ(http4, http_header_date(time4));
	EXPECT_EQ(http5, http_header_date(time5));
	EXPECT_EQ(http6, http_header_date(time6));
	EXPECT_EQ(http7, http_header_date(time7));
	EXPECT_EQ(http8, http_header_date(time8));
	EXPECT_EQ(http9, http_header_date(time9));
}


// access
static const std::string al1 = R"out(192.168.2.20 - - [28/Jul/2006:10:27:10 -0300] "GET /cgi-bin/try/ HTTP/1.0" 200 3395 "-" "okhttp/3.4.1" 0.401 . 777 "2a01:84a0:1001:a001::1f:4" "cy1.morphcast.com")out";
static const std::string al2 = R"out(127.0.0.1 - - [28/Jul/2006:10:22:04 -0300] "GET / HTTP/1.0" 200 2216 "-" "okhttp/3.4.1" 0.477 . 710 "2a01:84a0:1001:a001::36:2" "cy1.morphcast.com")out";
static const std::string al3 = R"out(127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326 "https://www.linkedin.com/" "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/53.0.2785.116 Safari/537.36" 0.000 . 428 "-" "www.cynny.com")out";
static const std::string al4 = R"out(127.0.0.1 - - [28/Jul/2006:10:27:32 -0300] "GET /hidden/ HTTP/1.0" 404 7218 "-" "internetVista monitor (Mozilla compatible)" 0.000 p 155 "-" "ww1.cynny.video")out";

static const logging::access_record ar1 = make_access_record(
			"192.168.2.20",
			"",
			"GET /cgi-bin/try/ HTTP/1.0",
			200,
			3395,
			"",
			"okhttp/3.4.1",
			logging::access_record::clock::time_point{},
			logging::access_record::clock::time_point{} + std::chrono::milliseconds{401},
			false,
			777,
			"2a01:84a0:1001:a001::1f:4",
			"cy1.morphcast.com"
			);

static const logging::access_record ar2 = make_access_record(
			"127.0.0.1",
			"",
			"GET / HTTP/1.0",
			200,
			2216,
			"",
			"okhttp/3.4.1",
			logging::access_record::clock::time_point{},
			logging::access_record::clock::time_point{} + std::chrono::milliseconds{477},
			false,
			710,
			"2a01:84a0:1001:a001::36:2",
			"cy1.morphcast.com"
			);

static const logging::access_record ar3 = make_access_record(
			"127.0.0.1",
			"frank",
			"GET /apache_pb.gif HTTP/1.0",
			200,
			2326,
			"https://www.linkedin.com/",
			"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/53.0.2785.116 Safari/537.36",
			logging::access_record::clock::time_point{},
			logging::access_record::clock::time_point{},
			false,
			428,
			"",
			"www.cynny.com"
			);

static const logging::access_record ar4 = make_access_record(
			"127.0.0.1",
			"",
			"GET /hidden/ HTTP/1.0",
			404,
			7218,
			"",
			"internetVista monitor (Mozilla compatible)",
			logging::access_record::clock::time_point{},
			logging::access_record::clock::time_point{},
			true,
			155,
			"",
			"ww1.cynny.video"
			);

static const std::vector<std::string> al_v = {al1, al2, al3, al4};
static const std::vector<size_t> al_beg_date = {18, 15, 19, 15};

// error
static const std::string el1  = "[Fri Dec 16 01:46:23.012345 2005] [trace7] [pid 35708:tid 4328636416] [client 1.2.3.4] Directory index forbidden by rule: /home/test/";
static const std::string el2  = "[Fri Dec 16 01:54:34.012345 2005] [trace6] [pid 35708:tid 4328636416] [client 1.2.3.5] Directory index forbidden by rule: /apache/web-data/test2";
static const std::string el3  = "[Fri Dec 16 02:25:55.012345 2005] [trace5] [pid 35708:tid 4328636416] [client 1.2.3.6] Client sent malformed Host header";
static const std::string el4  = "[Mon Dec 19 23:02:01.012345 2005] [trace4] [pid 35708:tid 4328636416] [client 1.2.3.7] user test: authentication failure for \"/~dcid/test1\": Password Mismatch";
static const std::string el5  = "[Sat Aug 12 04:05:51.012345 2006] [trace3] [pid 35708:tid 4328636416] Apache/1.3.11 (Unix) mod_perl/1.21 configured -- resuming normal operations";
static const std::string el6  = "[Thu Jun 22 14:20:55.012345 2006] [trace2] [pid 35708:tid 4328636416] Digest: generating secret for digest authentication ...";
static const std::string el7  = "[Thu Jun 22 14:20:55.012345 2006] [trace1] [pid 35708:tid 4328636416] Digest: done";
static const std::string el8  = "[Thu Jun 22 14:20:55.012345 2006] [debug] [pid 35708:tid 4328636416] Apache/2.0.46 (Red Hat) DAV/2 configured -- resuming normal operations";
static const std::string el9  = "[Sat Aug 12 04:05:49.012345 2006] [info] [pid 35708:tid 4328636416] SIGHUP received.  Attempting to restart";
static const std::string el10 = "[Sat Aug 12 04:05:51.012345 2006] [notice] [pid 35708:tid 4328636416] suEXEC mechanism enabled (wrapper: /usr/local/apache/sbin/suexec)";
static const std::string el11 = "[Sat Jun 24 09:06:22.012345 2006] [warn] [pid 35708:tid 4328636416] pid file /opt/CA/BrightStorARCserve/httpd/logs/httpd.pid overwritten -- Unclean shutdown of previous Apache run?";
static const std::string el12 = "[Sat Jun 24 09:06:23.012345 2006] [error] [pid 35708:tid 4328636416] Apache/2.0.46 (Red Hat) DAV/2 configured -- resuming normal operations";
static const std::string el13 = "[Sat Jun 24 09:06:22.012345 2006] [crit] [pid 35708:tid 4328636416] Digest: generating secret for digest authentication ...";
static const std::string el14 = "[Sat Jun 24 09:06:22.012345 2006] [alert] [pid 35708:tid 4328636416] Digest: done";
static const std::string el15 = "[Thu Jun 22 11:35:48.012345 2006] [emerg] [pid 35708:tid 4328636416] caught SIGTERM, shutting down";

static const std::vector<std::string> el_v = {el1, el2, el3, el4, el5, el6, el7, el8, el9, el10, el11, el12, el13, el14, el15};
static const std::vector<size_t> el_len = {65, 76, 52, 90, 77, 57, 14, 71, 39, 67, 112, 71, 55, 13, 30};

TEST(Log, Access)
{
	const std::string dir_name = "dummy_access";
	const std::string filename = dir_name + ".log";
	{
		using std::chrono::milliseconds;

		logging::access_log al(".", dir_name);
		al.log(ar1);
		al.log(ar2);
		al.log(ar3);
		al.log(ar4);
	}

	auto path = dir_name + "_old/" + filename;
	ASSERT_TRUE(fs::exists(path));

	std::ifstream log_file(path);
	std::string line;
	size_t i{};
	for(; i < al_v.size(); ++i)
	{
		EXPECT_TRUE(std::getline(log_file, line));
		EXPECT_TRUE(std::equal(al_v[i].begin(), al_v[i].begin() + al_beg_date[i], line.begin()));
		EXPECT_TRUE(std::equal(al_v[i].begin() + al_beg_date[i] + 26, al_v[i].end(), line.begin() + al_beg_date[i] + 26))
				<< "it should be: " << std::string(al_v[i].begin() + al_beg_date[i] + 26, al_v[i].end())
				<< "\nbut it is: " << std::string(line.begin() + al_beg_date[i] + 26, line.end());
	}
	EXPECT_EQ(i, al_v.size());

	fs::removeDirectory(dir_name + "_old");
}
