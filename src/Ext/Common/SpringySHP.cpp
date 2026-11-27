#include "SpringySHP.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
#include <vector>

#include <FileFormats/SHP.h>

#include <Utilities/Debug.h>

#include <Ext/Helper/Scripts.h>


namespace
{
	struct FramePixels
	{
		int W = 0;
		int H = 0;
		short Left = 0;
		short Top = 0;
		std::vector<byte> Pixels;
	};

	// 缓存键：原 SHP + 量化后的形变矩阵 + 枢轴。
	// 倍率按 1/1000、剪切按 1/4 像素量化，保持全整数比较：
	// 不同 AE 参数但结果相同的会共享同一份，结果不同的不会撞键拿到别人的帧。
	// 波形是 8 档正弦表，所以每张 SHP 稳态只会落 8 个键，条目数有界
	// (SHP, 量化倍率x2, 量化剪切x2, 单位, 枢轴x2, usePivot, inPlace)
	using CacheKey = std::tuple<SHPStruct*, int, int, int, int, int, int, int, bool, bool>;

	std::map<CacheKey, SHPStruct*> Cache{};

	/// @brief 把一帧解码成 Width*Height 的调色板索引缓冲（0 = 透明）
	/// @param pSHP 源 SHP
	/// @param frame 帧号
	/// @param out 输出的像素缓冲，按行优先，长度 W*H
	/// @param w,h 输出该帧的宽高
	/// @param left,top 输出该帧在画布中的左上角
	/// @return 成功与否
	/// @note 宽或高为 0 的帧不算「解出来了」：这种帧没有像素可读，按失败跳过，
	///       写出的帧面积为 0，帧索引仍与原始 SHP 对齐
	bool DecodeFrame(SHPStruct* pSHP, int frame, std::vector<byte>& out, int& w, int& h, short& left, short& top)
	{
		const auto pFile = pSHP->GetData();
		if (!pFile || frame < 0 || frame >= pFile->Frames)
		{
			return false;
		}

		const auto& fh = pFile->GetFrameHeader(frame);
		w = fh.Width;
		h = fh.Height;
		left = fh.Left;
		top = fh.Top;

		// 空帧或异常尺寸：没有可用像素
		if (w <= 0 || h <= 0 || w > 2048 || h > 2048)
		{
			return false;
		}

		const byte* src = pSHP->GetPixels(frame);
		if (!src)
		{
			return false;
		}

		out.assign(static_cast<size_t>(w) * h, 0);

		if (pSHP->HasCompression(frame))
		{
			// 行格式：开头 2 字节是该行编码后的总字节数（含这个长度字段本身），
			// 其后是 RLE 负载。负载语义：
			//   非零字节  = 一个不透明像素，值即调色板索引
			//   零字节 + N = 跳过 N 个透明像素（N 可以为 0，表示不跳）
			// 每行恰好还原 Width 个像素，下一行从 rowStart + rowLen 开始
			const byte* p = src;
			byte* dst = out.data();

			for (int y = 0; y < h; y++)
			{
				const byte* rowStart = p;
				const unsigned short rowLen = *reinterpret_cast<const unsigned short*>(p);
				p += 2;

				int x = 0;
				while (x < w)
				{
					const byte c = *p++;
					if (c != 0)
					{
						dst[x++] = c;
					}
					else
					{
						x += *p++;
					}
				}

				p = rowStart + rowLen;
				dst += w;
			}
		}
		else
		{
			std::memcpy(out.data(), src, static_cast<size_t>(w) * h);
		}

		return true;
	}

