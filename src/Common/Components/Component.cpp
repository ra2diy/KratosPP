#include "Component.h"
#include "ComponentPool.h"



void Component::SetExtData(IExtData* extData)
{
	_extData = extData;
	for (Component* c : _children)
	{
		c->SetExtData(extData);
	}
}


void Component::OnUpdate()
{
	IComponent::OnUpdate();
}

void Component::EnsureAwaked()
{
	int callId = 0;
#ifdef DEBUG_COMPONENT
	static std::atomic<int> globalCallId{ 0 };
	callId = ++globalCallId;

	LOG_COMPONENT("[EnsureAwaked #%d START] %s, _awaked=%d.\n", callId, thisName.c_str(), _awaked);
#endif // DEBUG_COMPONENT

	if (!_extData) {
		// 如果触发，说明组件被唤醒时已脱离GameObject。
		Debug::Log("致命错误：组件 [%s]%p 的 _extData 为 NULL！\n", Name.c_str(), this);
		// 可以选择安全地跳过初始化或自我销毁
		this->Disable();
		return;
	}

	// 如果已经在Active或Disabling状态，直接返回
	if (IsAwaked() || IsDisabling())
	{
		LOG_COMPONENT("[EnsureAwaked #%d] Component already awaked (state: %s)\n", callId, _disable ? "Disabling" : "Active");
		return;
	}

#ifdef DEBUG_COMPONENT
	// 获取调用栈
	void* returnAddress = _ReturnAddress();
	LOG_COMPONENT("[EnsureAwaked #%d] Caller: %p\n", callId, returnAddress);
#endif // DEBUG_COMPONENT

	// 尝试执行Awake
	try {
		LOG_COMPONENT("[EnsureAwaked #%d] Calling Awake()\n", callId);

		Awake();
		_awaked = true;

		// 检查Awake过程中是否被禁用
		if (_disable)
		{
			// 标记为 Disabling
			LOG_COMPONENT("[EnsureAwaked #%d] Component disabled during Awake, marking as Disabling\n", callId);
		}
		else {
			// 成功激活
			LOG_COMPONENT("[EnsureAwaked #%d] Component activated successfully\n", callId);
		}
	}
	catch (...)
	{
		// 发生异常，回滚状态
		LOG_COMPONENT("[EnsureAwaked #%d] Exception in Awake(), state reset\n", callId);
		throw;
	}

	// 唤醒子组件（不清理disable的组件）
	std::vector<Component*> childrenToProcess;
	childrenToProcess.reserve(_children.size());
	for (Component* child : _children) {
		if (child->IsUninitialized()) {
			childrenToProcess.push_back(child);
		}
	}

	for (Component* child : childrenToProcess) {
		child->EnsureAwaked();
	}
	LOG_COMPONENT("[EnsureAwaked #%d END] %s, _awaked=%d\n", callId, thisName.c_str(), _awaked);
}

void Component::EnsureDestroy()
{
    // 不再检查IsDisabling()，允许从Disabling状态进入销毁流程
#ifdef DEBUG_COMPONENT
	static std::set<Component*> destroyingSet;
	if (destroyingSet.find(this) != destroyingSet.end()) {
		LOG_COMPONENT("Warning: EnsureDestroy recursion for %s\n",
			thisName.c_str());
		return;
	}
	destroyingSet.insert(this);

	LOG_COMPONENT("Component %s - EnsureDestroy START, state: Disable=%d, Awaked=%d, children=%zu\n", thisName.c_str(), _disable, _awaked, _children.size());
#endif

	// 标记为Disabling状态，可以回收
	_disable = true;

	// 从父组件移除自己
	if (_parent)
	{
		Component* parent = _parent;
		_parent = nullptr;
		parent->RemoveComponent(this, false);
	}

	// 保存子组件列表，然后清空
	std::vector<Component*> childrenCopy = _children;
	_children.clear();

	// 递归销毁子组件
	for (Component* child : childrenCopy)
	{
		if (child) {
			child->_parent = nullptr;
			child->EnsureDestroy();
		}
	}

	// 销毁自己
	try
	{
		Destroy();
	}
	catch (...)
	{
		Debug::Log("Error: EXCEPTION in Destroy() for %s\n", thisName.c_str());
	}

	// 释放自己到对象池
	FreeComponent();

#ifdef DEBUG_COMPONENT
    destroyingSet.erase(this);
    LOG_COMPONENT("Component %s - EnsureDestroy END\n", thisName.c_str());
#endif
}

