#pragma once
//#include <CommClass.h>
#include "../../stdext/stdext.h"
#include <mysql.h>

namespace jcdb
{
	class /*_TREEBASEPP_EXT_CLASS*/ CJcdbException :
		public stdext::CJCException
	{
	public:
		enum __ERROR_ID {
			DBERR = 0x00100000,
			DBERR_MYSQL = 0x00010000,

			//DBERR_START = 2,				DBERR_CANNOTOPENTABLE = 3,
			//DBERR_ITEMNOTEXIST = 4,			DBERR_ILLEGAL_FIELD_ID = 5,
		};

	public:
		CJcdbException(const char * msg, int id = 0) 
			: stdext::CJCException(msg, stdext::CJCException::ERR_APP, DBERR | id)
		{}
	};




	class /*_TREEBASEPP_EXT_CLASS*/ CMySQLException :
		public CJcdbException
	{
	public:
		CMySQLException(MYSQL * lpMySQL, LPCSTR msg, ...);
	};

};

#define THROW_DB_ERR(msg, ...)	{						\
		char* __temp_str_ = new char[512];				\
		sprintf_s(__temp_str_, 512, msg, __VA_ARGS__);	\
		jcdb::CJcdbException err(__temp_str_);				\
		delete [] __temp_str_;							\
		LOG_ERROR(_T("%S"), err.what());				\
		throw err; }

#define THROW_MYSQL_ERR(mysql, msg, ...)	{		\
	jcdb::CMySQLException err(mysql, msg, __VA_ARGS__);		\
    LOG_ERROR(_T("%S"), err.what());				\
	throw err; }

//
//#ifdef _DEBUG
//#define TB_ERROR(msg, debugmsg)														\
//	{																				\
//	CAtlString str;																	\
//	str.Format(																		\
//		_T("Exception : %s @\r\n\tFile : %s\r\n\tFunction : %s\r\n\tLine : %u\r\n")	\
//		_T("Debug message : %s"),													\
//		msg, _T(__FILE__), _T(__FUNCTION__), __LINE__, debugmsg);					\
//	throw (new CMySQLException(str));													\
//	}																				\
//
//#define MYSQL_ERROR(mysql, debugmsg)												\
//	{																				\
//	CAtlString str;																	\
//	str.Format(																		\
//		_T("\r\nMySQL Error! @\r\n\tFile : %s\r\n\tFunction : %s\r\n\tLine : %u\r\n")	\
//		_T("Debug message : %s"),													\
//		_T(__FILE__), _T(__FUNCTION__), __LINE__, debugmsg);						\
//	throw (new CMySQLException(mysql, str));											\
//	}
//#else
//#define TB_ERROR(msg, debugmsg)		throw (new CMySQLException(msg));
//#define MYSQL_ERROR(mysql, debugmsg)	throw (new CMySQLException(mysql));
//#endif	//_DEBUG
