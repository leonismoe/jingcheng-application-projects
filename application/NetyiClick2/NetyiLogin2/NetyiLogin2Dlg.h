// NetyiLogin2Dlg.h : 头文件
//

#pragma once


// CNetyiLogin2Dlg 对话框
class CNetyiLogin2Dlg : public CDialog
{
// 构造
public:
	CNetyiLogin2Dlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_NETYILOGIN2_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
};
