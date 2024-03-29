// wqlcl.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <vld.h>
#include <stdext.h>
#include <jcparam.h>
#include <jcapp.h>

#include <atlbase.h>

#include <Windows.h>
#include <Wbemidl.h>

#include <vector>

LOCAL_LOGGER_ENABLE(_T("wqlcl"), LOGGER_LEVEL_NOTICE);

#define MAX_LINE_BUF	1024
enum MDMODE	{
	MDM_UNKNOWN = 0, MDM_ALONE, MDM_CLONE, MDM_EXPAND, 
	MDM_WQL,
	MDM_ICONOV,
};

BEGIN_ENUM_TABLE(MDMODE)
	(_T("ALONE"),	MDM_ALONE)
	(_T("CLONE"),	MDM_CLONE)
	(_T("EXPAND"),	MDM_EXPAND)
	(_T("WQL"),		MDM_WQL)
	(_T("ICON"),	MDM_ICONOV)
END_ENUM_TABLE

class CWqlCmdLine : public jcapp::CJCAppBase<jcapp::AppArguSupport>
{
public:
	CWqlCmdLine(void);
	~CWqlCmdLine(void) {};

public:
	virtual int Run(void);
	bool Initialize(void);
	void CleanUp(void);
	
	void ShowResult(IEnumWbemClassObject * enumerator);
	HRESULT DealWithUnknownTypeItem( CComBSTR bstrName, CComVariant Value, CIMTYPE type, LONG lFlavor );
	void PrintfItem( CComBSTR bstrName, CComVariant Value, CIMTYPE type, LONG lFlavor );
	HRESULT DealWithSingleItem( CComBSTR bstrName, CComVariant Value, CIMTYPE type, LONG lFlavor );

	void RunWql(void);

	void EnumDisplayInfo(void);
	void SwitchToExpand(int primery);
	void SwitchToAlone(int primery);
	void SwitchToClone(void);
	void ReadDisplayInfo(void);

	void DetachDisplay(DWORD disply_num);
	void AttachDisplay(DWORD disply_num);

public:
	CJCStringT	m_namespace;
	MDMODE		m_mode;
	int			m_primary;

protected:
	IWbemLocator *	m_locator;
	IWbemServices *	m_server;

	DEVMODE	m_dev_mode[2];
};

CWqlCmdLine::CWqlCmdLine(void)
	: m_locator(NULL)
	, m_server(NULL)
	, m_mode(MDM_UNKNOWN)
	, m_primary(0)
{
	m_namespace = _T("ROOT\\WMI");
}

//CWqlCmdLine::~CWqlCmdLine(void)
//{
//}

bool CWqlCmdLine::Initialize(void)
{
	LOG_DEBUG(_T("name space = %s"), m_namespace.c_str() );
	LOG_DEBUG(_T("mode = %d"), m_mode);

	HRESULT hr = CoInitializeEx(NULL, 
		//COINIT_MULTITHREADED);
		COINIT_APARTMENTTHREADED);
	if ( FAILED(hr) )	THROW_ERROR(ERR_APP, _T("failed on initializing com"));

	hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, 
		RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
	if ( FAILED(hr) )	THROW_ERROR(ERR_APP, _T("failed on initializing security"));


	RPC_WSTR str;
	UuidToString(&CLSID_WbemLocator, &str);
	LOG_DEBUG(_T("clsid = '%s'"), str);
	RpcStringFree(&str);
/*
	hr = CoCreateInstance(CLSID_WbemLocator, 0, 
        CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &m_locator);
	if ( FAILED(hr) )	THROW_ERROR(ERR_APP, _T("failed on creating wbem locator"));

    // Connect to the root\default namespace with the current user.
    hr = m_locator->ConnectServer(
            BSTR(m_namespace.c_str() ), 
            NULL, NULL, 0, NULL, 0, 0, & m_server);
	if ( FAILED(hr) )	THROW_ERROR(ERR_APP, _T("failed on connecting to server"));
	
	// Set the proxy so that impersonation of the client occurs.
    hr = CoSetProxyBlanket(m_server,
       RPC_C_AUTHN_WINNT,
       RPC_C_AUTHZ_NONE,
       NULL,
       RPC_C_AUTHN_LEVEL_CALL,
       RPC_C_IMP_LEVEL_IMPERSONATE,
       NULL,
       EOAC_NONE
    );
	if ( FAILED(hr) )	THROW_ERROR(ERR_APP, _T("failed on setting proxy"));
	*/
	return true;

}

