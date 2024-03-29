// dbif.h
//	定义一组数据库使用的接口
#pragma once

#include "../../stdext/stdext.h"

namespace jcdb
{
	class CDBFieldInfo;
	class IResultSet;

	class IRecordRow : virtual public IJCInterface
	{
	public:
		virtual int GetItemID(void) const = 0;
		virtual int GetFieldSize() const = 0;
		//virtual DWORD GetFieldLen(int field)	const = 0;

		// 从一个通用的行中取得通用的列数据
		//virtual const char * GetFieldData(int field, CJCStringT& data)	const = 0;
		virtual void GetFieldData(int field, CJCStringT& data)	const = 0;
		
		//virtual const char * GetFieldData(const char * field_name) const = 0;
		virtual bool GetFieldData(const char * field_name, CJCStringT& data) const = 0;

	//	virtual const CFieldInfo * GetFieldInfo() const = 0;

		virtual const CJCStringA & GetFieldName(int field_id) const = 0;

		// 从一个类型化的行得到列的文本数据
		//virtual void GetFieldDataText(int field_num, CAtlStringW & data)/* const*/ = 0;
		//virtual void GetFieldDataText(int field_num, CAtlStringA & data)/* const*/ = 0;
		//virtual void SetFieldDataText(int field_num, LPCSTR data) = 0;
		//virtual /*const*/ IResultSet * GetResultSet(void) const = 0;

		// 行复制，通常奖一个通用行复制到一个类型化的专用行
	//	virtual void SetData(const IRecordRow * row) = 0;
	};

	class IResultSet : virtual public IJCInterface
	{
		// IResultSet 面向数据库驱动程序提供一个统一的结果集的使用界面
		// IResultSet 的派生类维护特定数据库的结果集指针，和其引用计数。

		// 假设条件：IResultSet的派生类必须维护查询结果，直到IResultSet对象被解构。
		//	即被查询的纪录的生命周期从第一次被访问开始，直到对象被解构
	public:
		// 关于浏览结果集的方法
		virtual int GetRowCount() const = 0;
		//virtual IRecordRow * GetRow(int i) = 0;
		//virtual IRecordRow * GetCurrentRow() = 0;
		virtual bool GetNextRow(IRecordRow * &)  = 0;
		//virtual IRecordRow * GetPrevRow() = 0;
		//virtual void GotoRow(int i) = 0;

		// 关于字段的方法
		virtual int	GetFieldSize() const = 0;					// 返回字段的个数
		virtual const CDBFieldInfo * GetField(int field)/* const*/ = 0;			// 根据编号取得字段
		virtual CDBFieldInfo * GetField(LPCTSTR strName) = 0;		// 根据名称取得字段


	//	typedef LONGLONG	INDICATOR;
	//
	//public:
	//	//virtual void FetchRow(LONGLONG iRow, char ** &strFields, DWORD * &arrFieldLen) = 0;
	//	virtual void MoveToNext(INDICATOR & i) = 0;
	//	virtual void MoveToPrev(INDICATOR & i) = 0;
	//
	//	virtual IBrief * GetNextRow(INDICATOR & i) = 0;
	//	virtual IBrief * GetPrevRow(INDICATOR & i) = 0;
	//	virtual INDICATOR	GetFirstIndicator(void) = 0;
	//	virtual INDICATOR	GetLastIndicator(void) = 0;
	//	virtual IBrief * GetFirstRow() = 0;
	//
	//	virtual int GetFields() const = 0;
	//	virtual LONGLONG	GetCount(void) const = 0;
	//
	//// Service
	//public:

	};

	class IDataBaseDriver : virtual public IJCInterface
	{
		//virtual void Connect(
		//	LPCTSTR strServer, UINT iPort, 
		//	LPCTSTR strDataBase, LPCTSTR strUserName, LPCTSTR strPassword) = 0;
		virtual void Connect(
			LPCSTR strServer, UINT iPort, 
			LPCSTR strDataBase, LPCSTR strUserName, LPCSTR strPassword) = 0;

		virtual void Disconnect(void) = 0;
		virtual void Reconnect(void) = 0;

		virtual BOOL IsConnected(void) = 0;

		virtual void Query(LPCTSTR strSQL, IResultSet * &) = 0;
		virtual void Query(LPCSTR strSQL, IResultSet * &) = 0;

		virtual LONGLONG Execute(LPCWSTR strSQL) = 0;
		virtual LONGLONG Execute(LPCSTR strSQL) = 0;
		virtual LONGLONG GetAutoIncreaseID(void) = 0;

		virtual bool Insert(IRecordRow * row) = 0;
		virtual bool Update(IRecordRow * row) = 0;
		virtual bool Delete(IRecordRow * row) = 0;
	};
};