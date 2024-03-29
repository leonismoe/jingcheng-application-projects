﻿// Author:      SFr frydaysoft@gmx.de

#include "stdafx.h"
#include "../res/resource.h"

#include "UpdateSnTool.h"
#include "ColorListCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#ifndef nColorListCtrlCols
	#define nColorListCtrlCols 10
#endif

//TCHAR ColsName[nColorListCtrlCols][32];
/////////////////////////////////////////////////////////////////////////////
// CColorListCtrl

CColorListCtrl::CColorListCtrl(int colSize)
{
    m_columnSize=colSize;       // number of columns
    m_fullColumnLines=TRUE;     // paint vert. lines | | |
    m_withSingleRect=FALSE;     // paint single rect for all sub-items
#ifdef _DEBUG
    m_BigFocusRect=TRUE;        // focus-rect with small/big dots
#endif
#ifndef _DEBUG
    m_BigFocusRect=FALSE;       // focus-rect with small/big dots
#endif
    IsCreated=FALSE;
}

//***************************************************************
CColorListCtrl::~CColorListCtrl()
{
}

//***************************************************************
BEGIN_MESSAGE_MAP(CColorListCtrl, CListCtrl)
	//{{AFX_MSG_MAP(CColorListCtrl)
	ON_WM_MEASUREITEM_REFLECT( )
	ON_MESSAGE(WM_SETFONT, OnSetFont)
	ON_WM_PAINT()
	ON_WM_DESTROY()
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen f・ Nachrichten CColorListCtrl 
void CColorListCtrl::OnDestroy() 
{
	DeleteAllItems();	
	CListCtrl::OnDestroy();
}

//***************************************************************
void CColorListCtrl::SetColumnType(BOOL bNew)
{
    m_fullColumnLines=bNew;     // paint no/vert. lines | | |
    if (IsCreated) 
    {
        RedrawWindow();
    }
}
void CColorListCtrl::SetRectType(BOOL bNew)
{
    m_withSingleRect=bNew;     // paint no/single rect for all sub-items
    if (IsCreated)
    {
        RedrawWindow();
    }
}
void CColorListCtrl::SetFocusType(BOOL bNew)
{
    m_BigFocusRect=bNew;       // focus-rect with small/big dots
    if (IsCreated)
    {
        RedrawWindow();
    }
}

//***************************************************************
CMyLParam*	CColorListCtrl::GetMyLParam(int Index)
{
    LVITEM	Item;
	Item.mask		= LVIF_PARAM;
	Item.iItem		= Index;
	Item.iSubItem	= 0;
	CListCtrl::GetItem(&Item);
	return (CMyLParam*)Item.lParam;
}

//***************************************************************
CRect CColorListCtrl::GetFullCellRect(int nItem, BOOL outside)
{
	static CRect rectCell;
    CListCtrl::GetItemRect(nItem, rectCell, LVIR_BOUNDS);
    if (!outside)
    {   //decrease 1 pixel
        rectCell.left++;    rectCell.right--;
        rectCell.top++;     rectCell.bottom--;
    }
	return rectCell;
}

//***************************************************************
CRect CColorListCtrl::GetCellRect(int nItem, int nColumn, BOOL outside)
{
	CRect rectCell;

	// Get the height
    CListCtrl::GetItemRect(nItem, rectCell, LVIR_BOUNDS);
    LV_COLUMN column;
	column.mask = LVCF_WIDTH;        // Set the mask to only return the width

    for (int x=0; x<nColumn; x++)
    {
        column.iSubItem=1;
        CListCtrl::GetColumn(x, &column); // Get rect-edge (left/top)
        rectCell.left+=column.cx;
    }
    column.iSubItem=1;
    CListCtrl::GetColumn(nColumn, &column); // Get rect-edge (right/bottom)
    rectCell.right = rectCell.left + column.cx;

    if (!outside)
    {   //decrease 1 pixel
        rectCell.left++;    rectCell.right--;
        rectCell.top++;     rectCell.bottom--;
    }
	return rectCell;
}

