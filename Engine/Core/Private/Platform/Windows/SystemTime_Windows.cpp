#include "StdAfx.h"
#ifdef SG_PLATFORM_WINDOWS
#include "Platform/SystemTime.h"

namespace SG
{

	Size GetSystemTimeHour()
	{
		time_t tm = time(NULL);
		struct tm BackupTime;
		localtime_s(&BackupTime, &tm);
		return BackupTime.tm_hour;
	}

	Size GetSystemTimeMinute()
	{
		time_t tm = time(NULL);
		struct tm BackupTime;
		localtime_s(&BackupTime, &tm);
		return BackupTime.tm_min;
	}

	Size GetSystemTimeSecond()
	{
		time_t tm = time(NULL);
		struct tm BackupTime;
		localtime_s(&BackupTime, &tm);
		return BackupTime.tm_sec;
	}

	Size GetSystemTimeYear()
	{
		time_t tm = time(NULL);
		struct tm BackupTime;
		localtime_s(&BackupTime, &tm);
		return BackupTime.tm_year + 1900;
	}

	Size GetSystemTimeMonth()
	{
		time_t tm = time(NULL);
		struct tm BackupTime;
		localtime_s(&BackupTime, &tm);
		return BackupTime.tm_mon + 1;
	}

	Size GetSystemTimeDay()
	{
		time_t tm = time(NULL);
		struct tm BackupTime;
		localtime_s(&BackupTime, &tm);
		return BackupTime.tm_mday;
	}

}
#endif