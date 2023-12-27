// tsds.cpp : Определяет функции для статической библиотеки.
//

#include "pch.h"
#include "tsds.h"
#include "framework.h"

#pragma once
#include <string>
#include <stdexcept>
#include <memory>
#include <string_view>
#include <fstream>
#include <filesystem>
#include <codecvt>
#include <chrono>
#include <thread>

#define _TSDS_CONFIGFILE_FORMAT_WSTR L"tsds_conf"

#define _TSDS_CONFIGFILE_ENDFORMAT_WSTR L"." _TSDS_CONFIGFILE_FORMAT_WSTR

#define _TSDS_SERIESFILE_PREFIX_WSTR L"_"

#define _TSDS_SERIESFILE_FORMAT_WSTR L"tsds_series_chunk"

#define _TSDS_SERIESFILE_ENDFORMAT_WSTR L"." _TSDS_SERIESFILE_FORMAT_WSTR

#define TSDS_CONFIGFILE_WSTR(TSDS_FILENAME) L#TSDS_FILENAME _TSDS_CONFIGFILE_ENDFORMAT_WSTR

#define TSDS_SERIESFILE_WSTR(TSDS_FILENAME) L#TSDS_FILENAME _TSDS_SERIESFILE_ENDFORMAT_WSTR

#pragma warning(disable: 4996)

using convert_type = std::codecvt_utf8<wchar_t>;
static std::wstring_convert<convert_type, wchar_t> g_u16u8converter;

#pragma warning(default: 4996)

std::allocator<TsdsDatabase> g_databaseAllocator;

std::allocator<TsdsPhysicalStore> g_physicalStoreAllocator;

std::allocator<TsdsSeries> g_seriesAllocator;

std::allocator<void> g_memoryAllocator;

struct TsdsDatabaseConfig
{

};

struct TsdsSeriesConfig
{
	TSDS_INDEX_FORMAT	indexFormat			{ TSDS_INDEX_FORMAT_UNSIGNED_INTEGER_64 };
	TsdsIndex		indexBegin			{   0ULL  };
	TsdsIndex		indexStep			{   0ULL  };
	TsdsSize		elementsCount			{   0ULL  };
	TsdsSize		chankSize			{   0ULL  };
	TsdsSize		elementSize			{   0ULL  };
};

typedef struct TsdsPhysicalStore
{
	std::wstring			folderPath	{	};

	std::vector<TsdsDatabase*> 	databaseList	{	};
} TsdsPhysicalStore;

typedef struct TsdsDatabase
{
	TsdsPhysicalStore*		pPhysicalStore		{ nullptr };
	TsdsDatabaseConfig		databaseConfig		{         };

	std::wstring			databaseName		{         };
	std::wstring			fullPath		{         };

	std::vector<TsdsSeries*> 	seriesList		{	  };
} TsdsDatabase;

typedef struct TsdsSeries
{
	TsdsDatabase*		pDatabase			{ nullptr };

	TsdsSeriesConfig	config{};

	std::wstring		seriesName			{         };
	std::wstring		fullPath			{         };
	std::wstring		configFileName			{         };

} TsdsSeries;

inline void _TsdsThrow(
	const std::string& message,
	const std::string& functionName,
	unsigned long long lineOfCode
)
{
	throw std::runtime_error(
		std::string("TSDS Error in function \"") + functionName +
		"\", line " + std::to_string(lineOfCode) + ": " + message
	);
}

#pragma warning(disable: 4996)

void TsdsTimeFromTimestamp(
	TsdsTime* time, 
	long long timestamp
)
{
	if (timestamp == 0LL) return;
	constexpr int years_since = 1900;
	std::chrono::system_clock::time_point time_point =
		std::chrono::system_clock::from_time_t(timestamp);
	long long local_time = std::chrono::system_clock::to_time_t(time_point);
	std::tm* gmtm = std::gmtime(&local_time);
	if (gmtm == nullptr)
	{
		_TsdsThrow(
			std::string("wrong parameter \"timestamp\": " +
				std::to_string(timestamp)),
			__FUNCSIG__, __LINE__
		);
	}
	std::memcpy(time, gmtm, sizeof(int) * 8ULL);
	time->year += years_since;
}

void TsdsTimeFromTimestampWithMilliseconds(
	TsdsTime* time, 
	long long timestamp
)
{
	if (timestamp == 0LL) return;
	constexpr long long milli_in_sec = 1'000LL;
	TsdsTimeFromTimestamp(time, timestamp / milli_in_sec);
	time->milli = timestamp % milli_in_sec;
}

