// NewNetworkDialog.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "NewNetworkDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNewNetworkDlg dialog


CNewNetworkDlg::CNewNetworkDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CNewNetworkDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNewNetworkDlg)
	NetworkName = _T("");
	//}}AFX_DATA_INIT
}


void CNewNetworkDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewNetworkDlg)
	DDX_Text(pDX, IDC_NEWNETWORKNAME, NetworkName);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CNewNetworkDlg, CDialog)
	//{{AFX_MSG_MAP(CNewNetworkDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewNetworkDlg message handlers
