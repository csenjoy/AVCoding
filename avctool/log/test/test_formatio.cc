#include <stdio.h> //依赖snprintf格式化可变参数
#include <string>
#include <stdarg.h> //...转化为可变参数va_list

#include <sstream>

/**
 * vsnprintf函数使用的固定大小buffer，可以使用动态方式申请每一次格式化日志大小
*/
#define MYFORMAT_BUFFER_SIZE 4096

/**
 * @brief 试验性质： 处理C++流式以及C字符串格式形式
 * 
*/
typedef void (*log_output)(int level, std::string &&log);

static void my_log_output(int level, std::string &&log) {
    printf("%s", log.c_str());
    //fflush(stdout);
}

static log_output output = my_log_output;


/**
 * 演示cpp，流式格式化I/O
 *      1）流式格式I/O，需要声明一个对象，并且从在operator<<操作符
 * 
 * cppformatter在析构的时候，获取日志正文（析构时，表明用户不在继续输入日志内容了)
*/
struct cppformatter 
{
    cppformatter(int level, int line, const char *func) : level_(level), line_(line), func_(func) {}
    cppformatter() {}

    ~cppformatter() {
        //析构的时候，输出字符串
        auto content = toString();
        //mycformat_wrapper(1, line_, func_, content.c_str());
        
        int nwritten = snprintf(log_buffer, MYFORMAT_BUFFER_SIZE, "[%s:%d]: %s\n", func_, line_, content.c_str());
        std::string out(log_buffer, nwritten);
        output(level_, std::move(out));
    }

    template<class T>
    cppformatter &operator<<(T &&data) {
        /**
         * 此处使用std::forward完美转发参数，可能会有自定义类型实现流式<<的移动语义
        */
        oss_ << std::forward<T>(data);
        return *this;
    }

    std::string toString() const {
        return oss_.str();
    }

    std::ostringstream oss_;   
    int level_ = 0;
    int line_  = 0;
    const char *func_ = nullptr;

    static char log_buffer[MYFORMAT_BUFFER_SIZE];
};

char cppformatter::log_buffer[MYFORMAT_BUFFER_SIZE];
/**
 * C++类型，流式I/O
*/
//---------------------------------------------------------------------------------------------

#define LOG_FORMAT cppformatter(1, __LINE__, __FUNCTION__)
/**
 * 演示c类型格式化I/O
 *      1）使用可变参数...作为函数声明
 *      2) 使用可变参数va_list作为函数声明
*/
std::string mycformat(const char *format, .../*va_list args*/) {
    static char my_buffer[4096] = {0};
    int nwritten = 0;
    va_list args;
    va_start(args, format);
    /**
     * nwritten: 返回写入数组字符数，若出错返回负数
    */
    nwritten = vsnprintf(my_buffer, MYFORMAT_BUFFER_SIZE, format, args);
    va_end(args);
    return std::string(my_buffer, nwritten > 0 ? nwritten : 0);
}

std::string mycformat(const char *format, va_list args) {
    static char my_buffer[4096] = {0};
    int nwritten = 0;
    /**
     * nwritten: 返回写入数组字符数，若出错返回负数
    */
    nwritten = vsnprintf(my_buffer, MYFORMAT_BUFFER_SIZE, format, args);
    return std::string(my_buffer, nwritten > 0 ? nwritten : 0);
}

void mycformat_wrapper(int level, int line, const char *func, const char *format, ...) {
    //假设固定格式形式[func:line]: xxxxx
    static char log_buffer[MYFORMAT_BUFFER_SIZE];
    /**
     * 日志正文内容
    */
    std::string content; int nwritten;
    va_list args;
    va_start(args, format);
    /**
     * 先格式日志正文
    */
    content = mycformat(format, args);
    va_end(args);

    /**
     * C格式化字符，依赖cppformat输出日志
    */   
    cppformatter cppformat(level, line, func);
    cppformat << content;
}

/*
 * C类型，格式化I/O，通过宏展开增加额外信息
*/
#define LOG_CFORMAT(...) mycformat_wrapper(1, __LINE__, __FUNCTION__, __VA_ARGS__)
//===========================================================================================================================
/**
 *上述代码演示了, C++流式日志格式方式与
 *              C格式化I/O方式
 * 
 * 从软件分层角度思考： C格式化I/O方式，完成日志正文格式后，调用C++流式日志格式完成日志输入（此时相当于流式工作方式一个输入参数（C格式化完成的日志文本）
*/


int main(int argc, char **argv) {
    //C字符串格式，通过vsnprintf格式化可变参数
    auto message1 = mycformat("%s, %d", "jack", 17);
    auto message2 = mycformat("%s, %d", "jack2", 17);
    auto message3 = mycformat("%s, %d", "jack3", 17);

    {
        cppformatter formatter;
        formatter << "jack4" << ", " << 17;
        auto message4 = formatter.toString();
    }

   LOG_CFORMAT("%s, %d", "jack", 17);

    {
        LOG_FORMAT << "jack, " << 17;
    }
    

    return 0;
}