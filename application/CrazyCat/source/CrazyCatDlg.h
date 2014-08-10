
// CrazyCatDlg.h : 头文件
//

#pragma once
#include "afxwin.h"

#include "ChessBoard.h"



// CCrazyCatDlg 对话框
class CCrazyCatDlg : public CDialog
	, public IRefereeListen
{
// 构造
public:
	CCrazyCatDlg(CWnd* pParent = NULL);	// 标准构造函数
	~CCrazyCatDlg(void);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_CRAZYCAT_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

	enum PLAY_STATUS
	{	
		PS_SETTING = 0,	PS_PLAYING = 0x10, PS_WIN = 0x20,
	};

	enum AI_TYPE
	{
		AI_HUMEN = 0, AI_NO_SORT = 1, AI_SORT = 2,
	};
// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

	afx_msg void OnCellRadiusChanged(void);
	afx_msg void OnClickedPlay();
	afx_msg void OnClickedSearch();
	afx_msg void OnClickedShowPath();
	afx_msg LRESULT OnChessClicked(WPARAM wp, LPARAM lp);
	afx_msg LRESULT OnCompleteMove(WPARAM wp, LPARAM lp);
	afx_msg LRESULT OnRobotMove(WPARAM wp, LPARAM lp);
	afx_msg void OnClickedStop();
	afx_msg void OnClickedReset();
	afx_msg void OnClickUndo();
	afx_msg void OnClickedSave();
	afx_msg void OnClickedLoad();
	afx_msg void OnClickedRedo();

	// for debug only
	afx_msg void OnClidkMakeRndTab();

public:
	int	m_cell_radius;
	bool m_init;

	//CCrazyCatEvaluator * m_evaluate;

protected:
	virtual void SendTextMsg(const CJCStringT & txt);
	virtual void SearchCompleted(MOVEMENT * move);
	void DoMovement(CCrazyCatMovement * mv);

protected:
	// 棋盘及AI
	CChessBoard * m_board;
	int m_search_depth;
	IRobot * m_robot[3];		// 当角色的类型为AI时，AI类指针
	int m_move_count;
	CCrazyCatEvaluator	m_eval;

	//界面变量
	CButton m_btn_play;
	CButton m_btn_stop;
	CButton m_ctrl_turn;
	CEdit m_message_wnd;
	CChessBoardUi m_board_ctrl;
	CString m_txt_location;		// 鼠标位置
	int m_player[3];			// 各角色的玩家类型，TRUE：AI，FALSE：用户

	BOOL m_show_path, m_show_search;
	CString m_checksum;		// 棋局的HASH
	PLAYER m_turn;			// 轮流标志
	PLAY_STATUS	m_status;

public:
	static LPCTSTR PLAYER_NAME[3];
	CComboBox m_ctrl_player_catcher;
};
