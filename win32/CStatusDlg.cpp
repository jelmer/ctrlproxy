// StatusDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "CStatusDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CStatusDlg dialog


CStatusDlg::CStatusDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CStatusDlg::IDD, pParent)
{
	CDialog::Create(IDD, pParent);
	//{{AFX_DATA_INIT(CStatusDlg)
	//}}AFX_DATA_INIT
}


void CStatusDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CStatusDlg)
	DDX_Control(pDX, IDC_NUMCLIENTS, m_numclients);
	DDX_Control(pDX, IDC_NUMCHANNELS, m_numchannels);
	DDX_Control(pDX, IDC_NUMNETWORKS, m_numnetworks);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CStatusDlg, CDialog)
	//{{AFX_MSG_MAP(CStatusDlg)
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStatusDlg message handlers

void CStatusDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CDialog::OnShowWindow(bShow, nStatus);
	if(!bShow) return;
		
	CString tmp;
	tmp.Format("Connected to %d networks", g_list_length(get_network_list()));
	m_numnetworks.SetWindowText(tmp);

	int nch = 0, ncl = 0;
	GList *gl = get_network_list();
	while(gl) {
		struct network *n = (struct network *)gl->data;
		ncl+= g_list_length(n->clients);
		nch+= g_list_length(n->channels);
		gl = gl->next;
	}

	tmp.Format("Connected to %d channels", nch);
	m_numchannels.SetWindowText(tmp);

	tmp.Format("Connected to %d clients", nch);
	m_numclients.SetWindowText(tmp);

}