void CWqlCmdLine::CleanUp(void)
{
	if (m_locator) m_locator->Release();
	if (m_server) m_server->Release();
	CoUninitialize();
}

HRESULT CWqlCmdLine::DealWithSingleItem( CComBSTR bstrName, CComVariant Value, CIMTYPE type, LONG lFlavor )  
{  
    HRESULT hr = WBEM_S_NO_ERROR;   
    switch ( Value.vt ) {  
        case VT_UNKNOWN : {  
            DealWithUnknownTypeItem(bstrName, Value, type, lFlavor);  
            }break;  
        default: {  
            PrintfItem(bstrName, Value, type, lFlavor);  
        };  
    }  
    return hr;  
}  
  
HRESULT CWqlCmdLine::DealWithUnknownTypeItem( CComBSTR bstrName, CComVariant Value, CIMTYPE type, LONG lFlavor )  
{  
    HRESULT hr = WBEM_S_NO_ERROR;  
    if ( NULL == Value.punkVal ) {  
        return hr;  
    }  
    // object类型转换成IWbemClassObject接口指针，通过该指针枚举其他属性  
    CComPtr<IWbemClassObject> pObjInstance = (IWbemClassObject*)Value.punkVal;  
    hr = pObjInstance->BeginEnumeration(WBEM_FLAG_LOCAL_ONLY);  
    do {  
        //CHECKHR(hr);  
        CComBSTR bstrNewName;  
        CComVariant NewValue;  
        CIMTYPE newtype;  
        LONG lnewFlavor = 0;  
        hr = pObjInstance->Next(0, &bstrNewName, &NewValue, &newtype, &lnewFlavor);  
        //CHECKHR(hr);  
        PrintfItem(bstrNewName, NewValue, newtype, lnewFlavor);  
		if (NewValue.vt == VT_EMPTY) break;
    }while ( WBEM_S_NO_ERROR == hr );  
    hr = pObjInstance->EndEnumeration();  
    return WBEM_S_NO_ERROR;  
}  
  
void CWqlCmdLine::PrintfItem( CComBSTR bstrName, CComVariant Value, CIMTYPE type, LONG lFlavor )  
{  
    wprintf(L"%s\t",bstrName.m_str);  
    switch ( Value.vt ){  
        case VT_BSTR: {  
            wprintf(L"%s",Value.bstrVal);          
                      }break;  
        case VT_I1:  
        case VT_I2:  
        case VT_I4:  
        case VT_I8:   
        case VT_INT: {  
                wprintf(L"%d",Value.intVal);   
            }break;  
        case VT_UI8:  
        case VT_UI1:      
        case VT_UI2:  
        case VT_UI4:  
        case VT_UINT:{  
            wprintf(L"0x%u",Value.intVal);       
            }break;  
        case VT_BOOL:{  
            wprintf(L"%s", Value.boolVal ? L"TRUE" : L"FASLE" );  
                     }break;  
		case VT_EMPTY:
			_tprintf(_T("NULL"));
			break;

        default:{  
            ATLASSERT(FALSE);  
                };  
    }  
    wprintf(L"\n");  
}  

void CWqlCmdLine::ShowResult(IEnumWbemClassObject * enumerator)
{
	ULONG uReturn = 0;
	while (1) 
	{
		IWbemClassObject * pclsObj = NULL;
		HRESULT hr = enumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if ( 0 == uReturn) break;  
		// deal with result objects
        CComVariant vtClass;   
  
        hr = pclsObj->Get(_T("__CLASS"), 0, &vtClass, NULL, NULL);   
        //CHECKWMIHR(hr);  
        if ( VT_BSTR == vtClass.vt ) 
		{  
            _tprintf(_T("\n%s\n"), vtClass.bstrVal);  
        }  
  
        hr = pclsObj->BeginEnumeration(WBEM_FLAG_LOCAL_ONLY);  
  
        do {
            CComBSTR bstrName;  
            CComVariant Value;  
            CIMTYPE type;  
            LONG lFlavor = 0;  
            hr = pclsObj->Next(0, &bstrName, &Value, &type, &lFlavor);  
			if ( WBEM_S_NO_ERROR != hr ) break;
            //CHECKWMIHR(hr);  
            DealWithSingleItem(bstrName, Value, type, lFlavor);              
        }while ( WBEM_S_NO_ERROR == hr );  
  
        hr = pclsObj->EndEnumeration();  
		pclsObj->Release();
	} 
}

