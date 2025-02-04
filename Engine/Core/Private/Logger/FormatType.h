#pragma once

#include "Base/BasicTypes.h"
#include "Platform/SystemTime.h"
#include "Thread/Thread.h"

#include "Stl/string.h"
#include <EASTL/type_traits.h>

namespace SG
{
	namespace fmt
	{

		//! dummy struct for complete type
		struct SBaseFormat
		{
			static string GetDescription() { return ""; }
		};

		//! Time format
		struct STimeFormat : public SBaseFormat
		{
			static string GetDescription() { return ""; }
		};

		//! Year of the system (%y)
		struct STimeYearFormat : public STimeFormat
		{
			static string GetDescription()
			{
				auto str = eastl::to_string(GetSystemTimeYear());
				return eastl::move(str);
			}
		};

		//! Month of the system (%o)
		struct STimeMonthFormat : public STimeFormat
		{
			static string GetDescription()
			{
				auto str = eastl::to_string(GetSystemTimeMonth());
				str = str.size() == 1 ? '0' + str : str;
				return eastl::move(str);
			}
		};

		//! Day of the system (%d)
		struct STimeDayFormat : public STimeFormat
		{
			static string GetDescription()
			{
				auto str = eastl::to_string(GetSystemTimeDay());
				str = str.size() == 1 ? '0' + str : str;
				return eastl::move(str);
			}
		};

		//! Hour in 24-hour system (%h)
		struct STimeHourFormat : public STimeFormat
		{
			static string GetDescription()
			{
				auto str = eastl::to_string(GetSystemTimeHour());
				str = str.size() == 1 ? '0' + str : str;
				return eastl::move(str);
			}
		};

		//! Minute in 60-minute system (%m)
		struct STimeMinuteFormat : public STimeFormat
		{
			static string GetDescription()
			{
				auto str = eastl::to_string(GetSystemTimeMinute());
				if (str.size() == 1)
					str = str.size() == 1 ? '0' + str : str;
				return eastl::move(str);
			}
		};

		//! Second in 60-second system (%s)
		struct STimeSecondFormat : public STimeFormat
		{
			static string GetDescription()
			{
				auto str = eastl::to_string(GetSystemTimeSecond());
				str = str.size() == 1 ? '0' + str : str;
				return eastl::move(str);
			}
		};

		struct SThreadFormat : public SBaseFormat
		{
			string threadName;
			Size   threadId;
			static string GetDescription()
			{
				return eastl::move(GetCurrThreadName());
			}
		};

		template <typename T>
		string FormatString()
		{
			SG_COMPILE_ASSERT((eastl::is_base_of<SBaseFormat, T>::value), "Format type is not base of SBaseForamt");
			return eastl::move(T::GetDescription());
		}
	}
}