//
// Copyright RIME Developers
// Distributed under the BSD License
//
// register components
//
// 2011-06-30 GONG Chen <chen.sst@gmail.com>
//
#include <fstream>
#include <filesystem>
#ifndef RIME_WASM
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#endif
#include <rime/dict/mapped_file.h>

namespace rime {

#ifndef RIME_WASM
class MappedFileImpl {
 public:
  enum OpenMode {
    kOpenReadOnly,
    kOpenReadWrite,
  };

  MappedFileImpl(const string& file_name, OpenMode mode) {
    boost::interprocess::mode_t file_mapping_mode =
        (mode == kOpenReadOnly) ? boost::interprocess::read_only
                                : boost::interprocess::read_write;
    file_.reset(new boost::interprocess::file_mapping(file_name.c_str(),
                                                      file_mapping_mode));
    region_.reset(
        new boost::interprocess::mapped_region(*file_, file_mapping_mode));
  }
  ~MappedFileImpl() {
    region_.reset();
    file_.reset();
  }
  bool Flush() { return region_->flush(); }
  void* get_address() const { return region_->get_address(); }
  size_t get_size() const { return region_->get_size(); }

 private:
  the<boost::interprocess::file_mapping> file_;
  the<boost::interprocess::mapped_region> region_;
};
#else
class MappedFileImpl {
 public:
  bool Flush() { return false; }
  void* get_address() const { return nullptr; }
  size_t get_size() const { return 0; }
};
#endif

MappedFile::MappedFile(const string& file_name) : file_name_(file_name) {}

MappedFile::~MappedFile() {
  if (file_) {
    file_.reset();
  }
}

bool MappedFile::Create(size_t capacity) {
  if (is_memory_open_) {
    LOG(ERROR) << "cannot create file while memory mapping is open.";
    return false;
  }
  if (Exists()) {
    LOG(INFO) << "overwriting file '" << file_name_ << "'.";
    Resize(capacity);
  } else {
    LOG(INFO) << "creating file '" << file_name_ << "'.";
    std::filebuf fbuf;
    fbuf.open(file_name_.c_str(), std::ios_base::in | std::ios_base::out |
                                      std::ios_base::trunc |
                                      std::ios_base::binary);
    if (capacity > 0) {
      fbuf.pubseekoff(capacity - 1, std::ios_base::beg);
      fbuf.sputc(0);
    }
    fbuf.close();
  }
  LOG(INFO) << "opening file for read/write access.";
#ifndef RIME_WASM
  file_.reset(new MappedFileImpl(file_name_, MappedFileImpl::kOpenReadWrite));
#else
  LOG(ERROR) << "file mapping is not supported in wasm build.";
  return false;
#endif
  size_ = 0;
  return bool(file_);
}

bool MappedFile::OpenReadOnly() {
  if (is_memory_open_) {
    LOG(ERROR) << "cannot open file while memory mapping is open.";
    return false;
  }
  if (!Exists()) {
    LOG(ERROR) << "attempt to open non-existent file '" << file_name_ << "'.";
    return false;
  }
#ifndef RIME_WASM
  file_.reset(new MappedFileImpl(file_name_, MappedFileImpl::kOpenReadOnly));
  size_ = file_->get_size();
#else
  LOG(ERROR) << "file mapping is not supported in wasm build.";
  return false;
#endif
  return bool(file_);
}

bool MappedFile::OpenReadOnlyFromMemory(const void* data, size_t size) {
  if (!data || size == 0) {
    LOG(ERROR) << "attempt to open empty memory buffer.";
    return false;
  }
  if (IsOpen()) {
    Close();
  }
  memory_address_ = const_cast<char*>(static_cast<const char*>(data));
  memory_size_ = size;
  is_memory_open_ = true;
  size_ = size;
  return true;
}

bool MappedFile::OpenReadWrite() {
  if (is_memory_open_) {
    LOG(ERROR) << "cannot open file while memory mapping is open.";
    return false;
  }
  if (!Exists()) {
    LOG(ERROR) << "attempt to open non-existent file '" << file_name_ << "'.";
    return false;
  }
#ifndef RIME_WASM
  file_.reset(new MappedFileImpl(file_name_, MappedFileImpl::kOpenReadWrite));
#else
  LOG(ERROR) << "file mapping is not supported in wasm build.";
  return false;
#endif
  size_ = 0;
  return bool(file_);
}

void MappedFile::Close() {
  if (file_) {
    file_.reset();
  }
  if (is_memory_open_) {
    memory_address_ = nullptr;
    memory_size_ = 0;
    is_memory_open_ = false;
  }
  size_ = 0;
}

bool MappedFile::Exists() const {
  return std::filesystem::exists(file_name_);
}

bool MappedFile::IsOpen() const {
  return bool(file_) || is_memory_open_;
}

bool MappedFile::Flush() {
  if (is_memory_open_) {
    return true;
  }
  if (!file_)
    return false;
  return file_->Flush();
}

bool MappedFile::ShrinkToFit() {
  LOG(INFO) << "shrinking file to fit data size. capacity: " << capacity();
  return Resize(size_);
}

bool MappedFile::Remove() {
  if (IsOpen())
    Close();
#ifndef RIME_WASM
  return boost::interprocess::file_mapping::remove(file_name_.c_str());
#else
  return false;
#endif
}

bool MappedFile::Resize(size_t capacity) {
  LOG(INFO) << "resize file to: " << capacity;
  if (IsOpen())
    Close();
  try {
    std::filesystem::resize_file(file_name_.c_str(), capacity);
  } catch (...) {
    return false;
  }
  return true;
}

String* MappedFile::CreateString(const string& str) {
  String* ret = Allocate<String>();
  if (ret && !str.empty()) {
    CopyString(str, ret);
  }
  return ret;
}

bool MappedFile::CopyString(const string& src, String* dest) {
  if (!dest)
    return false;
  size_t size = src.length() + 1;
  char* ptr = Allocate<char>(size);
  if (!ptr)
    return false;
  std::strncpy(ptr, src.c_str(), size);
  dest->data = ptr;
  return true;
}

size_t MappedFile::capacity() const {
  if (file_) {
    return file_->get_size();
  }
  return memory_size_;
}

char* MappedFile::address() const {
  if (file_) {
    return reinterpret_cast<char*>(file_->get_address());
  }
  return memory_address_;
}

}  // namespace rime