void CWqlCmdLine::RunWql(void)
{
	stdext::auto_array<TCHAR> line_buf(MAX_LINE_BUF);
	while (1)
	{
		_tprintf(_T(">"));
		_getts_s(line_buf, MAX_LINE_BUF-1);
		if ( _tcscmp(_T(".exit"), line_buf) == 0) break;

		IEnumWbemClassObject * pEnumerator = NULL;

		HRESULT hr = m_server->ExecQuery(   
				BSTR( _T("WQL") ),  
				BSTR(line_buf),  
				WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,  
				NULL,  
				&pEnumerator );  
		// ERROR HANDLING
		if (pEnumerator)
		{
			ShowResult(pEnumerator);
			pEnumerator->Release();
		}
	}
}

#define szPrimaryDisplay _T( "\\\\.\\DISPLAY1" )
#define szSecondaryDisplay _T( "\\\\.\\DISPLAY2" )
#define szThirdDisplay _T( "\\\\.\\DISPLAY3" )

static const LPCTSTR DISPLAY_NAME[2] = {_T("\\\\.\\DISPLAY1"), _T("\\\\.\\DISPLAY2")};

void OutputDisplayInfo(const DISPLAY_DEVICE & dd)
{
	LOG_DEBUG(_T("\t name = '%s'"), dd.DeviceName);
	LOG_DEBUG(_T("\t device string = '%s'"), dd.DeviceString);
	LOG_DEBUG(_T("\t id = '%s'"), dd.DeviceID);
	LOG_DEBUG(_T("\t key = '%s'"), dd.DeviceKey);
	// output flag
	LOG_DEBUG(_T("\t flag:"));
	if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)	LOG_DEBUG(_T("\t\t desktop"));
	if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)			LOG_DEBUG(_T("\t\t primary"));
	if (dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)		LOG_DEBUG(_T("\t\t mirroring"));
	if (dd.StateFlags & DISPLAY_DEVICE_MODESPRUNED)			LOG_DEBUG(_T("\t\t spruned"));
	if (dd.StateFlags & DISPLAY_DEVICE_REMOVABLE)				LOG_DEBUG(_T("\t\t removeable"));
	if (dd.StateFlags & DISPLAY_DEVICE_VGA_COMPATIBLE)			LOG_DEBUG(_T("\t\t vga"));
}

void CWqlCmdLine::DetachDisplay(DWORD disp_num)
{
	DISPLAY_DEVICE  DisplayDevice;
// initialize DisplayDevice
    ZeroMemory(&DisplayDevice, sizeof(DisplayDevice));
    DisplayDevice.cb = sizeof(DisplayDevice);
	BOOL br = EnumDisplayDevices(NULL, disp_num, &DisplayDevice, 0);
	if (!br) THROW_ERROR(ERR_APP, _T("enumerate display %d failed."), disp_num);

	OutputDisplayInfo(DisplayDevice);
	
	DEVMODE   defaultMode;
	ZeroMemory(&defaultMode, sizeof(DEVMODE));
	defaultMode.dmSize = sizeof(DEVMODE);
	br = EnumDisplaySettings(DisplayDevice.DeviceName, ENUM_REGISTRY_SETTINGS, &defaultMode);
	if (!br) THROW_ERROR(ERR_APP, _T("read display setting failed."));

	LOG_DEBUG(_T("default size = %d x %d"), defaultMode.dmPelsWidth, defaultMode.dmPelsHeight);

    if ((DisplayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) /*&&
            !(DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)*/)
	{
		DEVMODE    DevMode;
        ZeroMemory(&DevMode, sizeof(DevMode));
        DevMode.dmSize = sizeof(DevMode);
        //DevMode.dmFields =  | DM_BITSPERPEL 
        //                 ;
		//DevMode.dmPosition.x = defaultMode.dmPosition.x;
		//DevMode.dmPosition.y = defaultMode.dmPosition.y;

        DevMode.dmFields = DM_POSITION | DM_PELSWIDTH | DM_PELSHEIGHT/* | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS*/;
        LONG ir = ChangeDisplaySettingsEx(DisplayDevice.DeviceName, 
                                            &DevMode,
                                            NULL,
                                            CDS_UPDATEREGISTRY,
                                            NULL);
		if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
			ERR_APP, _T("failure on setting primary display '%s', code %d"), 
			(LPSTR)DisplayDevice.DeviceName, ir)
	}
}

