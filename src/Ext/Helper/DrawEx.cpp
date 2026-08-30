#include "DrawEx.h"

#include <MapClass.h>

#include <Extension/EBoltExt.h>
#include <Ext/EBoltType/EBoltStatus.h>
#include <Ext/Helper/Scripts.h>

#include <Ext/Common/CommonStatus.h>

int GetDefaultColor_Int(ConvertClass* pConvert, int idx)
{
	if (pConvert->BytesPerPixel == 1)
	{
		return reinterpret_cast<uint8_t*>(pConvert->PaletteData)[idx];
	}
	else
	{
		return reinterpret_cast<uint16_t*>(pConvert->PaletteData)[idx];
	}
}

#pragma region LaserType
void DrawLaser(LaserType laser, CoordStruct sourcePos, CoordStruct targetPos, ColorStruct houseColor)
{
	ColorStruct innerColor = laser.InnerColor;
	ColorStruct outerColor = laser.OuterColor;
	if (laser.IsHouseColor)
	{
		innerColor = houseColor;
		outerColor = Colors::Empty;
	}
	if (laser.IsSingleColor)
	{
		outerColor = Colors::Empty;
	}
	if (laser.RandomColor || laser.ColorListRandom)
	{
		int count = laser.ColorList.size();
		if (count > 0)
		{
			// 随机颜色列表中的颜色
			int index = Random::RandomRanged(0, count - 1);
			innerColor = laser.ColorList[index];
			if (!laser.IsSingleColor)
			{
				int r = innerColor.R / 2;
				int g = innerColor.G / 2;
				int b = innerColor.B / 2;
				outerColor = ColorStruct{ static_cast<BYTE>(r), static_cast<BYTE>(g), static_cast<BYTE>(b) };
			}
		}
		else
		{
			// 随机产生一个颜色
			int r = Random::RandomRanged(1, 255);
			int g = Random::RandomRanged(1, 255);
			int b = Random::RandomRanged(1, 255);
			innerColor = ColorStruct{ static_cast<BYTE>(r), static_cast<BYTE>(g), static_cast<BYTE>(b) };
			if (!laser.IsSingleColor)
			{
				int r2 = r / 2;
				int g2 = g / 2;
				int b2 = b / 2;
				outerColor = ColorStruct{ static_cast<BYTE>(r2), static_cast<BYTE>(g2), static_cast<BYTE>(b2) };
			}
		}
	}
	// 视觉散布
	if (laser.VisualScatter)
	{
		targetPos = MapClass::GetRandomCoordsNear(targetPos, Random::RandomRanged(laser.VisualScatterMin, laser.VisualScatterMax), false);
	}
	LaserDrawClass* pLaser = GameCreate<LaserDrawClass>(
		sourcePos, targetPos,
		innerColor, outerColor, laser.OuterSpread,
		laser.Duration);

	pLaser->Thickness = laser.Thickness;
	pLaser->IsHouseColor = laser.Fade || laser.IsSingleColor || laser.IsHouseColor;
	pLaser->IsSupported = laser.IsSupported || (laser.Thickness > 5 && !laser.Fade);
}

void DrawLaser(CoordStruct sourcePos, CoordStruct targetPos, ColorStruct innerColor, ColorStruct outerColor, int thickness, int duration)
{
	LaserType laser{};
	laser.InnerColor = innerColor;
	laser.OuterColor = outerColor;
	laser.Thickness = thickness;
	laser.Duration = duration;
	DrawLaser(laser, sourcePos, targetPos);
}

void LaserHelper::RedLine(CoordStruct sourcePos, CoordStruct targetPos, int thickness, int duration)
{
	DrawLaser(sourcePos, targetPos, Colors::Red, Colors::Empty, thickness, duration);
}
void LaserHelper::GreenLine(CoordStruct sourcePos, CoordStruct targetPos, int thickness, int duration)
{
	DrawLaser(sourcePos, targetPos, Colors::Green, Colors::Empty, thickness, duration);
}
void LaserHelper::BlueLine(CoordStruct sourcePos, CoordStruct targetPos, int thickness, int duration)
{
	DrawLaser(sourcePos, targetPos, Colors::Blue, Colors::Empty, thickness, duration);
}