void Component::Disable()
{
	// 对于Disabling状态，什么都不做（已经在销毁过程中）
	if (_disable) return;

	// 关闭自己
	_disable = true;
	// 下属所有子组件也一并Disable
	for (Component* c : _children)
	{
		c->Disable();
	}
}

/// <summary>
/// 将Component加入子列表，同时赋予自身储存的IExtData
/// </summary>
void Component::AddComponent(Component* component, int index)
{
	if (!component) return;

	// 将要加入的组件的子组件的extData全部更换
	component->SetExtData(_extData);
	component->_parent = this;
	if (index < 0 || index >= (int)_children.size())
	{
		_children.push_back(component);
	}
	else
	{
		// 插入指定位置
		auto it = _children.begin();
		std::advance(it, index);
		_children.insert(it, component);
	}
	LOG_COMPONENT("Add Component %s to %s.\n", component->thisName.c_str(), this->thisName.c_str());
}

Component* Component::AddComponent(const std::string& name, int index)
{
	try
	{
		Component* c = ComponentPool::GetInstance().Create(name);
		if (c)
		{
			AddComponent(c, index);
		}
		return c;
	}
	catch (const std::exception& e)
	{
		Debug::Log("Exception when creating component %s: %s\n", name.c_str(), e.what());
	}
	catch (...)
	{
		Debug::Log("Unknown exception when creating component %s\n", name.c_str());
	}
	return nullptr;
}

// <summary>
// 将Component从子列表中移除
// </summary>
void Component::RemoveComponent(Component* component, bool disable)
{
	auto it = std::find(_children.begin(), _children.end(), component);
	if (it != _children.end())
	{
		LOG_COMPONENT("Remove Component %s from %s.\n", component->thisName.c_str(), this->thisName.c_str());
		Component* c = *it;
		c->_parent = nullptr;
		if (disable)
		{
			c->Disable();
		}
		// 从_children清单中删除
		_children.erase(it);
	}
}


/// <summary>
/// 在结束循环后需要从_children中清理已经标记为disable的component
/// 由GameObject调用，逐层清理
/// 当一个父节点失效时，其子组件会被强制失效并回收
/// </summary>
void Component::ClearDisableComponent()
{
// #ifdef DEBUG_COMPONENT
// 	size_t initialChildren = _children.size();
// 	LOG_COMPONENT("ClearDisableComponent START for %s, children: %zu\n", thisName.c_str(), initialChildren);
// #endif
	size_t i = 0;
	while (i < _children.size())
	{
		Component* child = _children[i];

		if (!child)
		{
			// 移除空指针
			Debug::Log("Warning: ClearDisableComponent found null child in %s, removing.\n", thisName.c_str());
			_children.erase(_children.begin() + i);
			continue;
		}

		// 先递归清理子组件的子组件
		child->ClearDisableComponent();

		if (child->IsDisabling())
		{
			LOG_COMPONENT("Clear disable Component %s from %s.\n", child->thisName.c_str(), this->thisName.c_str());

			// 移除父子关系
			child->_parent = nullptr;

			// 从向量中移除
			_children.erase(_children.begin() + i);

			// 销毁组件
			child->EnsureDestroy();
			// 不增加i，因为移除了一个元素，下一个元素会移动到当前位置
		}
		else
		{
			++i;  // 移动到下一个元素
		}
	}
// #ifdef DEBUG_COMPONENT
// 	LOG_COMPONENT("ClearDisableComponent END for %s, remaining children: %zu\n", thisName.c_str(), _children.size());
// #endif
}