void CWqlCmdLine::AttachDisplay(DWORD disp_num)
{
	DISPLAY_DEVICE  DisplayDevice;
// initialize DisplayDevice
    ZeroMemory(&DisplayDevice, sizeof(DisplayDevice));
    DisplayDevice.cb = sizeof(DisplayDevice);
	BOOL br = EnumDisplayDevices(NULL, disp_num, &DisplayDevice, 0);
	if (!br) THROW_ERROR(ERR_APP, _T("enumerate display %d failed."), disp_num);

	OutputDisplayInfo(DisplayDevice);
	
	DEVMODE   defaultMode;
	ZeroMemory(&defaultMode, sizeof(DEVMODE));
	defaultMode.dmSize = sizeof(DEVMODE);
	br = EnumDisplaySettings(DisplayDevice.DeviceName, ENUM_REGISTRY_SETTINGS, &defaultMode);
	if (!br) THROW_ERROR(ERR_APP, _T("read display setting failed."));

		//DEVMODE    DevMode;
  //      ZeroMemory(&DevMode, sizeof(DevMode));
  //      DevMode.dmSize = sizeof(DevMode);
        //DevMode.dmFields =  | DM_BITSPERPEL 
        //                 ;
		//DevMode.dmPosition.x = defaultMode.dmPosition.x;
		//DevMode.dmPosition.y = defaultMode.dmPosition.y;

        //DevMode.dmFields = DM_POSITION | DM_PELSWIDTH | DM_PELSHEIGHT/* | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS*/;
        LONG ir = ChangeDisplaySettingsEx(DisplayDevice.DeviceName, 
                                            &defaultMode,
                                            NULL,
                                            CDS_UPDATEREGISTRY,
                                            NULL);
		if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
			ERR_APP, _T("failure on setting primary display '%s', code %d"), 
			(LPSTR)DisplayDevice.DeviceName, ir)
	//}
}


void CWqlCmdLine::ReadDisplayInfo(void)
{
	LOG_STACK_TRACE();
	ZeroMemory( m_dev_mode, 2 * sizeof(DEVMODE) );

	for (int ii = 0; ii < 2; ++ii)
	{
		m_dev_mode[ii].dmSize = sizeof(DEVMODE);
		LPCTSTR device_name = DISPLAY_NAME[ii];
		LOG_DEBUG(_T("reading display='%s'"), device_name);
		BOOL br = EnumDisplaySettings(device_name, ENUM_REGISTRY_SETTINGS, m_dev_mode + ii);
		if (!br) THROW_ERROR(ERR_APP, _T("failure on reading display %s"), device_name);
		
		LOG_DEBUG(_T("device name = '%s'"), m_dev_mode[ii].dmDeviceName);
		LOG_DEBUG(_T("size = %d x %d"), m_dev_mode[ii].dmPelsWidth, m_dev_mode[ii].dmPelsHeight);
		LOG_DEBUG(_T("position = %d, %d"), m_dev_mode[ii].dmPosition.x, m_dev_mode[ii].dmPosition.y);
		LOG_DEBUG(_T("flag = 0x%08X"), m_dev_mode[ii].dmDisplayFlags);
	}
}

