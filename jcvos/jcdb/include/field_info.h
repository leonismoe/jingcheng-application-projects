#pragma once

#include <map>
#include <vector>
#include "../../jcparam/jcparam.h"

namespace jcdb
{
	class /*_TREEBASEPP_EXT_CLASS*/ CDBFieldInfo
	{
		// 1、提供结果集中，每个段的信息。主要包括段名、段的宽度、段的类型等。
		// 2、提供段数据的转换方法，标准IResultSet以char类型返回每个段的数据。
		//	该类根据描述的段类型，提供段类型和char*之间的相互转换。
		//
		// 根据结果及类型的不同，提供两种不同的实现方式。对于一般结果集，由于事先不知道结果集的构成，
		//	必须等结果集返回后，才能得到段信息。根据段的类型，通过case语句，自动进行类型转换。
		//	对于数据表，由于事先知道结果集的构成，因此通过在Row类中固定编码的类型转换，这个可以通过模板实现。


	public:
		CDBFieldInfo(LPCSTR field_name, int field_width, int field_id)
			: m_field_name(field_name)
			, m_field_width(field_width)
			, m_field_id(field_id)
		{
		}

		virtual ~CDBFieldInfo(void) {};

	protected:
		CJCStringA						m_field_name;
		int								m_field_width;
		int								m_field_id;

	public:
		int GetFieldId() const { return m_field_id; };

		const CJCStringA & GetFieldName() const { return m_field_name; }

		int GetFieldWidth() const { return m_field_width; }

		virtual void GetDataText(void * row, CJCStringT & str) const { }

		virtual void SetDataText(void * row, LPCTSTR str) const { }
	};

	///////////////////////////////////////////////////////////////////////////////////////////
	template <typename DATATYPE, typename CONVERTOR = CConvertor<DATATYPE>>
	class CTypedFieldInfo : public CDBFieldInfo
	{
	protected:
		size_t							m_offset;			// 段对应的变量，在Row中的偏移量

	public:
		CTypedFieldInfo(LPCTSTR field_name, size_t offset) 
			: CDBFieldInfo(field_name, 0), m_offset(offset)	{
			//TEMPLATE_CLASS_SIZE1(CTypedFieldInfo, int);
			//TEMPLATE_CLASS_SIZE1(CTypedFieldInfo, CAtlString);
		}
		virtual ~CTypedFieldInfo(void) {
		}

		virtual void GetDataText(void * row, CJCStringT & str) const {
			JCASSERT(m_offset < UINT_MAX);		JCASSERT(row);
			DATATYPE* p = (DATATYPE*)(((char*)row) + m_offset);
			CONVERTOR::T2S(*p, str);
		}

		virtual void SetDataText(void * row, LPCTSTR str) const {
			JCASSERT(m_offset < UINT_MAX);		JCASSERT(row);
			DATATYPE t;
			CONVERTOR::S2T(str, t);
			*(DATATYPE*)( ((char*)row) + m_offset) = t;
		}
	};

	///////////////////////////////////////////////////////////////////////////////////////////
	//template class __declspec(dllexport) CSimpleMap<CAtlString, CDBFieldInfo*>;

	class /*_TREEBASEPP_EXT_CLASS*/ CDBFieldMap 
		//: public std::map<CStringT, const CDBFieldInfo*>
	{
		// 提供从段名到段编号 和 段编号到段名的映射

	public:
		CDBFieldMap(void);
		CDBFieldMap(int count, ...);
		virtual ~CDBFieldMap(void);

	public:
		const CDBFieldInfo * GetFieldInfo(int field_num) const {
			JCASSERT(field_num < m_field_size);
			JCASSERT(m_field_array);
			return m_field_array[field_num];
		}

		const int GetFieldSize() const {
			return m_field_size;
		}
		//void InsertField(int field_num, const CDBFieldInfo * field);
		void InsertField(int id, const char * name, int width);
		void PushField(int id, const char * name, int width);
		void SetFieldSize(int size);

		int GetFieldID(const char * name);

	protected:
		typedef std::map<CJCStringA, const CDBFieldInfo*>	FIELD_MAP;
		typedef FIELD_MAP::iterator FIELD_MAP_ITERATOR;

		FIELD_MAP		m_field_map;
		//std::vector<const CDBFieldInfo*>				m_field_vector;
		CDBFieldInfo *	*								m_field_array;
		int												m_array_capacit;
		int												m_field_size;
	};
};