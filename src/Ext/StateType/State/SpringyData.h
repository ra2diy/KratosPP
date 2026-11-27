#pragma once

#include <string>
#include <algorithm>
#include <cctype>

#include <GeneralStructures.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/CastEx.h>
#include <Ext/Common/SpringyTransform.h>


enum class SpringyMode : int
{
	STATIC = 0, // 固定倍率：挂上后按 ScaleX/Y/Z 恒定缩放
	WAVE = 1    // 循环波形：按 Rate 周期在静止与宽扁/窄高之间往复
};


template <>
inline bool Parser<SpringyMode>::TryParse(const char* pValue, SpringyMode* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'W':
		if (outValue)
		{
			*outValue = SpringyMode::WAVE;
		}
		return true;
	default:
		if (outValue)
		{
			*outValue = SpringyMode::STATIC;
		}
		return true;
	}
}


template <>
inline bool Parser<SpringyShearMode>::TryParse(const char* pValue, SpringyShearMode* outValue)
{
	const char* p = pValue;
	while (*p == ' ' || *p == '\t')
	{
		++p;
	}
	const char first = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
	// PX / PERCENT 都以 P 开头，看第二个字符区分；也接受 % / PCT
	const char second = static_cast<char>(toupper(static_cast<unsigned char>(p[0] ? p[1] : '\0')));
	const bool percent = first == '%' || (first == 'P' && (second == 'E' || second == 'C'));
	if (outValue)
	{
		*outValue = percent ? SpringyShearMode::PERCENT : SpringyShearMode::PX;
	}
	return true;
}

enum class SpringyAnimPivot : int
{
	ANIM = 0,  // 依附动画以自身帧的底边为枢轴缩放（独立特效不跟着单位上移）
	OWNER = 1  // 依附动画以依附单位的底边为枢轴缩放（贴身特效跟着单位长高）
};


template <>
inline bool Parser<SpringyAnimPivot>::TryParse(const char* pValue, SpringyAnimPivot* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'O':
		if (outValue)
		{
			*outValue = SpringyAnimPivot::OWNER;
		}
		return true;
	default:
		if (outValue)
		{
			*outValue = SpringyAnimPivot::ANIM;
		}
		return true;
	}
}


/// @brief 一条 Springy AE 的配置。
///
/// 分段：ScaleX/Y/Z 是这条 AE 的基准倍率（两种模式都参与），
/// WidthMax/WidthMin/HeightMax/HeightMin 只在 WAVE 模式下描述波形的两个端点。
/// 最终倍率 = 基准倍率 × 波形系数 × 过渡进度，由 SpringyEffect 逐帧算出。
class SpringyData : public EffectData
{
public:
	EFFECT_DATA(Springy);

	SpringyMode Mode = SpringyMode::STATIC;

	// 基准倍率，对应绘制矩阵的三个轴：X=前后(F)、Y=左右(L)、Z=高度(H)
	double ScaleX = 1.0;
	double ScaleY = 1.0;
	double ScaleZ = 1.0;

	// 从原样长到生效所需帧数，0 = 瞬间到位
	// 只在挂上时使用；AE 移除时系数直接消失，不做缩短过渡
	int TransitionFrames = 0;

	// 依附动画的缩放枢轴；一个单位挂多条时取最后挂上的那条。
	// 默认 OWNER：建筑的覆盖动画（活动/待机/损毁火焰）通常只是建筑的一部分，
	// 必须绕建筑底边缩放才贴着本体；绕自身底边会把子部件甩出去。
	// 非建筑单位的动画不会被套用枢轴（画布不同源），所以这个默认对它们无影响
	SpringyAnimPivot AnimPivot = SpringyAnimPivot::OWNER;

	// WAVE 模式：一个完整周期的帧数
	int Rate = 55;

	// WAVE 模式：波形的四个端点，按半周分别取用。100 = 原尺寸，120 = 放大两成，80 = 缩到八成。
	// 正半周取「宽峰 + 矮谷」，负半周取「窄谷 + 高峰」，pulse 过零时两端都精确回到 100。
	// 这样写的每个值就是屏幕上量到的那个尺寸，不再是「在 100 上加多少」
	int WidthMax = 100;  // 宽峰，脉冲为正那一侧的横向极值
	int WidthMin = 100;   // 宽谷，脉冲为负那一侧的横向极值
	int HeightMax = 100; // 高峰，脉冲为负那一侧的纵向极值
	int HeightMin = 100;  // 高谷，脉冲为正那一侧的纵向极值

