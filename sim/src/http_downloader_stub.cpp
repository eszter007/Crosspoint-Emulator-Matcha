// Stub implementation of HttpDownloader for emulator (no HTTP in sim).
#include "network/HttpDownloader.h"

bool HttpDownloader::fetchUrl(const std::string& url, std::string& outContent, 
                              const std::string& username, const std::string& password) {
  (void)url;
  (void)outContent;
  (void)username;
  (void)password;
  return false;
}

bool HttpDownloader::fetchUrl(const std::string& url, Stream& stream, 
                              const std::string& username, const std::string& password) {
  (void)url;
  (void)stream;
  (void)username;
  (void)password;
  return false;
}

bool HttpDownloader::fetchUrl(const std::string& url, const DataCallback& onData, 
                              const std::string& username, const std::string& password) {
  (void)url;
  (void)onData;
  (void)username;
  (void)password;
  return false;
}

HttpDownloader::DownloadError HttpDownloader::downloadToFile(const std::string& url,
                                                             const std::string& destPath,
                                                             ProgressCallback progress,
                                                             bool* cancelFlag,
                                                             const std::string& username,
                                                             const std::string& password) {
  (void)url;
  (void)destPath;
  (void)progress;
  (void)cancelFlag;
  (void)username;
  (void)password;
  return HttpDownloader::HTTP_ERROR;
}
