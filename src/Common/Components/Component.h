#pragma once

#include <functional>
#include <typeinfo>
#include <string>
#include <algorithm>
#include <vector>
#include <list>
#include <set>
#include <memory>
#include <type_traits>
#include <atomic>

#include <Common/EventSystems/EventSystem.h>

#include <Utilities/Stream.h>
#include <Utilities/Debug.h>
#include <iterator>

#include "ComponentPool.h"

#ifdef DEBUG
#define LOG_DEBUG(pFormat, ...) Debug::Log(pFormat, __VA_ARGS__)
#else
#define LOG_DEBUG(pFormat, ...)
#endif

#ifdef DEBUG_COMPONENT
#define LOG_COMPONENT(pFormat, ...) Debug::Log(pFormat, __VA_ARGS__)
#else
#define LOG_COMPONENT(pFormat, ...)
#endif

#define DECLARE_COMPONENT(CLASS_NAME, ...) \
	CLASS_NAME() : __VA_ARGS__() \
	{ \
		this->Name = ScriptName; \
		LOG_COMPONENT("Component %s is created.\n\n", thisName.c_str()); \
	} \
	\
	virtual void FreeComponent() override \
	{ \
		ComponentPool::GetInstance().Release(this); \
	} \
	\
	static Component* Create() \
	{ \
		return ComponentPool::GetInstance().Acquire<CLASS_NAME>(); \
	} \
	\
	virtual Component* Clone() const override \
	{ \
		CLASS_NAME* clone = ComponentPool::GetInstance().Acquire<CLASS_NAME>(); \
		if (clone) \
		{ \
			clone->CopyFrom(*this); \
		} \
		else \
		{ \
			Debug::Log("Error: Failed to clone component %s\n", ScriptName.c_str()); \
		} \
		return clone; \
	} \
	\
	virtual void CopyFrom(const Component& other) override \
	{ \
		Component::CopyFrom(other); \
		const CLASS_NAME* otherDerived = static_cast<const CLASS_NAME*>(&other); \
		CopyDerivedFrom(*otherDerived); \
	} \
	\
	virtual void CopyDerivedFrom(const CLASS_NAME& other) {} \
	\
	inline static std::string ScriptName = #CLASS_NAME; \
	\
	static int RegisterComponent() \
	{ \
		return ComponentFactory::GetInstance().Register(#CLASS_NAME, CLASS_NAME::Create); \
	} \
	inline static int g_temp_##CLASS_NAME = RegisterComponent(); \


class IExtData
{
public:
	virtual void AttachComponents() = 0;
};

class IComponent
{
public:
	IComponent() {};
	virtual ~IComponent() {};

	/// <summary>
	/// ExtChanged is called when the ExtData has chaned like Techno's Type has changed.
	/// GameObject call
	/// </summary>
	virtual void ExtChanged() {};

	/// <summary>
	/// Clean is called when component instance back to object-pool or initialization.
	/// </summary>
	virtual void Clean() = 0;

	/// <summary>
	/// Awake is called when an enabled instance is being created.
	/// TechnoExt::ExtData() call
	/// </summary>
	virtual void Awake() {};

	/// <summary>
	/// Destroy is called when enabled instance is delete.
	/// </summary>
	virtual void Destroy() {};

	virtual void OnUpdate() {};
	virtual void OnUpdateEnd() {};
	virtual void OnWarpUpdate() {};

	/// <summary>
	/// OwnerIsRelease is called when TBase pointer is delete.
	/// </summary>
	virtual void OwnerIsRelease(void* ptr) {};

	/// <summary>
	/// ForeachEnd is called when Component::Foreach is end.
	/// </summary>
	virtual void OnForeachEnd() {};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) = 0;
	virtual bool Save(ExStreamWriter& stream) const = 0;
};

class Component : public IComponent
{
public:
	// 状态机状态，用于控制组件的生命周期。
	enum class ComponentState : uint8_t {
		Uninitialized 	= 0,  // 未初始化（刚创建或Clean后），对应 _awaked = false
		Active			= 1,  // 激活状态（已Awake，且未禁用），对应 _awaked = true, _disable = false
		Disabling		= 2   // 正在禁用（已Awake，但被标记为disable），对应 _awaked = true, _disable = true
		// - Initializing状态可以通过_awaked和m_State的组合判断
		// - Destroyed状态由_disable和对象池管理
	};

