#pragma once

#include <stack>
#include <memory>
#include <unordered_map>
#include <map>
#include <set>
#include <typeindex>

#include <Common/EventSystems/EventSystem.h>

#include <Utilities/Debug.h>

// 前置声明
class Component;

// 组件对象池管理器 - 支持转移
class ComponentPool
{
public:
	static ComponentPool& GetInstance();

	// 通过名称创建
	Component* Create(const std::string& name);

	void ClearAll();

	static void Clear(EventSystem* sender, Event e, void* args);

	// 从对象池获取组件
	template<typename T>
	T* Acquire()
	{
		static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");

#ifdef DEBUG_COMPONENT
		static int callCount = 0;
		callCount++;
		Debug::Log("=== ComponentPool::Acquire< %s > START call #%d ===\n",
			typeid(T).name(), callCount);

		// 添加内存状态检查
		// Debug::Log("  - sizeof(T) = %zu\n", sizeof(T));
		// Debug::Log("  - alignof(T) = %zu\n", alignof(T));

			// 添加类型验证
		static_assert(std::has_virtual_destructor<T>::value, "Component must have virtual destructor");

		const char* typeName = typeid(T).name();
		size_t typeHash = typeid(T).hash_code();

		Debug::Log("Acquire: name=%s, hash=%zu\n", typeName, typeHash);

		// 记录已分配类型
		static std::set<size_t> allocatedTypes;
		if (allocatedTypes.find(typeHash) == allocatedTypes.end())
		{
			allocatedTypes.insert(typeHash);
			Debug::Log("First allocation for type hash %zu\n", typeHash);
		}
#endif

		void* memory = AllocateRaw(typeid(T), sizeof(T));
		if (!memory)
		{
			Debug::Log("ERROR: ComponentPool failed to allocate memory for %s\n",
				typeid(T).name());
			throw std::bad_alloc();
		}

#ifdef DEBUG_COMPONENT
		// Debug::Log("  - Memory allocated at %p, size=%zu, align=%zu\n", memory, sizeof(T), alignof(T));
		// Debug::Log("  - Memory region check: [%p, %p]\n", memory, (char*)memory + sizeof(T) - 1);
		// Debug::Log("  - Allocated memory at %p\n", memory);
		// Debug::Log("  - Memory alignment check: %s\n", (reinterpret_cast<uintptr_t>(memory) % alignof(T) == 0) ? "OK" : "BAD");
#endif

		// 使用placement new构造
		T* obj = nullptr;
		try
		{
			// 分离构造和Clean的日志
#ifdef DEBUG_COMPONENT
			// Debug::Log("  - Constructing object...\n");
#endif
			obj = new (memory) T();

#ifdef DEBUG_COMPONENT
			// Debug::Log("  - Object constructed at %p\n", obj);
			// Debug::Log("  - vtable ptr = %p\n", *(void**)obj);  // 检查虚函数表
#endif

			// 重置组件以供复用，清理实例ID
			obj->ResetForReuse();
		}
		catch (const std::exception& e)
		{
			Debug::Log("ERROR: Exception during construction/Clean of %s: %s\n",
				typeid(T).name(), e.what());
			throw;
		}
		catch (...)
		{
			Debug::Log("ERROR: Unknown exception during construction/Clean of %s\n",
				typeid(T).name());
			throw;
		}

#ifdef DEBUG_COMPONENT
		Debug::Log("=== ComponentPool::Acquire< %s > END call #%d ===\n", typeid(T).name(), callCount);
#endif

		return obj;
	}

	// 释放组件到对象池
	template<typename T>
	void Release(T* component)
	{
		if (!component) return;

		component->Clean();  // 重置状态
		component->~T();
		DeallocateRaw(typeid(T), component);
	}

	// 转移操作
	template<typename T>
	std::shared_ptr<T> TransferOwnership(T* component)
	{
		if (!component) return nullptr;

		// 创建新实例并拷贝数据
		T* newComponent = Acquire<T>();
		newComponent->CopyFrom(*component);

		// 转移extData
		if (component->extData)
		{
			newComponent->SetExtData(component->extData);
		}

		// 释放原组件到对象池
		Release(component);

		// 返回智能指针
		return std::shared_ptr<T>(newComponent, [this](T* ptr) {
			Release(ptr);
			});
	}


	template<typename T>
	void TransferToPool(T* component)

	{
		if (!component) return;

		component->Clean();  // 重置状态
		DeallocateRaw(typeid(T), component);
	}

	// 预分配
	template<typename T>
	void Preallocate(size_t count)
	{
		auto& pool = m_pools[std::type_index(typeid(T))];
		pool.objectSize = sizeof(T);

		for (size_t i = 0; i < count; ++i)
		{
			void* memory = std::malloc(sizeof(T));
			if (memory)
			{
				pool.objects.push(memory);
				pool.totalCreated++;
			}
		}
	}

private:
	ComponentPool();
	~ComponentPool();

	struct Pool
	{
		std::stack<void*> objects;
		size_t objectSize = 0;
		size_t totalCreated = 0;
		size_t totalReused = 0;
	};

	using PoolMap = std::unordered_map<std::type_index, Pool>;
	PoolMap m_pools;

	void* AllocateRaw(const std::type_info& type, size_t size);
	void DeallocateRaw(const std::type_info& type, void* ptr);
};
