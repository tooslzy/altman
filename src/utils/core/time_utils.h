#pragma once
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

inline std::string formatRelativeFuture(time_t timestamp) {
	using namespace std::chrono;
	auto now = system_clock::now();
	auto future = system_clock::from_time_t(timestamp);
	auto diff = duration_cast<seconds>(future - now).count();
	if (diff <= 0)
		return "now";

	long long years = diff / 31556952LL;
	diff %= 31556952LL;
	long long months = diff / 2629746LL;
	diff %= 2629746LL;
	long long days = diff / 86400LL;
	diff %= 86400LL;
	long long hours = diff / 3600LL;
	diff %= 3600LL;
	long long minutes = diff / 60LL;
	diff %= 60LL;
	long long seconds = diff;

	std::ostringstream ss;
	if (years > 0)
		ss << years << " year" << (years == 1 ? "" : "s");
	else if (months > 0)
		ss << months << " month" << (months == 1 ? "" : "s");
	else if (days > 0)
		ss << days << " day" << (days == 1 ? "" : "s");
	else if (hours > 0)
		ss << hours << " hour" << (hours == 1 ? "" : "s");
	else if (minutes > 0)
		ss << minutes << " minute" << (minutes == 1 ? "" : "s");
	else
		ss << seconds << " second" << (seconds == 1 ? "" : "s");
	return ss.str();
}

inline std::string formatCountdown(time_t timestamp) {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto target = system_clock::from_time_t(timestamp);
    long long diff = duration_cast<seconds>(target - now).count();

    if (diff <= 0) return "now";

    long long days = diff / 86400;
    diff %= 86400;

    long long hours = diff / 3600;
    diff %= 3600;

    long long minutes = diff / 60;
    long long seconds = diff % 60;

    std::ostringstream ss;

    // if the difference is exactly n days w/ no remainder, return only "x day(s)"
    if (days > 0 && hours == 0 && minutes == 0 && seconds == 0) {
        ss << days << (days == 1 ? " day" : " days");
        return ss.str();
    }

    if (days > 0) {
        ss << days << (days == 1 ? " day " : " days ");
    }

    ss << std::setfill('0');

    // if hours > 0, include the hours part else, skip it (regardless of days)
    if (hours > 0) {
        // hours w/o & minutes w/ leading zeros
        ss << hours << ":" << std::setw(2) << minutes;
    } else {
        // minutes w/o leading zeros
        ss << minutes;
    }

    // seconds always w/ leading zeros since minutes should always exist
    ss << ":" << std::setw(2) << seconds;

    return ss.str();
}

inline time_t parseIsoTimestamp(const std::string &isoRaw) {
        std::string iso = isoRaw;
        if (auto dot = iso.find('.'); dot != std::string::npos)
                iso = iso.substr(0, dot) + 'Z';
        if (auto plus = iso.find('+'); plus != std::string::npos)
                iso = iso.substr(0, plus) + 'Z';
        std::tm tm{};
        std::istringstream ss(iso);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        if (ss.fail())
                return 0;
#if defined(_WIN32)
        return _mkgmtime(&tm);
#else
        return timegm(&tm);
#endif
}