//***************************************************************
void CColorListCtrl::InvalidateFullItem(int nItem, BOOL bErase)
{
    CRect rcBounds=GetFullCellRect(nItem, TRUE);
	InvalidateRect(rcBounds, bErase);

}

//***************************************************************
// create ListCtrl-head
int CColorListCtrl::InitCtrl(CStringArray *phead, CByteArray *pProcent)
{
    if(g_FontSize <0 || g_FontSize > 28)
		g_FontSize = 16;
	if ( (!phead)||(!pProcent)) return 1;
	LV_COLUMN lvc;
	int cx=0;
    int count = phead->GetSize(); 
    if (count!=m_columnSize) return 2;
    if (pProcent->GetSize() != count) return 3;
    for (int x=0; x<count; x++) cx+=pProcent->GetAt(x);
    if (cx!=100) return 4;

	CRect rect_listbox;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM ;
	GetWindowRect( rect_listbox );
	cx=int((rect_listbox.right-rect_listbox.left)-20); //some Pixel for Scrollbar

    //set head-columns in ListBox
	lvc.iSubItem = 0;
	//=== Johnny Edited for Word display 2005-11-18 ===// ===>>>
	lvc.fmt = LVCFMT_CENTER;
	//=== Johnny Edited for Word display 2005-11-18 ===// <<<===
    for (int x=0; x<count; x++)
    {
        lvc.cx = int(cx*pProcent->GetAt(x)/100);
	    lvc.pszText = (TCHAR*)((LPCTSTR)(phead->GetAt(x))); 
	    CListCtrl::InsertColumn(x,&lvc);
    }
	m_AdvHdr.Init(GetHeaderCtrl());
    return 0;
}


//***************************************************************
// add an item
int CColorListCtrl::AddItem(CStringArray *line, int pos)
{
    int ret_index=LISTCTRL_ERROR;
    if (!line) return ret_index;

	LV_ITEM lvi;
    
	//set List-properties
	lvi.mask = LVIF_TEXT ; 
	lvi.iSubItem = 0;
    lvi.pszText = _T("");

    int count=GetItemCount();
    int columns=line->GetSize();

    if (m_columnSize!=columns) return ret_index;

    if (DWORD(pos)<=DWORD(count)) count=pos;
	
    lvi.iItem = count;
    //lvi.lParam= count;
	lvi.mask = LVIF_TEXT ; 
	lvi.iSubItem = 0;   //refer to no Item
    // insert an new line
    ret_index=InsertItem(&lvi);
    // set item text for additional columns		
    for (int x=0; x<columns; x++)
    {
        CListCtrl::SetItemText(count,x,(TCHAR*)LPCTSTR(line->GetAt(x)) );
    }
	lvi.mask = LVIF_STATE;
	lvi.iSubItem = 1;
	lvi.stateMask = 0xFFFFFFFF ; 
	lvi.state = 0;

	if (CListCtrl::SetItem(&lvi)) 
    {
        CMyLParam *p=new CMyLParam(m_columnSize); //init with: DEF_BACKGROUND, def_text

        #ifdef TESTCOLOR
            // only for TEST
            if (ret_index<3)
            {
                p->SetBackColor(ITEM_COLOR(ret_index+BLUE),0);
                p->SetTextColor(ITEM_COLOR(ret_index+RED ),0);
            }
            else
            if (ret_index<=6)
            {
                p->SetBackColor(ITEM_COLOR(ret_index+BLUE),1);
                p->SetTextColor(ITEM_COLOR(ret_index+RED ),1);
            }
            else
            {
                p->SetBackColor(ITEM_COLOR((ret_index+BLUE)&0xF),2);
                p->SetTextColor(ITEM_COLOR((ret_index+RED )&0xF),2);
            }
            // end: only for TEST
        #endif

        CListCtrl::SetItemData( ret_index, DWORD(p) );
        return ret_index;
    }
    return LISTCTRL_ERROR;
}

