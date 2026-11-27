#include "SpringyEffect.h"

#include <algorithm>
#include <cmath>

#include <Fundamentals.h>

#include <Ext/Common/SpringyRefresh.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/StateType/State/SpringyState.h>

namespace
{
	// 一个完整波形的离散步数。0 = 静止，负半周变窄变高，正半周变宽压扁
	constexpr int Steps = SpringySteps;

	/// @brief 当前帧落在波形的第几步
	/// @param rate 一个完整周期的帧数
	/// @return 0~Steps-1
	/// @note 相位不入存档：它完全由帧号推导，读档后天然对齐，存了反而会与帧号冲突
	int WaveStep(int rate)
	{
		rate = std::max(rate, 1);
		const int bucket = static_cast<int>(
			static_cast<long long>(Unsorted::CurrentFrame % rate) * Steps / rate);
		return std::min(bucket, Steps - 1);
	}

	/// @brief 某一步的波形系数
	/// @param step WaveStep 的结果
	/// @return -1 ~ +1：0 = 原样，负 = 变窄变高，正 = 变宽压扁
	double WavePulse(int step)
	{
		static constexpr double Table[Steps] =
		{
			0.0,
			-0.70710678118654752, -1.0, -0.70710678118654752,
			0.0,
			0.70710678118654752, 1.0, 0.70710678118654752
		};
		return Table[std::clamp(step, 0, Steps - 1)];
	}

	/// @brief 按给定相位与过渡进度求一组倍率与剪切
	/// @param data 段配置
	/// @param waveStep 波形相位档（只有 WAVE 模式用得到）
	/// @param progress 过渡进度：0 = 完全原样，1 = 完全到位
	/// @param x,y,z 输出的三个轴倍率
	/// @param shearX,shearY 输出的两个方向剪切（0 = 不推）
	/// @note 剪切与缩放共用同一条脉冲：正半周走 Max 端、负半周走 Min 端。
	///       端点是带符号的斜率，所以 Max=+30 / Min=-30 就是一左一右往复
	void Evaluate(const SpringyData* data, int waveStep, double progress,
		double& x, double& y, double& z, double& shearX, double& shearY)
	{
		const int quarter = Steps / 4;

		double waveX = 1.0;
		double waveZ = 1.0;
		double shearXTarget = 0.0;
		double shearYTarget = 0.0;
		if (data->Mode == SpringyMode::WAVE)
		{
			// 两个轴各自取相位：相位每加一档就是把采样点往前挪 90°（Steps/4 步）。
			// 默认两轴同相 → 横向正半周取宽峰时纵向正处在矮谷（宽扁 / 窄高）；
			// 纵向相位 +2（即 180°）让两个轴同时到达各自的「大」端 → 同步放大
			const double pulseW = WavePulse((waveStep + data->WidthPhase * quarter) % Steps);
			const double pulseH = WavePulse((waveStep + data->HeightPhase * quarter) % Steps);

			// 四个端点是屏幕上量到的实际尺寸（100 = 原样）：脉冲为正走「大」端、为负走「小」端，
			// 幅度按 |脉冲| 从 1.0 线性走到该端；脉冲过零时精确回到 100。
			// 不能拿两个端点做整体插值 —— 那样过零点会落在两端的中点，端点不对称时就不再经过原尺寸
			const double widthPct = pulseW >= 0.0 ? data->WidthMax : data->WidthMin;
			const double heightPct = pulseH >= 0.0 ? data->HeightMin : data->HeightMax;
			waveX = 1.0 + (widthPct - 100.0) / 100.0 * std::abs(pulseW);
			waveZ = 1.0 + (heightPct - 100.0) / 100.0 * std::abs(pulseH);

			// 剪切的相位独立（默认两轴同相，一左一右同步推）
			const double pulseSX = WavePulse((waveStep + data->ShearXPhase * quarter) % Steps);
			const double pulseSY = WavePulse((waveStep + data->ShearYPhase * quarter) % Steps);
			const double endSX = pulseSX >= 0.0 ? data->ShearXMax : data->ShearXMin;
			const double endSY = pulseSY >= 0.0 ? data->ShearYMax : data->ShearYMin;
			// 端点就是像素：脉冲为正走 Max、为负走 Min，幅度按 |脉冲| 线性走
			shearXTarget = endSX * std::abs(pulseSX);
			shearYTarget = endSY * std::abs(pulseSY);
		}
		else
		{
			// STATIC：没有波形，端点直接当固定斜率用（与 ScaleX/Y/Z 的静态口径一致）
			shearXTarget = data->ShearXMax;
			shearYTarget = data->ShearYMax;
		}

		// X 与 Y 同属横向，共用一个波形系数：波形只做「变宽 / 变窄」，不区分前后与左右
		const double targetX = data->ScaleX * waveX;
		const double targetY = data->ScaleY * waveX;
		const double targetZ = data->ScaleZ * waveZ;

		// 过渡从「完全原样」插值到目标值；progress 为 1 时就是目标值本身
		x = 1.0 + (targetX - 1.0) * progress;
		y = 1.0 + (targetY - 1.0) * progress;
		z = 1.0 + (targetZ - 1.0) * progress;
		// 剪切从 0 长到目标斜率：过渡结束时正好是配置值
		shearX = shearXTarget * progress;
		shearY = shearYTarget * progress;
	}
}


