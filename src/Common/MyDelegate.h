#pragma once

#include <typeinfo>
#include <list>
#include <vector>

namespace Delegate
{

	// IDelegate   提供接口的基类

	template <typename ReturnType, typename... ParamType>
	class IDelegate
	{
	public:
		IDelegate() {}
		virtual ~IDelegate() {}
		virtual bool isType(const std::type_info &_type) = 0;
		virtual ReturnType invoke(ParamType... params) = 0;
		virtual bool compare(IDelegate<ReturnType, ParamType...> *_delegate) const = 0;
	};

	// StaticDelegate 普通函数的委托

	template <typename ReturnType, typename... ParamType>
	class CStaticDelegate : public IDelegate<ReturnType, ParamType...>
	{
	public:
		typedef ReturnType (*Func)(ParamType...);

		CStaticDelegate(Func _func) : mFunc(_func) {}

		virtual bool isType(const std::type_info &_type) { return typeid(CStaticDelegate<ReturnType, ParamType...>) == _type; }

		virtual ReturnType invoke(ParamType... params) { return mFunc(params...); }

		virtual bool compare(IDelegate<ReturnType, ParamType...> *_delegate) const
		{
			if (0 == _delegate || !_delegate->isType(typeid(CStaticDelegate<ReturnType, ParamType...>)))
				return false;
			CStaticDelegate<ReturnType, ParamType...> *cast = static_cast<CStaticDelegate<ReturnType, ParamType...> *>(_delegate);
			return cast->mFunc == mFunc;
		}

		virtual ~CStaticDelegate() {}

	private:
		Func mFunc;
	};

	// 普通函数的委托特化版本
	template <typename ReturnType, typename... ParamType>
	class CStaticDelegate<ReturnType (*)(ParamType...)> : public IDelegate<ReturnType, ParamType...>
	{
	public:
		// 定义 Func 为 void (void) 函数类型指针。
		typedef ReturnType (*Func)(ParamType...);

		CStaticDelegate(Func _func) : mFunc(_func) {}

		virtual bool isType(const std::type_info &_type) { return typeid(CStaticDelegate<ReturnType (*)(ParamType...)>) == _type; }

		virtual ReturnType invoke(ParamType... params) { return mFunc(params...); }

		virtual bool compare(IDelegate<ReturnType, ParamType...> *_delegate) const
		{
			if (0 == _delegate || !_delegate->isType(typeid(CStaticDelegate<ReturnType (*)(ParamType...)>)))
				return false;
			CStaticDelegate<ReturnType (*)(ParamType...)> *cast = static_cast<CStaticDelegate<ReturnType (*)(ParamType...)> *>(_delegate);
			return cast->mFunc == mFunc;
		}

		virtual ~CStaticDelegate() {}

	private:
		Func mFunc;
	};

	// 成员函数委托
	template <typename T, typename ReturnType, typename... ParamType>
	class CMethodDelegate : public IDelegate<ReturnType, ParamType...>
	{
	public:
		typedef ReturnType (T::*Method)(ParamType...);

		CMethodDelegate(T *_object, Method _method) : mObject(_object), mMethod(_method) {}

		virtual bool isType(const std::type_info &_type) { return typeid(CMethodDelegate<T, ReturnType, ParamType...>) == _type; }

		virtual ReturnType invoke(ParamType... params)
		{
			return (mObject->*mMethod)(params...);
		}

		virtual bool compare(IDelegate<ReturnType, ParamType...> *_delegate) const
		{
			if (0 == _delegate || !_delegate->isType(typeid(CMethodDelegate<ReturnType, ParamType...>)))
				return false;
			CMethodDelegate<ReturnType, ParamType...> *cast = static_cast<CMethodDelegate<ReturnType, ParamType...> *>(_delegate);
			return cast->mObject == mObject && cast->mMethod == mMethod;
		}

		CMethodDelegate() {}
		virtual ~CMethodDelegate() {}

	private:
		T *mObject;
		Method mMethod;
	};

	// 成员函数委托特化
	template <typename T, typename ReturnType, typename... ParamType>
	class CMethodDelegate<T, ReturnType (T::*)(ParamType...)> : public IDelegate<ReturnType, ParamType...>
	{
	public:
		typedef ReturnType (T::*Method)(ParamType...);

		CMethodDelegate(T *_object, Method _method) : mObject(_object), mMethod(_method) {}

		virtual bool isType(const std::type_info &_type) { return typeid(CMethodDelegate<T, ReturnType (T::*)(ParamType...)>) == _type; }

		virtual ReturnType invoke(ParamType... params)
		{
			return (mObject->*mMethod)(params...);
		}

		virtual bool compare(IDelegate<ReturnType, ParamType...> *_delegate) const
		{
			if (0 == _delegate || !_delegate->isType(typeid(CMethodDelegate<T, ReturnType (T::*)(ParamType...)>)))
				return false;
			CMethodDelegate<T, ReturnType (T::*)(ParamType...)> *cast = static_cast<CMethodDelegate<T, ReturnType (T::*)(ParamType...)> *>(_delegate);
			return cast->mObject == mObject && cast->mMethod == mMethod;
		}

		CMethodDelegate() {}
		virtual ~CMethodDelegate() {}

	private:
		T *mObject;
		Method mMethod;
	};

