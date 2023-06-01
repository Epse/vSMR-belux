#include "stdafx.h"
#include "HttpHelper.hpp"

//
// HttpHelper Class by Even Rognlien, used with permission
//

std::string HttpHelper::downloadedContents;

// Used for downloading strings from web:
size_t HttpHelper::handle_data(void *ptr, size_t size, size_t nmemb, void *stream) {
	unsigned int numbytes = size*nmemb;
	// The data is not null-terminated, so get the last character, and replace it with '\0'. 
	char lastchar = *((char *)ptr + numbytes - 1);
	*((char *)ptr + numbytes - 1) = '\0';
	downloadedContents.append((char *)ptr);
	downloadedContents.append(1, lastchar);
	*((char *)ptr + numbytes - 1) = lastchar;  // Might not be necessary. 
	return size*nmemb;
}

std::string HttpHelper::downloadStringFromURL(const std::string& url) {
	CURL *curl = curl_easy_init();
	if (!curl)
	{
		lastErrorMessage = "could not initialize curl";

		return "";
	}

	downloadedContents = "";
	// Tell libcurl the URL 
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	// Tell libcurl what function to call when it has data 
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpHelper::handle_data);

	// Do it! 

	const CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		lastErrorMessage = curl_easy_strerror(res);
	}
	else {
		lastErrorMessage = "";
	}

	curl_easy_cleanup(curl);
	return downloadedContents;
}

std::string HttpHelper::getLastErrorMessage() {
	return lastErrorMessage;
}