//***************************************************************
BOOL CColorListCtrl::DeleteAllItems()
{
	
	CProgressEntry *p=0;
	int Index=0;
	POSITION pos = m_ProgEntries.GetStartPosition();
	while (pos != NULL) {
		m_ProgEntries.GetNextAssoc(pos, Index, p);
		if(p)
			delete p;
	}

	m_ProgEntries.RemoveAll();
	
	while (CListCtrl::GetItemCount()){
		free(CColorListCtrl::m_ProgEntries[0]);
        CColorListCtrl::DeleteItem(0);
	}
    return TRUE;
}

//***************************************************************
BOOL CColorListCtrl::DeleteItem(int nItem )
{
    if (CListCtrl::GetItemCount()<=nItem) return FALSE;
    CMyLParam *p=(CMyLParam*)CListCtrl::GetItemData(nItem);
    if (p)
    {
        CListCtrl::SetItemData( nItem, p->GetData() );
        delete p;
        return CListCtrl::DeleteItem( nItem );
    }
    return TRUE;
}


//***************************************************************
DWORD CColorListCtrl::GetItemData( int nItem ) const
{
    CMyLParam *p=(CMyLParam*)CListCtrl::GetItemData(nItem);
    if (p)
    {
        return p->GetData();
        return TRUE;
    }
    return 0;
}

//***************************************************************
BOOL  CColorListCtrl::SetItemData( int nItem, DWORD dwData )
{
    CMyLParam *p=(CMyLParam*)CListCtrl::GetItemData(nItem);
    if (p)
    {
        p->SetData(dwData);
        return TRUE;
    }
    return FALSE;
}

//***************************************************************
BOOL CColorListCtrl::SetItemBackgndColor(ITEM_COLOR color, int Index, int iSub)	
{
    CMyLParam *p=GetMyLParam(Index);
    if (!p) return FALSE;
    if ( p->SetBackColor(color, iSub) )
    {
        RedrawItems( Index, Index );
        return TRUE;
    }
    return FALSE;
}

//***************************************************************
BOOL CColorListCtrl::SetItemTextColor(ITEM_COLOR color, int Index, int iSub)	
{
    CMyLParam *p=GetMyLParam(Index);
    if (!p) return FALSE;
    if ( p->SetTextColor(color, iSub) )
    {
        RedrawItems( Index, Index );
        return TRUE;
    }
    return FALSE;
}

//***************************************************************
COLORREF CColorListCtrl::GetItemBackgndColor(int Index, int iSub)
{
    CMyLParam *p=GetMyLParam(Index);
    if (!p) return 0;
    return p->GetBackColor(iSub);
}

//***************************************************************
COLORREF CColorListCtrl::GetItemTextColor(int Index, int iSub)
{
    CMyLParam *p=GetMyLParam(Index);
    if (!p) return 0;
    return p->GetTextColor(iSub);
}

//***************************************************************
void CColorListCtrl::OnPaint() 
{
	// First let the control do its default drawing.
	const MSG *msg = GetCurrentMessage();
	DefWindowProc( msg->message, msg->wParam, msg->lParam );

    if (!m_fullColumnLines) return;

	// Draw the lines only for LVS_REPORT mode
	if( (GetStyle() & LVS_TYPEMASK) == LVS_REPORT )
	{
		// Get the number of columns
		CClientDC dc(this );
		CHeaderCtrl* pHeader = (CHeaderCtrl*)GetDlgItem(0);
		int nColumnCount = pHeader->GetItemCount();

		// The bottom of the header corresponds to the top of the line 
		RECT rect;
		pHeader->GetClientRect( &rect );
		int top = rect.bottom;

		// Now get the client rect so we know the line length and
		// when to stop
		GetClientRect( &rect );

		// The border of the column is offset by the horz scroll
		int borderx = 0 - GetScrollPos( SB_HORZ );

        CPen       *pOldPen;
        CPen        pen;
        CGdiObject *pOldBrush;
        pen.CreatePen( PS_DOT, 0, GetColorRef(DEF_DESELTEXT) );
        pOldPen  =dc.SelectObject(&pen);
        pOldBrush=dc.SelectStockObject(NULL_BRUSH);

		for( int i = 0; i < nColumnCount; i++ )
		{
			// Get the next border
			borderx += GetColumnWidth( i );

			// if next border is outside client area, break out
			if( borderx >= rect.right ) break;

			// Draw the line.
			dc.MoveTo( borderx-1, top);
			dc.LineTo( borderx-1, rect.bottom );
		}
        dc.SelectObject(pOldPen);
        dc.SelectObject(pOldBrush);

	}
	// Do not call CListCtrl::OnPaint() for painting messages
}

