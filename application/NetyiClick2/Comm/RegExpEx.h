#pragma once

template <typename T, typename CHTYPE>
void GetMatchString(
		T & match, int iGroup,
		CStringT<CHTYPE, StrTraitMFC_DLL<CHTYPE>> & str)
{
	// 把正则表达始中符合条件的子串复制到str
	// 参数：
	//	[type] T			CAtlREMatchContext<TT>类型，其中TT
	//						是匹配的字符类型，ascii：CAtlRECharTraitsA
	//						UNICODE : CAtlRECharTraitsW
	//						多字节字符 : CAtlRECharTraitsB
	//	[in] T & match		Match的结果
	//	[in] int iGroup		需要复制的组号
	//	[out] CString str	基本类型和TT匹配的字符串，输出缓存
	//		
	ASSERT(iGroup < (int)(match.m_uNumGroups) );
	ASSERT(sizeof(CHTYPE) == sizeof(T::RECHAR) );
	typedef T::RECHAR	CHARTYPE;
	const CHARTYPE* szStart = 0;
	const CHARTYPE* szEnd = 0;
	match.GetMatch(iGroup, &szStart, &szEnd);
	size_t iLength = szEnd - szStart;
	CHTYPE * _str = str.GetBufferSetLength((int)iLength);
	rsize_t iBufSize = iLength * sizeof(CHTYPE);
	memcpy_s(_str, iBufSize, szStart, iBufSize);
	str.ReleaseBuffer();
}