// ConfigurationDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "ConfigurationDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConfigurationDlg dialog


CConfigurationDlg::CConfigurationDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigurationDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigurationDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CConfigurationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigurationDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigurationDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigurationDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigurationDlg message handlers