long long TsdsTimestampFromTime(TsdsTime* time)
{
	constexpr int years_since = 1'900;
	std::tm gmtm;
	std::memcpy(&gmtm, time, sizeof(int) * 8ULL);
	gmtm.tm_year -= years_since;
	long long timestamp = std::mktime(&gmtm);
	timestamp += 25'200LL;
	return timestamp;
}

long long TsdsTimestampFromTimeMilliseconds(TsdsTime* time)
{
	constexpr long long milli_in_sec = 1'000LL;
	long long timestamp = TsdsTimestampFromTime(time);
	timestamp *= milli_in_sec;
	timestamp += time->milli;
	return timestamp;
}

#pragma warning(default: 4996)

bool _TsdsIsFolderExists(std::wstring_view folderPathWstr)
{
	return std::filesystem::is_directory(folderPathWstr);
}

bool _TsdsIsFileExists(std::wstring_view filePathWstr)
{
	return std::filesystem::exists(filePathWstr);
}

void _TsdsCreateFolders(std::wstring_view folderPathWstr)
{
	std::filesystem::path folderPath = folderPathWstr;

	std::error_code errc;
	if (!std::filesystem::create_directories(folderPath, errc))
	{
		_TsdsThrow(
			std::string("failed to create folders (directories) (") + 
			g_u16u8converter.to_bytes(folderPathWstr.data()) + "): " +
			errc.message(),
			__FUNCSIG__, __LINE__
		);
	}
}

void _TsdsCreateFolder(std::wstring_view folderPathWstr)
{
	std::filesystem::path folderPath = folderPathWstr;

	std::error_code errc;
	if (!std::filesystem::create_directory(folderPath, errc))
	{
		_TsdsThrow(
			std::string("failed to create folder (directory) (") +
			g_u16u8converter.to_bytes(folderPathWstr.data()) + "): " +
			errc.message(),
			__FUNCSIG__, __LINE__
		);
	}
}

void _TsdsCreateFile(
	const std::wstring& filePathWstr, 
	const void* data = nullptr, 
	size_t dataSize = 0ULL
)
{
	std::ofstream fileStream;
	fileStream.open(filePathWstr, std::ios::out | std::ios::binary);
	if (fileStream.is_open())
	{
		if (data != nullptr && dataSize > 0ULL)
		{
			fileStream.write(reinterpret_cast<const char*>(data), dataSize);
			if (fileStream.bad())
			{
				std::error_code errc;
				bool isRemoveGood = std::filesystem::remove(filePathWstr, errc);
				fileStream.close();
				_TsdsThrow(
					std::string("failed to write file, consequently, an unsuccessful attempt was made to delete the file (") +
					g_u16u8converter.to_bytes(filePathWstr.data()) + "): " +
					errc.message(),
					__FUNCSIG__, __LINE__
				);
			}
		}
		fileStream.close();
	}
	else
	{
		fileStream.close();
		_TsdsThrow(
			std::string("failed to create file (") +
			g_u16u8converter.to_bytes(filePathWstr.data()) + ")",
			__FUNCSIG__, __LINE__
		);
	}
}

void _TsdsWriteFile(
	std::wstring_view filePathWstr,
	const void* data,
	size_t dataSize,
	bool clearFile = false
)
{
	std::ofstream fileStream;
	if (!clearFile)
	{
		fileStream.open(filePathWstr, std::ios::app | std::ios::binary);
	}
	else
	{
		fileStream.open(filePathWstr, std::ios::out | std::ios::binary);
	}
	if (fileStream.is_open())
	{
		if (data != nullptr && dataSize > 0ULL)
		{
			fileStream.write(reinterpret_cast<const char*>(data), dataSize);
			if (fileStream.bad())
			{
				fileStream.close();
				_TsdsThrow(
					std::string("failed to write file (") +
					g_u16u8converter.to_bytes(filePathWstr.data()) + ")",
					__FUNCSIG__, __LINE__
				);
			}
		}
		fileStream.close();
	}
	else
	{
		fileStream.close();
		_TsdsThrow(
			std::string("failed to open file (") +
			g_u16u8converter.to_bytes(filePathWstr.data()) + ")",
			__FUNCSIG__, __LINE__
		);
	}
}