	Component() {}

	virtual ~Component() override
	{

		LOG_COMPONENT("Component %s is release.\n\n", thisName.c_str());

		// 如果还有父组件，说明没有正确从父组件移除
		if (_parent)
		{
			Debug::Log("CRITICAL: Component [%s] still has parent [%s] during destruction!\n",
				Name.c_str(), _parent->Name.c_str());
		}

		// 确保所有子组件已经被清理
		// 在正常流程中，子组件应该已经在 EnsureDestroy 或 DestroyChildComponents 中处理了
		// 如果还有子组件，说明没有正确销毁子组件
		if (!_children.empty())
		{
			Debug::Log("Error: Component [%s] still has %d children during destruction!\n",
					Name.c_str(), _children.size());

			// 紧急清理（直接回收，不触发事件）
			while (!_children.empty())
			{
				// 弹出最后一个
				Component* c = _children.back();
				_children.pop_back();
				// 销毁
				c->_parent = nullptr; // 移除父子关系
				c->_disable = true; // 标记为已销毁

				// 直接返回对象池，不触发事件
				c->FreeComponent();
			}
			_children.clear();
		}
	}

	std::string InstanceId; // 实例ID，用于读存档时使用，用于区分同名组件

	std::string Name;
	std::string Tag;

	// 生成实例ID
	void GenerateInstanceId()
	{
		if (InstanceId.empty())
		{
			static std::atomic<uint64_t> nextId{ 1 };
			InstanceId = std::to_string(nextId.fetch_add(1));
		}
	}

	// 重置组件以供复用
	void ResetForReuse()
	{
		// 重新生成实例ID
		InstanceId.clear();
		GenerateInstanceId();

		// 重置状态
		Clean();
	}

	std::string GetThisName() const
	{
		char buff[1024];

		// 安全地获取字符串
		const char* name = Name.empty() ? "Unnamed" : Name.c_str();
		const char* id = InstanceId.empty() ? "NoID" : InstanceId.c_str();

		sprintf_s(buff, "[%s]%p#%s", name, this, id);
		return buff;
	}
	__declspec(property(get = GetThisName)) std::string thisName;

	std::string extId{};
	std::string extName{};

	std::string baseId{};
	std::string baseName{};

	// 设置扩展数据
	void SetExtData(IExtData* extData);

	virtual void OnUpdate() override;

#pragma region status
	// 状态映射：
	// Uninitialized:	_awaked = false,_disable = false
	// Awaked:			_awaked = true,	_disable = false
	// Disabling:		_awaked = true,	_disable = true

	// 状态查询辅助方法
	bool IsUninitialized() const { return !_awaked; } // 未初始化
	bool IsAwaked() const { return _awaked && !_disable; } // 已激活
	bool IsDisabling() const { return _disable; } // 正在销毁
	bool IsEnable() const { return !_disable; } // 启用状态

	/// <summary>
	/// 唤醒组件，进入可用状态，包括子组件
	/// </summary>
	void EnsureAwaked();
	/// <summary>
	/// 销毁组件，释放资源，返回对象池
	/// </summary>
	void EnsureDestroy();
	/// <summary>
	/// 关闭组件，标记为失效，组件会在执行完Foreach后被移除
	/// </summary>
	void Disable();

	/// <summary>
	/// 激活组件，使其可以执行Foreach逻辑
	/// </summary>
	virtual void Activate() { _active = true; }
	/// <summary>
	/// 失活组件，使其跳过执行Foreach逻辑
	/// </summary>
	virtual void Deactivate() { _active = false; }
	virtual bool IsActive() const { return _active; }
#pragma endregion

#pragma region foreach
	/// <summary>
	/// execute action for each components in root (include itself)
	/// </summary>
	/// <param name="action"></param>
	template<typename F>
	void Foreach(F action)
	{
		// 执行全部
		int level = 0;
		int maxLevel = -1;
		ForeachLevel(action, level, maxLevel);

		OnForeachEnd();
	}

