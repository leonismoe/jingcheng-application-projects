// ChessBoard.cpp : 实现文件
//

#include "stdafx.h"
#include "CrazyCat.h"
#include "ChessBoard.h"
#include "robot_cat.h"
#include "robot_catcher.h"

#include <stdext.h>

LOCAL_LOGGER_ENABLE(_T("board"), LOGGER_LEVEL_NOTICE);

// CChessBoardUi

IMPLEMENT_DYNAMIC(CChessBoardUi, CStatic)

CChessBoardUi::CChessBoardUi()
	: m_init(false)
	, m_chess_board(NULL)
	, m_hit(false), m_cx(0), m_cy(0)
	, m_evaluate(NULL)
	, m_draw_path(true), m_draw_search(false)
{
	m_cs = CHESS_RADIUS;
	m_ca = (float)1.155 * m_cs;		// 2/squr(3)
	m_cr = CHESS_RADIUS;
	m_evaluate = new CCrazyCatEvaluator;
}

CChessBoardUi::~CChessBoardUi()
{
	delete m_evaluate;
}

void CChessBoardUi::SetBoard(CChessBoard * board)
{
	JCASSERT(board);
	m_chess_board = board;
}

bool CChessBoardUi::Hit(int ux, int uy, int &cx, int &cy)
{
	bool hit = true;;
	LOG_DEBUG_(1, _T("ptr: (%d, %d)"), ux, uy)

	int yy = (int)(uy / (1.5 * m_ca));
	int xx = (int)(ux / m_cs);
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
	if (row & 1)	ux = (int)(2 * m_cs + col * 2 * m_cs);
	else			ux = (int)(m_cs + col * 2 * m_cs);
	uy = (int)(m_ca + row * 1.5 * m_ca);
}

