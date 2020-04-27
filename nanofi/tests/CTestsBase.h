/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _WIN32
#ifndef NANOFI_TESTS_CTESTSBASE_H_
#define NANOFI_TESTS_CTESTSBASE_H_

#include <vector>
#include <string>
#include <fstream>
#include <assert.h>
#include <sys/stat.h>

#include "core/file_utils.h"
#include "api/ecu.h"
#include "api/nanofi.h"

class FileManager {
public:
  FileManager(const std::string& filePath) :
      filePath_(filePath) {
    assert(!filePath_.empty() && "filePath provided cannot be empty!");
    remove(filePath_.c_str());
    outputStream_.open(filePath_, std::ios::binary);
    assert(outputStream_.good());
  }

  ~FileManager() {
    std::ifstream ifs(filePath_);
    if (ifs.good()) {
      remove(filePath_.c_str());
    }
  }

  void Write(const std::string& str) {
    outputStream_ << str;
  }

  std::string WriteNChars(uint64_t n, char c) {
    std::string s(n, c);
    outputStream_ << s;
    return s;
  }

  std::string getFilePath() const {
    return filePath_;
  }

  void OpenStream() {
    outputStream_.open(filePath_, std::ios::binary | std::ios::app);
  }

  void CloseStream() {
    outputStream_.flush();
    outputStream_.close();
  }

  uint64_t GetFileSize() {
    CloseStream();
    uint64_t fileSize = 0;
    int ret = get_file_size(filePath_.c_str(), &fileSize);
    REQUIRE(ret >= 0);
    return fileSize;
  }

private:
  std::string filePath_;
  std::ofstream outputStream_;
};

class TailFileTestResourceManager {
public:
  TailFileTestResourceManager(const std::string& processor_name,
      void (*callback)(processor_session * ps, processor_context * ctx)) {
    const char * port_str = "uuid";
    nifi_port port;
    port.port_id = (char *) port_str;
    const char * instance_str = "nifi";
    instance_ = create_instance(instance_str, &port);
    add_custom_processor(processor_name.c_str(), callback);
    processor_ = create_processor(processor_name.c_str(), instance_);
  }

  ~TailFileTestResourceManager() {
    remove_directory("./contentrepository");
    char uuid_str[37];
    get_proc_uuid_from_processor(processor_, uuid_str);
    delete_all_flow_files_from_proc(uuid_str);
    struct processor_params * tmp, *pp = NULL;
    HASH_ITER(hh, procparams, pp, tmp) {
      HASH_DEL(procparams, pp);
      free(pp);
    }
    free_standalone_processor(processor_);
    free_instance(instance_);
  }

  standalone_processor * getProcessor() const {
    return processor_;
  }

  nifi_instance * getInstance() const {
    return instance_;
  }

private:
  nifi_instance * instance_;
  standalone_processor * processor_;
};

struct processor_params * invoke_processor(TailFileTestResourceManager& mgr,
    const char * filePath) {
  standalone_processor * proc = mgr.getProcessor();
  set_standalone_property(proc, "file_path", filePath);
  set_standalone_property(proc, "delimiter", ";");

  flow_file_record * new_ff = invoke(proc);

  char uuid_str[37];
  get_proc_uuid_from_processor(proc, uuid_str);
  struct processor_params * pp = get_proc_params(uuid_str);
  return pp;
}

#endif /* NANOFI_TESTS_CTESTSBASE_H_ */
#endif