	template<typename F>
	void ForeachLevel(F action, int& level, int& maxLevel)
	{
		// 执行自身
		if (IsAwaked() && IsActive())
		{
			Component* _this = this;
			action(_this);
			int nextLevel = level + 1;
			if (maxLevel < 0 || nextLevel < maxLevel)
			{
#ifdef DEBUG_COMPONENT
				std::string s1 = "ForeachLevel _childrenInstanceIds: \n";
				std::vector<Component*> childrenCopy = _children;
				for (Component* c : childrenCopy)
				{
					s1.append("    - ").append(c->thisName.c_str()).append(", \n");
				}
#endif

				for (size_t i = 0; i < _children.size(); ++i)
				{
					Component* c = _children[i];
					if (!c) continue;
					// 检查组件是否仍在列表中，可能被移除
					if (i >= _children.size())
					{
						Debug::Log("Error: Component %s child [<%zu>] is removed during ForeachLevel!\n", thisName.c_str(), i);
#ifdef DEBUG_COMPONENT
						LOG_COMPONENT(s1.c_str());
#endif
						break;
					}
					if (_children[i] != c)
					{
						Debug::Log("Error: Component %s child [<%zu>] has changed during ForeachLevel!\n", thisName.c_str(), i);
#ifdef DEBUG_COMPONENT
						LOG_COMPONENT(s1.c_str());
#endif
						continue;
					}
					// 执行子组件
					c->ForeachLevel(action, nextLevel, maxLevel);
					if (c->IsBreak())
					{
						break;
					}
				}
			}
		}
		// 注意：不在这里调用ClearDisableComponent
		// 由GameObject的OnForeachEnd统一处理
		// ClearDisableComponent();
	}

	/// <summary>
	/// execute action for each child (exclude child's child)
	/// AE专用，无条件执行子组件，不检查active状态，只检查disable状态
	/// </summary>
	/// <param name="action"></param>
	template<typename F>
	void ForeachChild(F action, bool force = false)
	{
		for (size_t i = 0; i < _children.size(); ++i)
		{
			Component* c = _children[i];
			if (!c) continue;
			// 检查组件是否仍在列表中，可能被移除
			if (i >= _children.size())
			{
				Debug::Log("Error: Component %s child [<%zu>] is removed during ForeachChild!\n", thisName.c_str(), i);
				break;
			}
			if (_children[i] != c)
			{
				Debug::Log("Error: Component %s child [<%zu>] has changed during ForeachChild!\n", thisName.c_str(), i);
				continue;
			}
			if (force)
			{
				Debug::Log("ForeachChild force action for %s\n", c->thisName.c_str());
				action(c);
			}
			else
			{
				if (c->IsAwaked() && c->IsEnable())
				{
					action(c);
					if (c->IsBreak())
					{
						break;
					}
				}
			}
		}
		// for (Component* c : _children)
		// {
		// 	if (!c) continue;
		// 	if (c->IsAwaked() and c->IsEnable())
		// 	{
		// 		action(c);
		// 		if (!force)
		// 		{
		// 			if (c->IsBreak())
		// 			{
		// 				break;
		// 			}
		// 		}
		// 	}
		// }
	}

	void Break() { _break = true; }

	bool IsBreak()
	{
		if (_break)
		{
			_break = false;
			return true;
		}
		return _break;
	}
#pragma endregion

#pragma region manager

	/// <summary>
	/// 将Component加入子列表，同时赋予自身储存的IExtData
	/// </summary>
	void AddComponent(Component* component, int index = -1);

	Component* AddComponent(const std::string& name, int index = -1);

	// <summary>
	// 将Component加入子列表中移除
	// </summary>
	void RemoveComponent(Component* component, bool disable = true);

	/// <summary>
	/// 在结束循环后需要从_children中清理已经标记为disable的component
	/// </summary>
	void ClearDisableComponent();

	/// <summary>
	/// 从存档中恢复子组件列表
	/// </summary>
	void RestoreComponent();


#ifdef DEBUG
	struct ComponentInfo
	{
	public:
		std::string Name;
		bool Enable;
		bool Active;
	};
	void GetComponentInfos(std::vector<ComponentInfo>& infos, int& level)
	{
		// 自己
		std::string name = "";
		if (level > 1)
		{
			for (int i = 0; i < level - 1; i++)
			{
				name.append("  ");
			}
		}
		if (level > 0)
		{
			name.append("--");
		}
		name.append(this->Name);
		if (!this->Tag.empty())
		{
			name.append("#").append(this->Tag);
		}
		ComponentInfo info{ name, IsEnable(), IsActive() };
		infos.push_back(info);
		ForeachChild([&infos, &level](Component* c) {
			int l = level + 1;
			c->GetComponentInfos(infos, l);
			});
	}
#endif

