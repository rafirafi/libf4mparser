
/*
 g++ main.cpp -I../src/f4mparser/ -std=c++11 $(pkg-config --libs --cflags libcurl) -o testProg -l:../src/libf4mparser.so
*/

#include <curl/curl.h>
#include <iostream>
#include <vector>
#include "f4mparser.h"

// example of callback function using curl
std::vector<uint8_t> downloadFile(void *, std::string url, long &status)
{
    std::cerr << "Downloading " << url << std::endl;

    std::vector<uint8_t> response;

    typedef size_t(*CURL_WRITEFUNCTION_PTR)(void*, size_t, size_t, void*);
    auto curlWriteCb = [](void *ptr, size_t size, size_t nmemb, void *stream) -> size_t {
        std::vector<uint8_t> *buffer = static_cast<std::vector<uint8_t> *>(stream);
        buffer->insert(end(*buffer), static_cast<uint8_t *>(ptr), static_cast<uint8_t *>(ptr) + size * nmemb);
        return size*nmemb;
    };

    struct curl_slist *headers = NULL;

    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.5");
    headers = curl_slist_append(headers, "Connection: keep-alive");

    status = -1;

    CURL *process = curl_easy_init();
    if (process) {
        curl_easy_setopt(process, CURLOPT_URL, url.c_str());
        curl_easy_setopt(process, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(process, CURLOPT_HEADER, 0L);
        curl_easy_setopt(process, CURLOPT_AUTOREFERER, 1L);
        curl_easy_setopt(process, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(process, CURLOPT_WRITEFUNCTION, static_cast<CURL_WRITEFUNCTION_PTR>(curlWriteCb));
        curl_easy_setopt(process, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(process, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_perform(process);
        curl_easy_getinfo(process, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_cleanup(process);
    } else {
        std::cerr << "curl_easy_init failed" << std::endl;
    }

    curl_slist_free_all(headers);

    return response;
}

// retrieve bootstrapinfo from manifest
bool bootstrapDataIsAvailable(BootstrapInfo &bootstrapInfo)
{
    if (!bootstrapInfo.data.empty()) {
        return true;
    }

    if (bootstrapInfo.url.empty()) {
        return false;
    }

    // download it
    long status;
    bootstrapInfo.data = downloadFile(NULL, bootstrapInfo.url, status);
    if (status != 200) {
        std::cerr << __func__ << " http status " << status << std::endl;
        if (!bootstrapInfo.data.empty()) {
            // dump http if any
            std::cerr << std::string(&(bootstrapInfo.data)[0], &(bootstrapInfo.data)[0] + (bootstrapInfo.data).size()) << std::endl;
            bootstrapInfo.data.clear();
        }
        return false;
    }
    if (bootstrapInfo.data.empty()) {
        return false;
    }

    return true;
}

int main (int argc, char *argv[])
{
   if (argc != 2) {
        std::cerr << argv[0] << " needs f4m url as argument, p.ex. \"http://example.com/manifest.f4m\"" << std::endl;
        return -1;
    }

    std::string url{argv[1]};

    if (curl_global_init(CURL_GLOBAL_DEFAULT)) {
        std::cerr << "Curl_global_init failed" << std::endl;
        return -1;
    }

    Manifest manifest;
    if (!F4mParseManifest(NULL, downloadFile, url, &manifest)) {
        std::cerr << "Could not get/parse manifest" << std::endl;
    }

    if (!manifest.medias.empty()) {
        std::cerr << std::endl;
        std::cerr << "Available medias : " << std::endl;
        for (auto media : manifest.medias) {
            std::cerr << std::endl;
            std::cerr << "bitrate " << media.bitrate << std::endl;
            std::cerr << "base url " << media.url << std::endl;
            if (bootstrapDataIsAvailable(media.bootstrapInfo)) {
                std::cerr << "bootstrapinfo size " << media.bootstrapInfo.data.size() << std::endl;
            } else {
                std::cerr << "no valid bootstrapinfo" << std::endl;
            }
            if (!media.metadata.empty()) {
                std::cerr << "metadata size" << media.metadata.size() << std::endl;
            } else {
                std::cerr << "no valid metadata" << media.metadata.size() << std::endl;
            }
        }
    } else {
         std::cerr << "Couldn't find valid medias" << std::endl;
    }

    curl_global_cleanup();

    return 0;
}
