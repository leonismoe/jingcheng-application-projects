// ChessBoard.cpp : 实现文件
//

#include "stdafx.h"
#include "CrazyCat.h"
#include "ChessBoard.h"
#include "robot_cat.h"
#include "robot_catcher.h"

#include <stdext.h>

LOCAL_LOGGER_ENABLE(_T("chess_board"), LOGGER_LEVEL_DEBUGINFO);

// CChessBoardUi

IMPLEMENT_DYNAMIC(CChessBoardUi, CStatic)

CChessBoardUi::CChessBoardUi()
	: m_init(false)
	, m_chess_board(NULL)
	, m_hit(false), m_cx(0), m_cy(0)
	, m_evaluate(NULL)
	, m_draw_path(true), m_draw_search(false)
{
	m_cr = CHESS_RADIUS;
	m_ca = (float)1.155 * m_cr;		// 2/squr(3)
}

CChessBoardUi::~CChessBoardUi()
{
}

void CChessBoardUi::SetBoard(CChessBoard * board)
{
	JCASSERT(board);
	m_chess_board = board;
}

void CChessBoardUi::SetPath(CCrazyCatEvaluator * ev)
{
	m_evaluate = ev;
}

bool CChessBoardUi::Hit(int ux, int uy, int &cx, int &cy)
{
	bool hit = true;;
	LOG_DEBUG_(1, _T("ptr: (%d, %d)"), ux, uy)

	int yy = (int)(uy / (1.5 * m_ca));
	int xx = (int)(ux / m_cr);
	if (yy & 1)
	{
		if (xx == 0) hit = false;
		cx = (xx-1) /2;
	}
	else		cx = xx /2;
	LOG_DEBUG_(1, _T("xx=%d, cx=%d"), xx, cx);
	cy = yy;

	if (cx < 0 || cx >= BOARD_SIZE_COL) hit = false;
	if (cy < 0 || cy >= BOARD_SIZE_ROW) hit = false;

	return hit;
}

void CChessBoardUi::Board2Gui(char col, char row, int & ux, int & uy)
{
	if (row & 1)	ux = (int)(2 * m_cr + col * 2 * m_cr);
	else			ux = (int)(m_cr + col * 2 * m_cr);
	uy = (int)(m_ca + row * 1.5 * m_ca);
}

BEGIN_MESSAGE_MAP(CChessBoardUi, CStatic)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

// CChessBoardUi 消息处理程序
int CChessBoardUi::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	LOG_STACK_TRACE();
	int ir = CWnd::OnCreate(lpCreateStruct);
	if (ir != 0) return ir;

	CPaintDC dc(this);
	return ir;
}

void CChessBoardUi::Initialize(CDC * dc)
{
	JCASSERT(dc);

	GetClientRect(&m_client_rect);
	LOG_DEBUG(_T("client rect: (%d,%d) - (%d,%d)"), m_client_rect.left, m_client_rect.top, m_client_rect.right, m_client_rect.bottom);

	m_memdc.CreateCompatibleDC(dc);
	m_membitmap.CreateCompatibleBitmap(dc, m_client_rect.Width(), m_client_rect.Height() );
	m_memdc.SelectObject(&m_membitmap);

	// prepare drawing object
	m_pen_blue.CreatePen(PS_SOLID | PS_COSMETIC, 1, RGB(0, 0, 255) );
	m_pen_red.CreatePen(PS_SOLID | PS_COSMETIC, 1, RGB(255, 0, 0) );
	m_brush_blue.CreateSolidBrush(RGB(0, 0, 255));
	m_brush_red.CreateSolidBrush(RGB(255, 0, 0));
	m_brush_white.CreateSolidBrush(RGB(255, 255, 255));
	m_pen_path.CreatePen(PS_SOLID | PS_COSMETIC, 3, RGB(0, 255, 0) );

	m_init = true;
	Draw(0, 0, 0);
}

