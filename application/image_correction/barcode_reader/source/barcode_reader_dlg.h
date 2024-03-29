
// barcode_reader_dlg.h : 头文件
//

#pragma once
#include "afxwin.h"

#include "../include/barcode_reader_ctrl.h"
//#include "ConfigureDlg.h"


///////////////////////////////////////////////////////////////////////////////
// -- 
// CBarCodeReaderDlg 对话框
class CameraDefine
{
public:
	CameraDefine(const CJCStringT & name, ATOM atom, UINT menu_id)
		: m_name(name), m_atom(atom), m_menu_id(menu_id)
	{}
public:
	CJCStringT	m_name;
	ATOM		m_atom;
	UINT		m_menu_id;
};
typedef std::vector<CameraDefine> CameraList;

class CBarCodeReaderDlg : public CDialog
{
// 构造
public:
	CBarCodeReaderDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_BARCODE_READER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBarCodeSymbol(NMHDR * hdr, LRESULT * result);
	afx_msg void OnBnClickedMirror();
	afx_msg void OnBnClickedAutoinput();
	afx_msg void OnSelectCamera(UINT id);
	afx_msg void OnEnUpdateFrameRate();
	afx_msg void OnDestroy();
	afx_msg LRESULT OnTaskBar(WPARAM wp, LPARAM lp);
	afx_msg void OnConfigurate(UINT id);
	DECLARE_MESSAGE_MAP()

public:
	CBarCodeReaderCtrl m_barcode_ctrl;
	CString m_result;
	UINT	m_id;
	CString m_symbol;

protected:
	void SimulateKeyboardInput(const std::string & str);
	void GetKeyCode(TCHAR ch, BYTE &vcode, BYTE &scode, bool &shift);
	void UpdateCheckMenu(UINT menu_id, bool & var);
	CMenu * GetConfigMenu(UINT pos);
	void UpdateSize(void);
	// 添加摄像头到系统菜单以及注册快捷键
	bool AddCameraToList(UINT id, const CJCStringT & cam_name, CMenu * sys_menu);
	// 注册到系统托盘
	void TaskBar(void);

	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);


protected:
	//CComboBox m_camera_list;
	bool m_mirror;
	bool m_autoinput;	// 检测到bar code后，自动模拟键盘输入
	bool m_clipboard;	// 检测到bar code后，自动复制到剪贴板
	CMenu m_menu;
	CameraList m_cameras;
	int	 m_last_hot_key;	// 注册hot key的最大值

public:
	int m_frame_rate;
	afx_msg void OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2);
};
