// Machine generated IDispatch wrapper class(es) created by Microsoft Visual C++

// NOTE: Do not modify the contents of this file.  If this class is regenerated by
//  Microsoft Visual C++, your modifications will be overwritten.


// Dispatch interfaces referenced by this interface
class COleFont;

/////////////////////////////////////////////////////////////////////////////
// CRat wrapper class

class CRat : public CWnd
{
protected:
    DECLARE_DYNCREATE(CRat)
public:
    CLSID const& GetClsid()
    {
        static CLSID const clsid
            = { 0xba634607, 0xb771, 0x11d0, { 0x92, 0x96, 0x0, 0xc0, 0x4f, 0xb6, 0x67, 0x8b } };
        return clsid;
    }
    virtual BOOL Create(LPCTSTR lpszClassName,
        LPCTSTR lpszWindowName, DWORD dwStyle,
        const RECT& rect,
        CWnd* pParentWnd, UINT nID,
        CCreateContext* pContext = NULL)
    { return CreateControl(GetClsid(), lpszWindowName, dwStyle, rect, pParentWnd, nID); }

    BOOL Create(LPCTSTR lpszWindowName, DWORD dwStyle,
        const RECT& rect, CWnd* pParentWnd, UINT nID,
        CFile* pPersist = NULL, BOOL bStorage = FALSE,
        BSTR bstrLicKey = NULL)
    { return CreateControl(GetClsid(), lpszWindowName, dwStyle, rect, pParentWnd, nID,
        pPersist, bStorage, bstrLicKey); }

// Attributes
public:
    short GetBorderStyle();
    void SetBorderStyle(short);
    BOOL GetEnabled();
    void SetEnabled(BOOL);
    COleFont GetFont();
    void SetFont(LPDISPATCH);
    CString GetCaption();
    void SetCaption(LPCTSTR);

// Operations
public:
    void SetAdminTarget(LPCTSTR szMachineName, LPCTSTR szMetaTarget);
    void DoClick();
};