void LaserHelper::RedLineZ(CoordStruct sourcePos, int length, int thickness, int duration)
{
	RedLine(sourcePos, sourcePos + CoordStruct{ 0, 0, length }, thickness, duration);
}
void LaserHelper::GreenLineZ(CoordStruct sourcePos, int length, int thickness, int duration)
{
	GreenLine(sourcePos, sourcePos + CoordStruct{ 0, 0, length }, thickness, duration);
}
void LaserHelper::BlueLineZ(CoordStruct sourcePos, int length, int thickness, int duration)
{
	BlueLine(sourcePos, sourcePos + CoordStruct{ 0, 0, length }, thickness, duration);
}

void LaserHelper::Cell(CoordStruct sourcePos, int length, ColorStruct lineColor, ColorStruct outerColor, int thickness, int duration)
{
	CoordStruct p1 = sourcePos + CoordStruct{ length, length, 0 };
	CoordStruct p2 = sourcePos + CoordStruct{ -length, length, 0 };
	CoordStruct p3 = sourcePos + CoordStruct{ -length, -length, 0 };
	CoordStruct p4 = sourcePos + CoordStruct{ length, -length, 0 };
	DrawLaser(p1, p2, lineColor, outerColor, thickness, duration);
	DrawLaser(p2, p3, lineColor, outerColor, thickness, duration);
	DrawLaser(p3, p4, lineColor, outerColor, thickness, duration);
	DrawLaser(p4, p1, lineColor, outerColor, thickness, duration);
}
void LaserHelper::Crosshair(CoordStruct sourcePos, int length, ColorStruct lineColor, ColorStruct outerColor, int thickness, int duration)
{
	DrawLaser(sourcePos, sourcePos + CoordStruct{ length, 0, 0 }, lineColor, outerColor, thickness, duration);
	DrawLaser(sourcePos, sourcePos + CoordStruct{ -length, 0, 0 }, lineColor, outerColor, thickness, duration);
	DrawLaser(sourcePos, sourcePos + CoordStruct{ 0, -length, 0 }, lineColor, outerColor, thickness, duration);
	DrawLaser(sourcePos, sourcePos + CoordStruct{ 0, length, 0 }, lineColor, outerColor, thickness, duration);
}

void LaserHelper::RedCrosshair(CoordStruct sourcePos, int length, int thickness, int duration)
{
	Crosshair(sourcePos, length, Colors::Red, Colors::Empty, thickness, duration);
}
void LaserHelper::GreenCrosshair(CoordStruct sourcePos, int length, int thickness, int duration)
{
	Crosshair(sourcePos, length, Colors::Green, Colors::Empty, thickness, duration);
}
void LaserHelper::BlueCrosshair(CoordStruct sourcePos, int length, int thickness, int duration)
{
	Crosshair(sourcePos, length, Colors::Blue, Colors::Empty, thickness, duration);
}

void LaserHelper::RedCell(CoordStruct sourcePos, int length, int thickness, int duration, bool crosshair)
{
	if (crosshair)
	{
		RedCrosshair(sourcePos, length, thickness, duration);
	}
	Cell(sourcePos, length, Colors::Red, Colors::Empty, thickness, duration);
}
void LaserHelper::GreenCell(CoordStruct sourcePos, int length, int thickness, int duration, bool crosshair)
{
	if (crosshair)
	{
		GreenCrosshair(sourcePos, length, thickness, duration);
	}
	Cell(sourcePos, length, Colors::Green, Colors::Empty, thickness, duration);
}
void LaserHelper::BlueCell(CoordStruct sourcePos, int length, int thickness, int duration, bool crosshair)
{
	if (crosshair)
	{
		BlueCrosshair(sourcePos, length, thickness, duration);
	}
	Cell(sourcePos, length, Colors::Blue, Colors::Empty, thickness, duration);
}

void LaserHelper::MapCell(CoordStruct sourcePos, ColorStruct lineColor, ColorStruct outerColor, int thickness, int duration, bool crosshair)
{
	if (CellClass* pCell = MapClass::Instance->TryGetCellAt(sourcePos))
	{
		CoordStruct cellPos = pCell->GetCoordsWithBridge();
		if (crosshair)
		{
			Crosshair(cellPos, 128, lineColor, outerColor, thickness, duration);
		}
		Cell(cellPos, 128, lineColor, outerColor, thickness, duration);
	}
}