	Component* GetComponentInParentByName(const std::string& name);
	Component* GetComponentInChildrenByName(const std::string& name);

	/// <summary>
	/// 查找子组件，深度优先
	/// </summary>
	Component* GetComponentByName(const std::string& name);

	template <typename TComponent>
	TComponent* GetComponentInParent()
	{
		static_assert(std::is_base_of<Component, TComponent>::value, "TComponent must inherit from Component");

		if (!_parent)
		{
			return nullptr;
		}

		// 在兄弟组件中查找
		for (Component* c : _parent->_children)
		{
			if (c != this)
			{
				if (TComponent* typed = dynamic_cast<TComponent*>(c))
				{
					return typed;
				}
			}
		}
		// 在父组件的父节点中查找
		return _parent->GetComponentInParent<TComponent>();
	}

	template <typename TComponent>
	TComponent* GetComponentInChildren()
	{
		static_assert(std::is_base_of<Component, TComponent>::value, "TComponent must inherit from Component");

		// 在当前层级查找
		for (Component* child : _children)
		{
			if (TComponent* typed = dynamic_cast<TComponent*>(child))
			{
				return typed;
			}
		}
		// 在子组件中查找，深度优先
		for (Component* child : _children)
		{
			if (TComponent* found = child->GetComponentInChildren<TComponent>())
			{
				return found;
			}
		}
		return nullptr;
	}

	template <typename TComponent>
	TComponent* GetComponent()
	{
		return GetComponentInChildren<TComponent>();
	}

	// 父子关系
	Component* GetParent() { return _parent; }
	const std::vector<Component*>& GetChildren() { return _children; }

	// 组件查找
	Component* FindOrAllocate(const std::string& name);

	template <typename TScript>
	TScript* FindOrAttach()
	{
		static_assert(std::is_base_of<Component, TScript>::value, "TScript must inherit from Component");

		// 先查找
		TScript* found = GetComponent<TScript>();
		if (found)
		{
			return dynamic_cast<TScript*>(found);
		}

		// 通过名称创建
		const std::string& scriptName = TScript::ScriptName;
		Component* newComponent = FindOrAllocate(scriptName);

		// 安全的类型转换
		return dynamic_cast<TScript*>(newComponent);
	}

	void AttachToComponent(Component* component);

	void DetachFromParent(bool disable = true);

#pragma endregion

	Component& operator=(const Component& other)
	{
		if (this != &other)
		{
			Name = other.Name;
			Tag = other.Tag;

			// 只赋值控制位，不赋值上下层关系
			_awaked = other._awaked;
			_disable = other._disable;
			_active = other._active;
			_break = other._break;
		}
		return *this;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream, bool isLoad)
	{
		// 储存Component的控制参数
		stream
			// 存取子组件清单
			.Process(this->_childrenNames)
			.Process(this->_childrenInstanceIds) // 存取子组件实例ID，用于存档时恢复

			// 存取Component自身的属性
			.Process(this->InstanceId)
			.Process(this->Name)
			.Process(this->Tag)
			// 每次读档之后，所有的Component实例都是重新创建的，不从存档中读取，只获取事件控制
			.Process(this->_awaked)
			.Process(this->_disable)
			.Process(this->_active)
			.Process(this->_break);
		return stream.Success();
	}

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		// 读取自身的属性，包括排序好的_childrenNames和_childrenInstanceIds
		bool loaded = this->Serialize(stream, true);
		if (loaded)
		{
			// 验证数据完整性
			if (this->_childrenNames.size() != this->_childrenInstanceIds.size())
			{
				Debug::Log("Error: Load failed! Children names and instance ids mismatch!");
				return false;
			}
			// 根据子组件清单恢复
			RestoreComponent();

#ifdef DEBUG_COMPONENT
		if (!this->_children.empty())
		{
			std::string s1 = "Load _childrenInstanceIds: ";
			for (Component* c : this->_children)
			{
				s1.append(c->InstanceId).append(", ");
			}
			s1.append("\n");
			LOG_COMPONENT(s1.c_str());
		}
#endif

			// 读取每个子组件的内容
			// this->ForeachChild([&stream, &registerForChange](Component* c) { c->Load(stream, registerForChange); }); // ForeachChild会跳过未激活组件
			for (Component* c : this->_children)
			{
				c->Load(stream, registerForChange);
			}
		}
		return loaded;
	}

	virtual bool Save(ExStreamWriter& stream) const override
	{
		Component* pThis = const_cast<Component*>(this);

		// 生成子组件清单
		pThis->_childrenNames.clear();
		pThis->_childrenInstanceIds.clear();
		// 重要：需要与储存的子组件用一样的条件
		pThis->ForeachChild([&pThis](Component* c) {
			pThis->_childrenNames.push_back(c->Name);
			pThis->_childrenInstanceIds.push_back(c->InstanceId);
			});

#ifdef DEBUG_COMPONENT
		if (!pThis->_childrenInstanceIds.empty())
		{
			std::string s1 = "Saved _childrenInstanceIds: ";
			for_each(pThis->_childrenInstanceIds.begin(), pThis->_childrenInstanceIds.end(), [&](const std::string& s) {
				s1.append(s).append(", ");
				});
			s1.append("\n");
			LOG_COMPONENT(s1.c_str());
		}
#endif

		bool saved = pThis->Serialize(stream, false);
		// 存入每个子组件的内容
		pThis->ForeachChild([&stream](Component* c) { c->Save(stream); });
		return saved;
	}