	/// @brief 按形变矩阵重采样全部帧并拼成一个新的内存 SHP
	/// @param pOriginal 原始 SHP
	/// @param tf 画布形变（缩放 + 剪切）
	/// @param pivotX,pivotY 形变枢轴（SHP 帧坐标）
	/// @param usePivot true = 绕给定枢轴；false = 每帧自身底边中点
	/// @return 新 SHP；失败返回 nullptr
	/// @note 每帧的画布是「形变后包围盒」：帧头里的 Left/Top/W/H 是逐帧的，
	///       所以可以变大；文件级 Width/Height 保持原值（引擎按它算锚点，改了会整体漂移）。
	///       采样用逆映射：平行四边形四角落在源范围之外，写透明
	SHPStruct* BuildTransformed(SHPStruct* pOriginal, double scaleW, double scaleH,
		double shearX, double shearY, SpringyShearMode shearMode,
		int pivotX, int pivotY, bool usePivot,
		bool useOwnerSlope, double ownerSlopeX, double ownerSlopeY, bool inPlace)
	{
		const auto pFile = pOriginal->GetData();
		if (!pFile)
		{
			return nullptr;
		}

		const int frames = pFile->Frames;
		if (frames <= 0 || frames > 4096)
		{
			return nullptr;
		}

		std::vector<FramePixels> scaled(frames);

		for (int f = 0; f < frames; f++)
		{
			std::vector<byte> px;
			int w = 0;
			int h = 0;
			short left = 0;
			short top = 0;
			// 单帧解不出来就跳过这一帧（写 0 面积），其余帧照常变形：
			// 一份 SHP 里夹着坏帧不该让整个单位失去效果
			if (!DecodeFrame(pOriginal, f, px, w, h, left, top))
			{
				continue;
			}

			// 枢轴：贴身部件绕给定枢轴；独立特效绕自身底边中点，
			// 这样纯缩放时与旧实现完全等价（水平居中、垂直贴底）
			// 非枢轴模式：水平居中、纵向贴底。py0 取「帧底行像素中心」，底行位移才严格为 0
			const double px0 = usePivot ? static_cast<double>(pivotX) : (static_cast<double>(left) + w / 2.0);
			const double py0 = usePivot ? static_cast<double>(pivotY) : (static_cast<double>(top) + h - 0.5);

			// 百分比模式：数值是「整图高度/宽度的百分之几」，先换算成像素
			double shX = shearX;
			double shY = shearY;
			if (useOwnerSlope)
			{
				// 与本体同角度：位移 = 斜率 × 自己的跨度，动画就不会相对本体逐行错开
				shX = ownerSlopeX * static_cast<double>(h - 1);
				shY = ownerSlopeY * static_cast<double>(w - 1);
			}
			else if (shearMode == SpringyShearMode::PERCENT)
			{
				shX = shearX * static_cast<double>(h - 1) / 100.0;
				shY = shearY * static_cast<double>(w - 1) / 100.0;
			}

			// 剪切要除以**这一帧**的尺寸：写 300 就是顶边位移 300 像素，缩放不会把它放大
			SpringyTransform tf;
			tf.SetFromPixels(scaleW, scaleH, shX, shY,
				static_cast<double>(w), static_cast<double>(h));

			double ia = 1.0;
			double ib = 0.0;
			double ic = 0.0;
			double id = 1.0;
			if (!tf.Invert(ia, ib, ic, id))
			{
				continue;
			}

			// 帧矩形相对枢轴的坐标，过一遍矩阵取外接矩形
			double bx = 0.0;
			double by = 0.0;
			double bw = 0.0;
			double bh = 0.0;
			tf.MapRect(static_cast<double>(left - px0), static_cast<double>(top - py0),
				static_cast<double>(w), static_cast<double>(h), bx, by, bw, bh);

			// inPlace：画布固定成原帧矩形，只换像素。建筑本体与建筑动画是画进建筑自己的缓存画布的，
			// 换一张更大的帧会让它越过缓存边界写坏邻行（实测就是一排竖条纹），
			// 所以这类场合不移动原点、也不放大画布，超出的部分裁掉
			const int ox = inPlace ? (left - static_cast<int>(px0)) : static_cast<int>(std::floor(bx));
			const int oy = inPlace ? (top - static_cast<int>(py0)) : static_cast<int>(std::floor(by));
			const int w2 = inPlace ? w : (static_cast<int>(std::ceil(bx + bw)) - ox);
			const int h2 = inPlace ? h : (static_cast<int>(std::ceil(by + bh)) - oy);
			if (w2 <= 0 || h2 <= 0 || w2 > 0x2000 || h2 > 0x2000)
			{
				continue;
			}

			std::vector<byte> sp(static_cast<size_t>(w2) * h2, 0);

			// 目标像素 (x,y) 的画布坐标 = (ox+x, oy+y)，相对枢轴后反解回源帧坐标。
			// 最近邻取样：调色板索引不能插值，插值会混出不存在的颜色
			for (int y = 0; y < h2; y++)
			{
				const double dy = static_cast<double>(oy + y) + 0.5;
				for (int x = 0; x < w2; x++)
				{
					const double dx = static_cast<double>(ox + x) + 0.5;
					const double fx = ia * dx + ib * dy - static_cast<double>(left - px0);
					const double fy = ic * dx + id * dy - static_cast<double>(top - py0);
					const int sx = static_cast<int>(std::floor(fx));
					const int sy = static_cast<int>(std::floor(fy));
					if (sx >= 0 && sx < w && sy >= 0 && sy < h)
					{
						sp[static_cast<size_t>(y) * w2 + x] = px[static_cast<size_t>(sy) * w + sx];
					}
				}
			}

			auto& fp = scaled[f];
			fp.W = w2;
			fp.H = h2;
			fp.Left = static_cast<short>(px0 + ox);
			fp.Top = static_cast<short>(py0 + oy);
			fp.Pixels = std::move(sp);
		}

		const size_t headerSize = 8 + static_cast<size_t>(frames) * 0x18;
		size_t total = headerSize;
		for (const auto& fp : scaled)
		{
			total += fp.Pixels.size();
		}
		// 单份上限：防止异常倍率把内存吃光
		if (total > (64u << 20))
		{
			return nullptr;
		}

		auto* pBuf = new byte[total];

		*reinterpret_cast<short*>(pBuf + 0) = 0; // Type: 文件（不是引用）
		// 画布尺寸保持原值。引擎按「x - Width/2, y - Height」定位一份 SHP，
		// 这里跟着倍率放大会把整张图连同锚点一起推走，表现为整体向左上漂移；
		// 放大后的像素落到原矩形之外，是脏区要盖住的事，不靠改画布尺寸解决
		*reinterpret_cast<short*>(pBuf + 2) = pFile->Width;
		*reinterpret_cast<short*>(pBuf + 4) = pFile->Height;
		*reinterpret_cast<short*>(pBuf + 6) = static_cast<short>(frames);

		int pixelOffset = static_cast<int>(headerSize);
		for (int f = 0; f < frames; f++)
		{
			const auto& fp = scaled[f];
			byte* fh = pBuf + 8 + static_cast<size_t>(f) * 0x18;

			// 几何信息换成缩放后的值
			*reinterpret_cast<short*>(fh + 0x00) = fp.Left;
			*reinterpret_cast<short*>(fh + 0x02) = fp.Top;
			*reinterpret_cast<short*>(fh + 0x04) = static_cast<short>(fp.W);
			*reinterpret_cast<short*>(fh + 0x06) = static_cast<short>(fp.H);
			// 其余三个字段一律写 0：像素是裸写的（无压缩），
			// 也不带阵营色重映射——remap 由外层绘制时按所有者加，不写在帧头
			*reinterpret_cast<int*>(fh + 0x08) = 0;
			*reinterpret_cast<int*>(fh + 0x0C) = 0;
			*reinterpret_cast<int*>(fh + 0x10) = 0;
			*reinterpret_cast<int*>(fh + 0x14) = pixelOffset;

			if (!fp.Pixels.empty())
			{
				std::memcpy(pBuf + pixelOffset, fp.Pixels.data(), fp.Pixels.size());
			}
			pixelOffset += static_cast<int>(fp.Pixels.size());
		}

		return reinterpret_cast<SHPStruct*>(pBuf);
	}
}