void CWqlCmdLine::EnumDisplayInfo(void)
{

	int   nModeSwitch = NULL;
	DEVMODE dmPrimary;
	ZeroMemory( &dmPrimary, sizeof(dmPrimary) );
	dmPrimary.dmSize = sizeof(dmPrimary);

	DEVMODE dmSecondary;
	ZeroMemory( &dmSecondary, sizeof(dmSecondary) );
	dmSecondary.dmSize = sizeof(dmSecondary);

	DEVMODE dmThird;
	ZeroMemory( &dmThird, sizeof(dmThird) );
	dmThird.dmSize = sizeof(dmThird);

	DEVMODE savedmSecondary;
	ZeroMemory( &savedmSecondary, sizeof(savedmSecondary) );
	savedmSecondary.dmSize = sizeof(savedmSecondary);

	BOOL result;
	HDC handle;
	DWORD iDevNum = 0;
	DWORD dwFlags = EDD_GET_DEVICE_INTERFACE_NAME;

	
	DISPLAY_DEVICE lpDisplayDeviceOne;
	ZeroMemory(&lpDisplayDeviceOne, sizeof(lpDisplayDeviceOne));
	lpDisplayDeviceOne.cb = sizeof(lpDisplayDeviceOne);
	
	DISPLAY_DEVICE lpDisplayDeviceTwo;    
	ZeroMemory(&lpDisplayDeviceTwo, sizeof(lpDisplayDeviceTwo));
	lpDisplayDeviceTwo.cb = sizeof(lpDisplayDeviceTwo);

	// All this does is confirm the number of display devices the graphics board is 
	//	capable of handling
	std::vector<CJCStringT>		display_name_list;

	CJCStringT adaptor_name;
	while(1)
	{
		DISPLAY_DEVICE dd;
		ZeroMemory(&dd, sizeof(dd));       
		dd.cb = sizeof(dd);

		BOOL br =  EnumDisplayDevices(NULL, iDevNum, &dd, dwFlags) ;
		if (!br) break;
		LOG_DEBUG(_T("device num = %d"), iDevNum);
		OutputDisplayInfo(dd);
		LOG_DEBUG(_T("end device %d"), iDevNum);
		
		display_name_list.push_back(dd.DeviceName);


		if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
		{
			DISPLAY_DEVICE dd1;
			ZeroMemory(&dd1, sizeof(dd1));
			dd1.cb = sizeof(dd1);

			LPCTSTR device_name = dd.DeviceName;

			LOG_DEBUG(_T("device name = %s"), device_name);

			BOOL br = EnumDisplayDevices(device_name, 0, &dd1, EDD_GET_DEVICE_INTERFACE_NAME);
			if (!br)	LOG_DEBUG(_T("failed on enum display device "))
			else		OutputDisplayInfo(dd1);
			LOG_DEBUG(_T("end device"));

		}

		if (iDevNum == 0)
		{
		}
		iDevNum ++;
	}

	std::vector<CJCStringT>::iterator it = display_name_list.begin();
	std::vector<CJCStringT>::iterator endit = display_name_list.end();

	for ( ; it != endit; ++it)
	{
		for (int ii = 0; ii<2; ++ii)
		{
			DEVMODE dm;
			ZeroMemory( &dm, sizeof(dm) );
			dm.dmSize = sizeof(dm);
			LPCTSTR device_name = (*it).c_str();

			DWORD mode = ii ? (ENUM_REGISTRY_SETTINGS) : (ENUM_CURRENT_SETTINGS);
			LOG_DEBUG(_T("display='%s', mode=%s"), device_name, ii ? _T("REGISTRY") : _T("CURRENT"));

			BOOL br = EnumDisplaySettings(device_name, mode, &dm);
			if (br)
			{
				LOG_DEBUG(_T("\t device name = '%s'"), dm.dmDeviceName);
				LOG_DEBUG(_T("\t size = %d x %d"), dm.dmPelsWidth, dm.dmPelsHeight);
				LOG_DEBUG(_T("\t position = %d, %d"), dm.dmPosition.x, dm.dmPosition.y);
				LOG_DEBUG(_T("\t flag = 0x%08X"), dm.dmDisplayFlags);
			}
			else	LOG_DEBUG(_T("\t failed on enumerate display setting."));
			LOG_DEBUG(_T("end enumerate setting"));
		}
	}


	//for (int ii = 0; ii<4; ++ii)
	//{
	//	DEVMODE dm;
	//	ZeroMemory( &dm, sizeof(dm) );
	//	dm.dmSize = sizeof(dm);
	//	//dm.dmDriverExtra = sizeof(dm);

	//	DWORD mode = (ii & 1) ? (ENUM_REGISTRY_SETTINGS) : (ENUM_CURRENT_SETTINGS);
	//	LOG_DEBUG(_T("display='%s', mode=%d"), DISPLAY_NAME[ii >> 1 ], mode);
	//	BOOL br = EnumDisplaySettings(DISPLAY_NAME[ii >> 1 ], mode, &dm);
	//	if (br)
	//	{
	//		LOG_DEBUG(_T("\t device name = '%s'"), dm.dmDeviceName);
	//		LOG_DEBUG(_T("\t size = %d x %d"), dm.dmPelsWidth, dm.dmPelsHeight);
	//		LOG_DEBUG(_T("\t position = %d, %d"), dm.dmPosition.x, dm.dmPosition.y);
	//		LOG_DEBUG(_T("\t flag = 0x%08X"), dm.dmDisplayFlags);
	//	}
	//	else	LOG_DEBUG(_T("\t failed on enumerate display setting."));
	//	LOG_DEBUG(_T("end enumerate setting"));
	//}
}

