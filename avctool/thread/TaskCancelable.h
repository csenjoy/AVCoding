#ifndef THREAD_TASKCANCELABLE_H
#define THREAD_TASKCANCELABLE_H

#include <memory>

namespace avc {
namespace util {

/**
 * 可取消任务接口
*/
class TaskCancelable {
public:
    using Ptr = std::shared_ptr<TaskCancelable>;

    virtual ~TaskCancelable() {}
    virtual void cancel() = 0;
};//class TaskCancelable

/**
 * template<class R, class ...ARGS>
 * class TaskCancelableImpl<R(ARGS...)> : public TaskCancelable {
 * };
 * 上述模板类定义，需要此前置模板类声明
*/
template<class R, class ...ARGS>
class TaskCancelableImpl;

/**
 * 可取消任务模板类： 支持各种任务类型
 *      通过模板参数声明函数对象类型，用于执行各种签名任务类型 
 *      
 *      class TaskCancelableImpl<R(ARGS...)>此形式声明后
 *      下列重载函数模板才有效，否则此函数提示function returns functions错误
 *      R operator()(ARGS &&...) const {}
 *      此外，使用模板类时，类似于函数对象:
 *        using Task = TaskCancelableImpl<void()>;
 *      为什么使用class TaskCancelableImpl<R(ARGS...)>此形式声明
 *         （1）TaskCancelImpl<R(ARGS...)>作为function<R(ARGS...)>的一层wrapper，并提供可取消功能
 *          (2) 提供可重载的R operator()(ARGS &&...) const函数
*/
template<class R, class ...ARGS>
class TaskCancelableImpl<R(ARGS...)>: public TaskCancelable {
public:
    using Ptr = std::shared_ptr<TaskCancelableImpl>;
    using FuncType = std::function<R(ARGS...)>;

    template<class ...ARGS>
    static Ptr create(ARGS &&...args) {
      return std::make_shared<TaskCancelableImpl>(std::forward<ARGS>(args)...);
    }
    
    /**
     * 此处使用： FuncType作为参数类型与使用模板提供函数类型有何区别
     *           template<class FUNC>
     *           TaskCancelableImp(FUNC &&func) {}
    */
#if 0
    explicit TaskCancelableImpl(FuncType&& task) {
        strongTask_ = std::make_shared<FuncType>(std::forward<FuncType>(task));
        weakTask_ = strongTask_;
    }
#else
    template<class FUNC>
    explicit TaskCancelableImpl(FUNC&& task) {
        strongTask_ = std::make_shared<FuncType>(std::forward<FUNC>(task));
        weakTask_ = strongTask_;
    }
#endif

    ~TaskCancelableImpl() {
      cancel();
    }
    /**
     * 任务取消
    */
    void cancel() override {
        strongTask_ = nullptr;
    }
    /**
     * 任务取消
    */
    void operator=(std::nullptr_t) {
      strongTask_ = nullptr;
    }

    /**
     * 隐式bool操作符，判断任务是否有效
    */
    operator bool() {
        return strongTask_ && *strongTask_;
    }

    /**
     *  操作符重载执行任务
    */
    R operator()(ARGS &&...args) const {
      auto strongTask = weakTask_.lock();
      if (strongTask && (*strongTask)) {
        return (*strongTask)(std::forward<ARGS>(args)...);
      }
      return defaultValue<R>();
    }

    /**
     * 特化返回值类型： void
     * C++17提供的is_void_v<T>等价于std::is_void<T>::value
    */
    template<class T>
    static typename std::enable_if<std::is_void<T>::value,void>::type defaultValue() {}

    /**
     * 特化返回值类型：整型
    */
    template<class T>
    static typename std::enable_if<std::is_integral<T>::value, T>::type defaultValue() {
        return 0;
    }

    /**
     * 特化返回值类型：指针
    */
    template<class T>
    static typename std::enable_if<std::is_pointer<T>::value, T>::type defaultValue() {
        return nullptr;
    }
private:
    std::shared_ptr<FuncType> strongTask_;
    std::weak_ptr<FuncType> weakTask_;
};//class TaskCancelableImpl

} //namespace util 
} //namespace avc

#endif