#pragma once

#include <map>
#include <list>
#include <string>
#include <cstring>

#include <Common.h>
#include <Common/MyDelegate.h>

using namespace Delegate;

struct IStream;

class EventSystem;
class Event;

extern void* EventArgsLate;
extern void* EventArgsEmpty;

typedef void (*HandleEvent)(EventSystem*, Event, void*);

// 事件类型
class Event
{
public:
	Event(const char* Name, const char* Dest);

	// 按字符串内容比较，而不是比较 const char* 指针地址
	auto operator <=>(const Event& other) const
	{
		if (int cmp = std::strcmp(Name, other.Name); cmp != 0)
		{
			return cmp <=> 0;
		}
		return std::strcmp(Dest, other.Dest) <=> 0;
	}

	const char* Name;
	const char* Dest;
};

// 事件管理器
class EventSystem
{
public:
	EventSystem(const char* name);

	void AddHandler(Event e, HandleEvent func);

	template<typename T, typename F>
	void AddHandler(Event e, T* _obj, F func)
	{
		_handlers[e] += newDelegate(_obj, func);
	}

	void RemoveHandler(Event e, HandleEvent func);

	template<typename T, typename F>
	void RemoveHandler(Event e, T* _obj, F func)
	{
		_handlers[e] -= newDelegate(_obj, func);
	}

	void Broadcast(Event e, void* args = EventArgsEmpty);

	const char* Name;
private:
	std::map<Event, CMultiDelegate<void, EventSystem*, Event, void*>> _handlers;
};


class EventSystems
{
public:
	// 事件管理器
	static EventSystem General;
	static EventSystem Render;
	static EventSystem Logic;
	static EventSystem SaveLoad;
};

class Events
{
public:
	// 程序生命周期事件
	static Event ExeRun;
	static Event ExeTerminate;
	static Event CmdLineParse;
	// 游戏进程事件
	static Event DetachAll;
	static Event ObjectUnInitEvent;
	static Event PointerExpireEvent;
	static Event ScenarioClearClassesEvent;
	static Event ScenarioStartEvent;
	// 渲染事件
	static Event GScreenRenderEvent;
	static Event SidebarRenderEvent;
	// 单位逻辑事件
	static Event LogicUpdateEvent;
	static Event TypeChangeEvent;
	// 游戏保存事件
	static Event SaveGameEvent;
	// 游戏读取事件
	static Event LoadGameEvent;
};

// 事件参数
class SaveLoadEventArgs
{
public:
	SaveLoadEventArgs(const char* fileName, bool isStart);
	SaveLoadEventArgs(IStream* stream, bool isStart);

	bool InStream();
	bool IsStart();
	bool IsEnd();
	bool IsStartInStream();
	bool IsEndInStream();

	const char* FileName;
	IStream* Stream;
private:
	bool isStart;
	bool isStartInStream;
};

class SaveGameEventArgs : public SaveLoadEventArgs
{
public:
	using SaveLoadEventArgs::SaveLoadEventArgs;
};

class LoadGameEventArgs :public SaveLoadEventArgs
{
public:
	using SaveLoadEventArgs::SaveLoadEventArgs;
};

class TechnoClass;

class TypeChangeEventArgs
{
public:
	TechnoClass* pTechno;
	bool IsTransform;
};
