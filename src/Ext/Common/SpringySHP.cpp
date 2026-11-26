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

	// 缓存键：原 SHP + 量化后的倍率 + 枢轴。
	// 倍率按千分之一量化，保持全整数比较：不同 AE 参数但结果相同的会共享同一份，
	// 结果不同的不会撞键拿到别人的帧。
	using CacheKey = std::tuple<SHPStruct*, int, int, int, int, bool>;

	std::map<CacheKey, SHPStruct*> Cache{};

	int Quantize(double scale)
	{
		return static_cast<int>(std::lround(scale * 1000.0));
	}

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

	/// @brief 按给定倍率重采样全部帧并拼成一个新的内存 SHP
	/// @return 新 SHP；失败返回 nullptr
	SHPStruct* BuildScaled(SHPStruct* pOriginal, double scaleW, double scaleH,
		int pivotX, int pivotY, bool usePivot)
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
			// 单帧解不出来就跳过这一帧（写 0 面积），其余帧照常缩放：
			// 一份 SHP 里夹着坏帧不该让整个单位失去缩放
			if (!DecodeFrame(pOriginal, f, px, w, h, left, top))
			{
				continue;
			}

			const int w2 = std::max(1, static_cast<int>(std::lround(w * scaleW)));
			const int h2 = std::max(1, static_cast<int>(std::lround(h * scaleH)));
			std::vector<byte> sp(static_cast<size_t>(w2) * h2, 0);

			// 最近邻取样：不插值。插值会混出不存在的调色板索引，看起来是脏点
			for (int y = 0; y < h2; y++)
			{
				const int sy = std::min(h - 1, y * h / h2);
				for (int x = 0; x < w2; x++)
				{
					const int sx = std::min(w - 1, x * w / w2);
					sp[static_cast<size_t>(y) * w2 + x] = px[static_cast<size_t>(sy) * w + sx];
				}
			}

			auto& fp = scaled[f];
			fp.W = w2;
			fp.H = h2;
			if (usePivot)
			{
				// 绕给定枢轴缩放：贴身子部件要绕单位底边变，否则会脱体
				fp.Left = static_cast<short>(std::lround(pivotX + (left - pivotX) * scaleW));
				fp.Top = static_cast<short>(std::lround(pivotY + (top - pivotY) * scaleH));
			}
			else
			{
				// 底边对齐：水平居中、垂直贴底，否则单位会浮空或陷地
				fp.Left = static_cast<short>(left + (w - w2) / 2);
				fp.Top = static_cast<short>(top + (h - h2));
			}
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


SHPStruct* SpringySHP::GetScaled(SHPStruct* pOriginal, double scaleW, double scaleH,
	int pivotX, int pivotY, bool usePivot)
{
	if (!pOriginal)
	{
		return nullptr;
	}
	// 倍率没变就没有变形，直接用原图：省掉一份完全相同的副本
	if (std::abs(scaleW - 1.0) < 0.0005 && std::abs(scaleH - 1.0) < 0.0005)
	{
		return pOriginal;
	}

	const CacheKey key{ pOriginal, Quantize(scaleW), Quantize(scaleH), pivotX, pivotY, usePivot };
	auto it = Cache.find(key);
	if (it != Cache.end())
	{
		return it->second;
	}

	SHPStruct* pScaled = BuildScaled(pOriginal, scaleW, scaleH, pivotX, pivotY, usePivot);
	// 失败不入缓存：缓存了空指针会让这张 SHP 永久失效，
	// 连「资源稍后才加载好」这种正常情况也救不回来
	if (pScaled)
	{
		Cache.emplace(key, pScaled);
	}

	// 每张 SHP 的每个倍率组合只构建一次，所以这条日志不会刷屏。
	// 不放在条件编译里：Release 下看不到日志时，排查「SHP 不缩放」会完全没有依据
	const char* name = pOriginal->IsReference() && pOriginal->AsReference()->Filename
		? pOriginal->AsReference()->Filename
		: "?";
	Debug::Log("[Springy]SHP %s 倍率(%.2f,%.2f) 构建%s\n",
		name, scaleW, scaleH, pScaled ? "成功" : "失败");

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
