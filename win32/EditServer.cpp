// EditServer.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "EditServer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CEditServer dialog


CEditServer::CEditServer(CWnd* pParent, xmlNodePtr p)
	: CDialog(CEditServer::IDD, pParent)
{
	node = p;
	//{{AFX_DATA_INIT(CEditServer)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CEditServer::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditServer)
	DDX_Control(pDX, IDC_PORT, m_port);
	DDX_Control(pDX, IDC_HOST, m_host);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEditServer, CDialog)
	//{{AFX_MSG_MAP(CEditServer)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEditServer message handlers

void CEditServer::OnOK() 
{
	CString tmp;
	m_host.GetWindowText(tmp);
	const char *host = tmp;
	xmlSetProp(node, (xmlChar *)"host", (xmlChar *)host);
	m_port.GetWindowText(tmp);
	const char *port = tmp;
	xmlSetProp(node, (xmlChar *)"port", (xmlChar *)port);
	
	CDialog::OnOK();
}

BOOL CEditServer::OnInitDialog() 
{
	CDialog::OnInitDialog();

	char *host = (char *)xmlGetProp(node, (xmlChar *)"host");
	if(host) m_host.SetWindowText(host);
	xmlFree(host);

	char *port = (char *)xmlGetProp(node, (xmlChar *)"port");
	if(port) m_port.SetWindowText(port);
	xmlFree(port);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
