#pragma once
#ifndef VERSION_H
#define VERSION_H

#define WSTR(x) WSTR_(x)
#define WSTR_(x) L ## #x
#define STR(x) STR_(x)
#define STR_(x) #x

#pragma region Release build version numbering

// Indicates project maturity and completeness
#define VERSION_MAJOR 0

// Indicates major changes and significant additions, like new logics
#define VERSION_MINOR 2

// Indicates minor changes, like vanilla bugfixes, unhardcodings or hacks
#define VERSION_REVISION 2

// Indicates Kratos-related bugfixes only
#define VERSION_PATCH 11

#pragma endregion

// Build number. Incremented on each released build.
#define BUILD_NUMBER 1

// Helper macros for version string formatting
#ifdef DEBUG
	#define VERSION_SHORT_STR "Debug " STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_REVISION)
	#define VERSION_SHORT_WSTR L"Debug " WSTR(VERSION_MAJOR) L"." WSTR(VERSION_MINOR) L"." WSTR(VERSION_REVISION)
#else // Release build
	#if VERSION_PATCH == 0
		#define VERSION_SHORT_STR "Ver." STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_REVISION)
		#define VERSION_SHORT_WSTR L"Ver." WSTR(VERSION_MAJOR) L"." WSTR(VERSION_MINOR) L"." WSTR(VERSION_REVISION)
	#else
		#define VERSION_SHORT_STR "Ver." STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_REVISION) "p" STR(VERSION_PATCH)
		#define VERSION_SHORT_WSTR L"Ver." WSTR(VERSION_MAJOR) L"." WSTR(VERSION_MINOR) L"." WSTR(VERSION_REVISION) L"p" WSTR(VERSION_PATCH)
	#endif // VERSION_PATCH
#endif // DEBUG

// version infomation
#define PRODUCT_NAME "Kratos"
#define COMPANY_NAME "ChrisLv_CN (https://space.bilibili.com/276838)"
#define LEGAL_COPYRIGHT "© The ChrisLv_CN 🐼 Contributors 2023"
#define FILE_DESCRIPTION "Kratos, Ares-like YR engine extension"
#define FILE_VERSION_STR STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_REVISION) "." STR(VERSION_PATCH)
#define FILE_VERSION VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_PATCH
#ifdef DEBUG // Debug build metadata
	#define SAVEGAME_ID ((BUILD_NUMBER << 24) | (BUILD_NUMBER << 12) | (BUILD_NUMBER))
	#define PRODUCT_VERSION "Debug Build " STR(BUILD_NUMBER)
#else // Release build metadata
	#define SAVEGAME_ID ((VERSION_MAJOR << 24) | (VERSION_MINOR << 16) | (VERSION_REVISION << 8) | VERSION_PATCH)
	#define PRODUCT_VERSION STR(VERSION_MAJOR) "." STR(VERSION_MINOR)
#endif // DEBUG
#define INTERNAL_NAME "Kratos.dll"
#define ORIGINAL_FILENAME "Kratos.dll"

#endif // VERSION_H