void CChessBoardUi::Draw(int level, char col, char row)
{
	if (!m_chess_board) return;
	// draw back ground
	m_memdc.FillSolidRect(&m_client_rect, GetSysColor(COLOR_3DFACE) );

	// draw cells
	float dx, dy;
	dy = m_ca;
	for ( int ii = 0; ii < BOARD_SIZE_ROW; ++ii)
	{
		if (ii & 1)	dx = 2 * m_cr;
		else		dx = m_cr;
		int y0 = (int)(dy - m_cr);
		int y1 = (int)(dy + m_cr);

		for ( int jj = 0; jj < BOARD_SIZE_COL; ++jj)
		{
			int x0 = (int)(dx - m_cr);
			int x1 = (int)(dx + m_cr);

			BYTE c = m_chess_board->CheckPosition(jj, ii);
			switch (c)
			{
			case 0:	// empty
				if ( (m_hit) && (ii == m_cy) && (jj == m_cx) )		m_memdc.SelectObject(&m_pen_red);
				else						m_memdc.SelectObject(&m_pen_blue);
				m_memdc.SelectObject(&m_brush_white);
				break;
			case 1: // blue
				m_memdc.SelectObject(&m_pen_blue);
				m_memdc.SelectObject(&m_brush_blue);
				break;
			case 2: // red
				m_memdc.SelectObject(&m_pen_red);
				m_memdc.SelectObject(&m_brush_red);
				break;
			}			
			m_memdc.Ellipse(x0, y0, x1, y1);
			dx += 2 * m_cr;
		}
		dy += (float)1.5 * m_ca;
	}

	if (m_draw_path  && m_evaluate)	DrawPath(&m_memdc);
	if (m_draw_search && m_evaluate) DrawSearch(&m_memdc);

	RedrawWindow();
}

void CChessBoardUi::DrawPath(CDC * dc)
{
	JCASSERT(m_evaluate);
	dc->SelectObject(&m_pen_path);
	
	int ix, iy;

	CSearchNode * node = m_evaluate->GetSuccess();
	if (node)	
	{
		Board2Gui(node->m_cat_col, node->m_cat_row, ix, iy);
		dc->MoveTo(ix, iy);
		node = node->m_father;
	}

	while (node)
	{
		Board2Gui(node->m_cat_col, node->m_cat_row, ix, iy);
		dc->LineTo(ix, iy);
		node = node->m_father;
	}
}

void CChessBoardUi::DrawSearch(CDC * dc)
{
	JCASSERT(m_evaluate);
	CSearchNode * node = m_evaluate->GetHead();

	UINT ii = 0;

	while (node)
	{
		int ix, iy;
		char col = node->m_cat_col, row = node->m_cat_row;
		Board2Gui(col, row, ix, iy);
		LOG_DEBUG(_T("(%d, %d) : (%d, %d)"), col, row, ix, iy);
		if (col >= 0 && row >= 0)
		{
			if (ii < m_evaluate->m_closed_qty)	
				dc->FillSolidRect(ix-3, iy-3, 6, 6, RGB(128, 0, 0));
			else	
				dc->FillSolidRect(ix-3, iy-3, 6, 6, RGB(128, 64, 64));
		}
		ii ++;
		node = node->m_next;
	}
}


void CChessBoardUi::OnLButtonUp(UINT flag, CPoint point)
{
	int col, row;
	bool hit = Hit(point.x, point.y, col, row);
	if (hit) GetParent()->SendMessage(WM_MSG_CLICKCHESS, MAKEWORD(col, row));

	/*
	bool br = false;
	switch (m_chess_board->Status())
	{
	case PS_SETTING:	{
		BYTE ss = m_chess_board->CheckPosition(cx, cy);
		if ( 1 == ss ) ss = 0;
		else if ( 0 == ss ) ss = 1;
		else return;
		m_chess_board->SetPosition(cx, cy, ss);
		br = true;
		Draw(0, 0, 0);
		break;				}

	case PS_CATCHER_MOVE:
		if (!m_catcher_ai)
		{
			br = m_chess_board->Move(PLAYER_CATCHER, cx, cy);
			if (br)		
			{
				SearchCatEscape();
				PostMessage(WM_MSG_COMPLETEMOVE, 0, 0);
			}
		}
		break;

	case PS_CAT_MOVE:
		if (!m_cat_robot) 
		{	// cat是player
			br = m_chess_board->Move(PLAYER_CAT, cx, cy);
			if (br)
			{
				SearchCatEscape();
				PostMessage(WM_MSG_COMPLETEMOVE, 0, 0);
			}
		}
		break;
	}
	*/
}