/// <summary>
/// 从存档中恢复子组件列表
/// </summary>
void Component::RestoreComponent()
{
	// 1. 创建InstanceId到组件的映射
	std::unordered_map<std::string, Component*> instanceIdMap;
	for (Component* c : _children)
	{
		if (!c->IsDisabling())
		{
			// 确保组件有InstanceId
			if (c->InstanceId.empty())
			{
				c->GenerateInstanceId();
			}
			instanceIdMap[c->InstanceId] = c;
		}
	}

	// 2. 准备新的组件列表（按照存档顺序）
	std::vector<Component*> newChildren;
	newChildren.reserve(_childrenInstanceIds.size());

	// 3. 按照存档顺序精确匹配
	for (size_t i = 0; i < _childrenInstanceIds.size(); ++i)
	{
		const std::string& name = _childrenNames[i];
		const std::string& targetInstanceId = _childrenInstanceIds[i];

		Component* component = nullptr;
		auto it = instanceIdMap.find(targetInstanceId);

		if (it != instanceIdMap.end())
		{
			// 找到精确匹配的组件
			component = it->second;

			// 验证名称是否匹配
			if (component->Name != name)
			{
				Debug::Log("Info: Component instance %s renamed from %s to %s\n", targetInstanceId.c_str(), component->Name.c_str(), name.c_str());
				component->Name = name; // 更新为存档中的名称
			}

			// 从映射中移除，避免重复使用
			instanceIdMap.erase(it);
		}
		else
		{
			// 没有找到精确匹配，需要创建新组件
			LOG_COMPONENT("RestoreStructure: Creating new Component [%s] with InstanceId %s\n", name.c_str(), targetInstanceId.c_str());

			component = ComponentPool::GetInstance().Create(name);
			if (component)
			{
				// 设置存档中的InstanceId
				component->InstanceId = targetInstanceId;

				// 添加到结构
				component->SetExtData(_extData);
				component->_parent = this;

				LOG_COMPONENT("Add Component %s to %s (structure only).\n", component->thisName.c_str(), thisName.c_str());
			}
		}

		if (component)
		{
			newChildren.push_back(component);
		}
	}

	// 4. 销毁不再需要的组件（在instanceIdMap中剩余的）
	for (auto& [instanceId, component] : instanceIdMap)
	{
		if (!component->IsDisabling())
		{
			LOG_COMPONENT("RestoreStructure: Destroying unused component %s (InstanceId: %s)\n", component->thisName.c_str(), instanceId.c_str());
			component->_parent = nullptr;
			component->EnsureDestroy();
		}
	}

	// 5. 更新_children（已经按照存档顺序）
	_children = std::move(newChildren);
}



Component* Component::GetComponentInParentByName(const std::string& name)
{
	if (!_parent)
	{
		return nullptr;
	}
	// 在兄弟组件中查找
	for (Component* c : _parent->_children)
	{
		if (c != this && c->Name == name)
		{
			return c;
		}
	}
	// 在父组件的父节点中查找
	return _parent->GetComponentInParentByName(name);
}

Component* Component::GetComponentInChildrenByName(const std::string& name)
{
	// 在当前层级查找
	for (Component* c : _children)
	{
		if (c->Name == name)
		{
			return c;
		}
	}
	// 在子组件中查找，深度优先
	for (Component* c : _children)
	{
		Component* found = c->GetComponentInChildrenByName(name);
		if (found)
		{
			return found;
		}
	}
	return nullptr;
}

Component* Component::GetComponentByName(const std::string& name)
{
	return GetComponentInChildrenByName(name);
}

// 组件查找
Component* Component::FindOrAllocate(const std::string& name)
{
	Component* c = GetComponentByName(name);
	if (!c)
	{
		// 添加新的Component
		c = AddComponent(name);
		if (c)
		{
			// 激活新的Component
			c->EnsureAwaked();
		}
	}
	return c;
}


