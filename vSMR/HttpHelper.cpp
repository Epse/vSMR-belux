#include "stdafx.h"
#include "HttpHelper.hpp"
#include <wininet.h>

std::optional<std::string> HttpHelper::downloadStringFromURL(const std::string& url) {
	const HINTERNET internet_handle = InternetOpen("Belux Plugin", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (internet_handle == nullptr)
	{
		return {}; // wininet GetLastError has the actual error, we ignore
	}

	const HINTERNET url_handle = InternetOpenUrl(internet_handle, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, NULL);
	if (url_handle == nullptr)
	{
		return {};
	}

	constexpr size_t buffer_block_size = 5120; // 5kiB, should be enough in most cases as a start
	size_t buffer_size = buffer_block_size;
	char* buffer = new char[buffer_size];
	DWORD bytes_read = 0;

	std::string output;

	// Try to read data from the URL
	do
	{
		if (InternetReadFile(url_handle, buffer, buffer_size, &bytes_read))
		{
			if (bytes_read == 0)
				break;

			output = output + std::string(buffer, bytes_read);
		}
		else
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			{
				// TODO handle error
				break;
			}

			// If we failed due to insufficient buffer, grow buffer and loop
			delete[] buffer;
			buffer_size += buffer_block_size;
			buffer = new char[buffer_size];
		}
	}
	while (true);

	InternetCloseHandle(url_handle);
	InternetCloseHandle(internet_handle);

	return output;
}

DWORD HttpHelper::getLastErrorCode()
{
	return GetLastError();
}

std::string HttpHelper::getLastErrorMessage()
{
	const auto code = getLastErrorCode();

    LPTSTR lpMsgBuf = nullptr;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

	if (lpMsgBuf == nullptr)
	{
		return "";
	}

	return lpMsgBuf;
}
