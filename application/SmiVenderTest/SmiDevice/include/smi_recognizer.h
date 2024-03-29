﻿#pragma once

#include "ismi_device.h"
#include <map>


class CStorageDeviceInfo
{
public:
	CStorageDeviceInfo(LPCTSTR name, STORAGE_CREATOR creator = NULL);
public:
	LPCTSTR			m_name;
	STORAGE_CREATOR	m_creator;
};

class CSmiDeviceCreator
{
public:
	CSmiDeviceCreator(LPCTSTR name, DEVICE_RECOGNIZER recognizer, DEVICE_CREATOR creator)
		: m_name(name), m_recognizer(recognizer), m_creator(creator)
	{}

public:
	LPCTSTR				m_name;
	DEVICE_RECOGNIZER	m_recognizer;
	DEVICE_CREATOR		m_creator;
};

class CSmiRecognizer
{
protected:
	typedef std::map<CJCStringT, CStorageDeviceInfo>	STORAGE_MAP;
	typedef STORAGE_MAP::iterator						STORAGE_ITERATOR;
	typedef std::pair<CJCStringT, CStorageDeviceInfo>	STORAGE_PAIR;

	typedef std::map<CJCStringT, CSmiDeviceCreator>		DEVICE_MAP;
	typedef DEVICE_MAP::iterator						DEVICE_ITERATOR;
	typedef std::pair<CJCStringT, CSmiDeviceCreator>	DEVICE_PAIR;

public:
	static bool RegisterStorageDevice(const CStorageDeviceInfo & info);
	static bool RegisterSmiDevice(const CSmiDeviceCreator & info);
	static void Register(void);

//--
	static bool CreateDummyDevice(ISmiDevice * & smi_dev);
	static bool RecognizeDevice(LPCTSTR device_name, ISmiDevice * &smi_dev, const CJCStringT & force_storage, const CJCStringT & force_device);
	static bool AutoStorageDevice(const CJCStringT & dev_name, const CJCStringT & force_storage, IStorageDevice * & dev);
	static bool CreateStorageDevice(const CJCStringT & dev_name, HANDLE dev, IStorageDevice * & i_dev);
	// 从逻辑盘符取得物理驱动器号
	static UINT GetDriveNumber(TCHAR drive_letter);

protected:
	static SMI_DEVICE_TYPE AchieveDeviceType(IStorageDevice * dev);

protected:
	static STORAGE_MAP	m_storage_map;
	static DEVICE_MAP	m_device_map;

	static CSmiDeviceCreator	m_smi_dummy_creator;
};

///////////////////////////////////////////////////////////////////////////////
//-- class factories
extern "C"
{
	//bool AutoStorageDevice(const CJCStringT & dev_name, const CJCStringT & force_storage, IStorageDevice * & dev);
	bool CreateSmiDevice(const CJCStringT & ctrl_name, IStorageDevice * storage, ISmiDevice * & i_dev);
};