void CWqlCmdLine::SwitchToAlone(int primary)
{
	JCASSERT(primary < 2);

	if (primary == 0)
	{
		// enalbe 0
		//m_dev_mode[0].dmFields = DM_POSITION;
		//m_dev_mode[0].dmPosition.x = 0;         // set DISPLAY1 as the primary display
		//m_dev_mode[0].dmPosition.y = 0;         // set DISPLAY1 as the primary display
		// disable 1
		memset(m_dev_mode + 1, 0, sizeof(DEVMODE) );
		m_dev_mode[1].dmSize = sizeof(DEVMODE);
		m_dev_mode[1].dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_POSITION
                        | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS ;
		m_dev_mode[1].dmPosition.x = 0;         // set DISPLAY1 as the primary display
		m_dev_mode[1].dmPosition.y = 0;         // set DISPLAY1 as the primary display

	}
	else
	{
	}

/*
	// enable primary
	m_dev_mode[primary].dmFields = DM_POSITION;
	m_dev_mode[primary].dmPosition.x = 0;         // set DISPLAY1 as the primary display
	m_dev_mode[primary].dmPosition.y = 0;         // set DISPLAY1 as the primary display
*/
	LONG ir = ChangeDisplaySettingsEx(
		DISPLAY_NAME[1], m_dev_mode + 1, 
		NULL, (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);

	if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
		ERR_APP, _T("failure on setting primary display '%s', code %d"), 
		DISPLAY_NAME[1], ir)
/*
	// disable another
	int secondary = 1-primary;
	JCASSERT(secondary < 2);

	m_dev_mode[secondary].dmFields = DM_POSITION;
	m_dev_mode[secondary].dmPelsWidth = 0; //dmPrimary.dmPelsWidth;         // resize the primary display to match the secondary display
	m_dev_mode[secondary].dmPelsHeight = 0;//dmPrimary.dmPelsHeight;            // resize the primary display to match the secondary display    
	ir = ChangeDisplaySettingsEx(DISPLAY_NAME[secondary], m_dev_mode + secondary, NULL, (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);
	if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
		ERR_APP, _T("failure on setting sencondary display '%s', code %d"), 
		DISPLAY_NAME[secondary], ir)
*/

	ir = ChangeDisplaySettingsEx(NULL, NULL, NULL, 0, NULL);
	if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
		ERR_APP, _T("failure on invoke setting, code %d"), ir)
}

void CWqlCmdLine::SwitchToClone(void)
{
	LONG ir = 0;
	for (int ii = 0; ii<2; ++ii)
	{
		m_dev_mode[ii].dmFields = DM_POSITION;
		m_dev_mode[ii].dmPosition.x = 0;         // set DISPLAY1 as the primary display
		m_dev_mode[ii].dmPosition.y = 0;         // set DISPLAY1 as the primary display
		ir = ChangeDisplaySettingsEx(
			DISPLAY_NAME[ii], m_dev_mode + ii, 
			NULL, (CDS_UPDATEREGISTRY /*| CDS_NORESET | CDS_SET_PRIMARY*/), NULL);

		if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
			ERR_APP, _T("failure on setting display '%s', code %d"), 
			DISPLAY_NAME[ii], ir)
	}

	ir = ChangeDisplaySettingsEx(NULL, NULL, NULL, 0, NULL);
	if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
		ERR_APP, _T("failure on invoke setting, code %d"), ir)
}

