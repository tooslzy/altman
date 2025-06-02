#include "games_utils.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <cmath>

using namespace std;
using namespace chrono;

string formatPrettyDate(const string &isoTimestampRaw) {
    string isoTimestamp = isoTimestampRaw;
    if (auto dot = isoTimestamp.find('.'); dot != string::npos)
        isoTimestamp = isoTimestamp.substr(0, dot) + 'Z';
    if (auto plus = isoTimestamp.find('+'); plus != string::npos)
        isoTimestamp = isoTimestamp.substr(0, plus) + 'Z';

    tm timeStructUtc{};
    istringstream timestampStream(isoTimestamp);
    timestampStream >> get_time(&timeStructUtc, "%Y-%m-%dT%H:%M:%SZ");
    if (timestampStream.fail())
        return isoTimestampRaw;

    time_t timeUtc;
#if defined(_WIN32)
    timeUtc = _mkgmtime(&timeStructUtc);
#else
	timeUtc = timegm(&timeStructUtc);
#endif
    if (timeUtc == -1)
        return isoTimestampRaw;

    tm timeStructLocal{};
#if defined(_WIN32)
    localtime_s(&timeStructLocal, &timeUtc);
#else
	localtime_r(&timeUtc, &timeStructLocal);
#endif
    char dateBuffer[64];
    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d, %I:%M %p", &timeStructLocal);

    using namespace chrono;
    auto now = system_clock::now();
    auto then = system_clock::from_time_t(timeUtc);
    auto diffTotalSeconds = duration_cast<seconds>(now - then).count();

    if (diffTotalSeconds < 0)
        return string(dateBuffer) + " (in the future)";
    if (diffTotalSeconds < 60)
        return string(dateBuffer) + " (just now)";

    long long years = diffTotalSeconds / (31556952LL);
    diffTotalSeconds %= (31556952LL);
    long long months = diffTotalSeconds / (2629746LL);
    diffTotalSeconds %= (2629746LL);
    long long days = diffTotalSeconds / (86400LL);
    diffTotalSeconds %= (86400LL);
    long long hours = diffTotalSeconds / (3600LL);
    diffTotalSeconds %= (3600LL);
    long long minutes = diffTotalSeconds / (60LL);

    ostringstream relativeTimeStream;
    if (years > 0)
        relativeTimeStream << years << " year" << (years == 1 ? "" : "s");
    else if (months > 0)
        relativeTimeStream << months << " month" << (months == 1 ? "" : "s");
    else if (days > 0)
        relativeTimeStream << days << " day" << (days == 1 ? "" : "s");
    else if (hours > 0)
        relativeTimeStream << hours << " hour" << (hours == 1 ? "" : "s");
    else if (minutes > 0)
        relativeTimeStream << minutes << " minute" << (minutes == 1 ? "" : "s");
    else
        relativeTimeStream << "moments";
    relativeTimeStream << " ago";

    ostringstream outputStream;
    outputStream << dateBuffer << " (" << relativeTimeStream.str() << ')';
    return outputStream.str();
}

string formatWithCommas(long long value) {
    bool isNegative = value < 0;
    unsigned long long absoluteValue = isNegative ? -value : value;
    string numberString = to_string(absoluteValue);
    for (int insertPosition = static_cast<int>(numberString.length()) - 3; insertPosition > 0; insertPosition -= 3)
        numberString.insert(insertPosition, ",");
    return isNegative ? "-" + numberString : numberString;
}