void _TsdsReadFile(
	std::wstring_view filePathWstr,
	void* data,
	size_t dataSize,
	size_t dataOffset = 0
)
{
	std::ifstream fileStream;
	fileStream.open(filePathWstr, std::ios::in | std::ios::binary);
	if (fileStream.is_open())
	{
		if (dataOffset > 0)
		{
			fileStream.seekg(dataOffset);
		}
		fileStream.read(reinterpret_cast<char*>(data), dataSize);
		if (fileStream.bad())
		{
			std::error_code errc;
			fileStream.close();
			_TsdsThrow(
				std::string("failed to read file (") +
				g_u16u8converter.to_bytes(filePathWstr.data()) + ")",
				__FUNCSIG__, __LINE__
			);
		}
		fileStream.close();
	}
	else
	{
		fileStream.close();
		_TsdsThrow(
			std::string("failed to create file (") +
			g_u16u8converter.to_bytes(filePathWstr.data()) + ")",
			__FUNCSIG__, __LINE__
		);
	}
}

TsdsPhysicalStore* TsdsCreatePhysicalStore(TsdsPhysicalStoreDesc physicalStoreDesc)
{
	std::wstring_view folderPath = physicalStoreDesc.folderPath;

	if (folderPath.empty())
	{
		_TsdsThrow(
			"physicalStoreDesc.folderPath must be not empty",
			__FUNCSIG__, __LINE__
		);
	}

	if (!_TsdsIsFolderExists(folderPath))
	{
		_TsdsCreateFolders(folderPath);
	}

	TsdsPhysicalStore* pPhysicalStore = g_physicalStoreAllocator.allocate(1);
	g_physicalStoreAllocator.construct(pPhysicalStore);

	pPhysicalStore->folderPath = physicalStoreDesc.folderPath;

	return pPhysicalStore;
}

void TsdsReleasePhysicalStore(TsdsPhysicalStore* pPhysicalStore)
{
	for (TsdsDatabase* pDatabase : pPhysicalStore->databaseList)
	{
		TsdsReleaseDatabase(pDatabase);
	}
	g_physicalStoreAllocator.deallocate(pPhysicalStore, 1);
}

