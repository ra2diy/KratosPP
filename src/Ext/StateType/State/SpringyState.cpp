#include "SpringyState.h"

#include <algorithm>

#include <Ext/EffectType/Effect/SpringyEffect.h>
#include <Ext/Helper/Scripts.h>


void SpringyState::Clean()
{
	StateScript<SpringyData>::Clean();

	// 表是运行时结构，不跨复用残留
	_segments.clear();
}

void SpringyState::AddSegment(SpringyEffect* segment)
{
	if (!segment)
	{
		return;
	}
	// 血量条件恢复时会把自己重新报一次，已在表里就不重复添加
	if (std::find(_segments.begin(), _segments.end(), segment) != _segments.end())
	{
		return;
	}
	_segments.push_back(segment);
	// 段按附着顺序保存，最后一条即「最后挂上的那条」，替身与动画的枢轴取它
	if (_segments.size() == 1)
	{
		Activate();
	}
}

void SpringyState::RemoveSegment(SpringyEffect* segment)
{
	if (!segment)
	{
		return;
	}
	auto it = std::find(_segments.begin(), _segments.end(), segment);
	if (it == _segments.end())
	{
		return;
	}
	_segments.erase(it);
	// 段全没了就休眠：没有缩放的单位不占引擎的组件遍历
	if (_segments.empty())
	{
		Deactivate();
	}
}

bool SpringyState::GetGeometry(double& x, double& y, double& z, double& shearX, double& shearY, AffectWho viewer)
{
	x = 1.0;
	y = 1.0;
	z = 1.0;
	shearX = 0.0;
	shearY = 0.0;

	bool any = false;
	for (SpringyEffect* segment : _segments)
	{
		if (!segment)
		{
			continue;
		}
		// ALL 对两边都生效；MASTER / STAND 只对各自那一边生效
		AffectWho target = segment->GetAffectWho();
		if (target != AffectWho::ALL && target != viewer)
		{
			continue;
		}
		double sx = 1.0;
		double sy = 1.0;
		double sz = 1.0;
		double kx = 0.0;
		double ky = 0.0;
		segment->GetTransform(sx, sy, sz, kx, ky);
		x *= sx;
		y *= sy;
		z *= sz;
		// 剪切是像素位移：多条段在屏幕上叠加，直接相加
		shearX += kx;
		shearY += ky;
		any = true;
	}
	return any;
}
SpringyAnimPivot SpringyState::GetAnimPivot()
{
	if (_segments.empty())
	{
		return SpringyAnimPivot::ANIM;
	}
	return _segments.back()->GetAnimPivot();
}

