#include "Util.h"

#include <chrono>
#include <string>
#include <sstream>

#if defined(_MSC_VER)
#include <Windows.h>
#endif

namespace avc {
namespace util {


void setThreadName(const char *name) {
  //assert(name);
#if defined(__linux) || defined(__linux__) || defined(__MINGW32__)
  pthread_setname_np(pthread_self(), limitString(name, 16).data());
#elif defined(__MACH__) || defined(__APPLE__)
  pthread_setname_np(limitString(name, 32).data());
#elif defined(_MSC_VER)
  // SetThreadDescription was added in 1607 (aka RS1). Since we can't guarantee the user is running 1607 or later, we need to ask for the function from the kernel.
  using SetThreadDescriptionFunc = HRESULT(WINAPI*)(_In_ HANDLE hThread, _In_ PCWSTR lpThreadDescription);
  static auto setThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(::GetProcAddress(::GetModuleHandle("Kernel32.dll"), "SetThreadDescription"));
  if (setThreadDescription) {
    // Convert the thread name to Unicode
    wchar_t threadNameW[MAX_PATH];
    size_t numCharsConverted;
    errno_t wcharResult = mbstowcs_s(&numCharsConverted, threadNameW, name, MAX_PATH - 1);
    if (wcharResult == 0) {
      HRESULT hr = setThreadDescription(::GetCurrentThread(), threadNameW);
      if (!SUCCEEDED(hr)) {
        int i = 0;
        i++;
      }
    }
  }
  else {
    // For understanding the types and values used here, please see:
    // https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code

    const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
    struct THREADNAME_INFO {
      DWORD dwType = 0x1000; // Must be 0x1000
      LPCSTR szName;         // Pointer to name (in user address space)
      DWORD dwThreadID;      // Thread ID (-1 for caller thread)
      DWORD dwFlags = 0;     // Reserved for future use; must be zero
    };
#pragma pack(pop)

    THREADNAME_INFO info;
    info.szName = name;
    info.dwThreadID = (DWORD)-1;

    __try {
      RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<const ULONG_PTR*>(&info));
    }
    __except (GetExceptionCode() == MS_VC_EXCEPTION ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_EXECUTE_HANDLER) {
    }
  }
#else
  thread_name = name ? name : "";
#endif
}

std::string getThreadName()
{
#if ((defined(__linux) || defined(__linux__)) && !defined(ANDROID)) || (defined(__MACH__) || defined(__APPLE__)) || (defined(ANDROID) && __ANDROID_API__ >= 26) || defined(__MINGW32__)
  string ret;
  ret.resize(32);
  auto tid = pthread_self();
  pthread_getname_np(tid, (char*)ret.data(), ret.size());
  if (ret[0]) {
    ret.resize(strlen(ret.data()));
    return ret;
  }
  return to_string((uint64_t)tid);
#elif defined(_MSC_VER)
  using GetThreadDescriptionFunc = HRESULT(WINAPI*)(_In_ HANDLE hThread, _In_ PWSTR* ppszThreadDescription);
  static auto getThreadDescription = reinterpret_cast<GetThreadDescriptionFunc>(::GetProcAddress(::GetModuleHandleA("Kernel32.dll"), "GetThreadDescription"));

  if (!getThreadDescription) {
    std::ostringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
  }
  else {
    PWSTR data;
    HRESULT hr = getThreadDescription(GetCurrentThread(), &data);
    if (SUCCEEDED(hr) && data[0] != '\0') {
      char threadName[MAX_PATH];
      size_t numCharsConverted;
      errno_t charResult = wcstombs_s(&numCharsConverted, threadName, data, MAX_PATH - 1);
      if (charResult == 0) {
        LocalFree(data);
        std::ostringstream ss;
        ss << threadName;
        return ss.str();
      }
      else {
        if (data) {
          LocalFree(data);
        }
        return std::to_string((uint64_t)GetCurrentThreadId());
      }
    }
    else {
      if (data) {
        LocalFree(data);
      }
      return std::to_string((uint64_t)GetCurrentThreadId());
    }
  }
#else
  if (!thread_name.empty()) {
    return thread_name;
  }
  std::ostringstream ss;
  ss << std::this_thread::get_id();
  return ss.str();
#endif
}

#if defined(_WIN32)
int gettimeofday(struct timeval *tm, void *ptz) {
    (void)ptz;
    //通过C++接口获取，自Epoch（1970-1-1 00:00:00 UTC) 以来的微秒数
    using namespace std::chrono;
    auto now_stamp_ms = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
    tm->tv_sec = decltype(tm->tv_sec)(now_stamp_ms / 1000000L);
    //转为微秒
    tm->tv_usec = now_stamp_ms % 1000000L;
    return 0;
}

int vasprintf(char **strp, const char *fmt, va_list ap) {
    // _vscprintf tells you how big the buffer needs to be
    int len = _vscprintf(fmt, ap);
    if (len == -1) {
        return -1;
    }
    size_t size = (size_t)len + 1;
    char *str = (char*)malloc(size);
    if (!str) {
        return -1;
    }
    // _vsprintf_s is the "secure" version of vsprintf
    int r = vsprintf_s(str, len + 1, fmt, ap);
    if (r == -1) {
        free(str);
        return -1;
    }
    *strp = str;
    return r;
}

 int asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(strp, fmt, ap);
    va_end(ap);
    return r;
}

#if HAS_STRING_VIEW
std::string filename(std::string_view path) {
#else
std::string filename(const std::string & path) {
#endif
  auto pos = path.rfind("/");
  if (pos == std::string::npos) {
    pos = path.rfind("\\");
  }

  std::string filename;
  if (pos != std::string::npos) {
    filename = path.substr(pos + 1);
  }
  else {
    filename = path;
  }
  return filename;
}

#endif



}
}