TsdsDatabase* TsdsCreateDatabase(TsdsDatabaseDesc databaseDesc)
{
	std::wstring_view databaseName = databaseDesc.databaseName;

	if (databaseDesc.pPhysicalStore == nullptr)
	{
		_TsdsThrow(
			std::string("failed to create database \"") +
			g_u16u8converter.to_bytes(databaseName.data()) +
			"\": databaseDesc.pPhysicalStore must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (databaseName.empty())
	{
		_TsdsThrow(
			std::string("failed to create database \"") +
			g_u16u8converter.to_bytes(databaseName.data()) +
			"\": databaseDesc.databaseName must be not empty",
			__FUNCSIG__, __LINE__
		);
	}

	std::wstring databaseFolderPath;
	databaseFolderPath += databaseDesc.pPhysicalStore->folderPath;
	databaseFolderPath.push_back('\\');
	databaseFolderPath += databaseName;

	if (!_TsdsIsFolderExists(databaseFolderPath))
	{
		_TsdsCreateFolder(databaseFolderPath);
	}

	std::wstring databaseConfigFileName = databaseFolderPath;
	databaseConfigFileName.push_back('\\');
	databaseConfigFileName += TSDS_CONFIGFILE_WSTR(database_config);

	TsdsDatabaseConfig databaseConfig;

	if (!_TsdsIsFileExists(databaseConfigFileName))
	{
		_TsdsCreateFile(
			databaseConfigFileName,
			&databaseConfig, 
			sizeof TsdsDatabaseConfig
		);
	}
	else
	{
		_TsdsThrow(
			std::string("failed to create database \"") +
			g_u16u8converter.to_bytes(databaseName.data()) +
			"\": database already exists",
			__FUNCSIG__, __LINE__
		);
	}

	TsdsDatabase* pDatabase = g_databaseAllocator.allocate(1);
	g_databaseAllocator.construct(pDatabase);

	pDatabase->databaseName = databaseName;
	pDatabase->pPhysicalStore = databaseDesc.pPhysicalStore;
	pDatabase->databaseConfig = databaseConfig;
	pDatabase->fullPath = databaseFolderPath;

	pDatabase->pPhysicalStore->databaseList.push_back(pDatabase);

	return pDatabase;
}

bool TsdsIsDatabaseExists(
	TsdsPhysicalStore* pPhysicalStore, 
	const wchar_t* databaseName
)
{
	std::wstring databaseFolderPath = pPhysicalStore->folderPath;
	databaseFolderPath.push_back('\\');
	databaseFolderPath += databaseName;

	if (!_TsdsIsFolderExists(databaseFolderPath))
	{
		return false;
	}

	std::wstring& databaseConfigFileName = databaseFolderPath;
	databaseConfigFileName.push_back('\\');
	databaseConfigFileName += TSDS_CONFIGFILE_WSTR(database_config);

	return _TsdsIsFileExists(databaseConfigFileName);
}

TsdsDatabase* TsdLoadDatabase(
	TsdsPhysicalStore* pPhysicalStore, 
	const wchar_t* databaseName
)
{
	if (pPhysicalStore == nullptr)
	{
		_TsdsThrow(
			std::string("failed to load database \"") +
			g_u16u8converter.to_bytes(databaseName) +
			"\": parameter pPhysicalStore must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (databaseName == nullptr)
	{
		_TsdsThrow(
			std::string("failed to load database \"") +
			g_u16u8converter.to_bytes(databaseName) +
			"\": parameter databaseName must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	std::wstring databaseFolderPath = pPhysicalStore->folderPath;
	databaseFolderPath.push_back('\\');
	databaseFolderPath += databaseName;

	if (!_TsdsIsFolderExists(databaseFolderPath))
	{
		_TsdsThrow(
			std::string("failed to load database \"") +
			g_u16u8converter.to_bytes(databaseName) +
			"\": folder of database does not exists",
			__FUNCSIG__, __LINE__
		);
	}

	std::wstring databaseConfigFileName = databaseFolderPath;
	databaseConfigFileName.push_back('\\');
	databaseConfigFileName += TSDS_CONFIGFILE_WSTR(database_config);

	TsdsDatabaseConfig databaseConfig;

	if (_TsdsIsFileExists(databaseConfigFileName))
	{
		_TsdsReadFile(
			databaseConfigFileName,
			&databaseConfig,
			sizeof TsdsDatabaseConfig
		);
	}
	else
	{
		_TsdsThrow(
			std::string("failed to load database \"") +
			g_u16u8converter.to_bytes(databaseName) +
			"\": configuration file of database does not exists",
			__FUNCSIG__, __LINE__
		);
	}

	TsdsDatabase* pDatabase = g_databaseAllocator.allocate(1);
	g_databaseAllocator.construct(pDatabase);

	pDatabase->databaseName = databaseName;
	pDatabase->pPhysicalStore = pPhysicalStore;
	pDatabase->databaseConfig = databaseConfig;
	pDatabase->fullPath = databaseFolderPath;

	pDatabase->pPhysicalStore->databaseList.push_back(pDatabase);

	return pDatabase;
}

void TsdsReleaseDatabase(TsdsDatabase* pDatabase)
{
	for (TsdsSeries* pSeries : pDatabase->seriesList)
	{
		TsdsReleaseSeries(pSeries);
	}
	g_databaseAllocator.deallocate(pDatabase, 1);
}

void TsdsGetDatabaseDesc(
	TsdsDatabase* pDatabase, 
	TsdsDatabaseDesc* pSeriesDesc
)
{
	if (pDatabase == nullptr)
	{
		return;
	}

	if (pSeriesDesc == nullptr)
	{
		return;
	}

	pSeriesDesc->databaseName = pDatabase->databaseName.c_str();
	pSeriesDesc->pPhysicalStore = pDatabase->pPhysicalStore;
}

TsdsSeries* TsdsCreateSeries(TsdsSeriesDesc seriesDesc)
{
	std::wstring_view seriesName = seriesDesc.seriesName;

	if (seriesDesc.pDatabase == nullptr)
	{
		_TsdsThrow(
			std::string("failed to create series \"") +
			g_u16u8converter.to_bytes(seriesName.data()) +
			"\": parameter pDatabase must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (seriesName.empty())
	{
		_TsdsThrow(
			std::string("failed to create series \"") +
			g_u16u8converter.to_bytes(seriesName.data()) +
			"\": TsdsSeriesDesc.seriesName must be not empty",
			__FUNCSIG__, __LINE__
		);
	}

	if (seriesDesc.chankSize < 1ULL)
	{
		_TsdsThrow(
			std::string("failed to create series \"") +
			g_u16u8converter.to_bytes(seriesName.data()) +
			"\": seriesDesc.chankSize must be greater than 0",
			__FUNCSIG__, __LINE__
		);
	}

	if (seriesDesc.elementSize < 1ULL)
	{
		_TsdsThrow(
			std::string("failed to create series \"") +
			g_u16u8converter.to_bytes(seriesName.data()) +
			"\": seriesDesc.elementSize must be greater than 0",
			__FUNCSIG__, __LINE__
		);
	}

	if (seriesDesc.indexBegin == ~(0ULL))
	{
		_TsdsThrow(
			std::string("failed to create series \"") +
			g_u16u8converter.to_bytes(seriesName.data()) +
			"\": seriesDesc.indexBegin is not valid",
			__FUNCSIG__, __LINE__
		);
	}

	std::wstring seriesFolderPath;
	seriesFolderPath += seriesDesc.pDatabase->fullPath;
	seriesFolderPath.push_back('\\');
	seriesFolderPath += seriesName;

	if (!_TsdsIsFolderExists(seriesFolderPath))
	{
		_TsdsCreateFolder(seriesFolderPath);
	}

	std::wstring seriesConfigFileName = seriesFolderPath;
	seriesConfigFileName.push_back('\\');
	seriesConfigFileName += TSDS_CONFIGFILE_WSTR(series_config);

	TsdsSeriesConfig seriesConfig;

	seriesConfig.indexFormat = seriesDesc.indexFormat;
	seriesConfig.indexBegin = seriesDesc.indexBegin;
	seriesConfig.indexStep = seriesDesc.indexStep;
	seriesConfig.chankSize = seriesDesc.chankSize;
	seriesConfig.elementSize = seriesDesc.elementSize;

	if (!_TsdsIsFileExists(seriesConfigFileName))
	{
		_TsdsCreateFile(
			seriesConfigFileName,
			&seriesConfig,
			sizeof TsdsSeriesConfig
		);
	}
	else
	{
		_TsdsThrow(
			std::string("failed to create series \"") +
			g_u16u8converter.to_bytes(seriesName.data()) +
			"\": series already exists",
			__FUNCSIG__, __LINE__
		);
	}

	TsdsSeries* pSeries = g_seriesAllocator.allocate(1);
	g_seriesAllocator.construct(pSeries);

	pSeries->pDatabase = seriesDesc.pDatabase;
	pSeries->seriesName = seriesDesc.seriesName;
	pSeries->configFileName = seriesConfigFileName;
	pSeries->fullPath = seriesFolderPath;
	pSeries->config = seriesConfig;

	pSeries->pDatabase->seriesList.push_back(pSeries);

	return pSeries;
}

bool TsdsIsSeriesExists(
	TsdsDatabase* pDatabase, 
	const wchar_t* seriesName
)
{
	if (pDatabase == nullptr)
	{
		return false;
	}

	if (seriesName == nullptr)
	{
		return false;
	}

	std::wstring seriesFolderPath = pDatabase->fullPath;
	seriesFolderPath.push_back('\\');
	seriesFolderPath += seriesName;

	if (!_TsdsIsFolderExists(seriesFolderPath))
	{
		return false;
	}

	std::wstring& seriesConfigFileName = seriesFolderPath;
	seriesConfigFileName.push_back('\\');
	seriesConfigFileName += TSDS_CONFIGFILE_WSTR(series_config);

	return _TsdsIsFileExists(seriesConfigFileName);
}

TsdsSeries* TsdsLoadSeries(
	TsdsDatabase* pDatabase,
	const wchar_t* seriesName
)
{
	if (pDatabase == nullptr)
	{
		_TsdsThrow(
			std::string("failed to load series \"") +
			g_u16u8converter.to_bytes(seriesName) +
			"\": parameter pDatabase must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (seriesName == nullptr)
	{
		_TsdsThrow(
			std::string("failed to load series \"") +
			g_u16u8converter.to_bytes(seriesName) +
			"\": parameter seriesName must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	std::wstring seriesFolderPath = pDatabase->fullPath;
	seriesFolderPath.push_back('\\');
	seriesFolderPath += seriesName;

	if (!_TsdsIsFolderExists(seriesFolderPath))
	{
		_TsdsThrow(
			std::string("failed to load series \"") +
			g_u16u8converter.to_bytes(seriesName) +
			"\": folder of series not exists",
			__FUNCSIG__, __LINE__
		);
	}

	std::wstring seriesConfigFileName = seriesFolderPath;
	seriesConfigFileName.push_back('\\');
	seriesConfigFileName += TSDS_CONFIGFILE_WSTR(series_config);

	TsdsSeriesConfig seriesConfig;

	if (_TsdsIsFileExists(seriesConfigFileName))
	{
		_TsdsReadFile(
			seriesConfigFileName, 
			&seriesConfig, 
			sizeof TsdsSeriesConfig
		);
	}
	else
	{
		_TsdsThrow(
			std::string("failed to load series \"") +
			g_u16u8converter.to_bytes(seriesName) +
			"\": configuration file of series not exists",
			__FUNCSIG__, __LINE__
		);
	}

	TsdsSeries* pSeries = g_seriesAllocator.allocate(1);
	g_seriesAllocator.construct(pSeries);

	pSeries->pDatabase = pDatabase;
	pSeries->seriesName = seriesName;
	pSeries->fullPath = seriesFolderPath;
	pSeries->configFileName = seriesConfigFileName;
	pSeries->config = seriesConfig;

	pSeries->pDatabase->seriesList.push_back(pSeries);

	return pSeries;
}

void TsdsReleaseSeries(TsdsSeries* pSeries)
{
	g_seriesAllocator.deallocate(pSeries, 1);
}

void TsdsGetSeriesDesc(
	TsdsSeries* pSeries, 
	TsdsSeriesDesc* pSeriesDesc
)
{
	if (pSeries == nullptr)
	{
		_TsdsThrow(
			"failed to get series descriptor: parameter pSeries must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (pSeriesDesc == nullptr)
	{
		_TsdsThrow(
			std::string("failed to get series descriptor\"") +
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter pSeriesDesc must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	pSeriesDesc->chankSize = pSeries->config.chankSize;
	pSeriesDesc->elementSize = pSeries->config.elementSize;
	pSeriesDesc->indexBegin = pSeries->config.indexBegin;
	pSeriesDesc->indexFormat = pSeries->config.indexFormat;
	pSeriesDesc->indexStep = pSeries->config.indexStep;
	pSeriesDesc->seriesName = pSeries->seriesName.c_str();
	pSeriesDesc->pDatabase = pSeries->pDatabase;
}

void _TsdsAddChankFileName(
	std::wstring& chankFileName, 
	TsdsIndex chankIndex
)
{
	chankFileName += std::wstring(_TSDS_SERIESFILE_PREFIX_WSTR) + 
		std::to_wstring(chankIndex) + _TSDS_SERIESFILE_ENDFORMAT_WSTR;
}

void _TsdsSeriesAddValues(
	TsdsSeries* pSeries, 
	const void* values, 
	TsdsSize count
)
{
	while (count >= 1)
	{
		auto& dbElementsCount = pSeries->config.elementsCount;
		const auto dbElementSize = pSeries->config.elementSize;
		const auto dbChankSize = pSeries->config.chankSize;

		const auto elementsInCurChankCount = dbElementsCount == 0ULL ? 0ULL : dbElementsCount % dbChankSize;
		const auto curChankAvailableElementsCountToWrite = dbChankSize - elementsInCurChankCount;
		const auto dbChankIdx = (dbElementsCount - elementsInCurChankCount) / dbChankSize;
		const auto elementsCountToWrite =
			count >= curChankAvailableElementsCountToWrite ? curChankAvailableElementsCountToWrite : count;
		const auto dataSize = elementsCountToWrite * dbElementSize;

		std::wstring appChankFileName = pSeries->fullPath;
		appChankFileName.push_back('\\');
		_TsdsAddChankFileName(appChankFileName, dbChankIdx);
		;
		if (!_TsdsIsFileExists(appChankFileName))
		{
			_TsdsCreateFile(appChankFileName, values, dataSize);
		}
		else
		{
			_TsdsWriteFile(appChankFileName, values, dataSize);
		}

		dbElementsCount += elementsCountToWrite;
		count -= elementsCountToWrite;
		values = reinterpret_cast<const char*>(values) + elementsCountToWrite * dbElementSize;
	}

	_TsdsWriteFile(
		pSeries->configFileName,
		&pSeries->config,
		sizeof TsdsSeriesConfig,
		true
	);
}

void TsdsSeriesAddValues(
	TsdsSeries* pSeries, 
	const void* values, 
	TsdsSize count
)
{
	if (pSeries == nullptr)
	{
		_TsdsThrow(
			"failed to add values to series: parameter pSeries must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (values == nullptr)
	{
		_TsdsThrow(
			std::string("failed to add values to series\"") + 
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter values must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (count < 1ULL)
	{
		_TsdsThrow(
			std::string("failed to add values to series\"") +
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter count must be greater than 0",
			__FUNCSIG__, __LINE__
		);
	}

	_TsdsSeriesAddValues(pSeries, values, count);
}

void _TsdsSeriesReadValuesByCountRawIndexes(
	TsdsSeries* pSeries, 
	void* values, 
	TsdsIndex beginIndex, 
	TsdsSize count
)
{
	while (count >= 1)
	{
		const auto dbElementsCount = pSeries->config.elementsCount;

		if (!(beginIndex + count <= dbElementsCount))
		{
			return;
		}

		const auto dbElementSize = pSeries->config.elementSize;
		const auto dbChankSize = pSeries->config.chankSize;

		const auto elementsInCurChankCount = beginIndex == 0ULL ? 0ULL : beginIndex % dbChankSize;
		const auto curChankAvailableElementsCountToRead = dbChankSize - elementsInCurChankCount;
		const auto dbChankIdx = (beginIndex - elementsInCurChankCount) / dbChankSize;
		const auto elementsCountToRead =
			count >= curChankAvailableElementsCountToRead ? curChankAvailableElementsCountToRead : count;
		const auto dataSize = elementsCountToRead * dbElementSize;
		const auto dataOffset = elementsInCurChankCount * dbElementSize;

		std::wstring appChankFileName = pSeries->fullPath;
		appChankFileName.push_back('\\');
		_TsdsAddChankFileName(appChankFileName, dbChankIdx);

		if (_TsdsIsFileExists(appChankFileName))
		{
			_TsdsReadFile(appChankFileName, values, dataSize, dataOffset);
		}
		else
		{
			_TsdsThrow(
				std::string("failed to read values from series\"") +
				g_u16u8converter.to_bytes(pSeries->seriesName) +
				"\": data file of series not exists (" + 
				g_u16u8converter.to_bytes(appChankFileName) + ")",
				__FUNCSIG__, __LINE__
			);
		}

		beginIndex += elementsCountToRead;
		count -= elementsCountToRead;
		values = reinterpret_cast<char*>(values) + elementsCountToRead * dbElementSize;
	}
}

void TsdsSeriesReadValuesByCountRawIndexes(
	TsdsSeries* pSeries, 
	void* values, 
	TsdsIndex beginIndex,
	TsdsSize count
)
{
	if (pSeries == nullptr)
	{
		_TsdsThrow(
			"failed to read values from series: parameter pSeries must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (values == nullptr)
	{
		_TsdsThrow(
			std::string("failed to read values from series\"") +
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter values must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (count < 1ULL)
	{
		_TsdsThrow(
			std::string("ffailed to read values from series\"") +
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter count must be greater than 0",
			__FUNCSIG__, __LINE__
		);
	}

	_TsdsSeriesReadValuesByCountRawIndexes(pSeries, values, beginIndex, count);
}

void TsdsSeriesReadValuesByBeginEndRawIndexes(
	TsdsSeries* pSeries, 
	void* values, 
	TsdsIndex beginIndex, 
	TsdsIndex endIndex
)
{
	if (pSeries == nullptr)
	{
		_TsdsThrow(
			"failed to read values from series: parameter pSeries must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (values == nullptr)
	{
		_TsdsThrow(
			std::string("failed to read values from series\"") +
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter values must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (beginIndex < 1ULL)
	{
		_TsdsThrow(
			std::string("failed to read values from series\"") +
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter beginIndex must be greater than 0",
			__FUNCSIG__, __LINE__
		);
	}

	if (endIndex < 1ULL || endIndex <= beginIndex)
	{
		_TsdsThrow(
			std::string("failed to read values from series\"") +
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter beginIndex must be greater than 0 and must be grater than parameter beginIndex",
			__FUNCSIG__, __LINE__
		);
	}

	auto count = endIndex - beginIndex;
	_TsdsSeriesReadValuesByCountRawIndexes(pSeries, values, beginIndex, count);
}

template <typename _Index_numeric_type>
inline TsdsIndex _TsdsConvertIndexXType(
	const void* rawIndex, 
	TsdsIndex indexBegin, 
	TsdsIndex indexStep
)
{
	auto xIndex = *reinterpret_cast<const _Index_numeric_type*>(rawIndex);
	const auto xIndexBegin = *reinterpret_cast<const _Index_numeric_type*>(&indexBegin);
	const auto xIndexStep = *reinterpret_cast<const _Index_numeric_type*>(&indexStep);
	xIndex -= xIndexBegin;
	xIndex /= xIndexStep;
	TsdsIndex index = static_cast<decltype(index)>(xIndex);
	return index;
}

TsdsIndex TsdsConvertIndex(
	TsdsSeries* pSeries, 
	const void* rawIndex
)
{
	TSDS_INDEX_FORMAT indexFormat = pSeries->config.indexFormat;
	const auto indexBegin = pSeries->config.indexBegin;
	const auto indexStep = pSeries->config.indexStep;
	TsdsIndex index;

	switch (indexFormat)
	{
	case TSDS_INDEX_FORMAT_UNSIGNED_INTEGER_64:
		index = _TsdsConvertIndexXType<unsigned long long>(rawIndex, indexBegin, indexStep);
		break;
	case TSDS_INDEX_FORMAT_UNSIGNED_INTEGER_32:
		index = _TsdsConvertIndexXType<unsigned int>(rawIndex, indexBegin, indexStep);
		break;
	case TSDS_INDEX_FORMAT_INTEGER_64:
		index = _TsdsConvertIndexXType<long long>(rawIndex, indexBegin, indexStep);
		break;
	case TSDS_INDEX_FORMAT_INTEGER_32:
		index = _TsdsConvertIndexXType<int>(rawIndex, indexBegin, indexStep);
		break;
	case TSDS_INDEX_FORMAT_FLOAT_S1E11M52:
		index = _TsdsConvertIndexXType<double>(rawIndex, indexBegin, indexStep);
		break;
	case TSDS_INDEX_FORMAT_FLOAT_S1E8M23:
		index = _TsdsConvertIndexXType<float>(rawIndex, indexBegin, indexStep);
		break;
	default:
		index = ~(0ULL);
		break;
	}

	return index;
}

void TsdsSeriesReadValuesByBeginEnd(
	TsdsSeries* pSeries, 
	void* values, 
	const void* beginIndex, 
	const void* endIndex
)
{
	if (pSeries == nullptr)
	{
		_TsdsThrow(
			"failed to read values from series: parameter pSeries must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (values == nullptr)
	{
		_TsdsThrow(
			std::string("failed to read values from series\"") +
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter values must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	TsdsSeriesReadValuesByBeginEndRawIndexes(
		pSeries, values, 
		TsdsConvertIndex(pSeries, beginIndex),
		TsdsConvertIndex(pSeries, endIndex)
	);
}

void TsdsSeriesReadValuesByCount(
	TsdsSeries* pSeries, 
	void* values, 
	const void* beginIndex, 
	TsdsSize count
)
{
	if (pSeries == nullptr)
	{
		_TsdsThrow(
			"failed to read values from series: parameter pSeries must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	if (values == nullptr)
	{
		_TsdsThrow(
			std::string("failed to read values from series\"") +
			g_u16u8converter.to_bytes(pSeries->seriesName) +
			"\": parameter values must be not nullptr",
			__FUNCSIG__, __LINE__
		);
	}

	TsdsSeriesReadValuesByCountRawIndexes(
		pSeries, values,
		TsdsConvertIndex(pSeries, beginIndex),
		count
	);
}


template<typename _Index_numeric_type>
unsigned long long _TsdsConvertIndexXType(_Index_numeric_type index)
{
	unsigned long long indexBegin = {};
	auto pIndexBegin = reinterpret_cast<decltype(index)*>(&indexBegin);
	*pIndexBegin = index;
	return indexBegin;
}

unsigned long long TsdsConvertIndex(double index)
{
	return _TsdsConvertIndexXType(index);
}

unsigned long long TsdsConvertIndex(float index)
{
	return _TsdsConvertIndexXType(index);
}

unsigned long long TsdsConvertIndex(int index)
{
	return _TsdsConvertIndexXType(index);
}

unsigned long long TsdsConvertIndex(unsigned int index)
{
	return _TsdsConvertIndexXType(index);
}

unsigned long long TsdsConvertIndex(long long index)
{
	return _TsdsConvertIndexXType(index);
}

unsigned long long TsdsConvertIndex(unsigned long long index)
{
	return index;
}

unsigned long long TsdsConvertIndex(TsdsTime index)
{
	return TsdsTimestampFromTime(&index);
}
