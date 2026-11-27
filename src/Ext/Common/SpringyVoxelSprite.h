#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include <Drawing.h>

#include <Ext/Common/SpringyTransform.h>


/// @brief 体素精灵（VoxelCacheStruct）的位图放大
/// @note 精灵的像素区是**逐行游程编码**：每行开头 `uint16 行总字节数（含本字段）`，
///       其后是负载；负载里非零字节 = 一个像素（调色板索引），`0x00` + n = 跳过 n 个透明像素。
///       依据是构建函数 0x6C89E0：它逐行取源表面的一行，调 0x661590 压缩到临时缓冲，
///       再把**压缩后的字节数**拷进精灵场地；而 0x661590 就是「写 2 字节行长 + 调 0x661530 压负载」。
///
///       放大只能在贴图这一刻做：光栅化器按绘制矩阵的步长在屏幕行列上取样，把放大尺寸
///       直接交给它，相邻采样之间就留 倍率−1 个空位，画面上是一片离散像素被网格状缝隙切开。
///       而引擎按原尺寸光栅化出来的精灵本身是完整的，重采样它不会凭空产生缝。
///
///       **一行的实际像素数不假定等于 `Width`**：压缩器的源是连续表面，数行尾透明段时会多走一步、
///       看到行尾之后那一个字节，跨行的透明段会被并进上一行，于是某些行解出的像素数会比 `Width`
///       多出若干格。引擎自己贴图时按可见宽度截断，看不出来；这里如果把每行硬裁到 `Width`，
///       行内位置就会整行偏移，放大后表现为成片的调色板杂色。所以先按行量出实际像素数，
///       以其中最大值作为放大前的源宽度，每行不足的部分补透明，各行的相对位置才不会被裁歪。
namespace SpringyVoxelSprite
{
	/// @brief 引擎自己的行压缩器
	/// @note `__stdcall`，参数依次是源行、目标、行宽，返回写出的行总长（含行首那 2 字节）。
	///       必须用它而不是自己写：它数行尾透明段时会把计数多记 1（比较之后用的是 `jg`
	///       而不是 `jge`，循环多走一步，还会顺带看行尾后面那个字节），自己按「精确值」写出的
	///       字节流在它的解压器眼里并不等价，后续行会整体错位，画面上就是零散的调色板杂色
	using CompressRowFn = int(__stdcall*)(unsigned char* src, unsigned char* dst, int width);
	inline CompressRowFn CompressRow = reinterpret_cast<CompressRowFn>(0x661590);

	/// @brief 放大结果轮流占用的槽数
	/// @note 同一帧内主体、炮塔、炮管、影子各贴一次，每次都会重写缓冲。
	///       引擎是在贴图调用内部就把精灵用完，多留几个槽可以彻底避免后一次覆盖前一次
	constexpr int PoolSize = 8;

	struct Slot
	{
		std::vector<unsigned char> Encoded;
		VoxelCacheStruct Sprite{};
	};

	inline Slot& NextSlot()
	{
		static Slot slots[PoolSize];
		static int next = 0;

		Slot& slot = slots[next];
		next = (next + 1) % PoolSize;
		return slot;
	}

	inline std::vector<unsigned char>& RawPixels()
	{
		static std::vector<unsigned char> buffer;
		return buffer;
	}

	inline std::vector<unsigned char>& ScaledRow()
	{
		static std::vector<unsigned char> buffer;
		return buffer;
	}

	inline std::vector<const unsigned char*>& RowPtrs()
	{
		static std::vector<const unsigned char*> rows;
		return rows;
	}

	inline std::vector<int>& RowCounts()
	{
		static std::vector<int> counts;
		return counts;
	}

	/// @brief 行尾位置（行长字段含自身那 2 字节）
	inline const unsigned char* RowEnd(const unsigned char* pRow)
	{
		return pRow + *reinterpret_cast<const uint16_t*>(pRow);
	}

	/// @brief 数一行负载里实际包含多少个像素（透明也算）
	/// @param p 负载起点（行长字段之后）
	/// @param end 负载终点（行尾位置）
	/// @note 只按游程规则累加，不假设任何宽度。行尾的透明段被压缩侧多记几格也照实计入，
	///       这样量出来的才是这一行真正占了多少格
	inline int CountPixels(const unsigned char* p, const unsigned char* end)
	{
		int count = 0;
		while (p < end)
		{
			const unsigned char value = *p++;
			if (value)
			{
				++count;
			}
			else
			{
				if (p >= end)
				{
					break;
				}
				count += *p++;
			}
		}
		return count;
	}