void CWqlCmdLine::SwitchToExpand(int primary)
{
	JCASSERT(primary < 2);
	// enable primary
	m_dev_mode[primary].dmFields = DM_POSITION;
	m_dev_mode[primary].dmPosition.x = 0;         // set DISPLAY1 as the primary display
	m_dev_mode[primary].dmPosition.y = 0;         // set DISPLAY1 as the primary display
	LONG ir = ChangeDisplaySettingsEx(
		DISPLAY_NAME[primary], m_dev_mode + primary, 
		NULL, (CDS_UPDATEREGISTRY | CDS_NORESET | CDS_SET_PRIMARY), NULL);

	if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
		ERR_APP, _T("failure on setting primary display '%s', code %d"), 
		DISPLAY_NAME[primary], ir)

	// disable another
	int secondary = 1-primary;
	JCASSERT(secondary < 2);
	m_dev_mode[secondary].dmFields = DM_POSITION;
	m_dev_mode[secondary].dmPosition.x = m_dev_mode[primary].dmPaperWidth;         // set DISPLAY1 as the primary display
	m_dev_mode[secondary].dmPosition.y = 0;         // set DISPLAY1 as the primary display
	ir = ChangeDisplaySettingsEx(DISPLAY_NAME[secondary], m_dev_mode + secondary, 
		NULL, (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);
	if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
		ERR_APP, _T("failure on setting sencondary display '%s', code %d"), 
		DISPLAY_NAME[secondary], ir)

	ir = ChangeDisplaySettingsEx(NULL, NULL, NULL, 0, NULL);
	if (ir != DISP_CHANGE_SUCCESSFUL)	THROW_ERROR(
		ERR_APP, _T("failure on invoke setting, code %d"), ir)

	// load DISPLAY1 monitor information // ENUM_CURRENT_SETTINGS     
	//if (!EnumDisplaySettings(szPrimaryDisplay, ENUM_REGISTRY_SETTINGS, (DEVMODE*)&dmPrimary))
	//{
	//	LOG_DEBUG(_T("EnumDisplaySettings unable to enumerate primary display"));
	//	return;
	//}

	//if (!EnumDisplaySettings(szSecondaryDisplay, ENUM_REGISTRY_SETTINGS, (DEVMODE*)&dmSecondary))
	//	LOG_DEBUG(_T("EnumDisplaySettings unable to enumerate secondary display display"));

	//// these don't enumerate in clone mode
	//if (!EnumDisplaySettings(szSecondaryDisplay, ENUM_CURRENT_SETTINGS, (DEVMODE*)&savedmSecondary))  
	//	LOG_DEBUG(_T("EnumDisplaySettings unable to enumerate secondary display using ENUM_CURRENT_SETTINGS settings"));

	// disable a display, doesn't work
	//    nModeSwitch = ChangeDisplaySettingsEx (szSecondaryDisplay, NULL, NULL, NULL, NULL);
	//    CDdx::ErrorDisplayDevice(nModeSwitch);                      // test and TRACE result

