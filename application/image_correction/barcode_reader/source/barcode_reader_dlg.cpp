
// barcode_readerDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "barcode_reader.h"
#include "barcode_reader_dlg.h"

#include <zbar.h>
#include <stdext.h>

#define VIDEO_TIMER	300

LOCAL_LOGGER_ENABLE(_T("barcode"), LOGGER_LEVEL_NOTICE);

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CBarCodeReaderDlg 对话框

///////////////////////////////////////////////////////////////////////////////
// -- 

#define WM_TASKBAR	(WM_USER + 200)

CBarCodeReaderDlg::CBarCodeReaderDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CBarCodeReaderDlg::IDD, pParent)
	, m_result(_T("")), m_id(0)
	, m_mirror(false)
	, m_autoinput(false)
	, m_clipboard(false)
	, m_frame_rate(0)
	, m_last_hot_key('A'-1)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDI_QRCODE);
}

void CBarCodeReaderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_BARCODE, m_barcode_ctrl);
	DDX_Text(pDX, IDC_RESULT, m_result);
	DDX_Text(pDX, IDC_FRAME_RATE, m_frame_rate);
}

BEGIN_MESSAGE_MAP(CBarCodeReaderDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_NOTIFY(BRN_SYMBOL_DETECT, IDC_BARCODE, OnBarCodeSymbol)
	ON_BN_CLICKED(IDC_MIRROR, &CBarCodeReaderDlg::OnBnClickedMirror)
	ON_BN_CLICKED(IDC_AUTOINPUT, &CBarCodeReaderDlg::OnBnClickedAutoinput)
	ON_EN_UPDATE(IDC_FRAME_RATE, &CBarCodeReaderDlg::OnEnUpdateFrameRate)
	ON_WM_DESTROY()
	ON_WM_HOTKEY()
	ON_MESSAGE(WM_TASKBAR, OnTaskBar)
	ON_COMMAND_RANGE(ID_CONFIG_MIRROR, ID_CONFIG_CAMERA_END, OnConfigurate)
END_MESSAGE_MAP()


// CBarCodeReaderDlg 消息处理程序

BOOL CBarCodeReaderDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// 扩展系统菜单
	m_menu.LoadMenu(IDR_CONFIG_MENU);
	CMenu * config_menu = m_menu.GetSubMenu(0);	JCASSERT(config_menu);

	// 枚举camera
	CMenu menu_selcam;
	menu_selcam.CreateMenu();

	StringArray cameras;
	AddCameraToList(0, _T("No Camera"), &menu_selcam);
	size_t ir = m_barcode_ctrl.EnumerateCameras(cameras);
	StringArray::iterator it = cameras.begin();
	StringArray::iterator endit = cameras.end();
	for (UINT cam_id = 1; it!=endit; ++it, cam_id++)
	{
		CJCStringT & str_camera = (*it);
		AddCameraToList(cam_id, str_camera, &menu_selcam);
	}
	menu_selcam.CheckMenuRadioItem(
		ID_CONFIG_CAMERA_OFF, ID_CONFIG_CAMERA_END, 
		ID_CONFIG_CAMERA_OFF, MF_BYCOMMAND);
	config_menu->ModifyMenu(0, MF_BYPOSITION|MF_POPUP,
		(UINT)(menu_selcam.m_hMenu), _T("Select Camera"));

	CMenu * menu_rotation = GetConfigMenu(1);
	menu_rotation->CheckMenuRadioItem(0, 3, 0, MF_BYPOSITION);

	pSysMenu->InsertMenu(0, MF_BYPOSITION|MF_POPUP, (UINT)(config_menu->m_hMenu), _T("Config"));
	TaskBar();
	UpdateSize();
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CBarCodeReaderDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	LOG_STACK_TRACE();
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else if ((nID & 0xFFF0) == SC_CLOSE)
	{
		CMenu * menu = GetConfigMenu(0);
		menu->CheckMenuRadioItem(
			ID_CONFIG_CAMERA_OFF, ID_CONFIG_CAMERA_END, 
			ID_CONFIG_CAMERA_OFF, MF_BYCOMMAND);
		OnSelectCamera(ID_CONFIG_CAMERA_OFF);
	}
	else if ( nID >= ID_CONFIG_MIRROR && nID <= ID_CONFIG_CAMERA_END)
	{
		OnConfigurate(nID);
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

void CBarCodeReaderDlg::OnConfigurate(UINT nID)
{
	LOG_STACK_TRACE();
	if ( nID >= ID_CONFIG_CAMERA_OFF && nID <= ID_CONFIG_CAMERA_END)
	{
		CMenu * menu = GetConfigMenu(0);
		menu->CheckMenuRadioItem(
			ID_CONFIG_CAMERA_OFF, ID_CONFIG_CAMERA_END, 
			nID - ID_CONFIG_CAMERA_OFF, MF_BYCOMMAND);
		OnSelectCamera(nID);
		//UpdateSize();
	}
	else if ( nID >= ID_ROTATION_000 && nID <= ID_ROTATION_270)
	{
		m_barcode_ctrl.SetBCRProperty(CBarCodeReaderCtrl::BCR_ROTATION, nID-ID_ROTATION_000);
		CMenu * menu_rotation = GetConfigMenu(1);
		menu_rotation->CheckMenuRadioItem(0, 3, nID - ID_ROTATION_000, MF_BYPOSITION);
		UpdateSize();
	}
	else if (nID == ID_CONFIG_MIRROR)
	{
		UpdateCheckMenu(ID_CONFIG_MIRROR, m_mirror);
		m_barcode_ctrl.SetBCRProperty(CBarCodeReaderCtrl::BCR_MIRROR, m_mirror);
	}
	else if (nID == ID_CONFIG_AUTOINPUT)
	{
		UpdateCheckMenu(ID_CONFIG_AUTOINPUT, m_autoinput);
	}
	else if (nID == ID_CONFIG_EXIT)
	{
		DestroyWindow();
	}
	else if (nID == ID_CONFIG_CLIPBOARD)
	{
		UpdateCheckMenu(ID_CONFIG_CLIPBOARD, m_clipboard);
	}
}


// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CBarCodeReaderDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CBarCodeReaderDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CBarCodeReaderDlg::GetKeyCode(TCHAR ch, BYTE &vcode, BYTE &scode, bool &shift)
{
	shift = false;

	if(ch >= 97 && ch <= 122)		vcode=ch-32,	shift = false;	//小写a-z
	else if(ch >= 65 && ch <= 90)	vcode=(BYTE)ch,	shift = true;	//大写A-Z
	else if(ch >= 48 && ch <= 57)	vcode=(BYTE)ch,	shift = false;	//数字0-9
	else //特殊符号
	{
		switch(ch)
		{
		case '`':	vcode = 192, shift = false;		break;
		case '~':	vcode = 192, shift = true;		break;
		//!和1在同一个键盘，使用同一个键盘码，以下相同
		case '!':	vcode = '1', shift = true;		break;
		case '@':	vcode = '2', shift = true;		break;						
		case '#':	vcode = '3', shift = true;		break;
		case '$':	vcode = '4', shift = true;		break;
		case '%':	vcode = '5', shift = true;		break;
		case '^':	vcode = '6', shift = true;		break;
		case '&':	vcode = '7', shift = true;		break;
		case '*':	vcode = '8', shift = true;		break;
		case '(':	vcode = '9', shift = true;		break;
		case ')':	vcode = '0', shift = true;		break;
		case '-':	vcode = 189, shift = false;		break;
		case '=':	vcode = 187, shift = false;		break;
		case '_':	vcode = 189, shift = true;		break;
		case '+':	vcode = 187, shift = true;		break;
		case '[':	vcode = 219, shift = false;		break;
		case '{':	vcode = 219, shift = true;		break;
		case ']':	vcode = 221, shift = false;		break;
		case '}':	vcode = 221, shift = true;		break;
		case '\\':	vcode = 220, shift = false;		break;
		case '|':	vcode = 220, shift = true;		break;
		case ';':	vcode = 186, shift = false;		break;
		case ':':	vcode = 186, shift = true;		break;
		case '\'':	vcode = 222, shift = false;		break;
		case '\"':	vcode = 222, shift = true;		break;
		case ',':	vcode = 188, shift = false;		break;
		case '<':	vcode = 188, shift = true;		break;
		case '.':	vcode = 190, shift = false;		break;
		case '>':	vcode = 190, shift = true;		break;
		case '/':	vcode = 191, shift = false;		break;
		case '?':	vcode = 191, shift = true;		break;
		}
	}
	scode = MapVirtualKey(vcode, MAPVK_VK_TO_VSC);
}

void CBarCodeReaderDlg::SimulateKeyboardInput(const std::string & str)
{
	HWND hwnd = ::GetDesktopWindow();
	int str_len = str.length();

	for (int cc=0; cc<str_len; ++cc)
	{
		char ch = str.at(cc);
/*
		BYTE vcode=0, scode=0;
		bool shift = false;
		GetKeyCode(ch, vcode, scode, shift);
		if(shift)	
		{
			BYTE sshift=MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC);
			keybd_event(VK_SHIFT, sshift, 0, 0);
			keybd_event(vcode,scode, 0,0);
			keybd_event(vcode,scode,KEYEVENTF_KEYUP,0);
			keybd_event(VK_SHIFT, sshift, KEYEVENTF_KEYUP,0);
		}
		else
		{
			keybd_event(vcode,scode, 0,0);
			keybd_event(vcode,scode,KEYEVENTF_KEYUP,0);
		}
*/
		INPUT key_input;
		memset(&key_input, 0, sizeof(INPUT));
		key_input.type = INPUT_KEYBOARD;
		key_input.ki.wScan = ch;
		key_input.ki.dwFlags = KEYEVENTF_UNICODE;
		SendInput(1, &key_input, sizeof(INPUT));
		key_input.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
		SendInput(1, &key_input, sizeof(INPUT));


		

		//if (ch < 'A') continue;
		//if ('a'<=ch && ch<='z') continue;
		//int scan_code = MapVirtualKey(ch, MAPVK_VK_TO_VSC);
		//LOG_DEBUG(_T("key event: '%c', 0x%X, 0x%X"), ch, ch, scan_code);
		//keybd_event(ch, 0, 0, NULL);
		////UINT param = MAKELONG(1,MAKEWORD(scan_code&0xFF,0x0));
		//////::PostMessage(hwnd, WM_CHAR, ch, 0x40000001);
		////::PostMessage(hwnd, WM_KEYDOWN, ch, param);
		//Sleep(5);
		////param = MAKELONG(1,MAKEWORD(scan_code&0xFF,0xC0));
		////::PostMessage(hwnd, WM_KEYUP, ch, 0xC0000001);
		//keybd_event(ch, 0, KEYEVENTF_KEYUP, NULL);
	}
}

void CBarCodeReaderDlg::OnBarCodeSymbol(NMHDR * hdr, LRESULT * result)
{
	BRC_SYMBOL_DETECT * symbol = reinterpret_cast<BRC_SYMBOL_DETECT*>(hdr);
	std::string & str = symbol->symbol_data;
	if (m_autoinput)	SimulateKeyboardInput(str);
	while (m_clipboard)
	{
		BOOL br = OpenClipboard();
		if (!br) break;
		HGLOBAL clipbuf;
		size_t buf_len = str.length() +1;
		clipbuf = GlobalAlloc(GHND, buf_len);	JCASSERT(clipbuf);
		char * buf = (char*)(GlobalLock(clipbuf));				JCASSERT(buf);
		strcpy_s(buf, buf_len, str.c_str());
		GlobalUnlock(clipbuf);
		EmptyClipboard();
		SetClipboardData(CF_TEXT, clipbuf);
		CloseClipboard();
		break;
	}

	m_result.Format(_T("%S (%d)"), symbol->symbol_data.c_str(), m_id++);
	UpdateData(false);

	*result = 0;
}


///////////////////////////////////////////////////////////////////////////////
// -- 
void CBarCodeReaderDlg::OnSelectCamera(UINT id)
{
	int cam_id = id-ID_CONFIG_CAMERA_OFF;
	m_barcode_ctrl.Close();
	if (cam_id > 0)
	{
		bool br = m_barcode_ctrl.Open(cam_id - 1);
		if (!br)
		{
			CString str;
			str.Format(_T("Open video failed"));
			AfxMessageBox(str);
		}
		UpdateSize();
	}
	else
	{
		ShowWindow(SW_HIDE);
	}
}

void CBarCodeReaderDlg::OnBnClickedMirror()
{
	UpdateData();
	m_barcode_ctrl.SetBCRProperty(CBarCodeReaderCtrl::BCR_MIRROR, m_mirror);
}

void CBarCodeReaderDlg::OnBnClickedAutoinput()
{
	UpdateData();
}

void CBarCodeReaderDlg::OnEnUpdateFrameRate()
{
	UpdateData();
	m_barcode_ctrl.SetBCRProperty(CBarCodeReaderCtrl::BCR_FRAME_RATE, m_frame_rate);
}

CMenu * CBarCodeReaderDlg::GetConfigMenu(UINT pos)
{
	CMenu * config_menu = m_menu.GetSubMenu(0);	JCASSERT(config_menu);
	CMenu * sub_menu = config_menu->GetSubMenu(pos);
	return sub_menu;
}

void CBarCodeReaderDlg::OnDestroy()
{
	m_barcode_ctrl.Close();
	CameraList::iterator it = m_cameras.begin();
	CameraList::iterator endit = m_cameras.end();
	for (;it!=endit; ++it)
	{
		CameraDefine & cam = (*it);
		UnregisterHotKey(this->GetSafeHwnd(), cam.m_atom);
		GlobalDeleteAtom(cam.m_atom);
	}
	NOTIFYICONDATA nid;
	memset(&nid, 0, sizeof(NOTIFYICONDATA));
    nid.cbSize=(DWORD)sizeof(NOTIFYICONDATA);  
	nid.hWnd=GetSafeHwnd();  
    nid.uID=IDR_MAINFRAME;  
	Shell_NotifyIcon(NIM_DELETE, &nid);//在托盘区添加图标  
	CDialog::OnDestroy();
}

void CBarCodeReaderDlg::UpdateSize(void)
{
	if (m_barcode_ctrl.IsOpened())
	{
		RECT barcode_rect;
		m_barcode_ctrl.GetWindowRect(&barcode_rect);
		SetWindowPos(NULL, 0, 0, barcode_rect.right - barcode_rect.left + 8,
			barcode_rect.bottom - barcode_rect.top + 22, 
			SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW);
	}
	else
	{
		ShowWindow(SW_HIDE);
	}
}

void CBarCodeReaderDlg::UpdateCheckMenu(UINT menu_id, bool & var)
{
	var = !var;
	CMenu * config_menu = m_menu.GetSubMenu(0);		JCASSERT(config_menu);
		config_menu->CheckMenuItem(menu_id,
			MF_BYCOMMAND | (var)?(MF_CHECKED):(MF_UNCHECKED) );
}

bool CBarCodeReaderDlg::AddCameraToList(UINT id, const CJCStringT & cam_name, CMenu * sys_menu)
{
	TCHAR key_atom[256];
	_stprintf_s(key_atom, _T("camera key: %s"), cam_name.c_str());
	ATOM cam_atom;
	cam_atom = GlobalAddAtom(key_atom);
	LOG_DEBUG(_T("camera:%s, atom:0x%X"), cam_name.c_str(), cam_atom)
	while (m_last_hot_key <= 'Z')
	{
		BOOL br = RegisterHotKey(this->GetSafeHwnd(), cam_atom, MOD_CONTROL | MOD_ALT, ++m_last_hot_key);
		if (br) 
		{
			LOG_DEBUG(_T("<ctrl>+<win>+%c is registered for camera %s"), m_last_hot_key, cam_name.c_str());
			break;	
		}
		else
		{
			LOG_DEBUG(_T("<ctrl>+<win>+%c has been registered"), m_last_hot_key);
		}
	}
	if (m_last_hot_key > 'Z')
	{
		LOG_ERROR(_T("no hot keys can be used."))
		return false;
	}
	// create a name for menu
	if (sys_menu)
	{
		_stprintf_s(key_atom, _T("%s\t<ctrl><alt>%c"), cam_name.c_str(), m_last_hot_key);
		sys_menu->AppendMenu(MF_STRING, ID_CONFIG_CAMERA_OFF+id,
			key_atom);
	}
	m_cameras.push_back( CameraDefine(cam_name, cam_atom, ID_CONFIG_CAMERA_OFF+id));
	return true;
}

void CBarCodeReaderDlg::OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2)
{
	// find hot key
	CameraList::iterator it = m_cameras.begin();
	CameraList::iterator endit = m_cameras.end();
	for (;it!=endit; ++it)
	{
		CameraDefine & cam = (*it);
		if (cam.m_atom == nHotKeyId)
		{
			OnSelectCamera(cam.m_menu_id);
			//UpdateSize();
			return;
		}
	}

	CDialog::OnHotKey(nHotKeyId, nKey1, nKey2);
}

void CBarCodeReaderDlg::TaskBar(void)
{
    // TODO: 在此添加控件通知处理程序代码   
	NOTIFYICONDATA nid;
	memset(&nid, 0, sizeof(NOTIFYICONDATA));
    nid.cbSize=(DWORD)sizeof(NOTIFYICONDATA);  
	nid.hWnd=GetSafeHwnd();  
    nid.uID=IDR_MAINFRAME;  
    nid.uFlags= NIF_ICON  | NIF_TIP | NIF_MESSAGE/*| NIF_INFO*/;  
    nid.uCallbackMessage=WM_TASKBAR;//自定义的消息名称,注意:这里的消息是用户自定义消息  
    nid.hIcon=LoadIcon(AfxGetInstanceHandle(),MAKEINTRESOURCE(IDI_QRCODE));  
    wcscpy_s(nid.szTip,_T("Bar Code Reader"));//信息提示条为"计划任务提醒"  
    //wcscpy_s(nid.szInfo,_T("标题"));  
    //wcscpy_s(nid.szInfoTitle,_T("内容"));  
    nid.dwInfoFlags=NIIF_USER;  
    nid.uTimeout=5000;        
	Shell_NotifyIcon(NIM_ADD, &nid);//在托盘区添加图标  
}

BOOL CBarCodeReaderDlg::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	LOG_STACK_TRACE();
	NMHDR * hdr = reinterpret_cast<NMHDR*>(lParam);
	if (hdr->code == NIN_SELECT )
	{
		hdr->idFrom;
	}
	return CDialog::OnNotify(wParam, lParam, pResult);
}


LRESULT CBarCodeReaderDlg::OnTaskBar(WPARAM wp, LPARAM lp)
{
	if (wp != IDR_MAINFRAME) return 0;
	LOG_DEBUG(_T("got task bar message, lp=0x%08X"), lp)
	switch (lp)
	{
	case WM_RBUTTONUP:	{
		POINT pos;
		::GetCursorPos(&pos);
		CMenu * config_menu = m_menu.GetSubMenu(0);
		config_menu->TrackPopupMenu(TPM_LEFTALIGN, pos.x, pos.y, this);
		break;			}
	}
	return 0;
}
