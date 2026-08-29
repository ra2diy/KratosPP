#include <CRTExt.h>

#include <cstring>

void CRTExt::strCopy(char* Dest, const char* Source, size_t Count) {
	if (Count == 0)
		return;
	strncpy_s(Dest, Count, Source, Count - 1);
	Dest[Count - 1] = 0;
}

void CRTExt::wstrCopy(wchar_t* Dest, const wchar_t* Source, size_t Count) {
	if (Count == 0)
		return;
	wcsncpy_s(Dest, Count, Source, Count - 1);
	Dest[Count - 1] = 0;
}