	// 多播委托
	template <typename ReturnType, typename... ParamType>
	class CMultiDelegate
	{

	public:
		typedef std::list<IDelegate<ReturnType, ParamType...> *> ListDelegate;
		typedef typename ListDelegate::iterator ListDelegateIterator;
		typedef typename ListDelegate::const_iterator ConstListDelegateIterator;

		CMultiDelegate() {}
		~CMultiDelegate() { clear(); }

		bool empty() const
		{
			for (ConstListDelegateIterator iter = mListDelegates.begin(); iter != mListDelegates.end(); ++iter)
			{
				if (*iter)
					return false;
			}
			return true;
		}

		void clear()
		{
			for (ListDelegateIterator iter = mListDelegates.begin(); iter != mListDelegates.end(); ++iter)
			{
				if (*iter)
				{
					delete (*iter);
					(*iter) = nullptr;
				}
			}
		}

		CMultiDelegate<ReturnType, ParamType...> &operator+=(IDelegate<ReturnType, ParamType...> *_delegate)
		{
			for (ListDelegateIterator iter = mListDelegates.begin(); iter != mListDelegates.end(); ++iter)
			{
				if ((*iter) && (*iter)->compare(_delegate))
				{
					delete _delegate;
					return *this;
				}
			}
			mListDelegates.push_back(_delegate);
			return *this;
		}

		CMultiDelegate<ReturnType, ParamType...> &operator-=(IDelegate<ReturnType, ParamType...> *_delegate)
		{
			for (ListDelegateIterator iter = mListDelegates.begin(); iter != mListDelegates.end(); ++iter)
			{
				if ((*iter) && (*iter)->compare(_delegate))
				{
					if ((*iter) != _delegate)
						delete (*iter); // 避免同一个地址被delete两次
					(*iter) = 0;
					break;
				}
			}
			delete _delegate;
			return *this;
		}

		std::vector<ReturnType> operator()(ParamType... params)
		{
			ListDelegateIterator iter = mListDelegates.begin();
			std::vector<ReturnType> _Results;
			while (iter != mListDelegates.end())
			{
				if (0 == (*iter))
				{
					iter = mListDelegates.erase(iter);
				}
				else
				{
					_Results.push_back((*iter)->invoke(params...));
					++iter;
				}
			}
			return _Results;
		}

	private:
		CMultiDelegate<ReturnType, ParamType...>(const CMultiDelegate &_event);
		CMultiDelegate<ReturnType, ParamType...> &operator=(const CMultiDelegate &_event);

	private:
		ListDelegate mListDelegates;
	};

	template <typename... ParamType>
	class CMultiDelegate<void, ParamType...>
	{

	public:
		typedef std::list<IDelegate<void, ParamType...> *> ListDelegate;
		typedef typename ListDelegate::iterator ListDelegateIterator;
		typedef typename ListDelegate::const_iterator ConstListDelegateIterator;

		CMultiDelegate() {}
		~CMultiDelegate() { clear(); }

		bool empty() const
		{
			for (ConstListDelegateIterator iter = mListDelegates.begin(); iter != mListDelegates.end(); ++iter)
			{
				if (*iter)
					return false;
			}
			return true;
		}

		void clear()
		{
			for (ListDelegateIterator iter = mListDelegates.begin(); iter != mListDelegates.end(); ++iter)
			{
				if (*iter)
				{
					delete (*iter);
					(*iter) = nullptr;
				}
			}
		}

		CMultiDelegate<void, ParamType...> &operator+=(IDelegate<void, ParamType...> *_delegate)
		{
			for (ListDelegateIterator iter = mListDelegates.begin(); iter != mListDelegates.end(); ++iter)
			{
				if ((*iter) && (*iter)->compare(_delegate))
				{
					delete _delegate;
					return *this;
				}
			}
			mListDelegates.push_back(_delegate);
			return *this;
		}

		CMultiDelegate<void, ParamType...> &operator-=(IDelegate<void, ParamType...> *_delegate)
		{
			for (ListDelegateIterator iter = mListDelegates.begin(); iter != mListDelegates.end(); ++iter)
			{
				if ((*iter) && (*iter)->compare(_delegate))
				{
					if ((*iter) != _delegate)
						delete (*iter); // 避免同一个地址被delete两次
					(*iter) = 0;
					break;
				}
			}
			delete _delegate;
			return *this;
		}

		void operator()(ParamType... params)
		{
			ListDelegateIterator iter = mListDelegates.begin();
			while (iter != mListDelegates.end())
			{
				if (0 == (*iter))
				{
					iter = mListDelegates.erase(iter);
				}
				else
				{
					(*iter)->invoke(params...);
					++iter;
				}
			}
		}

	private:
		CMultiDelegate<void, ParamType...>(const CMultiDelegate &_event);
		CMultiDelegate<void, ParamType...> &operator=(const CMultiDelegate &_event);

	private:
		ListDelegate mListDelegates;
	};

	template <typename T>
	CStaticDelegate<T> *newDelegate(T func)
	{
		return new CStaticDelegate<T>(func);
	}
	template <typename T, typename F>
	CMethodDelegate<T, F> *newDelegate(T *_object, F func)
	{
		return new CMethodDelegate<T, F>(_object, func);
	}
}
