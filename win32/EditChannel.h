#if !defined(AFX_EDITCHANNEL_H__6723E94A_7BB1_414E_8A4C_BE7DA0B7C235__INCLUDED_)
#define AFX_EDITCHANNEL_H__6723E94A_7BB1_414E_8A4C_BE7DA0B7C235__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// EditChannel.h : header file
//

extern "C" {
#include "internals.h"
}

/////////////////////////////////////////////////////////////////////////////
// CEditChannel dialog

class CEditChannel : public CDialog
{
// Construction
public:
	CEditChannel(CWnd* pParent, xmlNodePtr);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEditChannel)
	enum { IDD = IDD_EDIT_CHANNEL };
	CEdit	m_name;
	CEdit	m_key;
	CButton	m_autojoin;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditChannel)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	xmlNodePtr node;

	// Generated message map functions
	//{{AFX_MSG(CEditChannel)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EDITCHANNEL_H__6723E94A_7BB1_414E_8A4C_BE7DA0B7C235__INCLUDED_)