/*
	dmPrimary.dmFields = DM_POSITION;
	dmPrimary.dmPosition.x = 0;           // set DISPLAY1 as the primary display
	dmPrimary.dmPosition.y = 0;           // set DISPLAY1 as the primary display

	// set DISPLAY1 as primary display (dmPosition.x = 0)
	nModeSwitch = ChangeDisplaySettingsEx (szPrimaryDisplay, (DEVMODE*)&dmPrimary, NULL, (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);
	//  CDdx::ErrorDisplayDevice(nModeSwitch);                      // test and TRACE result

	// despite the other lines of code this next line is neccesary to wake the video projector
	dmSecondary = dmPrimary;

	dmSecondary.dmFields = DM_POSITION | DM_PELSWIDTH | DM_PELSHEIGHT;
	dmSecondary.dmPosition.x = dmPrimary.dmPelsWidth + 1;
	dmSecondary.dmPosition.y = 0;
	dmSecondary.dmPelsWidth = dmPrimary.dmPelsWidth;          // resize the primary display to match the secondary display
	dmSecondary.dmPelsHeight = dmPrimary.dmPelsHeight;            // resize the primary display to match the secondary display    

	nModeSwitch = ChangeDisplaySettingsEx (szSecondaryDisplay, (DEVMODE*)&dmSecondary, NULL, (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);
	//  CDdx::ErrorDisplayDevice(nModeSwitch);                      // test and TRACE result

	// load DISPLAY3 monitor information
	if (!EnumDisplaySettings(szThirdDisplay, ENUM_CURRENT_SETTINGS, (DEVMODE*)&dmThird))
	{
		LOG_DEBUG(_T("EnumDisplaySettings unable to enumerate third display display\n");
	}
	else
	{
		dmThird.dmPelsWidth = dmSecondary.dmPelsWidth;              // resize the primary display to match the secondary display
		dmThird.dmPelsHeight = dmSecondary.dmPelsHeight;            // resize the primary display to match the secondary display    
		dmThird.dmPosition.x = -dmThird.dmPelsWidth;                // set DISPLAY3 as the third display
		dmPrimary.dmPosition.y = 0;                                 // set DISPLAY1 as the third display

		// set DISPLAY3 as third display (-dmThird.dmPelsWidth)
		nModeSwitch = ChangeDisplaySettingsEx (szThirdDisplay, (DEVMODE*)&dmThird, NULL, (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);
		//    CDdx::ErrorDisplayDevice(nModeSwitch);                        // test and TRACE result
	}

	// really important line makes the whole thing happen
	nModeSwitch = ChangeDisplaySettingsEx (NULL, NULL, NULL, 0, NULL);
	//  CDdx::ErrorDisplayDevice(nModeSwitch);   // test and TRACE result
*/
}

void EnumerateIconOverlayers(void);

int CWqlCmdLine::Run(void)
{
	//JCASSERT(m_server);

	switch (m_mode)
	{
	case MDM_ICONOV:
		EnumerateIconOverlayers();
		break;

	case MDM_WQL:
		RunWql();
		break;

	case MDM_ALONE:
	case MDM_CLONE:
	case MDM_EXPAND:
		if ( (m_primary > 1) || (m_primary < 0) ) THROW_ERROR(ERR_USER, _T("primary must 0 or 1"));

		DetachDisplay(m_primary);
		//AttachDisplay(m_primary);
		EnumDisplayInfo();
		//SwitchToAlone(0);
		//SwitchToClone();

		//ReadDisplayInfo();
		//switch (m_mode)
		//{
		//case MDM_EXPAND:	SwitchToExpand(m_primary);		break;
		//case MDM_ALONE:		SwitchToAlone(m_primary);		break;
		//case MDM_CLONE:		SwitchToClone();				break;
		//}
		break;

	default:
		THROW_ERROR(ERR_USER, _T("missing test mode"));
		break;
	}


	return 0;
}


typedef jcapp::CJCApp<CWqlCmdLine>	CApplication;
static CApplication the_app;
#define _class_name_	CWqlCmdLine

BEGIN_ARGU_DEF_TABLE()
	ARGU_DEF_ENUM(_T("#00"),		0, MDMODE,		m_mode,			_T("select display mode") )
	ARGU_DEF_ITEM(_T("namespace"),	_T('n'), CJCStringT,	m_namespace,	_T("namespace") )
	ARGU_DEF_ITEM(_T("primary"),	_T('p'), int,			m_primary,		_T("specify a primary display") )
END_ARGU_DEF_TABLE()

int _tmain(int argc, _TCHAR* argv[])
{
	int ret_code = 0;
	try
	{
		the_app.Initialize();
		ret_code = the_app.Run();
	}
	catch (stdext::CJCException & err)
	{
		stdext::jc_fprintf(stderr, _T("error: %s\n"), err.WhatT() );
		ret_code = err.GetErrorID();
	}
	the_app.CleanUp();

	stdext::jc_printf(_T("Press any key to continue..."));
	getc(stdin);
	return ret_code;
}