	// WAVE 模式：两个轴各自的相位起点，每档 90°（0~3）。
	// 两轴默认同相：横向在宽峰时纵向正在矮谷（宽扁 ↔ 窄高）。
	// 把 HeightPhase 设成 2（180°）就让两个轴同时到达「大」端 —— 同步放大；
	// 设成 1 或 3（±90°）则一轴到极值时另一轴正在原样，先后错开
	int WidthPhase = 0;
	int HeightPhase = 0;

	// 剪切（推拉）：把矩形推成平行四边形。**单位是像素**：写 300 就是顶边相对底边右移 300 像素，
	// 底边不动、中间各行按「离底部的行数 / 总行数」线性递增。
	// 正 ShearX = 顶边右移，正 ShearY = 右边下移。
	// 全 0 = 不变形（与只有缩放的旧配置完全一致）；配合 WAVE 时同一波形同时驱动缩放与剪切，
	// 所以「一边胀缩一边推拉」不用额外开关，想纯推拉就把 Width/Height 端点都写 100
	// 剪切参数的单位：PX = 像素（默认），PERCENT = 整单位高度/宽度的百分比
	SpringyShearMode ShearMode = SpringyShearMode::PX;
	int ShearXMax = 0;
	int ShearXMin = 0;
	int ShearYMax = 0;
	int ShearYMin = 0;
	int ShearXPhase = 0;
	int ShearYPhase = 0;

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, TITLE);
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		EffectData::Read(reader, title);

		Mode = reader->Get(title + "Mode", Mode);

		ScaleX = reader->Get(title + "ScaleX", ScaleX);
		ScaleY = reader->Get(title + "ScaleY", ScaleY);
		ScaleZ = reader->Get(title + "ScaleZ", ScaleZ);
		// 倍率下限 0.1 防止除零与绘制塌成一点，上限 10 防止一帧拉出半个屏幕
		ScaleX = std::clamp(ScaleX, 0.1, 10.0);
		ScaleY = std::clamp(ScaleY, 0.1, 10.0);
		ScaleZ = std::clamp(ScaleZ, 0.1, 10.0);

		TransitionFrames = reader->Get(title + "TransitionFrames", TransitionFrames);
		TransitionFrames = std::clamp(TransitionFrames, 0, 600);

		AnimPivot = reader->Get(title + "AnimPivot", AnimPivot);

		Rate = reader->Get(title + "Rate", Rate);
		Rate = std::clamp(Rate, 1, 600);

		WidthMax = reader->Get(title + "WidthMax", WidthMax);
		WidthMin = reader->Get(title + "WidthMin", WidthMin);
		HeightMax = reader->Get(title + "HeightMax", HeightMax);
		HeightMin = reader->Get(title + "HeightMin", HeightMin);
		// 端点下限 1%：到 0 就是那一半周把图压成一条线，绘制直接消失。
		// 上限 1000%（十倍）只防手误；实际能画多大还受体素 256×256 临时表面与 SHP 画布限制
		WidthMax = std::clamp(WidthMax, 1, 1000);
		WidthMin = std::clamp(WidthMin, 1, 1000);
		HeightMax = std::clamp(HeightMax, 1, 1000);
		HeightMin = std::clamp(HeightMin, 1, 1000);

		WidthPhase = reader->Get(title + "WidthPhase", WidthPhase);
		HeightPhase = reader->Get(title + "HeightPhase", HeightPhase);
		// 相位只有四个档次，取模让 4 与 0 等价，避免写出 4 这种越界值
		WidthPhase = ((WidthPhase % 4) + 4) % 4;
		HeightPhase = ((HeightPhase % 4) + 4) % 4;

		ShearMode = reader->Get(title + "ShearMode", ShearMode);
		ShearXMax = reader->Get(title + "ShearXMax", ShearXMax);
		ShearXMin = reader->Get(title + "ShearXMin", ShearXMin);
		ShearYMax = reader->Get(title + "ShearYMax", ShearYMax);
		ShearYMin = reader->Get(title + "ShearYMin", ShearYMin);
		// 只做安全钳位（±3000 像素），不按角度限制：写多少就是顶边移多少，
		// 极端值由矩阵求逆失败时「按原样绘制」兜底
		ShearXMax = std::clamp(ShearXMax, -3000, 3000);
		ShearXMin = std::clamp(ShearXMin, -3000, 3000);
		ShearYMax = std::clamp(ShearYMax, -3000, 3000);
		ShearYMin = std::clamp(ShearYMin, -3000, 3000);

		ShearXPhase = reader->Get(title + "ShearXPhase", ShearXPhase);
		ShearYPhase = reader->Get(title + "ShearYPhase", ShearYPhase);
		ShearXPhase = ((ShearXPhase % 4) + 4) % 4;
		ShearYPhase = ((ShearYPhase % 4) + 4) % 4;

		// 没显式写 Enable 时，只要配了会改变画面的参数就自动启用（与 Paintball 一致）。
		// 显式写了 Enable 则以它为准，便于同一份 AE 段整体关停
		bool explicitEnable = false;
		if (!reader->TryGet(title + "Enable", explicitEnable))
		{
			const bool scaled = ScaleX != 1.0 || ScaleY != 1.0 || ScaleZ != 1.0;
			const bool sheared = ShearXMax != 0 || ShearXMin != 0 || ShearYMax != 0 || ShearYMin != 0;
			// WAVE 即使三个倍率都是 1、剪切全是 0 也会动，同样算启用
			if (scaled || sheared || Mode == SpringyMode::WAVE)
			{
				Enable = true;
			}
		}
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Mode)

			.Process(this->ScaleX)
			.Process(this->ScaleY)
			.Process(this->ScaleZ)

			.Process(this->TransitionFrames)
			.Process(this->AnimPivot)

			.Process(this->Rate)

			.Process(this->WidthMax)
			.Process(this->WidthMin)
			.Process(this->HeightMax)
			.Process(this->HeightMin)
			.Process(this->WidthPhase)
			.Process(this->HeightPhase)
			// 新增字段一律追加在末尾：旧存档按原顺序读到的还是原来那些值
			.Process(this->ShearXMax)
			.Process(this->ShearXMin)
			.Process(this->ShearYMax)
			.Process(this->ShearYMin)
			.Process(this->ShearXPhase)
			.Process(this->ShearYPhase)
			.Process(this->ShearMode)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectData::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		EffectData::Save(stream);
		return const_cast<SpringyData*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	inline static std::string TITLE = "Springy.";
};


