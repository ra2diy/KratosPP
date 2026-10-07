#pragma once

// =============================================================================
// Kratos 版本号解析与比较 —— 宿主侧（提示"可更新"用）
//
// ★ 判据是**4 个十进制数字**：主.次.修订.补丁（与 Version.h 的
//   VERSION_MAJOR / MINOR / REVISION / PATCH 一一对应）。
//   比较时把这些数字逐位比（高位优先），**完全不关心字符串长什么样**：
//     * 补丁写成 "p1" 还是省略（省略 = 补丁 0）→ 落到 patch 位，同一个数；
//     * 本机是 DEBUG 还是 Release → DEBUG 只影响显示前缀，4 个数字同源；
//     * 段数够不够 → 缺的按 0 补（"0.2" ≡ "0.2.0.0"）。
//   于是"要不要区分 DEBUG/Release"这个问题在比较层**根本不存在**。
//
// 版本串形态（发布方手写，与 Version.h 的 4 个宏对应）：
//     主.次.修订           例 "0.2.4"      ≡ {0,2,4,0}
//     主.次.修订p补丁      例 "0.2.4p1"    ≡ {0,2,4,1}
//     主.次.修订.补丁      例 "0.2.4.1"    ≡ {0,2,4,1}（与上行等价）
//   * 分隔成"数字段"的部分只能是十进制数字（允许前后空白，解析时忽略）；
//   * 补丁可以用 'p'（大小写均可）引入，也可以直接写第 4 段；'p0' 与"无补丁"等价；
//   * 空串 / 纯空白 ⇒ "无值"（没有版本信息），任何比较都返回"不大于"。
//
// 比较规则（**4 个数字逐位比**，高位优先）：
//   1) 先比 major，不等即出结论；再比 minor、revision、patch。
//   2) 4 个数字全等 ⇒ Equal。
//   3) 任一侧无法解析（空串 / 非法字符 / 段数不足 / 字母尾巴）⇒ Compare::Unknown
//      ⇒ **一律不提示**（fail-quiet：宁可少提示，也不误提示）。
//
// 为什么"无法比较时不提示"：提示的后果是让玩家去换文件；把非法/损坏的版本串当成
// "有新版"会造成噪声与误导，而漏提示只是少一次提醒（玩家仍可自行去发布页）。
//
// ---- 下面还保留了一套字符串比较（CompareStrings / IsNewer）----
// 它是本文件的历史实现，规则更宽（支持 "p1a" 这类字母尾巴、按字母序分级）。
// **新的调用点请用 CompareComponents / IsNewerComponents** ——
// 4 数字模型没有字母序这种歧义来源，也不会有"DEBUG 下补丁形态不同"的问题。
// 字符串版保留是因为已有单测矩阵覆盖它，删掉会丢掉那批回归用例；
// 但"版本更新检查"这条业务路径只用 4 数字版。
// =============================================================================

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace KratosVersion
{
	enum class Compare
	{
		// ★ 取值必须**两两不同**：曾经把 Unknown 与 Equal 都写成 0，
		//   结果 `cmp == Compare::Unknown` 在"两侧相等"时也成立，
		//   于是"版本相同"被误判成"版本串无法解析"（提示逻辑静默失灵）。
		//   单测矩阵当时也没抓到——因为两个枚举值都是 0，期望值与实际值恒等。
		Unknown = -2, // 任一侧无法解析 ⇒ 不比较、不提示
		Lower = -1,   // lhs 比 rhs 旧
		Equal = 0,
		Higher = 1,   // lhs 比 rhs 新
	};

	struct Parsed
	{
		bool valid = false;
		std::vector<uint64_t> segments{}; // 按 '.' 切出的数值段
		bool hasSuffix = false;           // 是否有 'p<数字>' 补丁后缀
		uint64_t suffixNumber = 0;        // 后缀数字（hasSuffix=false 时为 0）
		std::string suffixTail{};         // 后缀数字之后剩下的字母尾巴（原样，比较时忽略大小写）
	};

	inline bool IsSpaceChar(char c)
	{
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	/// <summary>
	/// 解析版本串。返回 valid=false 表示"无值/非法"，调用方必须按"无法比较"处理。
	/// </summary>
	inline Parsed Parse(const std::string& text)
	{
		Parsed out{};
		size_t i = 0;
		const size_t n = text.size();
		while (i < n && IsSpaceChar(text[i]))
		{
			++i;
		}
		size_t end = n;
		while (end > i && IsSpaceChar(text[end - 1]))
		{
			--end;
		}
		if (i >= end)
		{
			return out; // 空串/纯空白 = 无值
		}

		bool sawDigit = false;
		while (i < end)
		{
			// ---- 数值段：至少 1 位十进制数字 ----
			if (!std::isdigit(static_cast<unsigned char>(text[i])))
			{
				return out; // 非法起始字符
			}
			sawDigit = true;
			uint64_t value = 0;
			while (i < end && std::isdigit(static_cast<unsigned char>(text[i])))
			{
				// 溢出保护：超过 10^15 的段直接判非法（真实版本号不可能这么大）
				if (value > 1000000000000000ull)
				{
					return out;
				}
				value = value * 10u + static_cast<uint64_t>(text[i] - '0');
				++i;
			}
			out.segments.push_back(value);

			if (i >= end)
			{
				break; // 正常结束
			}
			if (text[i] == '.')
			{
				++i;
				if (i >= end || !std::isdigit(static_cast<unsigned char>(text[i])))
				{
					return out; // "1." / "1..2" 之类
				}
				continue;
			}

			// ---- 补丁后缀：'p'/'P' + 数字 [+ 字母尾巴] ----
			if (text[i] != 'p' && text[i] != 'P')
			{
				return out; // 不认识的字符
			}
			++i;
			if (i >= end || !std::isdigit(static_cast<unsigned char>(text[i])))
			{
				return out; // "p" 后面不是数字
			}
			out.hasSuffix = true;
			while (i < end && std::isdigit(static_cast<unsigned char>(text[i])))
			{
				if (out.suffixNumber > 1000000000000000ull)
				{
					return out;
				}
				out.suffixNumber = out.suffixNumber * 10u + static_cast<uint64_t>(text[i] - '0');
				++i;
			}
			// 尾巴：剩下的只能是字母（大小写不敏感比较）
			while (i < end)
			{
				const char c = text[i];
				if (!std::isalpha(static_cast<unsigned char>(c)))
				{
					return out; // "p1.2" / "p1-" 之类一律非法
				}
				out.suffixTail.push_back(c);
				++i;
			}
			break;
		}

		out.valid = sawDigit && !out.segments.empty();
		return out;
	}

	/// <summary>
	/// 按上述规则比较两个版本串。任一侧无值/非法 ⇒ Compare::Unknown。
	/// </summary>
	inline Compare CompareStrings(const std::string& lhs, const std::string& rhs)
	{
		const Parsed a = Parse(lhs);
		const Parsed b = Parse(rhs);
		if (!a.valid || !b.valid)
		{
			return Compare::Unknown;
		}

		const size_t count = a.segments.size() > b.segments.size() ? a.segments.size() : b.segments.size();
		for (size_t k = 0; k < count; ++k)
		{
			// 缺的段按 0 补（"1.2" == "1.2.0"）
			const uint64_t va = k < a.segments.size() ? a.segments[k] : 0ull;
			const uint64_t vb = k < b.segments.size() ? b.segments[k] : 0ull;
			if (va != vb)
			{
				return va > vb ? Compare::Higher : Compare::Lower;
			}
		}

		// 数值段全等 ⇒ 比补丁后缀。
		// 先做"p0 == 无后缀"的归一化：补丁号 0 视为没有补丁（"0.2.4p0" == "0.2.4"）。
		const bool aPatch = a.hasSuffix && a.suffixNumber != 0;
		const bool bPatch = b.hasSuffix && b.suffixNumber != 0;
		if (aPatch != bPatch)
		{
			// 有非零 'p' 后缀的一方更新（"0.2.4p1" > "0.2.4"）
			return aPatch ? Compare::Higher : Compare::Lower;
		}
		if (!aPatch)
		{
			return Compare::Equal; // 两侧都没有非零补丁号 ⇒ 相等
		}
		if (a.suffixNumber != b.suffixNumber)
		{
			return a.suffixNumber > b.suffixNumber ? Compare::Higher : Compare::Lower;
		}
		// 后缀数字相同 ⇒ 尾巴按字母序（大小写不敏感）；完全一致 ⇒ 相等
		const std::string ta = [] (std::string s) {
			for (char& c : s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return s;
		}(a.suffixTail);
		const std::string tb = [] (std::string s) {
			for (char& c : s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return s;
		}(b.suffixTail);
		if (ta == tb)
		{
			return Compare::Equal;
		}
		return ta > tb ? Compare::Higher : Compare::Lower;
	}

	/// <summary>
	/// 便捷判定：candidate 是否比 current 新（无法比较时 false ⇒ 不提示）。
	/// </summary>
	inline bool IsNewer(const std::string& candidate, const std::string& current)
	{
		return CompareStrings(candidate, current) == Compare::Higher;
	}

	// =========================================================================
	// 结构化版本号：4 个十进制数字（主.次.修订.补丁）
	// =========================================================================
	//
	// 为什么要有这一层：载荷里的 hostVersion 是**字符串**（发布方手写，形如
	// "0.2.4" / "0.2.4p1"），而宿主本机的版本号是**编译期 4 个宏**
	// （Version.h 的 VERSION_MAJOR / MINOR / REVISION / PATCH）。
	// 把两侧都归一到同一组数字再逐位比较，就不需要关心：
	//   * 字符串里补丁是写成 "p1" 还是省略（"0.2.4" ≡ 补丁 0）；
	//   * 本机是 DEBUG 还是 Release（DEBUG 只影响**显示前缀**，
	//     不改变 4 个数字本身）；
	//   * 段数够不够（缺的按 0 补）。
	//
	// 也就是说："DEBUG 要不要参与比较"这个问题在这一层**根本不存在** ——
	// 比较的是数字，不是字符串，而 DEBUG/Release 的 4 个数字是同源的。
	// =========================================================================

	/// <summary>
	/// 4 元版本号。补丁位 0 表示"无补丁"（与 "0.2.4" ≡ "0.2.4p0" 一致）。
	/// </summary>
	struct Components
	{
		uint64_t major = 0;
		uint64_t minor = 0;
		uint64_t revision = 0;
		uint64_t patch = 0;
	};

	/// <summary>
	/// 把版本串解析成 4 个数字。
	/// 返回 false 表示"无值/非法"，调用方必须按"无法比较"处理（不提示）。
	///
	/// 接受的形态：
	///   "0.2.4"       → {0,2,4,0}
	///   "0.2.4p1"     → {0,2,4,1}
	///   "0.2.4p0"     → {0,2,4,0}   （与 "0.2.4" 等价）
	///   "0.2"         → {0,2,0,0}   （缺的段按 0 补）
	///   "0.2.4p1a"    → 补丁后的字母尾巴**不接受**（见下）
	///
	/// 关于字母尾巴（"0.2.4p1a"）：本函数**只认 4 个数字**。
	/// 带字母尾巴的串在这里判为非法 —— 版本号一旦引入"补丁之后再分级"，
	/// 用 4 个数字就表达不完整，不如让发布方改用补丁号（p2 / p3）来区分。
	/// 好处是比较规则变成"纯数字逐位比"，没有字母序这种歧义来源。
	/// </summary>
	inline bool ParseComponents(const std::string& text, Components* out)
	{
		if (!out)
		{
			return false;
		}
		*out = Components{};

		const Parsed p = Parse(text);
		if (!p.valid)
		{
			return false;
		}
		// 字母尾巴：4 数字模型表达不了 ⇒ 判非法（调用方按"不提示"处理）
		if (!p.suffixTail.empty())
		{
			return false;
		}

		// 前 3 段依次落到 major/minor/revision（缺的按 0）
		if (p.segments.size() > 0) { out->major = p.segments[0]; }
		if (p.segments.size() > 1) { out->minor = p.segments[1]; }
		if (p.segments.size() > 2) { out->revision = p.segments[2]; }
		// ★ 第 4 段有**两种**来源，且都归一到 patch：
		//   * "1.2.3.4" 这种直接写第 4 段的；
		//   * "1.2.3p4" 这种用 'p' 后缀的（Version.h 的发布写法）。
		//   两者等价 —— 这正是"拆成 4 个数字"最直接的收益：
		//   补丁无论怎么写，落到 patch 位上就是同一个数。
		if (p.segments.size() > 3) { out->patch = p.segments[3]; }
		if (p.hasSuffix)
		{
			// "1.2.3.4p5" 这种既有第 4 段又有后缀的写法语义不明，判非法
			if (p.segments.size() > 3)
			{
				return false;
			}
			out->patch = p.suffixNumber;
		}

		return true;
	}

	/// <summary>
	/// 按 4 个数字逐位比较（高位优先）。任一侧无法解析 ⇒ Compare::Unknown。
	/// 这是"版本更新检查"应当使用的入口 —— 它不依赖任何字符串形态。
	/// </summary>
	inline Compare CompareComponents(const std::string& lhs, const std::string& rhs)
	{
		Components a{};
		Components b{};
		if (!ParseComponents(lhs, &a) || !ParseComponents(rhs, &b))
		{
			return Compare::Unknown;
		}

		if (a.major != b.major) { return a.major > b.major ? Compare::Higher : Compare::Lower; }
		if (a.minor != b.minor) { return a.minor > b.minor ? Compare::Higher : Compare::Lower; }
		if (a.revision != b.revision) { return a.revision > b.revision ? Compare::Higher : Compare::Lower; }
		if (a.patch != b.patch) { return a.patch > b.patch ? Compare::Higher : Compare::Lower; }
		return Compare::Equal;
	}

	/// <summary>
	/// 便捷判定：candidate 的 4 个数字是否**严格大于** current（无法解析时 false ⇒ 不提示）。
	/// </summary>
	inline bool IsNewerComponents(const std::string& candidate, const std::string& current)
	{
		return CompareComponents(candidate, current) == Compare::Higher;
	}
}
