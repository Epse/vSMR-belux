#pragma once
#include <optional>
#include <string>
#pragma comment(lib, "wininet.lib")

class HttpHelper
{
private:

public:
	static std::optional<std::string> downloadStringFromURL(const std::string& url);
	static DWORD getLastErrorCode();
	static std::string getLastErrorMessage();
};