void SpringyEffect::Clean()
{
	EffectScript::Clean();

	_progress = 0;
	_frozen = false;

	// 组件回池前必须先注销，否则汇总表里会留下一个已被回收的指针
	UnregisterFromState();
}

void SpringyEffect::OnStart()
{
	_progress = 0;
	_frozen = false;

	// 登记进所在单位的汇总表：这条段从此参与倍率合成
	RegisterToState();
}

void SpringyEffect::RegisterToState()
{
	// 段在新建时走 OnStart、读档时走 Load、被 InheritAE 换宿主时走 ExtChanged，三条路径都要登记
	if (_gameObject)
	{
		if (SpringyState* state = _gameObject->GetComponent<SpringyState>())
		{
			_state = state;
			state->AddSegment(this);
		}
	}
}

void SpringyEffect::UnregisterFromState()
{
	// 用登记时记下的指针把自己划掉：只做一次表内查找与删除，不碰宿主组件树，
	// 所以即使宿主正在销毁过程中也安全
	if (_state)
	{
		_state->RemoveSegment(this);
		_state = nullptr;
	}
}

void SpringyEffect::OnUpdate()
{
	// 冻结期间停在这里：_progress 不动，GetScale 因此一直返回暂停那一刻的倍率
	if (_frozen)
	{
		return;
	}
	const int frames = Data->TransitionFrames;
	if (frames > 0 && _progress < frames)
	{
		_progress++;
	}
}

void SpringyEffect::OnPause()
{
	// 本段留在汇总表里继续贡献倍率，只是不再推进过渡，避免血量在阈值附近抖动时画面闪烁
	_frozen = true;
}

void SpringyEffect::OnRecover()
{
	_frozen = false;
	// 恢复时重新登记一次：AddSegment 自带去重，重复登记无副作用
	RegisterToState();
}

void SpringyEffect::ExtChanged()
{
	// InheritAE 会把整棵 AE 树搬到新单位（只换宿主、不走 Clean）：
	// 先让基类重新识别宿主，再把自己登记到新单位的汇总表上
	EffectScript::ExtChanged();
	RegisterToState();
}

void SpringyEffect::End(CoordStruct location)
{
	// 段一结束就不再参与合成：倍率当帧回到剩余各段，不做缩短过渡
	UnregisterFromState();
	_frozen = false;

	// 倍率马上要撤回，当帧补一次重绘与脏矩形：否则画面会停在缩放后的样子，
	// 直到下一次别的原因触发重绘才恢复正常（静止的建筑尤其明显）
	if (pTechno)
	{
		SpringyRefresh::RefreshNow(pTechno);
	}
}

double SpringyEffect::Progress()
{
	const int frames = Data->TransitionFrames;
	if (frames <= 0)
	{
		// 未配置过渡：立刻到位
		return 1.0;
	}
	const int done = std::max(_progress, 0);
	return static_cast<double>(std::min(done, frames)) / frames;
}

void SpringyEffect::GetScale(double& x, double& y, double& z)
{
	double shearX = 0.0;
	double shearY = 0.0;
	GetTransform(x, y, z, shearX, shearY);
}

void SpringyEffect::GetTransform(double& x, double& y, double& z, double& shearX, double& shearY)
{
	const SpringyData* data = Data;
	Evaluate(data, WaveStep(data->Rate), Progress(), x, y, z, shearX, shearY);
}

void SpringyEffect::GetShear(double& shearX, double& shearY)
{
	double x = 1.0;
	double y = 1.0;
	double z = 1.0;
	GetTransform(x, y, z, shearX, shearY);
}

SpringyAnimPivot SpringyEffect::GetAnimPivot()
{
	return Data->AnimPivot;
}

AffectWho SpringyEffect::GetAffectWho()
{
	return Data->AffectWho;
}

bool SpringyEffect::Load(ExStreamReader& stream, bool registerForChange)
{
	EffectScript::Load(stream, registerForChange);

	// 读档恢复出来的段不会走 OnStart，这里补登记一次
	RegisterToState();

	return this->Serialize(stream);
}

bool SpringyEffect::Save(ExStreamWriter& stream) const
{
	EffectScript::Save(stream);
	return const_cast<SpringyEffect*>(this)->Serialize(stream);
}
