// EditChannel.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "EditChannel.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CEditChannel dialog


CEditChannel::CEditChannel(CWnd* pParent, xmlNodePtr p)
	: CDialog(CEditChannel::IDD, pParent)
{
	node = p;
	//{{AFX_DATA_INIT(CEditChannel)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CEditChannel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditChannel)
	DDX_Control(pDX, IDC_NAME, m_name);
	DDX_Control(pDX, IDC_KEY, m_key);
	DDX_Control(pDX, IDC_AUTOJOIN, m_autojoin);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEditChannel, CDialog)
	//{{AFX_MSG_MAP(CEditChannel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEditChannel message handlers

void CEditChannel::OnOK() 
{
	CString tmp;
	m_key.GetWindowText(tmp);
	const char *key = tmp;
	if(strlen(key))xmlSetProp(node, (xmlChar *)"key", (xmlChar *)key);
	m_name.GetWindowText(tmp);
	const char *name = tmp;
	xmlSetProp(node, (xmlChar *)"name", (xmlChar *)name);
	xmlSetProp(node, (xmlChar *)"autojoin", (xmlChar *)(m_autojoin.GetCheck()?"1":"0"));
	
	
	CDialog::OnOK();
}

BOOL CEditChannel::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	char *autojoin = (char *)xmlGetProp(node, (xmlChar *)"autojoin");
	char *key = (char *)xmlGetProp(node, (xmlChar *)"key");
	char *name = (char *)xmlGetProp(node, (xmlChar *)"name");
	m_key.SetWindowText(key);
	xmlFree(key);
	m_name.SetWindowText(name);
	xmlFree(name);
	if(autojoin && atol(autojoin) > 0)m_autojoin.SetCheck(TRUE);
	xmlFree(autojoin);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