//***************************************************************
void CColorListCtrl::RepaintSelectedItems()
{
	// Invalidate focused item so it can repaint 
	int nItem = GetNextItem(-1, LVNI_FOCUSED);

	if(nItem != -1)
	{
        InvalidateFullItem(nItem, FALSE);
	}

	// Invalidate selected items depending on LVS_SHOWSELALWAYS
	if((GetStyle() & LVS_SHOWSELALWAYS))
	{
		for(nItem = GetNextItem(-1, LVNI_SELECTED);
			nItem != -1; nItem = GetNextItem(nItem, LVNI_SELECTED))
		{
            InvalidateFullItem(nItem, FALSE);
		}
	}
	UpdateWindow();
}

//***************************************************************
BOOL CColorListCtrl::SetSel(int pos, BOOL bSelect)
{
    // LVS_ES_FULLROWSELECT only works with newer version of Comctl32
    // This style and some of the other new extended styles 
    // only work with version 4.72 (or higer) of COMCTL32.DLL 

    UINT nState=0;
    if (bSelect) nState=LVIS_SELECTED|LVS_EX_FULLROWSELECT; else nState=0;
    if (SetItemState(pos, nState, LVIS_SELECTED|LVS_EX_FULLROWSELECT))
    {
        if((GetStyle() & LVS_SHOWSELALWAYS))
        {
            InvalidateFullItem(pos, TRUE);
        }
        return TRUE;
    }
    return FALSE;
}
//***************************************************************
void CColorListCtrl::OnKillFocus(CWnd* pNewWnd) 
{
	SendMessage(WM_PAINT);
    CListCtrl::OnKillFocus(pNewWnd);
	// check if we are losing focus
	if(pNewWnd != NULL && pNewWnd->GetParent() == this)
		return;

	// repaint items that should change appearance
	if((GetStyle() & LVS_TYPEMASK) == LVS_REPORT)
		RepaintSelectedItems();
}

//***************************************************************
void CColorListCtrl::OnSetFocus(CWnd* pOldWnd) 
{
	CListCtrl::OnSetFocus(pOldWnd);
	
	// check if we are getting focus
	if(pOldWnd!=NULL && pOldWnd->GetParent()==this)
		return;

	// repaint items that should change appearance
	if((GetStyle() & LVS_TYPEMASK)==LVS_REPORT)
		RepaintSelectedItems();
}

