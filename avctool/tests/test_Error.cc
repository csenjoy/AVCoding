#include "error/Error.h"
#include "log/Log.h"

using namespace avc::util;

//void int 


avc_error_t subsubprocess() {
  avc_error_t ret = avc_error_sucess;

  ret = avc_error_new(AvcErrorCode::ERRNO_SYSTEM_FILE_OPEN, "打开文件失败");
  return ret;
}

avc_error_t subprocess() {
  auto ret = subsubprocess();
  if (ret != avc_error_sucess) {
    return avc_error_wrap(ret, "调用subsubproecess失败");
  }
  return ret;
}

avc_error_t test_avc_error_multiwrap() {
  avc_error_t ret = avc_error_sucess;
  ret = subprocess();

  if (ret != avc_error_sucess) {
    return avc_error_wrap(ret, "调用subprocess失败");
  }
  return ret;
}


avc_error_t test_avc_error_copy(avc_error_t from) {
  return avc_error_copy(from);
}

int main(int argc, char** argv) {
  //使用日志库打印标准输出
  Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));
  Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
  
  auto ret = test_avc_error_multiwrap();

  DebugL << "test_avc_error_multiwrap() ret: \n" << avc_error_summary(ret);

  auto ret_copy = test_avc_error_copy(ret);

  avc_free_safe(&ret_copy);
  avc_free_safe(&ret);

  getchar();
  return 0;
}