#include <exception>
#include <cstdint>

#include <GeneralStructures.h>
#include <SpecificStructures.h>
#include <ObjectClass.h>
#include <FootClass.h>
#include <TechnoClass.h>
#include <AnimClass.h>

#include <Extension.h>
#include <Utilities/Macro.h>

#include <FileFormats/SHP.h>

#include <Ext/AnimType/SpringyAnim.h>


// AnimClass::DrawIt 入口。ECX = 动画本体。
// 这里只做「判定归属 + 置绘制上下文」，不换 SHP；换 SHP 在下面四个 blit 点。
DEFINE_HOOK(0x422CA0, AnimClass_Draw_SpringyBegin, 0x5)
{
	GET(AnimClass*, pAnim, ECX);
	SpringyAnim::BeginDraw(pAnim);
	return 0;
}


// AnimClass::DrawIt 内部对共享 blitter(0x4AED70) 的四个调用点。
// 故意不挂 blitter 本身：那是所有 shape 绘制都会走的公共路径，
// 挂上去会波及与动画无关的绘制。
// blitter 的 shape 参数是最后 push 的，即 call 指令处 [esp+0]。
// REF_STACK 依赖 hook 体内的 R，不能抽成独立函数，所以直接写在 hook 体里。
DEFINE_HOOK_AGAIN(0x4233E9, AnimClass_Draw_BlitSHP_Springy, 0x5)
DEFINE_HOOK_AGAIN(0x4236F7, AnimClass_Draw_BlitSHP_Springy, 0x5)
DEFINE_HOOK_AGAIN(0x423827, AnimClass_Draw_BlitSHP_Springy, 0x5)
DEFINE_HOOK(0x42389E, AnimClass_Draw_BlitSHP_Springy, 0x5)
{
	REF_STACK(SHPStruct*, pSHP, 0x0);
	// 太小的值不是指针（原版会传一些常量标记），直接放过
	if (reinterpret_cast<uintptr_t>(pSHP) >= 0x10000)
	{
		if (SHPStruct* pScaled = SpringyAnim::SubstituteSHP(pSHP))
		{
			pSHP = pScaled;
		}
	}
	return 0;
}