//***************************************************************
void CColorListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
    if (!lpDrawItemStruct) return;
    IsCreated=TRUE;
	CDC*		pDC			= CDC::FromHandle(lpDrawItemStruct->hDC);
	CMyLParam*	pMyLParam	= GetMyLParam(lpDrawItemStruct->itemID);
	CRect		rcItem;
	
	CString szFont;
	if(!ENGLISHVERSION)
		szFont.LoadString(IDS_FONTNAME);
	else
		szFont="MS Sans Serif";

	//>>>>	Font and Size	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	LOGFONT m_logfont;
	
	memset(&m_logfont, 0, sizeof m_logfont);
	m_logfont.lfHeight = g_FontSize;
	lstrcpy(m_logfont.lfFaceName, szFont);
	m_logfont.lfOutPrecision = OUT_TT_PRECIS;
	m_logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	m_logfont.lfQuality = PROOF_QUALITY;
	m_logfont.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
	m_logfont.lfWeight = FW_NORMAL;
				
				
	LOGFONT		lf	= m_logfont;
	CFont		font;
	CFont*		pOldfont;
				
	if (font.CreateFontIndirect(&lf))
	{
		pOldfont = pDC->SelectObject(&font);
	}
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	
	
	if (!pMyLParam) return;
    int x=0;
    typedef enum {R_NOTHINGS=0, R_FOCUS, R_SELECT, R_NORMAL, R_SEL_NOFOCUS} redrawmode;
    redrawmode redraw=R_NORMAL;

	// Get item image and state info
	LV_ITEM lvi;
	lvi.mask = LVIF_IMAGE | LVIF_STATE;
	lvi.iItem = lpDrawItemStruct->itemID;
	lvi.iSubItem = 0;
	lvi.stateMask = 0xFFFF;		// get all state flags
	GetItem(&lvi);

	BOOL bHighlight =   (   (lvi.state & LVIS_DROPHILITED)
                            ||
				            (
                                (lvi.state & LVIS_SELECTED) 
                                && 
                                (  ( GetFocus() == this )  || ( GetStyle() & LVS_SHOWSELALWAYS ) )
					        )
				        );

    BOOL bLostFocus=(GetFocus()!=this);


    if ( (lpDrawItemStruct->itemAction&ODA_FOCUS)==ODA_FOCUS)
    {   //the control gains or loses input focus
        if (bLostFocus) redraw=R_SEL_NOFOCUS;
        else
        if ((lvi.state&LVIS_FOCUSED)==LVIS_FOCUSED) redraw=R_FOCUS;
        else 
        {
            if (bHighlight) redraw=R_SELECT;
            else redraw=R_NORMAL;
        }
    }
    else
    if ((lpDrawItemStruct->itemAction&ODA_SELECT)==ODA_SELECT)
    {   //the selection status has changed
            if (bHighlight)
            {
                if (bLostFocus) redraw=R_SEL_NOFOCUS;
                 else redraw=R_SELECT;
            }
            else redraw=R_NORMAL;
    }
    else
    {   //redraw the item
        if (bLostFocus)
        {
            if (bHighlight) redraw=R_SEL_NOFOCUS;
             else
              redraw=R_NORMAL;
        }
        else 
        {
            if ((lvi.state&LVIS_FOCUSED)==LVIS_FOCUSED) redraw=R_FOCUS;
            else 
            {
                if (bHighlight) redraw=R_SELECT;
                 else redraw=R_NORMAL;
            }
        }
    }

    CPen       *pOldPen;
    CPen        pen;
    CGdiObject *pOldBrush;

    switch(redraw)
    {

        case R_FOCUS:
            rcItem=GetFullCellRect(lpDrawItemStruct->itemID, TRUE);
            pDC->FillSolidRect(rcItem, GetColorRef(DEF_SELBACKGROUND));
            pDC->SetTextColor(GetColorRef(DEF_SELTEXT));
            if (m_BigFocusRect)
            {
                pen.CreatePen( PS_DOT, 0, GetColorRef(DEF_SELTEXT) );
                pOldPen  =pDC->SelectObject(&pen);
                pOldBrush=pDC->SelectStockObject(NULL_BRUSH);
                pDC->Rectangle(rcItem);
                pDC->SelectObject(pOldPen);
                pDC->SelectObject(pOldBrush);
            }
            else pDC->DrawFocusRect(rcItem);
            for (x=0; x<m_columnSize; x++)
            {
                rcItem	= GetCellRect(lpDrawItemStruct->itemID, x, FALSE);
                pDC->DrawText(CListCtrl::GetItemText(lpDrawItemStruct->itemID,x), rcItem, DT_SINGLELINE | DT_LEFT | DT_VCENTER );
            }
            break;
        case R_SELECT:
            rcItem=GetFullCellRect(lpDrawItemStruct->itemID, TRUE);
            pDC->FillSolidRect(rcItem, GetColorRef(DEF_SELBACKGROUND));
            pDC->SetTextColor(GetColorRef(DEF_SELTEXT));
            for (x=0; x<m_columnSize; x++)
            {
                rcItem	= GetCellRect(lpDrawItemStruct->itemID, x, FALSE);
                pDC->DrawText(CListCtrl::GetItemText(lpDrawItemStruct->itemID,x), rcItem, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }
            break;
        case R_SEL_NOFOCUS:
            rcItem=GetFullCellRect(lpDrawItemStruct->itemID, TRUE);
            pDC->FillSolidRect(rcItem, GetColorRef(DEF_DESELBACKGROUND));
            pDC->SetTextColor(pMyLParam->GetTextColor(DEF_DESELTEXT));
            pen.CreatePen( PS_DOT, 0, GetColorRef(DEF_DESELBACKGROUND) );
            pOldPen  =pDC->SelectObject(&pen);
            pOldBrush=pDC->SelectStockObject(NULL_BRUSH);
            if (!m_withSingleRect)
                pDC->Rectangle(rcItem);
            for (x=0; x<m_columnSize; x++)
            {
                rcItem	= GetCellRect(lpDrawItemStruct->itemID, x, FALSE);
                pDC->DrawText(CListCtrl::GetItemText(lpDrawItemStruct->itemID,x), rcItem, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }
            pDC->SelectObject(pOldPen);
            pDC->SelectObject(pOldBrush);
            break;
        case R_NORMAL:
            for (x=0; x<m_columnSize; x++)
            {
	            rcItem	= GetCellRect(lpDrawItemStruct->itemID, x, TRUE);
                pDC->FillSolidRect(rcItem, pMyLParam->GetBackColor(x));
                pDC->SetTextColor(pMyLParam->GetTextColor(x));
                if (m_withSingleRect)
                {
                    pOldBrush=pDC->SelectStockObject(NULL_BRUSH);
                    pDC->Rectangle(rcItem);
                    pDC->SelectObject(pOldBrush);
                    
                }
                rcItem	= GetCellRect(lpDrawItemStruct->itemID, x, FALSE);
                pDC->DrawText(CListCtrl::GetItemText(lpDrawItemStruct->itemID,x), rcItem, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            }
            break;
    }

	pDC->SelectObject(pOldfont);
    return;
}

void CColorListCtrl::OnSize(UINT nType, int cx, int cy) 
{
	CListCtrl::OnSize(nType, cx, cy);
    if (IsCreated) 
    {
        RedrawWindow();
    }
    ResizeProg();

}

LRESULT CColorListCtrl::OnSetFont(WPARAM wParam, LPARAM)
{
	LRESULT res =  Default();

	CRect rc;
	GetWindowRect( &rc );

	WINDOWPOS wp;
	wp.hwnd = m_hWnd;
	wp.cx = rc.Width();
	wp.cy = rc.Height();
	wp.flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER;
	SendMessage( WM_WINDOWPOSCHANGED, 0, (LPARAM)&wp );

	return res;
}

void CColorListCtrl::MeasureItem ( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
{
	LOGFONT lf;
	GetFont()->GetLogFont( &lf );

	if( lf.lfHeight < 0 )
		lpMeasureItemStruct->itemHeight = -lf.lfHeight;
	else
		lpMeasureItemStruct->itemHeight = lf.lfHeight;
}

void CColorListCtrl::SetMyFontSize(int nFontSize)
{
	LOGFONT m_logfont;
	CString szFont;
	if(!ENGLISHVERSION)
		szFont.LoadString(IDS_FONTNAME);
	else
		szFont="MS Sans Serif";

	g_FontSize = nFontSize;
	memset(&m_logfont, 0, sizeof m_logfont);
	m_logfont.lfHeight = nFontSize;
	lstrcpy(m_logfont.lfFaceName, szFont);
	m_logfont.lfOutPrecision = OUT_TT_PRECIS;
	m_logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	m_logfont.lfQuality = PROOF_QUALITY;
	m_logfont.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
	m_logfont.lfWeight = FW_NORMAL;
				
				
	LOGFONT		lf	= m_logfont;
	CFont		font;
				
	if (font.CreateFontIndirect(&lf))
	{
		SetFont(&font);
	}

}



void CColorListCtrl::SetMaxColumnWidth(int nCol)
{
	int nMaxWidth=0;
	int nCurrentWidth = CListCtrl::GetColumnWidth(nCol);

	CListCtrl::SetColumnWidth(nCol , LVSCW_AUTOSIZE );
	int nAutoSize	  = CListCtrl::GetColumnWidth(nCol);
	nAutoSize +=5;


	CListCtrl::SetColumnWidth(nCol ,LVSCW_AUTOSIZE_USEHEADER);
	int nAutoHeaderSize=CListCtrl::GetColumnWidth(nCol);
	nAutoHeaderSize+=5;

	
	if(nMaxWidth < nCurrentWidth)
		nMaxWidth = nCurrentWidth;
	if(nMaxWidth < nAutoSize)
		nMaxWidth = nAutoSize;
	if(nMaxWidth < nAutoHeaderSize)
		nMaxWidth = nAutoHeaderSize;

	CListCtrl::SetColumnWidth(nCol , nMaxWidth);
}
void CColorListCtrl::SetMaxColumnsWidth( int *ListShowIndex , int nLength)
{

	int nMaxWidth=0;
	int nCurrentWidth;

	int nAutoSize;

	int nAutoHeaderSize;

	for(int i = 0 ; i < nLength ; i++){
		nMaxWidth=0;
		if(ListShowIndex[i] >0){
			nCurrentWidth = CListCtrl::GetColumnWidth(ListShowIndex[i]);
			
			CListCtrl::SetColumnWidth(ListShowIndex[i] , LVSCW_AUTOSIZE );
			nAutoSize	  = CListCtrl::GetColumnWidth(ListShowIndex[i]);
			nAutoSize +=10;
			CListCtrl::SetColumnWidth(ListShowIndex[i] ,LVSCW_AUTOSIZE_USEHEADER);
			nAutoHeaderSize=CListCtrl::GetColumnWidth(ListShowIndex[i]);
			nAutoHeaderSize+=10;
			
			if(nMaxWidth < nCurrentWidth)
				nMaxWidth = nCurrentWidth;
			if(nMaxWidth < nAutoSize)
				nMaxWidth = nAutoSize;
			if(nMaxWidth < nAutoHeaderSize)
				nMaxWidth = nAutoHeaderSize;
			
			CListCtrl::SetColumnWidth(ListShowIndex[i] , nMaxWidth);
		}
	}
}


BOOL CColorListCtrl::DeleteColumn(int nCol){
	return CListCtrl::DeleteColumn(nCol);
}

BOOL CColorListCtrl::InsertColumn(int nCol, LPCTSTR lpszColumnHeading,	int nFormat , int nWidth , int nSubItem )
{
	return CListCtrl::InsertColumn(nCol,lpszColumnHeading,nFormat,nWidth,nSubItem);
}

BOOL CColorListCtrl::ColumnHide(int nCol, int *ListShowIndex, int nLength)
{
	int i ;
	//=== Release Progress Column ===// ===>>>
	if(nCol == 1){  
		CProgressEntry *p=0;
		int Index=0;
		POSITION pos = m_ProgEntries.GetStartPosition();
		while (pos != NULL) {
			m_ProgEntries.GetNextAssoc(pos, Index, p);
			if(p)
				delete p;
		}
		
		m_ProgEntries.RemoveAll();

	}
	//=== Release Progress Column ===// <<<===
	if(CListCtrl::DeleteColumn(ListShowIndex[nCol])){
		ListShowIndex[nCol]=-1;
		for(i = nCol+1 ; i < nLength ; i++){
			ListShowIndex[i]--;
		}
		return TRUE;
	}
	else{
		return FALSE;
	}
	

}

/*
BOOL CColorListCtrl::ColumnShow(int nCol, int *ListShowIndex, int nLength )
{
	int i;
	int j;
	for(j = nCol ; j >= 0 ; j--){
		if(ListShowIndex[j] >= 0 && nCol>ListShowIndex[j]){
			break;
		}
	}
	if (j >= 0 ) 
	{
		if(CListCtrl::InsertColumn(ListShowIndex[j]+1 , ColsName[nCol], LVCFMT_CENTER, -1 , -1  )){
			ListShowIndex[nCol] = ListShowIndex[j]+1;
			for(i = nCol+1 ; i < nLength ; i++){
				if(ListShowIndex[i] > 0){
					ListShowIndex[i]++;
				}
			}
			return TRUE;
		}
		else{
			return FALSE;
		}
	}
	else{
		return FALSE;
	}
}
*/


//============================== Johnny Add for Progress in the Color List Control ===================================//
void CColorListCtrl::CreateProgress(int Index)
{
  // can only create progress for an existing item
  if (Index >= GetItemCount())
     return;

  // CProgressEntry is a nested class containing Index,
  // SubIndex, and CProgressCtrl members.

  CProgressEntry* ProgEntry = new CProgressEntry(Index, 1);
  CRect ItemRect;
  GetSubItemRect(Index, ProgEntry->m_SubIndex, LVIR_BOUNDS,
                                ItemRect);
  int left = ItemRect.left;
  int top = ItemRect.top;
  int right = ItemRect.right;
  int bottom = ItemRect.bottom;
  (ProgEntry->m_Prog).Create(PBS_SMOOTH | WS_CHILD | WS_VISIBLE,
                     CRect(left, top, right, bottom), this, 1);
  (ProgEntry->m_Prog).SetRange(0, 100);
  (ProgEntry->m_Prog).SetPos(0);
  m_ProgEntries[Index] = ProgEntry;
}
//Here is the code for setting the progress bar for an item: 

void CColorListCtrl::SetProgress(int Index, int prog)
{
  CProgressEntry* ProgEntry;
  if (m_ProgEntries.Lookup(Index, ProgEntry) == TRUE)
     (ProgEntry->m_Prog).SetPos(prog);
}

void CColorListCtrl::ResizeProg()
{
  CRect ItemRect;
  CProgressEntry* ProgEntry=0;
  int Index=0;
  POSITION pos = m_ProgEntries.GetStartPosition();
  while (pos != NULL) {
    m_ProgEntries.GetNextAssoc(pos, Index, ProgEntry);
	if(!ProgEntry)
		break;
    GetSubItemRect(ProgEntry->m_Index, ProgEntry->m_SubIndex, LVIR_BOUNDS, ItemRect);
    int left = ItemRect.left;
    int top = ItemRect.top;
    int right = ItemRect.right;
    int bottom = ItemRect.bottom;

  	(ProgEntry->m_Prog).MoveWindow(left, top, (right - left), (bottom - top));
  }
  RedrawWindow();
}

void CColorListCtrl::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	// TODO: Add your message handler code here and/or call default
  ResizeProg();
	CListCtrl::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CColorListCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	// TODO: Add your message handler code here and/or call default
	CListCtrl::OnVScroll(nSBCode, nPos, pScrollBar);
}
void CColorListCtrl::OnWindowPosChanged(WINDOWPOS FAR* lpwndpos) 
{
	CListCtrl::OnWindowPosChanged(lpwndpos);
	
	// TODO: Add your message handler code here
  ResizeProg();
}


CColorListCtrl::CProgressEntry::CProgressEntry(int Index, int SubIndex) :
m_Index(Index), m_SubIndex(SubIndex)
{
}

CColorListCtrl::CProgressEntry::~CProgressEntry()
{
}


//============================== Johnny Add for Progress in the Color List Control ===================================//