void Component::AttachToComponent(Component* component)
{
	if (!component || component == this)
	{
		return;
	}
	DetachFromParent();
	component->AddComponent(this);
}

int Component::GetIndexInParent() const
{
	if (!_parent)
	{
		return -1;
	}
	for (size_t i = 0; i < _parent->_children.size(); ++i)
	{
		if (_parent->_children[i] == this)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

bool Component::MoveTo(Component* newParent, int index)
{
	if (!newParent)
	{
		Debug::Log("Error: Component [%s]%p MoveTo a null parent.\n", thisName.c_str(), this);
		return false;
	}
	if (newParent == this)
	{
		Debug::Log("Error: Component [%s]%p MoveTo itself.\n", thisName.c_str(), this);
		return false;
	}
	if (IsDisabling())
	{
		Debug::Log("Error: Component [%s]%p is disabling, refuse to MoveTo.\n", thisName.c_str(), this);
		return false;
	}
	// 禁止把祖先移动到自己的后代下，避免形成环
	for (Component* p = newParent; p; p = p->_parent)
	{
		if (p == this)
		{
			Debug::Log("Error: Component [%s]%p MoveTo its descendant [%s]%p.\n", thisName.c_str(), this, p->thisName.c_str(), p);
			return false;
		}
	}
	if (_parent == newParent)
	{
		return true;
	}

	Component* oldParent = _parent;
	int targetIndex = index;
	if (targetIndex < 0)
	{
		// 未指定位置时，保持本组件在旧父中的位置
		targetIndex = GetIndexInParent();
	}
	if (oldParent)
	{
		// 不 disable，只是换父
		oldParent->RemoveComponent(this, false);
	}

	try
	{
		newParent->AddComponent(this, targetIndex);
		return true;
	}
	catch (...)
	{
		Debug::Log("Error: Component [%s]%p MoveTo [%s]%p failed, rollback to [%s]%p.\n",
			thisName.c_str(), this, newParent->thisName.c_str(), newParent,
			oldParent ? oldParent->thisName.c_str() : "(null)", oldParent);
		// AddComponent 可能已设置 _parent 后才抛异常，先彻底摘除，避免留下悬空父子关系
		if (_parent)
		{
			Component* current = _parent;
			current->RemoveComponent(this, false);
			if (_parent == current)
			{
				_parent = nullptr;
			}
		}
		if (oldParent)
		{
			try
			{
				oldParent->AddComponent(this, targetIndex);
			}
			catch (...)
			{
				Debug::Log("Error: Component [%s]%p MoveTo rollback failed!\n", thisName.c_str(), this);
			}
		}
		return false;
	}
}

void Component::DetachFromParent(bool disable)
{
	if (_parent)
	{
		_parent->RemoveComponent(this, disable);
	}
}


Component* Component::Clone() const
{
	// 通过工厂创建新实例
	Component* newComponent = ComponentFactory::GetInstance().Create(this->Name);

	if (newComponent)
	{
		newComponent->CopyFrom(*this);
	}

	return newComponent;
}

void Component::CopyFrom(const Component& other)
{
	// 使用拷贝赋值运算符
	*this = other;

	// 复制子组件
	_children.clear();
	for (Component* child : other._children)
	{
		Component* clonedChild = child->Clone();
		if (clonedChild)
		{
			clonedChild->_parent = this;
			_children.push_back(clonedChild);
		}
	}
}

void Component::FreeComponent()
{
	// 检查是否已经通过EnsureDestroy释放
	if (!_disable)
	{
		// 这是严重的编程错误，但为了程序稳定性，我们仍然处理
		Debug::Log("Error: FreeComponent called on active component %s. Forcing disable.\n", Name.c_str());
		_disable = true;

		// 紧急调用Destroy()，但不递归调用EnsureDestroy
		try {
			Destroy();
		} catch (...) {
			Debug::Log("Error in emergency Destroy() for %s\n", Name.c_str());
		}
	}
	ComponentPool::GetInstance().Release(this);
}