SHPStruct* SpringySHP::GetTransformed(SHPStruct* pOriginal,
	double scaleW, double scaleH, double shearX, double shearY, SpringyShearMode shearMode,
	int pivotX, int pivotY, bool usePivot,
	bool useOwnerSlope, double ownerSlopeX, double ownerSlopeY, bool inPlace)
{
	if (!pOriginal)
	{
		return nullptr;
	}
	// 完全没有变形就直接用原图：省掉一份完全相同的副本
	if (scaleW == 1.0 && scaleH == 1.0 && shearX == 0.0 && shearY == 0.0)
	{
		return pOriginal;
	}

	const auto qScale = [](double v) { return static_cast<int>(std::lround(v * 1000.0)); };
	const auto qShear = [](double v) { return static_cast<int>(std::lround(v * 4.0)); };
	const CacheKey key{ pOriginal, qScale(scaleW), qScale(scaleH),
		qShear(useOwnerSlope ? ownerSlopeX * 10000.0 : shearX),
		qShear(useOwnerSlope ? ownerSlopeY * 10000.0 : shearY),
		static_cast<int>(shearMode) + (useOwnerSlope ? 100 : 0), pivotX, pivotY, usePivot, inPlace };
	auto it = Cache.find(key);
	if (it != Cache.end())
	{
		return it->second;
	}

	SHPStruct* pScaled = BuildTransformed(pOriginal, scaleW, scaleH, shearX, shearY, shearMode,
		pivotX, pivotY, usePivot, useOwnerSlope, ownerSlopeX, ownerSlopeY, inPlace);
	// 失败不入缓存：缓存了空指针会让这张 SHP 永久失效，
	// 连「资源稍后才加载好」这种正常情况也救不回来
	if (pScaled)
	{
		Cache.emplace(key, pScaled);
	}

	// 每张 SHP 的每个形变组合只构建一次，所以这条日志不会刷屏。
	// 不放在条件编译里：Release 下看不到日志时，排查「SHP 不变形」会完全没有依据
	const char* name = pOriginal->IsReference() && pOriginal->AsReference()->Filename
		? pOriginal->AsReference()->Filename
		: "?";
	Debug::Log("[Springy]SHP %s 缩放(%.2f,%.2f) 剪切(%.1f,%.1f)px 构建%s\n",
		name, scaleW, scaleH, shearX, shearY, pScaled ? "成功" : "失败");

	return pScaled;
}
void SpringySHP::ClearCache()
{
	for (auto& entry : Cache)
	{
		if (entry.second)
		{
			delete[] reinterpret_cast<byte*>(entry.second);
		}
	}
	Cache.clear();
}
