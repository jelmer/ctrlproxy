// EditListener.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "EditListener.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CEditListener dialog


CEditListener::CEditListener(CWnd* pParent, xmlNodePtr p)
	: CDialog(CEditListener::IDD, pParent)
{
	node = p;
	//{{AFX_DATA_INIT(CEditListener)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CEditListener::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditListener)
	DDX_Control(pDX, IDC_PORT, m_port);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEditListener, CDialog)
	//{{AFX_MSG_MAP(CEditListener)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEditListener message handlers

void CEditListener::OnOK() 
{
	CString tmp;
	m_port.GetWindowText(tmp);
	const char *port = tmp;
	xmlSetProp(node, (xmlChar *)"port", (xmlChar *)port);
	
	CDialog::OnOK();
}

BOOL CEditListener::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	char *tmp = (char *)xmlGetProp(node, (xmlChar *)"port");
	if(tmp)m_port.SetWindowText(tmp);
	xmlFree(tmp);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