/// @brief 一个完整波形的采样步数。
/// 波形表把周期分成这么多帧，每帧取一个固定正弦系数（照搬参考实现）。
/// 它与绘制尺寸无关：绘制倍率始终是连续量，不往这个步数上取整。
constexpr int SpringySteps = 8;

/// @brief 一个单位当前这一帧应该用的绘制倍率与形变。
///
/// 由 TechnoStatus 每帧刷新一次，渲染侧只读它，不自己去遍历段、算相位——
/// 绘制 hook 每帧会被调用上千次，让它每次都算会把同一组数算上千遍。
struct SpringySnapshot
{
	bool Active = false;

	// 本帧最终倍率：自身所有段的乘积（替身额外纳入母体上 AffectWho 允许的那些段）
	// 这是**世界轴**口径（F/L/H），给 FLH 偏移与挂点用，不是位图的缩放
	double ScaleX = 1.0;
	double ScaleY = 1.0;
	double ScaleZ = 1.0;

	// 本帧的画布形变（位图口径）：缩放 + 剪切复合后的矩阵。
	// 主体 / 炮塔 / 炮管 / 影子 / 依附动画都读它，保证同一条段在三条链路上形状一致
	// 本帧的剪切位移（像素）：顶边相对底边的水平/垂直位移。
	// 位图口径 —— 主体 / 炮塔 / 炮管 / 影子 / 依附动画都读它，三条链路形状一致。
	// 存像素而不是斜率：斜率要除以精灵尺寸，而尺寸只有到贴图那一刻才知道
	double ShearX = 0.0;
	SpringyShearMode ShearMode = SpringyShearMode::PX;
	double ShearY = 0.0;

	SpringyAnimPivot AnimPivot = SpringyAnimPivot::ANIM;

	void Reset()
	{
		*this = {};
		ScaleX = ScaleY = ScaleZ = 1.0;
		ShearX = 0.0;
		ShearY = 0.0;
	}
};