/*
LRESULT CChessBoardUi::OnRobotMove(WPARAM wp, LPARAM lp)
{
	LOG_STACK_TRACE();
	if (wp == PLAYER_CAT)
	{	// AI CAT下棋
		JCASSERT(m_cat_robot);
		m_cat_robot->StartSearch(m_chess_board, 0);
	}
	else
	{	// AI CATCHER下棋
		JCASSERT(m_catcher_robot);
		m_catcher_robot->StartSearch(m_chess_board, m_search_depth);
	}
	return 0;
}

LRESULT CChessBoardUi::OnCompleteMove(WPARAM wp, LPARAM lp)
{
	// PLAYER或者AI下完棋，更新状态。此函数一定在GUI线程中运行
	LOG_STACK_TRACE();

	JCASSERT(m_chess_board);

	PLAY_STATUS ss = m_chess_board->Status();

	CJCStringT str;
	switch (ss)
	{
	case PS_SETTING:		str = _T("setting");	break;
	case PS_CATCHER_MOVE:	
		str = _T("catcher moving");	
		// 轮到CATCHER下棋，如果是AI，则消息启动AI下棋。否则返回等待PLAYER下棋。
		if (m_catcher_robot) PostMessage(WM_MSG_ROBOTMOVE, PLAYER_CATCHER, 0);
		break;

	case PS_CAT_MOVE:		
		str = _T("cat moving");	
		// 轮到CAT下棋，如果是AI，则消息启动AI下棋。否则返回等待PLAYER下棋。
		if (m_cat_robot)	PostMessage(WM_MSG_ROBOTMOVE, PLAYER_CAT, 0);
		break;
	case PS_CATCHER_WIN:	str = _T("catcher win");	break;
	case PS_CAT_WIN:		str = _T("cat win");	break;
	}
	if (m_listen) m_listen->SetTextStatus(str);
	Draw(0, 0, 0);
	return 0;
}
*/

//void CChessBoardUi::UpdateStatus(PLAY_STATUS ss)
//{
//	LOG_STACK_TRACE();
//
//	if (!m_listen) return;
//	CJCStringT str;
//	switch (ss)
//	{
//	case PS_SETTING:		str = _T("setting");	break;
//	case PS_CATCHER_MOVE:	str = _T("catcher moving");	break;
//	case PS_CAT_MOVE:		
//		str = _T("cat moving");	
//		if (m_cat_robot) 
//		{	// 轮到Cat走棋
//			m_cat_robot->StartSearch(m_chess_board, 0);
//		}
//		break;
//	case PS_CATCHER_WIN:	str = _T("catcher win");	break;
//	case PS_CAT_WIN:		str = _T("cat win");	break;
//	}
//	if (m_listen) m_listen->SetTextStatus(str);
//}

//void CChessBoardUi::SearchCompleted(MOVEMENT * move)
//{
//	// AI搜索完博弈树后，回叫此函数。此函书可能在后台线程中运行。!!注意同步!!
//	LOG_STACK_TRACE();
//	CCrazyCatMovement * mv = reinterpret_cast<CCrazyCatMovement*>(move);
//	switch (m_chess_board->Status() )
//	{
//	case PS_CAT_MOVE:
//		if (mv->m_col < 0 && mv->m_row < 0)		m_chess_board->GiveUp(PLAYER_CAT);	// 认输
//		else
//		{
//			bool br = m_chess_board->Move(PLAYER_CAT, mv->m_col, mv->m_row);
//			JCASSERT(br);
//			SearchCatEscape();
//		}
//		break;
//
//	case PS_CATCHER_MOVE:
//		if (mv->m_col < 0 && mv->m_row < 0)		m_chess_board->GiveUp(PLAYER_CATCHER);	// 认输
//		else
//		{
//			bool br = m_chess_board->Move(PLAYER_CATCHER, mv->m_col, mv->m_row);
//			JCASSERT(br);
//			TCHAR str[256];
//			_stprintf_s(str, _T("searched node: %d\n"), m_catcher_robot->m_node);
//			if (m_listen) m_listen->SendTextMsg(str);
//			SearchCatEscape();	// 搜索CAT的逃逸路经
//		}
//		break;
//	}
//	PostMessage(WM_MSG_COMPLETEMOVE, 0, 0);
//}

void CChessBoardUi::OnMouseMove(UINT flag, CPoint point)
{
	//LOG_STACK_TRACE();
	m_hit = Hit(point.x, point.y, m_cx, m_cy);
	LOG_DEBUG_(1, _T("(%d, %d), %d"), point.x, point.y, m_hit);
	if (m_hit) Draw(0,0,0);
}

