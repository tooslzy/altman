#include "servers_utils.h"
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <vector>

using namespace std;

namespace
{
	vector<string> loadWordList(const string& path)
	{
		ifstream fin(path);
		if (!fin)
			throw runtime_error("cannot open word-list: " + path);

		vector<string> words;
		string line;
		while (getline(fin, line))
			if (!line.empty())
				words.emplace_back(move(line));

		if (words.empty())
			throw runtime_error("word-list empty: " + path);
		return words;
	}

	const vector<string>& adjectives()
	{
		static vector<string> data = loadWordList("assets/adjectives.txt");
		return data;
	}

	const vector<string>& nouns()
	{
		static vector<string> data = loadWordList("assets/nouns.txt");
		return data;
	}
}

array<uint8_t, 16> parseGuid(const string& guid)
{
	string hexOnly;
	hexOnly.reserve(32);
	for (char c : guid)
		if (isxdigit(static_cast<unsigned char>(c)))
			hexOnly += c;
	if (hexOnly.size() != 32)
		throw runtime_error("invalid GUID: " + guid + " (hex: " + hexOnly + ")");

	array<uint8_t, 16> bytes{};
	try
	{
		for (int i = 0; i < 16; ++i)
			bytes[i] = static_cast<uint8_t>(stoi(hexOnly.substr(i * 2, 2), nullptr, 16));
	}
	catch (const out_of_range& oor)
	{
		throw runtime_error("GUID parsing out of range: " + guid);
	}
	catch (const invalid_argument& ia)
	{
		throw runtime_error("GUID parsing invalid argument: " + guid);
	}
	return bytes;
}

size_t hashBytes(const array<uint8_t, 16>& bytes)
{
	size_t h = 0xcbf29ce484222325ULL;
	for (uint8_t b : bytes)
	{
		h ^= b;
		h *= 0x100000001b3ULL;
	}
	return h;
}

string guidToName(const string& guid)
{
	try
	{
		const auto& adj = adjectives();
		const auto& nou = nouns();
		if (adj.empty() || nou.empty())
			return guid;

		size_t h = hashBytes(parseGuid(guid));
		size_t adjIdx = h % adj.size();
		size_t nounIdx = (h / adj.size()) % nou.size();
		return adj[adjIdx] + '_' + nou[nounIdx];
	}
	catch (const runtime_error&)
	{
		return guid;
	}
}

string toLower(string s)
{
	transform(s.begin(), s.end(), s.begin(),
	          [](unsigned char c)
	          {
		          return static_cast<char>(tolower(c));
	          });
	return s;
}

bool containsCI(const string& haystack, const string& needle)
{
	if (needle.empty())
		return true;
	string h_lower = toLower(haystack);
	string n_lower = toLower(needle);
	return h_lower.find(n_lower) != string::npos;
}