BEGIN_MESSAGE_MAP(CChessBoardUi, CStatic)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONUP()
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
	//LOG_STACK_TRACE();
	if (!m_chess_board) return;
	// draw back ground
	m_memdc.FillSolidRect(&m_client_rect, GetSysColor(COLOR_3DFACE) );

	// draw cells
	float dx, dy;
	dy = m_ca;
	for ( int ii = 0; ii < BOARD_SIZE_ROW; ++ii)
	{
		if (ii & 1)	dx = 2 * m_cs;
		else		dx = m_cs;
		int y0 = (int)(dy) - m_cr;
		int y1 = (int)(dy) + m_cr;

		for ( int jj = 0; jj < BOARD_SIZE_COL; ++jj)
		{
			int x0 = (int)(dx) - m_cr;
			int x1 = (int)(dx) + m_cr;

			BYTE c = m_chess_board->CheckPosition(jj, ii);
			switch (c)
			{
			case 0:	// empty
				//if ( (m_hit) && (ii == m_cy) && (jj == m_cx) )		m_memdc.SelectObject(&m_pen_red);
				//else						m_memdc.SelectObject(&m_pen_blue);
				m_memdc.SelectObject(&m_pen_blue);
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
			dx += 2 * m_cs;
		}
		dy += (float)1.5 * m_ca;
	}

	// draw last movement
	CCrazyCatMovement mv;
	if ( m_chess_board->GetLastMovement(mv) )
	{
		int ix, iy;
		Board2Gui(mv.m_col, mv.m_row, ix, iy);
		if (mv.m_player == PLAYER_CATCHER)	m_memdc.SelectObject(&m_brush_blue);
		else								m_memdc.SelectObject(&m_brush_white);
		m_memdc.SelectObject(&m_pen_red);
		m_memdc.Ellipse((int)(ix) - m_cr, (int)(iy) - m_cr, (int)(ix) + m_cr, (int)(iy) + m_cr);
	}

	if (m_draw_path || m_draw_search)
	{
		JCASSERT(m_chess_board);
		m_evaluate->Reset(m_chess_board);
		int ir = m_evaluate->StartSearch();
		// output log
		if (ir < MAX_DISTANCE)
		{
			LOG_DEBUG_(1, _T("route found. depth = %d, expanded %d, closed %d"),
				ir, m_evaluate->m_open_qty, m_evaluate->m_closed_qty);
		}
		else
		{
			LOG_DEBUG_(1, _T("no route found. expanded %d, closed %d"),
				m_evaluate->m_open_qty, m_evaluate->m_closed_qty);
		}
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
	if (hit) GetParent()->SendMessage(WM_MSG_CLICKCHESS, MAKEWORD(col, row), CLICKCHESS_LEFT);
}

void CChessBoardUi::OnRButtonUp(UINT flag, CPoint point)
{
	int col, row;
	bool hit = Hit(point.x, point.y, col, row);
	if (hit) GetParent()->SendMessage(WM_MSG_CLICKCHESS, MAKEWORD(col, row), CLICKCHESS_RIGHT);
}

void CChessBoardUi::OnMouseMove(UINT flag, CPoint point)
{
	//LOG_STACK_TRACE();
	int col = 0, row = 0;
	m_hit = Hit(point.x, point.y, col, row);
	LOG_DEBUG_(1, _T("(%d, %d), %d"), point.x, point.y, m_hit);
	if (m_hit && (col != m_cx || row != m_cy) )
	{
		m_cx = col, m_cy = row;
		Draw(0,0,0);
		GetParent()->SendMessage(WM_MSG_CLICKCHESS, MAKEWORD(col, row), CLICKCHESS_MOVE);
	}
}

void CChessBoardUi::OnPaint(void)
{
	//LOG_STACK_TRACE();
	if (!m_chess_board) return;

	CPaintDC dc(this);
	if (!m_init) Initialize(&dc);
	dc.BitBlt(0, 0, m_client_rect.Width(), m_client_rect.Height(), &m_memdc, 0, 0, SRCCOPY);
}

///////////////////////////////////////////////////////////////////////////////
// -- 棋盘类，

LOG_CLASS_SIZE(CChessBoard);

LPCTSTR CChessBoard::PLAYER_NAME[3] = {
	_T("wrong player"),		_T("catcher"),		_T("cat"),
};

LPCTSTR CChessBoard::PLAYER_SNAME[3] = {
	_T("***"),		_T("CCH"),		_T("CAT"),
};


const UINT CChessBoard::RANDOM_TAB_CATCHER[BOARD_SIZE_COL][BOARD_SIZE_ROW] = {
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

const UINT CChessBoard::RANDOM_TAB_CAT[BOARD_SIZE_COL][BOARD_SIZE_ROW] = {
	{0x5E424F08,0x578CAA52,0xF46652F1,0x092F2C7B,0x766D9830,0xAA5E822D,0xE158A899,0x51E4471E,0x3DBEE10D,},
	{0x9835C581,0x92F00B5F,0x12AAF55C,0x75EA7DA6,0x99B55E0E,0xA34AE737,0x1E45A2E9,0xFEA308B8,0x4DCC47D4,},
	{0xA5FF39AA,0xCC1E1702,0xDE8B0A29,0x87B20F8F,0xEDC874C8,0xF8BEE518,0x352830EF,0xCD57C8CD,0x8926D25C,},
	{0xC1EF9C45,0xA2B982D7,0xF83CE347,0x3BEA2BD4,0xA76143AF,0xDCD3CBA3,0xCD2507FC,0xAF9F389F,0x5FE808BE,},
	{0x3841FBAC,0x0C858CBF,0xDA1F56AA,0x0B03BEBD,0x042FF554,0x714B09C9,0x7E6AC831,0x90BB6494,0x794C913F,},
	{0xCD9A4CAB,0x8D429BA7,0xDC52D8C1,0x8FACDC26,0x50FF02A7,0x298751A7,0xE4643F3A,0x71DF7A62,0x1F7959B0,},
	{0xBFB31698,0x2D2BE899,0xDD6C383A,0x039EC2A8,0x27FB6D4F,0xC12D40CC,0x43103849,0x8221297D,0x2162FC03,},
	{0x361EFEB6,0x9C171C03,0x764E96F8,0xD009E90D,0x1D17654A,0x956238CE,0xC5793554,0xD003E9BF,0x40D35A30,},
	{0x592EEECA,0x4F320B45,0xBF1DFC53,0x0FA761F5,0xEE92D5C7,0x0EA8540F,0xBB492D92,0xD2A7B458,0x228EAD49,},
};

CChessBoard::CChessBoard(void)
: m_eval(NULL)
	//: m_status(PS_SETTING)
{
	memset(m_board, 0, sizeof(m_board));

	m_cat_col = 4;
	m_cat_row = 4;

	m_board[m_cat_col][m_cat_row] = 2;
	m_stack_point = 0;
	m_stack_top = 0;
	m_turn = PLAYER_CATCHER;

	m_eval = new CCrazyCatEvaluator;
}

CChessBoard::~CChessBoard(void)
{
	delete m_eval;
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

bool CChessBoard::SetChess(BYTE chess, char col, char row)
{
	if (col < 0 || row < 0 || col >= BOARD_SIZE_COL || row >= BOARD_SIZE_ROW)	return false;
	if (chess == PLAYER_CATCHER)
	{	// 反转当前棋子
		if (m_board[col][row] == 1)		m_board[col][row] = 0;
		else if (m_board[col][row] == 0)	m_board[col][row] = 1;
		return false;
	}
	else if (chess == PLAYER_CAT)
	{	// 移动棋子
		if (m_board[col][row] != 0)	return false;
		m_board[col][row] = 2;
		m_board[m_cat_col][m_cat_row] =0;
		m_cat_col = col, m_cat_row = row;
	}
	return true;
}

bool CChessBoard::Undo(void)
{
	if (m_stack_point == 0) return false;
	m_stack_point --;
	
	CCrazyCatMovement & mv = m_recorder[m_stack_point];

	if (mv.m_player == PLAYER_CATCHER)
	{
		m_turn = PLAYER_CATCHER;
		m_board[mv.m_col][mv.m_row] = 0;
	}
	else
	{	// 由于CAT的stack中记录的是前一步的位置，因此undo以后，要把stack中的记录改成当前位置
		m_turn = PLAYER_CAT;
		char cc = mv.m_col, rr = mv.m_row;
		m_board[m_cat_col][m_cat_row] = 0;
		m_board[cc][rr] = PLAYER_CAT;
		mv.m_col = m_cat_col, mv.m_row = m_cat_row;
		m_cat_col = cc, m_cat_row = rr;
	}
	return true;
}

bool CChessBoard::Redo(void)
{
	if (m_stack_point >= m_stack_top) return false;
	CCrazyCatMovement & mv = m_recorder[m_stack_point];

	if (mv.m_player == PLAYER_CATCHER)
	{
		m_board[mv.m_col][mv.m_row] = PLAYER_CATCHER;
		m_turn = PLAYER_CAT;
	}
	else
	{	// 由于CAT的stack中记录的是前一步的位置，因此undo以后，要把stack中的记录改成当前位置
		char cc = mv.m_col, rr = mv.m_row;
		m_board[m_cat_col][m_cat_row] = 0;
		m_board[cc][rr] = PLAYER_CAT;
		mv.m_col = m_cat_col, mv.m_row = m_cat_row;
		m_cat_col = cc, m_cat_row = rr;
		m_turn = PLAYER_CATCHER;
	}

	m_stack_point ++;
	return true;
}


bool CChessBoard::IsValidMove(CCrazyCatMovement * mv)
{
	char col = mv->m_col, row = mv->m_row;
	if (col < 0 || col >= BOARD_SIZE_COL || row < 0 || row >= BOARD_SIZE_ROW) return false;
	if (m_board[col][row] != 0)	return false;
	if ( m_turn != mv->m_player ) return false;
	if (mv->m_player == PLAYER_CAT)
	{	// CAT只能走一步，检查是否在周围六格之内
		int ww = 0;
		for (; ww < MAX_MOVEMENT; ++ww)
		{
			char c1, r1;
			::Move(m_cat_col, m_cat_row, ww, c1, r1);
			if (c1 == col && r1 == row) break;
		}
		if (ww >= MAX_MOVEMENT) return false;
	}
	return true;
}

bool CChessBoard::Move(CCrazyCatMovement * mv/*BYTE player, int col, int row*/) 
{
	char col = mv->m_col, row = mv->m_row;
	JCASSERT(col >= 0 && col < BOARD_SIZE_COL && row >= 0 && row < BOARD_SIZE_ROW);
	JCASSERT(0 == m_board[col][row]);
	// correct turn
	JCASSERT( m_turn == mv->m_player );

	if (mv->m_player == PLAYER_CATCHER)
	{
		m_board[col][row] = 1;
		PushMovement(*mv);
		m_turn = PLAYER_CAT;
	}
	else
	{
		m_board[m_cat_col][m_cat_row] = 0;
		m_board[col][row] = 2;
		// backup for undo
		PushMovement(CCrazyCatMovement(m_cat_col, m_cat_row, PLAYER_CAT));
		m_cat_col = col, m_cat_row = row;
		m_turn = PLAYER_CATCHER;
	}
	return true;
}

void CChessBoard::PushMovement(const CCrazyCatMovement & mv)
{
	JCASSERT(m_stack_point < STACK_SIZE);
	m_recorder[m_stack_point] = mv;
	m_stack_point ++;
	// 由于重新压栈，清除其后所有内容
	m_stack_top = m_stack_point;
}
	
bool CChessBoard::IsCatcherWin(void)
{
	//for (int ww = 0; ww < MAX_MOVEMENT; ++ww)
	//{
	//	char c1, r1;
	//	::Move(m_cat_col, m_cat_row, ww, c1, r1);
	//	if (m_board[c1][r1] != 1) return false;
	//}
	// 注意！线程不安全
	//CCrazyCatEvaluator eval;
	m_eval->Reset(this);
	if (m_eval->StartSearch() >= MAX_DISTANCE) return true;
	else return false;
}

bool CChessBoard::IsCatWin(void)
{
	if (m_cat_col == 0 || m_cat_col == (BOARD_SIZE_COL-1) ) return true;
	if (m_cat_row == 0 || m_cat_row == (BOARD_SIZE_ROW-1) ) return true;
	return false;
}

bool CChessBoard::IsWin(PLAYER & player)
{
	if (IsCatWin() )	{player = PLAYER_CAT; return true;}
	else if (IsCatcherWin() ) {player = PLAYER_CATCHER; return true;}
	else return false;
}

bool CChessBoard::IsWinPlayer(const PLAYER player)
{
	if (player == PLAYER_CAT)			return IsCatcherWin();
	else if (player == PLAYER_CATCHER)	return IsCatWin();
	return false;
}

bool CChessBoard::GetLastMovement(CCrazyCatMovement & mv)
{
	if (m_stack_point == 0) return false;
	mv = m_recorder[m_stack_point -1];
	return true;
}

// 复制棋盘，用于博弈树搜索等。
void CChessBoard::Dupe(CChessBoard * & board) const
{
	JCASSERT(NULL == board);
	board = new CChessBoard;
	memcpy_s(board->m_board, sizeof(m_board), m_board, sizeof(board->m_board));
	board->m_cat_col = m_cat_col;
	board->m_cat_row = m_cat_row;
	board->m_turn = m_turn;
}

UINT CChessBoard::MakeHash(void)	const
{
	UINT checksum = 0;
	for (char rr = 0; rr < BOARD_SIZE_ROW; ++rr)
	{
		for (char cc = 0; cc < BOARD_SIZE_COL; ++cc)
		{
			if (m_board[cc][rr] == PLAYER_CATCHER )	checksum ^= RANDOM_TAB_CATCHER[cc][rr];
		}
	}
	checksum ^= (RANDOM_TAB_CAT[m_cat_col][m_cat_row]);
	return checksum;
}

void CChessBoard::SaveToFile(FILE * file) const
{
	JCASSERT(file);
	for (char rr = 0; rr < BOARD_SIZE_ROW; ++rr)
	{
		for (char cc = 0; cc < BOARD_SIZE_COL; ++cc)
		{
			if (m_board[cc][rr] != 0)	fprintf_s(file,("set,%d,(%d,%d)\n"), m_board[cc][rr], cc, rr);
		}
	}

	for (JCSIZE ii = 0; ii < m_stack_top; ++ ii)
	{
		const CCrazyCatMovement & mv = m_recorder[ii];
		fprintf_s(file, "mov,%d,(%d,%d)\n", mv.m_player, mv.m_col, mv.m_row);
	}
}

#define LINE_SIZE	128

bool CChessBoard::LoadFromFile(FILE * file)
{
	// must be reset first by ui
	JCASSERT(file);
	char buf[LINE_SIZE];
	char verb[LINE_SIZE];
	int player, cc, rr;
	int line = 0;

	while ( fgets(buf, LINE_SIZE, file) )
	{
		line ++;
		int ir = sscanf(buf, "%3s,%d,(%d,%d)", verb, &player, &cc, &rr);
		//if (ir < 4) THROW_ERROR(ERR_USER, _T("unknow format in line %d"), line);
		if (ir < 4)
		{
			LOG_ERROR(_T("unknow format in line %d"), line);
			continue;
		}

		if ( strcmp(verb, "set") ==0 )		SetChess(player, cc, rr);
		else if (strcmp(verb, "mov") ==0 )
		{/*m_recorder.push_back( CCrazyCatMovement(cc, rr, player));*/
			PushMovement(CCrazyCatMovement(cc, rr, player));
		}
		else LOG_ERROR(_T("unknow verb %s in line %d"), verb, line);
	}
	return true;
}