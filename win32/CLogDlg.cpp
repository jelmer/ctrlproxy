// LogDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ctrlproxyapp.h"
#include "CLogDlg.h"

extern "C" {
#include <glib.h>
void log_handler(const gchar *log_domain, GLogLevelFlags flags, const gchar *message, gpointer user_data);
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CListBox *logbox = NULL;

/////////////////////////////////////////////////////////////////////////////
// CLogDlg dialog


CLogDlg::CLogDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CLogDlg::IDD, pParent)
{
	Create(IDD, pParent);
	logbox = &m_log;
	//{{AFX_DATA_INIT(CLogDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CLogDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLogDlg)
	DDX_Control(pDX, IDC_LOG, m_log);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLogDlg, CDialog)
	//{{AFX_MSG_MAP(CLogDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void log_handler(const gchar *log_domain, GLogLevelFlags flags, const gchar *message, gpointer user_data) {
	if(logbox)logbox->AddString(message);
	TRACE(TEXT("%s\n"), message);
}
