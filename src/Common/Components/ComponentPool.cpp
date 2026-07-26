#include "ComponentPool.h"
#include "Component.h"
#include <cstdlib>
#include <cstring>

ComponentPool::ComponentPool() = default;

ComponentPool::~ComponentPool()
{
	ClearAll();
}

ComponentPool& ComponentPool::GetInstance()
{
	static ComponentPool instance;
	return instance;
}

void* ComponentPool::AllocateRaw(const std::type_info& type, size_t size)
{
	auto& pool = m_pools[std::type_index(type)];
	pool.objectSize = size;

	if (!pool.objects.empty())
	{
		void* ptr = pool.objects.top();
		pool.objects.pop();
		pool.totalReused++;

		LOG_COMPONENT("ComponentPool: Reused [%s], pool size = %zu, created = %zu, reused = %zu\n", type.name(), pool.objects.size(), pool.totalCreated, pool.totalReused);
		return ptr;
	}

	// 分配新内存
	void* ptr = std::malloc(size);
	if (ptr)
	{
		pool.totalCreated++;

		LOG_COMPONENT("ComponentPool: Created new [%s], total created = %zu\n", type.name(), pool.totalCreated);
	}
	else
	{
		// 内存不足
		LOG_COMPONENT("ERROR: ComponentPool failed to allocate %zu bytes for %s\n", size, type.name());
	}

	return ptr;
}

void ComponentPool::DeallocateRaw(const std::type_info& type, void* ptr)
{
	if (!ptr) return;

#ifdef DEBUG_COMPONENT
	const char* typeName = type.name();
	size_t typeHash = std::type_index(type).hash_code();

	LOG_COMPONENT("DeallocateRaw: type=%s, hash=%zu, ptr=%p\n", typeName, typeHash, ptr);
#endif

	// 检查是否已有池存在，并添加到池中或直接释放内存
	auto it = m_pools.find(std::type_index(type));
	if (it != m_pools.end())
	{
		LOG_COMPONENT("Found pool: size=%zu, created=%zu, reused=%zu\n", it->second.objects.size(), it->second.totalCreated, it->second.totalReused);

		constexpr size_t MAX_POOL_SIZE = 1000;
		if (it->second.objects.size() < MAX_POOL_SIZE)
		{
			it->second.objects.push(ptr);
			// 添加监控日志
			LOG_COMPONENT("ComponentPool: Type [%s] pool size = %zu\n", type.name(), it->second.objects.size());
			return;
		}
		else
		{
			LOG_COMPONENT("ComponentPool: Type [%s] pool full (%zu), freeing memory\n", type.name(), MAX_POOL_SIZE);
		}
	}
#ifdef DEBUG_COMPONENT
	else
	{
        LOG_COMPONENT("Warning: No pool found for type %s (hash %zu)\n", typeName, typeHash);
	}
#endif

	// 池已满，直接释放
	std::free(ptr);
}

Component* ComponentPool::Create(const std::string& name)
{
	return ComponentFactory::GetInstance().Create(name);
}

void ComponentPool::ClearAll()
{
	for (auto& kv : m_pools)
	{
		Pool& pool = kv.second;
		while (!pool.objects.empty())
		{
			std::free(pool.objects.top());
			pool.objects.pop();
		}
		pool.totalCreated = 0;
		pool.totalReused = 0;
	}

	Debug::Log("Cleared all Component pools.\n");
}

void ComponentPool::Clear(EventSystem* sender, Event e, void* args)
{
	GetInstance().ClearAll();
}