void LaserHelper::RedMapCell(CoordStruct sourcePos, int thickness, int duration, bool crosshair)
{
	MapCell(sourcePos, Colors::Red, Colors::Empty, thickness, duration, crosshair);
}
void LaserHelper::GreenMapCell(CoordStruct sourcePos, int thickness, int duration, bool crosshair)
{
	MapCell(sourcePos, Colors::Green, Colors::Empty, thickness, duration, crosshair);
}
void LaserHelper::BlueMapCell(CoordStruct sourcePos, int thickness, int duration, bool crosshair)
{
	MapCell(sourcePos, Colors::Blue, Colors::Empty, thickness, duration, crosshair);
}
#pragma endregion

#pragma region RadBeamType

void DrawBeam(CoordStruct sourcePos, CoordStruct targetPos, BeamType type, ColorStruct customColor)
{
	RadBeam* pBeam = RadBeam::Allocate(type.Type);
	if (pBeam)
	{
		pBeam->SetCoordsSource(sourcePos);
		// 视觉散布
		if (type.VisualScatter)
		{
			targetPos = MapClass::Instance->GetRandomCoordsNear(targetPos, Random::RandomRanged(type.VisualScatterMin, type.VisualScatterMax), false);
		}
		pBeam->SetCoordsTarget(targetPos);
		ColorStruct beamColor = type.Color;
		if (customColor != Colors::Empty)
		{
			beamColor = customColor;
		}
		pBeam->Color = beamColor;
		pBeam->Period = type.Period;
		pBeam->Amplitude = type.Amplitude;
	}
}

void DrawRadBeam(CoordStruct sourcePos, CoordStruct targetPos, ColorStruct customColor, int period, double amplitude)
{
	BeamType type{ RadBeamType::RadBeam };
	type.Period = period;
	type.Amplitude = amplitude;
	DrawBeam(sourcePos, targetPos, type, customColor);
}

void DrawTemporalBeam(CoordStruct sourcePos, CoordStruct targetPos, ColorStruct customColor, int period, double amplitude)
{
	BeamType type{ RadBeamType::Temporal };
	type.Period = period;
	type.Amplitude = amplitude;
	DrawBeam(sourcePos, targetPos, type, customColor);
}
#pragma endregion

#pragma region BoltType
void DrawBolt(CoordStruct sourcePos, CoordStruct targetPos, BoltType type)
{
	EBolt* pBolt = GameCreate<EBolt>();
	if (pBolt)
	{
		if (EBoltStatus* status = GetStatus<EBoltExt, EBoltStatus>(pBolt))
		{
			// 调整绘制
			status->ArcCount = type.ArcCount;

			status->Color1 = type.GetColor1();
			status->Color2 = type.GetColor2();
			status->Color3 = type.GetColor3();

			status->Disable1 = type.Disable1;
			status->Disable2 = type.Disable2;
			status->Disable3 = type.Disable3;

			status->DisableParticle = type.DisableParticle;
		}
		pBolt->AlternateColor = type.IsAlternateColor;
		pBolt->Fire(sourcePos, targetPos, 0);
	}
}
void DrawBolt(CoordStruct sourcePos, CoordStruct targetPos, bool alternate)
{
	BoltType type{ alternate };
	DrawBolt(sourcePos, targetPos, type);
}

void DrawBolt(TechnoClass* pShooter, AbstractClass* pTarget, WeaponTypeClass* pWeapon, CoordStruct sourcePos, CoordStruct flh, bool isOnTurret)
{
	if (EBolt* bolt = pShooter->Electric_Zap(pTarget, pWeapon, sourcePos))
	{
		if (!flh.IsEmpty())
		{
			if (EBoltStatus* status = GetStatus<EBoltExt, EBoltStatus>(bolt))
			{
				status->AttachTo(pShooter, flh, isOnTurret, pTarget);
			}
		}
	}
}
#pragma endregion

#pragma region ParticleSystem
void DrawParticle(ParticleSystemTypeClass* psType, CoordStruct &sourcePos, CoordStruct &targetPos, ObjectClass* pOwner, AbstractClass* pTarget, HouseClass* pHouse)
{
	GameCreate<ParticleSystemClass>(psType, sourcePos, pTarget, pOwner, targetPos, pHouse);
}

void DrawParticle(const char* systemName, CoordStruct &sourcePos, CoordStruct &targetPos)
{
	ParticleSystemTypeClass* psType = ParticleSystemTypeClass::Find(systemName);
	if (psType)
	{
		DrawParticle(psType, sourcePos, targetPos);
	}
}
#pragma endregion
