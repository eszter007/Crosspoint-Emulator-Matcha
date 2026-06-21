#pragma once

// Sim stub for device hal/HalStorage.h. On device this is the storage HAL;
// in the emulator we forward to SDCardManager so the same app code compiles.

#include <cstdio>
#include <cstring>

#include "SDCardManager.h"
#include "SdFat.h"
#include "WString.h"

class HalFile : public Print {
 public:
  HalFile() : file_() {}
  explicit HalFile(FsFile&& file) : file_(std::move(file)) {}
  ~HalFile() { close(); }

  HalFile(HalFile&& other) noexcept : file_(std::move(other.file_)) {}
  HalFile& operator=(HalFile&& other) noexcept {
   if (this != &other) {
     close();
     file_ = std::move(other.file_);
   }
   return *this;
  }

  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  void flush() override {}  // FsFile has no sync method; flush is a no-op in the stub
  size_t getName(char* name, size_t len) { return file_.getName(name, len); }
  size_t size() { return file_.size(); }
  size_t fileSize() { return size(); }
  uint64_t fileSize64() { return size(); }
  bool seek(size_t pos) { return file_.seek(pos); }
  bool seek64(uint64_t pos) { return seek(static_cast<size_t>(pos)); }
  bool seekCur(int64_t offset) { return file_.seekCur(offset); }
  bool seekSet(size_t offset) { return seek(offset); }
  int available() { return file_.available(); }
  size_t position() { return file_.position(); }
  int read(void* buf, size_t count) { return file_.read(reinterpret_cast<uint8_t*>(buf), count); }
  int read() { return file_.read(); }
  size_t write(uint8_t b) override { return file_.write(b); }
  size_t write(const uint8_t* buf, size_t size) override { return file_.write(buf, size); }
  size_t write(const void* buf, size_t size) { return file_.write(reinterpret_cast<const uint8_t*>(buf), size); }
  bool rename(const char* newPath) { return file_.rename(newPath); }
  bool isDirectory() const { return file_.isDirectory(); }
  void rewindDirectory() { file_.rewindDirectory(); }
  bool close() { file_.close(); return true; }
  HalFile openNextFile() { return HalFile(file_.openNextFile()); }
  bool isOpen() const { return static_cast<bool>(file_); }
  operator bool() const { return static_cast<bool>(file_); }

 private:
  FsFile file_;
};

class HalStorage {
 public:
  bool begin() { return SdMan.begin(); }
  bool ready() const { return SdMan.ready(); }
  HalFile open(const char* path, oflag_t oflag = O_RDONLY) { return HalFile(SdMan.open(path, oflag)); }
  bool exists(const char* path) { return SdMan.exists(path); }
  bool mkdir(const char* path, bool pFlag = true) { return SdMan.mkdir(path, pFlag); }
  bool remove(const char* path) { return SdMan.remove(path); }
  bool rmdir(const char* path) { return SdMan.rmdir(path); }
  bool rename(const char* oldPath, const char* newPath) { 
   FsFile file = SdMan.open(oldPath, O_RDONLY);
   if (!file) return false;
   bool ret = file.rename(newPath);
   file.close();
   return ret;
  }
  
  String readFile(const char* path) {
   String content;
   HalFile file = open(path, O_RDONLY);
   if (!file.isOpen()) return content;
    
   char buf[256];
   int n;
   while ((n = file.read(buf, sizeof(buf))) > 0) {
     content += String(buf);
   }
   file.close();
   return content;
  }
  
  bool writeFile(const char* path, const String& content) {
   HalFile file = open(path, O_WRONLY | O_CREAT | O_TRUNC);
   if (!file.isOpen()) return false;
   file.write(reinterpret_cast<const uint8_t*>(content.c_str()), content.length());
   file.close();
   return true;
  }
  
  bool readFileToStream(const char* path, Print& out, size_t chunkSize = 256) {
   HalFile file = open(path, O_RDONLY);
   if (!file.isOpen()) return false;
    
   char buf[256];
   int n;
   while ((n = file.read(buf, std::min(chunkSize, sizeof(buf)))) > 0) {
     out.write(reinterpret_cast<uint8_t*>(buf), n);
   }
   file.close();
   return true;
  }
  
  bool openFileForRead(const char* moduleName, const char* path, HalFile& file) {
   file = open(path, O_RDONLY);
   return file.isOpen();
  }
  bool openFileForRead(const char* moduleName, const std::string& path, HalFile& file) {
   return openFileForRead(moduleName, path.c_str(), file);
  }
  bool openFileForRead(const char* moduleName, const String& path, HalFile& file) {
   return openFileForRead(moduleName, path.c_str(), file);
  }
  bool openFileForWrite(const char* moduleName, const char* path, HalFile& file) {
   file = open(path, O_WRONLY | O_CREAT | O_TRUNC);
   return file.isOpen();
  }
  bool openFileForWrite(const char* moduleName, const std::string& path, HalFile& file) {
   return openFileForWrite(moduleName, path.c_str(), file);
  }
  bool openFileForWrite(const char* moduleName, const String& path, HalFile& file) {
   return openFileForWrite(moduleName, path.c_str(), file);
  }
  bool ensureDirectoryExists(const char* path) { return mkdir(path, true); }
  bool removeDir(const char* path) { return rmdir(path); }

  static HalStorage& getInstance() {
   static HalStorage s;
   return s;
  }
};

#define Storage HalStorage::getInstance()


