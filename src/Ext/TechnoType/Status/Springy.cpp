#include "../TechnoStatus.h"

#include <Drawing.h>
#include <Matrix3D.h>

#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/StateType/State/SpringyState.h>


void TechnoStatus::OnUpdate_Springy()
{
	// 必须通过 property 取状态，不能直接读 _Springy 成员：
	// STATE_VAR_INIT 只保证组件被创建，不负责给成员指针赋值；
	// 成员只有在 GetSpringy() 被调用过之后才非空。
	// 直接读成员会恒为空，快照永远不 Active，本效果器整个失效。
	SpringyState* ownSpringy = Springy;

	// 替身：母体活着且不是动画时才算，否则没有 TechnoStatus 可取
	TechnoStatus* masterStatus = nullptr;
	if (AmIStand() && pMyMaster && !MyMasterIsAnim && !IsDead(pMyMaster))
	{
		masterStatus = GetStatus<TechnoExt, TechnoStatus>(pMyMaster);
	}

	const bool hasOwn = ownSpringy && ownSpringy->HasSegment();
	// 母体身上可能挂着 AffectWho=STAND/ALL 的段，替身每帧现取，不进任何跨对象的表。
	// 母体侧同样走 property：替身可能先于母体更新，成员懒加载由这里补上
	SpringyState* masterSpringy = masterStatus ? masterStatus->Springy : nullptr;
	const bool hasMaster = masterSpringy && masterSpringy->HasSegment();
	if (!hasOwn && !hasMaster)
	{
		if (MySpringyData.Active)
		{
			MySpringyData.Reset();
		}
		// 本帧不生效：体素剪切枢轴也一起忘掉，下次挂上按新形状重新取地面线
		SpringyShearFrameValid = false;
		SpringyShearTop = 0.0;
		SpringyShearBottom = 0.0;
		SpringyShearLeft = 0.0;
		SpringyShearRight = 0.0;
		return;
	}

	double x = 1.0;
	double y = 1.0;
	double z = 1.0;
	bool counted = false;
	SpringyAnimPivot pivot = SpringyAnimPivot::ANIM;
	double shearX = 0.0;
	double shearY = 0.0;
	SpringyShearMode shearMode = SpringyShearMode::PX;

	// 主身视角：计入 AffectWho 为 MASTER / ALL 的段
	if (hasOwn && ownSpringy->GetGeometry(x, y, z, shearX, shearY, shearMode, AffectWho::MASTER))
	{
		counted = true;
		pivot = ownSpringy->GetAnimPivot();
	}

	// 替身视角：再并入母体上 AffectWho 为 STAND / ALL 的段
	if (hasMaster)
	{
		double mx = 1.0;
		double my = 1.0;
		double mz = 1.0;
		double msx = 0.0;
		double msy = 0.0;
		SpringyShearMode mshearMode = SpringyShearMode::PX;
		if (masterSpringy->GetGeometry(mx, my, mz, msx, msy, mshearMode, AffectWho::STAND))
		{
			x *= mx;
			y *= my;
			z *= mz;
			counted = true;
			shearX += msx;
			shearY += msy;
			shearMode = mshearMode;
		}
		// 枢轴一律按母体的设置：替身是母体的一部分，跟着母体的口径走。
		// 母体自己可能没生效（只挂 STAND 段），此时直接问母体的段表
		pivot = masterStatus->MySpringyData.Active
			? masterStatus->MySpringyData.AnimPivot
			: masterSpringy->GetAnimPivot();
	}

	if (!counted)
	{
		// 段存在但都被 AffectWho 过滤掉了（比如只挂着 STAND 段的主身）
		if (MySpringyData.Active)
		{
			MySpringyData.Reset();
		}
		// 本帧不生效：体素剪切枢轴也一起忘掉，下次挂上按新形状重新取地面线
		SpringyShearFrameValid = false;
		SpringyShearTop = 0.0;
		SpringyShearBottom = 0.0;
		SpringyShearLeft = 0.0;
		SpringyShearRight = 0.0;
		return;
	}

	MySpringyData.ScaleX = x;
	MySpringyData.ScaleY = y;
	MySpringyData.ScaleZ = z;
	MySpringyData.ShearX = shearX;
	MySpringyData.ShearY = shearY;
	MySpringyData.ShearMode = shearMode;
	MySpringyData.AnimPivot = pivot;
	MySpringyData.Active = true;
	// 记下这一份：AE 撤除当帧倍率就回到 1.0，届时补脏区要按撤除前的尺寸算
	LastSpringyDrawn = MySpringyData;

#ifdef DEBUG_AE
	Debug::Log("[Springy]%d 倍率 (%.3f,%.3f,%.3f) 枢轴%d\n",
		pTechno, x, y, z, static_cast<int>(pivot));
#endif // DEBUG_AE
}