void CChessBoardUi::OnPaint(void)
{
	//LOG_STACK_TRACE();
	if (!m_chess_board) return;

	CPaintDC dc(this);
	if (!m_init) Initialize(&dc);
	dc.BitBlt(0, 0, m_client_rect.Width(), m_client_rect.Height(), &m_memdc, 0, 0, SRCCOPY);
}

//void CChessBoardUi::SearchCatEscape(void)
//{
//
//}

//void CChessBoardUi::StartPlay(bool player_catcher, bool player_cat, int search_depth)
//{
//	//<TODO>
//	delete m_cat_robot;
//	m_cat_robot = NULL;
//	if (player_cat)
//	{
//		m_cat_robot = new CRobotCat(static_cast<IRefereeListen*>(this) );
//	}
//
//	delete m_catcher_robot;
//	if (player_catcher)
//	{
//		m_catcher_robot = new CRobotCatcher(static_cast<IRefereeListen*>(this) );
//	}
//
//	m_search_depth = search_depth;
//
//	PLAY_STATUS ps = m_chess_board->StartPlay();
//	PostMessage(WM_MSG_COMPLETEMOVE, 0, 0);
//}

//void CChessBoardUi::Undo(void)
//{
//	bool br=m_chess_board->Undo();
//	if (!br && m_listen) m_listen->SendTextMsg(_T("Cannot undo!\n"));
//	SearchCatEscape();
//	Draw(0, 0, 0);
//}

///////////////////////////////////////////////////////////////////////////////
// -- 棋盘类，

const UINT CChessBoard::RANDOM_TAB[BOARD_SIZE_COL][BOARD_SIZE_ROW] = {
	{0xE00F4349,0x788BC7B2,0x02953C3F,0x141C59A1,0x76BA845A,0xA737A9A6,0x59E4D6EE,0xBA3D7159,0xBD7E54C1,},
	{0x3629CE90,0x7EF8583A,0x3FCC6B19,0x21E6532C,0xAE43C6F6,0xDBFB6226,0x87020846,0xCA482635,0xED98E112,},
	{0xEE5AE2CF,0x7D4D40B5,0x31571457,0xCB8FEC16,0x44815AC7,0x0121E569,0x00470B7C,0x39CE56FD,0x808C5AAF,},
	{0xBAC240F8,0x12F6F309,0xD9AE0355,0x6FEDA851,0xCF9E0D7C,0xBC79580C,0x4D6D2965,0x21389F28,0x2F63D303,},
	{0xCE51FE15,0x3AD35972,0x5DCDA559,0xBF66B769,0xA7046E4E,0xABA65CA2,0x61F08548,0xD8F75F07,0x55F743AE,},
	{0xD6305AE6,0xC570690C,0x88AC24CB,0xE67EE534,0xCE61FBD3,0x62840D78,0x3125545A,0xE32F31AD,0xF5932E9B,},
	{0x2743E2BB,0x93E8B0A5,0x3DD0E9B3,0x8006DCBA,0x92D0A520,0xE020AEA9,0x3F6DA67F,0x23069120,0x87BADD61,},
	{0xD5D48D91,0x21640FC3,0x61085B71,0xACEB0966,0xB01C1A24,0xE85C8463,0xD690DC3D,0x9F9634BF,0xD7127BD9,},
	{0x4D53E172,0xC93281F2,0x993BFFC2,0xCF9FA26C,0xF4262742,0x1027F97A,0xA32EA602,0xB07475F5,0xB4620A00,},
};


CChessBoard::CChessBoard(void)
	: m_status(PS_SETTING)
{
	memset(m_board, 0, sizeof(m_board));

	m_cat_col = 4;
	m_cat_row = 4;

	m_board[m_cat_col][m_cat_row] = 2;
	//m_move_col = -1, m_move_row = -1;
}

CChessBoard::~CChessBoard(void)
{
}


void CChessBoard::GetCatPosition(char & col, char & row) const
{
	col = m_cat_col;
	row = m_cat_row;
}

BYTE CChessBoard::CheckPosition(char col, char row) const
{
	if (col < 0 || row < 0 || col >= BOARD_SIZE_COL || row >= BOARD_SIZE_ROW)	
		return 0xFF;	// out of range
	return m_board[col][row];
}