	/// @brief 把一行负载解成像素，写进 dst 的 stride 长度
	/// @param pRow 行首（含行长字段）
	/// @param count 这一行的实际像素数，由 CountPixels 量出
	/// @param stride 目标行宽（各行统一）
	/// @note 超出 count 的跳段不再写入，尾部按透明补齐到 stride
	inline void DecodeRow(const unsigned char* pRow, int count, unsigned char* dst, int stride)
	{
		const unsigned char* p = pRow + 2;
		const unsigned char* const end = RowEnd(pRow);

		int x = 0;
		while (p < end && x < count)
		{
			const unsigned char value = *p++;
			if (value)
			{
				dst[x++] = value;
			}
			else
			{
				if (p >= end)
				{
					break;
				}
				x += *p++;
			}
		}

		for (; x < stride; ++x)
		{
			dst[x] = 0;
		}
	}

	/// @brief 贴图的目标表面与绘制基准点
	/// @note 引擎把精灵画到「基准点 + (X, Y)」，基准点是目标表面上的落点原点。
	///       主体、炮塔、炮管画进的是一张 256×256 的临时表面，基准点恒为它的中心 (128,128)，
	///       也就是说屏幕上的体素最大只能占 256×256 —— 这是 `0x707480` 的硬限制。
	///       放大后的精灵一旦超出这张表面，超出的列不会消失：贴图按「行首 + 行号×表面宽度 + x」
	///       直接写，负的 x 落到上一行的行尾、超过宽度的 x 落到下一行的行首，画面上就是一条
	///       与车体分离、却跟着车走的竖直切片；行号越界更糟，会写到缓冲区之前的地址上。
	///       所以放大结果必须裁到表面以内：表面之外的像素本来就贴不出来，裁掉不损失任何可见内容
	struct TargetBox
	{
		int BaseX = 0;
		int BaseY = 0;
		int Width = 0;
		int Height = 0;
	};

