#pragma once

typedef unsigned long long TsdsIndex;

typedef unsigned long long TsdsSize;

typedef struct TsdsDatabase TsdsDatabase;

typedef struct TsdsPhysicalStore TsdsPhysicalStore;

typedef struct TsdsSeries TsdsSeries;

typedef enum TSDS_INDEX_FORMAT : unsigned long long
{
	//index type = unsigned long long
	TSDS_INDEX_FORMAT_UNSIGNED_INTEGER_64 = 0ULL,

	//index type = unsigned int
	TSDS_INDEX_FORMAT_UNSIGNED_INTEGER_32,

	//index type = long long
	TSDS_INDEX_FORMAT_INTEGER_64,

	//index type = int
	TSDS_INDEX_FORMAT_INTEGER_32,

	//index type = double
	TSDS_INDEX_FORMAT_FLOAT_S1E11M52,

	//index type = float
	TSDS_INDEX_FORMAT_FLOAT_S1E8M23
} TSDS_INDEX_FORMAT;

typedef struct TsdsTime
{
	// seconds after the minute - [0, 60] including leap second
	int						sec					{		  };

	// minutes after the hour - [0, 59]
	int						min					{		  };	

	// hours since midnight - [0, 23]
	int						hour				{		  };	

	// day of the month - [1, 31]
	int						mday				{		  };	

	// months since January - [0, 11]
	int						mon					{		  };	

	// years since 1900
	int						year				{		  };

	// days since Sunday - [0, 6]
	int						wday				{		  };

	// days since January 1 - [0, 365]
	int						yday				{		  };	

	// milliseconds after the seconds - [0, 1000]
	int						milli				{		  };	
} TsdsTime;


typedef struct TsdsPhysicalStoreDesc
{
	// Path to the folder that will contain the databases
	const wchar_t*			folderPath			{ nullptr };
} TsdsPhysicalStoreDesc;

typedef struct TsdsDatabaseDesc
{
	// Pointer to initialized TsdsPhysicalStore, pPhysicalStore != nullptr
	TsdsPhysicalStore*		pPhysicalStore		{ nullptr };

	// Name of Database, databaseName != nullptr
	const wchar_t*			databaseName		{ nullptr };
} TsdsDatabaseDesc;

typedef struct TsdsSeriesDesc
{
	// Pointer to initialized TsdsDatabase, pDatabase != nullptr
	TsdsDatabase*			pDatabase			{ nullptr };

	TSDS_INDEX_FORMAT		indexFormat			{		  };
	unsigned long long		indexBegin			{    0    };
	unsigned long long		indexStep			{    0    };

	unsigned long long		chankSize			{    0    };
	unsigned long long		elementSize			{    0    };

	const wchar_t*			seriesName			{ nullptr };
} TsdsSeriesDesc;

void TsdsTimeFromTimestamp(TsdsTime* time, long long timestamp);

void TsdsTimeFromTimestampWithMilliseconds(TsdsTime* time, long long timestamp);

long long TsdsTimestampFromTime(TsdsTime* time);

long long TsdsTimestampFromTimeMilliseconds(TsdsTime* time);

TsdsPhysicalStore* TsdsCreatePhysicalStore(TsdsPhysicalStoreDesc physicalStoreDesc);
void TsdsReleasePhysicalStore(TsdsPhysicalStore* pPhysicalStore);

TsdsDatabase* TsdsCreateDatabase(TsdsDatabaseDesc databaseDesc);
bool TsdsIsDatabaseExists(TsdsPhysicalStore* pPhysicalStore, const wchar_t* databaseName);
TsdsDatabase* TsdLoadDatabase(TsdsPhysicalStore* pPhysicalStore, const wchar_t* databaseName);
void TsdsReleaseDatabase(TsdsDatabase* pDatabase);
void TsdsGetDatabaseDesc(TsdsDatabase* pDatabase, TsdsDatabaseDesc* pSeriesDesc);

TsdsSeries* TsdsCreateSeries(TsdsSeriesDesc seriesDesc);
bool TsdsIsSeriesExists(TsdsDatabase* pDatabase, const wchar_t* seriesName);
TsdsSeries* TsdsLoadSeries(TsdsDatabase* pDatabase, const wchar_t* seriesName);
void TsdsReleaseSeries(TsdsSeries* pSeries);
void TsdsGetSeriesDesc(TsdsSeries* pSeries, TsdsSeriesDesc* pSeriesDesc);

void TsdsSeriesAddValues(TsdsSeries* pSeries, const void* values, TsdsSize count);

void TsdsSeriesReadValuesByCountRawIndexes(TsdsSeries* pSeries, void* values, TsdsIndex beginIndex, TsdsSize count);

void TsdsSeriesReadValuesByBeginEndRawIndexes(TsdsSeries* pSeries, void* values, TsdsIndex beginIndex, TsdsIndex endIndex);

TsdsIndex TsdsConvertIndex(TsdsSeries* pSeries, const void* rawIndex);

void TsdsSeriesReadValuesByBeginEnd(TsdsSeries* pSeries, void* values, const void* beginIndex, const void* endIndex);

void TsdsSeriesReadValuesByCount(
	TsdsSeries* pSeries,
	void* values,
	const void* beginIndex,
	TsdsSize count
);

unsigned long long TsdsConvertIndex(double index);
unsigned long long TsdsConvertIndex(float index);
unsigned long long TsdsConvertIndex(int index);
unsigned long long TsdsConvertIndex(unsigned int index);
unsigned long long TsdsConvertIndex(long long index);
unsigned long long TsdsConvertIndex(unsigned long long index);
unsigned long long TsdsConvertIndex(TsdsTime index);