#pragma endregion

	// 克隆支持
	virtual Component* Clone() const;
	virtual void CopyFrom(const Component& other);

	// 清理
	virtual void Clean() override
	{
		LOG_COMPONENT("Component::Clean called for %p\n", this);

		Tag.clear();

		// Ext由ScriptFactory传入
		_extData = nullptr;

		_awaked = false; // 已经唤醒，可以使用
		_disable = false; // 已经失效，等待移除
		_active = true; // 可以执行循环
		_break = false; // 中断上层循环

		// 添加的Component名单，在存档时生成
		_childrenNames.clear();

		_parent = nullptr;
		_children.clear();

		// 清理调试信息
		extId.clear();
		extName.clear();
		baseId.clear();
		baseName.clear();
	}

protected:
	friend class ComponentPool; // 对象池管理

	// Ext由ScriptFactory传入
	IExtData* _extData = nullptr;

	bool _awaked = false; // 已经唤醒，可以使用
	bool _disable = false; // 已经失效，等待移除
	bool _active = true; // 可以执行循环
	bool _break = false; // 中断上层循环

	// 添加的Component名单，在存档时生成
	mutable std::vector<std::string> _childrenNames{};
	mutable std::vector<std::string> _childrenInstanceIds{};

	Component* _parent = nullptr;
	std::vector<Component*> _children{};

	// 对象池相关
	virtual void FreeComponent() = 0;
};

// 组件工厂 - 单例模式
class ComponentFactory
{
public:
	static ComponentFactory& GetInstance()
	{
		static ComponentFactory instance;
		return instance;
	}

	// using ComponentCreator = std::function<Component* (void)>;
	using ComponentCreator = Component * (*) (void);

	int Register(const std::string& name, ComponentCreator creator)
	{
		_creatorMap.insert(make_pair(name, creator));
		return 0;
	}

	Component* Create(const std::string& name)
	{
		auto it = _creatorMap.find(name);
		if (it != _creatorMap.end())
		{
			// 直接调用注册的创建函数，已指向对象池的Acquire
			Component* c = it->second();
			if (c)
			{
				c->Name = name;
				LOG_COMPONENT("Create Component %s.\n", c->thisName.c_str());
			}
			return c;
		}
		return nullptr;
	}

	void PrintCreaterInfo()
	{
		if (!_creatorMap.empty())
		{
			Debug::Log("Component List:\n");
			for (auto it : _creatorMap)
			{
				std::string scriptName = it.first;
				Debug::Log(" -- %s\n", scriptName.c_str());
			}
		}
	}


private:
	ComponentFactory() {};
	~ComponentFactory() {};

	ComponentFactory(const ComponentFactory&) = delete;

	std::map<std::string, ComponentCreator> _creatorMap{};
};
