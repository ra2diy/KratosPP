#pragma once

#include <SuperWeaponTypeClass.h>

#include "TypeExtension.h"

class SuperWeaponTypeExt : public TypeExtension<SuperWeaponTypeClass, SuperWeaponTypeExt>
{
public:
	/// @brief 储存一些通用设置或者其他平台的设置
	class TypeData : public INIConfig
	{
	public:
		virtual void Read(INIBufferReader* ini) override
		{
		}

		// Ares
	};

	static constexpr DWORD Canary = 0x77777777;
	// static constexpr size_t ExtPointerOffset = 0x18;

	static SuperWeaponTypeExt::ExtContainer ExtMap;
};
