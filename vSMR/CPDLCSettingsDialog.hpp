#pragma once

#include "resource.h"

// CCPDLCSettingsDialog dialog

class CCPDLCSettingsDialog : public CDialogEx
{
	DECLARE_DYNAMIC(CCPDLCSettingsDialog)

public:
	CCPDLCSettingsDialog(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CCPDLCSettingsDialog();

	CString m_Logon;
	CString m_Password;
	int m_Sound;

// Dialog Data
	enum { IDD = IDD_DIALOG2 };

protected:
	void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedSound();
	afx_msg void OnBnClickedOk();
};