void CChessBoard::SetPosition(char col, char row, BYTE ss)
{
	if (col < 0 || row < 0 || col >= BOARD_SIZE_COL || row >= BOARD_SIZE_ROW)	
		return;
	m_board[col][row] = ss;
}

PLAY_STATUS CChessBoard::StartPlay(void)
{
	m_status = PS_CATCHER_MOVE;
	return m_status;
}

bool CChessBoard::Undo(void)
{
	//JCASSERT(m_move_col >= 0 && m_move_row >=0 );
	if (m_recorder.empty() ) return false;
	//if (m_move_col < 0 || m_move_row < 0) return false;

	CCrazyCatMovement mv = m_recorder.back();

	if (m_status == PS_CAT_MOVE || m_status == PS_CATCHER_WIN)
	{
		m_status = PS_CATCHER_MOVE;
		m_board[mv.m_col][mv.m_row] = 0;
	}
	else if (m_status == PS_CATCHER_MOVE || m_status == PS_CAT_WIN)
	{
		m_status = PS_CAT_MOVE;
		m_board[m_cat_col][m_cat_row] = 0;
		m_board[mv.m_col][mv.m_row] = 2;
		m_cat_col = mv.m_col, m_cat_row = mv.m_row;
	}
	//m_move_col = -1, m_move_row = -1;
	m_recorder.pop_back();
	return true;
}

bool CChessBoard::Move(BYTE player, int col, int row) 
{
	if (col < 0 || col >= BOARD_SIZE_COL || row < 0 || row >= BOARD_SIZE_ROW) return false;
	if (m_board[col][row] != 0)	return false;

	if (player == PLAYER_CATCHER)
	{
		if (m_status != PS_CATCHER_MOVE) return false;
		m_board[col][row] = 1;
		m_recorder.push_back(CCrazyCatMovement(col, row));
		//m_move_col = col, m_move_row = row;
		if ( IsCatcherWin() )	m_status = PS_CATCHER_WIN;
		else					m_status = PS_CAT_MOVE;
	}
	else
	{
		if (m_status != PS_CAT_MOVE) return false;
		// 检查是否在周围六格之内
		int ww = 0;
		for (; ww < MAX_MOVEMENT; ++ww)
		{
			char c1, r1;
			::Move(m_cat_col, m_cat_row, ww, c1, r1);
			if (c1 == col && r1 == row) break;
		}
		if (ww >= MAX_MOVEMENT) return false;
		m_board[m_cat_col][m_cat_row] = 0;
		m_board[col][row] = 2;
		// backup for undo
		m_recorder.push_back(CCrazyCatMovement(m_cat_col, m_cat_row));
		//m_move_col = m_cat_col, m_move_row = m_cat_row;
		m_cat_col = col, m_cat_row = row;
		
		if ( IsCatWin() )		m_status = PS_CAT_WIN;
		else					m_status = PS_CATCHER_MOVE;
	}
	return true;
}

bool CChessBoard::IsCatcherWin(void)
{
	for (int ww = 0; ww < MAX_MOVEMENT; ++ww)
	{
		char c1, r1;
		::Move(m_cat_col, m_cat_row, ww, c1, r1);
		if (m_board[c1][r1] != 1) return false;
	}
	return true;
}

bool CChessBoard::IsCatWin(void)
{
	if (m_cat_col == 0 || m_cat_col == (BOARD_SIZE_COL-1) ) return true;
	if (m_cat_row == 0 || m_cat_row == (BOARD_SIZE_ROW-1) ) return true;
	return false;
}

// 复制棋盘，用于博弈树搜索等。
void CChessBoard::Dupe(CChessBoard * & board) const
{
	JCASSERT(NULL == board);
	board = new CChessBoard;
	memcpy_s(board->m_board, sizeof(m_board), m_board, sizeof(board->m_board));
	board->m_cat_col = m_cat_col;
	board->m_cat_row = m_cat_row;
	board->m_status = m_status;
}

UINT CChessBoard::MakeHash(void)	const
{
	UINT checksum = 0;
	for (char rr = 0; rr < BOARD_SIZE_ROW; ++rr)
	{
		for (char cc = 0; cc < BOARD_SIZE_COL; ++cc)
		{
			if (m_board[cc][rr] ==1 )	checksum ^= RANDOM_TAB[cc][rr];
		}
	}
	checksum ^= (RANDOM_TAB[m_cat_col][m_cat_row] << 1);
	return checksum;
}