	/// @brief 按形变矩阵重采样一张精灵（缩放 + 剪切）
	/// @param src 引擎按原尺寸光栅化出来的精灵
	/// @param tf 本帧的画布形变（缩放 + 剪切复合，见 SpringyTransform）
	/// @param pivotY 剪切枢轴（贴图坐标里的一条水平线）：通常取这个单位所有部件里最低的那条边
	/// @param box 这次贴图的目标表面与基准点，结果会被裁到表面以内
	/// @return 变形后的精灵；格式不符或形变恰是恒等时返回 nullptr（调用方按原精灵画）
	/// @note 最近邻**逆映射**：每个目标像素反解到源坐标取样。
	///       逆映射不会让平行四边形里留洞（正射映射在放大时会漏格），
	///       反解到源范围之外的像素写透明 —— 平行四边形四角的透明区就是这么来的。
	///       枢轴是地面线：它不动，越往上偏移越大。绕贴图基准点（车身中心）做的话
	///       上下两半会朝相反方向移，看上去就是整体旋转，不是「推」。
	///       锚点按几何变换处理：把源包围盒整体过一遍矩阵、取外接矩形，
	///       所以主体、炮塔、炮管共用同一条地面线时不会互相拉偏
	inline VoxelCacheStruct* Transform(const VoxelCacheStruct* src, const SpringyTransform& tf,
		double pivotY, const TargetBox& box)
	{
		if (!src || !src->Buffer || src->Width == 0 || src->Height == 0 || tf.IsIdentity())
		{
			return nullptr;
		}

		double ia = 1.0;
		double ib = 0.0;
		double ic = 0.0;
		double id = 1.0;
		if (!tf.Invert(ia, ib, ic, id))
		{
			return nullptr;
		}

		const int w = src->Width;
		const int h = src->Height;
		const unsigned char* const base = static_cast<const unsigned char*>(src->Buffer);

		std::vector<const unsigned char*>& rows = RowPtrs();
		std::vector<int>& counts = RowCounts();
		rows.resize(h);
		counts.resize(h);

		// 第一遍：量出每行的位置与真实像素数。行长字段含自身那 2 字节，
		// 负载最坏是每个像素各成一段（2 字节），再多留几格余量
		const unsigned char* p = base;
		int sourceWidth = 0;
		for (int y = 0; y < h; ++y)
		{
			const unsigned int length = *reinterpret_cast<const uint16_t*>(p);
			if (length < 3 || length > static_cast<unsigned int>(2 + 2 * w + 16))
			{
				return nullptr;
			}

			const int count = CountPixels(p + 2, p + length);
			if (count <= 0)
			{
				return nullptr;
			}

			rows[y] = p;
			counts[y] = count;
			if (count > sourceWidth)
			{
				sourceWidth = count;
			}
			p += length;
		}

		// 各行像素数与 Width 相差过大，说明这一行的解析本身不可信，放弃变形
		if (sourceWidth < w || sourceWidth > w + 8)
		{
			return nullptr;
		}

		// 源精灵的像素 (px,py) 落在「基准点 + (X+px, Y+py)」，枢轴是基准点坐标里 y = pivotY
		// 那条水平线。把源矩形换成**相对枢轴**的坐标再过矩阵：枢轴上的点位移恒为 0，
		// 越往上偏移越大（这才是「从矩形推成平行四边形」）。取外接矩形后向外取整到像素边界：
		// 内缩取整会让最后一行/列落出包围盒，宁可多一格
		double bx = 0.0;
		double by = 0.0;
		double bw = 0.0;
		double bh = 0.0;
		tf.MapRect(static_cast<double>(src->X), static_cast<double>(src->Y) - pivotY,
			static_cast<double>(sourceWidth), static_cast<double>(h), bx, by, bw, bh);

		const int anchorX = static_cast<int>(std::floor(bx));
		const int anchorY = static_cast<int>(std::floor(by + pivotY));
		const int nw = static_cast<int>(std::ceil(bx + bw)) - anchorX;
		const int nh = static_cast<int>(std::ceil(by + bh + pivotY)) - anchorY;
		if (nw <= 0 || nh <= 0 || nw > 0x2000 || nh > 0x2000)
		{
			return nullptr;
		}

		// 裁到目标表面以内。表面之外的像素本来就贴不出来，但不裁不会「什么都不显示」：
		// 贴图按行写，负的列落到上一行行尾、超过宽度的列落到下一行行首，画面上就是一条
		// 与车体分离却跟着车走的竖条；行号越界还会写到缓冲区之前的地址上。
		// 裁掉的是看不见的部分，可见内容位置不变：裁掉左边几列，X 就补几列
		int cropLeft = 0;
		int cropTop = 0;
		int cropRight = nw;
		int cropBottom = nh;
		if (box.Width > 0 && box.Height > 0)
		{
			cropLeft = std::max(0, -box.BaseX - anchorX);
			cropTop = std::max(0, -box.BaseY - anchorY);
			cropRight = std::min(nw, box.Width - box.BaseX - anchorX);
			cropBottom = std::min(nh, box.Height - box.BaseY - anchorY);
			if (cropRight <= cropLeft || cropBottom <= cropTop)
			{
				return nullptr;
			}
		}

		const int ow = cropRight - cropLeft;
		const int oh = cropBottom - cropTop;

		std::vector<unsigned char>& raw = RawPixels();
		raw.assign(static_cast<size_t>(sourceWidth) * h, 0);

		for (int y = 0; y < h; ++y)
		{
			DecodeRow(rows[y], counts[y], &raw[static_cast<size_t>(y) * sourceWidth], sourceWidth);
		}

		Slot& slot = NextSlot();
		// 一行最坏是每个像素各占一段：负载 2×宽度 字节，再加行首那 2 字节
		slot.Encoded.assign(static_cast<size_t>(oh) * (2 * static_cast<size_t>(ow) + 2) + 8, 0);
		unsigned char* const dstBase = slot.Encoded.data();

		std::vector<unsigned char>& row = ScaledRow();
		row.assign(static_cast<size_t>(ow) + 1, 0);

		size_t offset = 0;
		for (int y = cropTop; y < cropBottom; ++y)
		{
			// 目标像素相对基准点的坐标（像素左上角）；先换成相对枢轴，再反解回源
			const double dy = static_cast<double>(anchorY + y) - pivotY;
			for (int x = cropLeft; x < cropRight; ++x)
			{
				const double dx = static_cast<double>(anchorX + x);
				const double fx = ia * dx + ib * dy - static_cast<double>(src->X);
				const double fy = ic * dx + id * dy + pivotY - static_cast<double>(src->Y);
				const int sx = static_cast<int>(std::floor(fx));
				const int sy = static_cast<int>(std::floor(fy));
				row[x - cropLeft] =
					(sx >= 0 && sx < sourceWidth && sy >= 0 && sy < h)
					? raw[static_cast<size_t>(sy) * sourceWidth + sx]
					: 0;
			}

			// 行尾哨兵：压缩器数行尾透明段时会多看后面一个字节，行尾之后必须是可读的非零字节，
			// 计数才停在「实际个数」处。它压自己那张连续表面时，行尾之后就是下一行的第一个像素，
			// 有内容时同样是这个结果，所以这里放一个非零字节等价，并且保证不会越界读
			row[ow] = 1;

			const int length = CompressRow(row.data(), dstBase + offset, ow);
			if (length < 3)
			{
				return nullptr;
			}
			offset += static_cast<size_t>(length);
		}

		slot.Sprite.Width = static_cast<unsigned short>(ow);
		slot.Sprite.Height = static_cast<unsigned short>(oh);
		slot.Sprite.Buffer = dstBase;

		slot.Sprite.X = static_cast<short>(std::clamp(anchorX + cropLeft, -32768, 32767));
		slot.Sprite.Y = static_cast<short>(std::clamp(anchorY + cropTop, -32768, 32767));

		return &slot.Sprite;
	}
}
