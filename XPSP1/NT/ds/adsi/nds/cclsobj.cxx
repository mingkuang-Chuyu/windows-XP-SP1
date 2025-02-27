//---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1992 - 1995
//
//  File:  cclsobj.cxx
//
//  Contents:  Microsoft ADs NDS Provider Generic Object
//
//
//  History:   01-30-95     krishnag    Created.
//
//----------------------------------------------------------------------------
#include "nds.hxx"
#pragma hdrstop

//  Class CNDSClass

DEFINE_IDispatch_Implementation(CNDSClass)
DEFINE_IADs_Implementation(CNDSClass)


CNDSClass::CNDSClass():
      _pDispMgr( NULL ),
      _bstrCLSID( NULL ),
      _bstrOID( NULL ),
      _bstrPrimaryInterface( NULL ),
      _bstrHelpFileName( NULL ),
      _lHelpFileContext( 0 ),

      _dwFlags(0),
      _lpClassName(0),
      _dwNumberOfSuperClasses(0),
      _lpSuperClasses(0),
      _dwNumberOfContainmentClasses(0),
      _lpContainmentClasses(0),
      _dwNumberOfNamingAttributes(0),
      _lpNamingAttributes(0),
      _dwNumberOfMandatoryAttributes(0),
      _lpMandatoryAttributes(0),
      _dwNumberOfOptionalAttributes(0),
      _lpOptionalAttributes(0)

{
    VariantInit(&_vFilter);

    ENLIST_TRACKING(CNDSClass);
}

HRESULT
CNDSClass::CreateClass(
    BSTR Parent,
    BSTR CommonName,
    LPNDS_CLASS_DEF lpClassDefs,
    CCredentials& Credentials,
    DWORD dwObjectState,
    REFIID riid,
    void **ppvObj
    )
{
    CNDSClass FAR * pClass = NULL;
    HRESULT hr = S_OK;

    hr = AllocateClassObject(Credentials, &pClass);
    BAIL_ON_FAILURE(hr);

    hr = pClass->InitializeCoreObject(
                Parent,
                CommonName,
                CLASS_CLASS_NAME,
                L"",
                CLSID_NDSClass,
                dwObjectState
                );
    BAIL_ON_FAILURE(hr);

    hr = pClass->QueryInterface(riid, ppvObj);
    BAIL_ON_FAILURE(hr);

    pClass->_Credentials = Credentials;

    pClass->_dwFlags = lpClassDefs->dwFlags;

    pClass->_dwNumberOfSuperClasses =
                    lpClassDefs->dwNumberOfSuperClasses;
    pClass->_lpSuperClasses = CreatePropertyList(
                                    lpClassDefs->lpSuperClasses
                                    );

    pClass->_dwNumberOfContainmentClasses =
                    lpClassDefs->dwNumberOfContainmentClasses;
    pClass->_lpContainmentClasses = CreatePropertyList(
                                        lpClassDefs->lpContainmentClasses
                                        );

    pClass->_dwNumberOfNamingAttributes =
                    lpClassDefs->dwNumberOfNamingAttributes;
    pClass->_lpNamingAttributes = CreatePropertyList(
                                        lpClassDefs->lpNamingAttributes
                                        );

    pClass->_dwNumberOfMandatoryAttributes =
                    lpClassDefs->dwNumberOfMandatoryAttributes;
    pClass->_lpMandatoryAttributes = CreatePropertyList(
                                          lpClassDefs->lpMandatoryAttributes
                                          );

    pClass->_dwNumberOfOptionalAttributes =
                    lpClassDefs->dwNumberOfOptionalAttributes;
    pClass->_lpOptionalAttributes = CreatePropertyList(
                                            lpClassDefs->lpOptionalAttributes
                                            );

    pClass->Release();

    RRETURN(hr);

error:

    delete pClass;
    RRETURN_EXP_IF_ERR(hr);
}

HRESULT
CNDSClass::CreateClass(
    BSTR Parent,
    BSTR CommonName,
    HANDLE hTree,
    CCredentials& Credentials,
    DWORD dwObjectState,
    REFIID riid,
    void **ppvObj
    )
{
    DWORD dwStatus = 0;
    HRESULT hr = S_OK;
    LPNDS_CLASS_DEF lpClassDefs = NULL;
    DWORD dwObjectsReturned = 0;
    DWORD dwInfoType = 0;
    HANDLE hOperationData = NULL;

    dwStatus = NwNdsCreateBuffer(
                    NDS_SCHEMA_READ_CLASS_DEF,
                    &hOperationData
                    );

    CHECK_AND_SET_EXTENDED_ERROR(dwStatus, hr);

    dwStatus = NwNdsPutInBuffer(
                    CommonName,
                    0,
                    NULL,
                    0,
                    0,
                    hOperationData
                    );

    CHECK_AND_SET_EXTENDED_ERROR(dwStatus, hr);

    dwStatus = NwNdsReadClassDef(
                    hTree,
                    NDS_CLASS_INFO_EXPANDED_DEFS,
                    &hOperationData
                    );

    CHECK_AND_SET_EXTENDED_ERROR(dwStatus, hr);

    dwStatus = NwNdsGetClassDefListFromBuffer(
                    hOperationData,
                    &dwObjectsReturned,
                    &dwInfoType,
                    (LPVOID *) &lpClassDefs
                    );

    CHECK_AND_SET_EXTENDED_ERROR(dwStatus, hr);

    if (!lpClassDefs) {
        hr = E_FAIL;
        BAIL_ON_FAILURE(hr);
    }

    hr = CNDSClass::CreateClass(
                    Parent,
                    CommonName,
                    lpClassDefs,
                    Credentials,
                    dwObjectState,
                    riid,
                    ppvObj
                    );

error:
    if (hOperationData) {
        NwNdsFreeBuffer(hOperationData);
    }

    RRETURN_EXP_IF_ERR(hr);
}

CNDSClass::~CNDSClass( )
{
    if ( _bstrCLSID ) {
        ADsFreeString( _bstrCLSID );
    }

    if ( _bstrOID ) {
        ADsFreeString( _bstrOID );
    }

    if ( _bstrPrimaryInterface ) {
        ADsFreeString( _bstrPrimaryInterface );
    }

    if ( _bstrHelpFileName ) {
        ADsFreeString( _bstrHelpFileName );
    }


    if (_lpSuperClasses) {

        FreePropertyList(_lpSuperClasses);
    }

    if (_lpContainmentClasses) {

        FreePropertyList(_lpContainmentClasses);
    }

    if (_lpNamingAttributes) {

        FreePropertyList(_lpNamingAttributes);
    }

    if (_lpMandatoryAttributes) {

        FreePropertyList(_lpMandatoryAttributes);
    }


    if (_lpOptionalAttributes) {

        FreePropertyList(_lpOptionalAttributes);
    }



    VariantClear( &_vFilter );

    delete _pDispMgr;
}

STDMETHODIMP
CNDSClass::QueryInterface(
    REFIID iid,
    LPVOID FAR* ppv
    )
{
    if (ppv == NULL) {
        RRETURN(E_POINTER);
    }

    if (IsEqualIID(iid, IID_IUnknown))
    {
        *ppv = (IADsClass FAR *) this;
    }
    else if (IsEqualIID(iid, IID_IADsClass))
    {
        *ppv = (IADsClass FAR *) this;
    }
    else if (IsEqualIID(iid, IID_IADs))
    {
        *ppv = (IADsClass FAR *) this;
    }
    else if (IsEqualIID(iid, IID_IDispatch))
    {
        *ppv = (IADsClass FAR *) this;
    }
    else if (IsEqualIID(iid, IID_ISupportErrorInfo))
    {
        *ppv = (ISupportErrorInfo FAR *) this;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return NOERROR;
}

STDMETHODIMP
CNDSClass::SetInfo(THIS)
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}

STDMETHODIMP
CNDSClass::GetInfo(THIS)
{
    RRETURN_EXP_IF_ERR(S_OK);
}

STDMETHODIMP
CNDSClass::GetInfoEx(THIS_ VARIANT vProperties, long lnReserved)
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}


HRESULT
CNDSClass::AllocateClassObject(
    CCredentials& Credentials,
    CNDSClass ** ppClass
    )
{
    CNDSClass FAR * pClass = NULL;
    CDispatchMgr FAR * pDispMgr = NULL;
    HRESULT hr = S_OK;

    pClass = new CNDSClass();
    if (pClass == NULL) {
        hr = E_OUTOFMEMORY;
    }
    BAIL_ON_FAILURE(hr);

    pDispMgr = new CDispatchMgr;
    if (pDispMgr == NULL) {
        hr = E_OUTOFMEMORY;
    }
    BAIL_ON_FAILURE(hr);

    hr = LoadTypeInfoEntry(
                pDispMgr,
                LIBID_ADs,
                IID_IADsClass,
                (IADsClass *)pClass,
                DISPID_REGULAR
                );
    BAIL_ON_FAILURE(hr);

    pClass->_pDispMgr = pDispMgr;
    *ppClass = pClass;

    RRETURN(hr);

error:
    delete  pDispMgr;

    RRETURN(hr);

}

/* ISupportErrorInfo method*/
STDMETHODIMP
CNDSClass::InterfaceSupportsErrorInfo(
    THIS_ REFIID riid
) 
{
    if (IsEqualIID(riid, IID_IADs) ||
        IsEqualIID(riid, IID_IADsClass)) {
        RRETURN(S_OK);
    } else {
        RRETURN(S_FALSE);
    }
}

STDMETHODIMP
CNDSClass::GetInfo(
    THIS_ DWORD dwApiLevel,
    BOOL fExplicit
    )
{
    RRETURN_EXP_IF_ERR(S_OK);
}


STDMETHODIMP
CNDSClass::Get(
    THIS_ BSTR bstrName,
    VARIANT FAR* pvProp
    )
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}



STDMETHODIMP
CNDSClass::Put(
    THIS_ BSTR bstrName,
    VARIANT vProp
    )
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}


STDMETHODIMP
CNDSClass::GetEx(
    THIS_ BSTR bstrName,
    VARIANT FAR* pvProp
    )
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}



STDMETHODIMP
CNDSClass::PutEx(
    THIS_ long lnControlCode,
    BSTR bstrName,
    VARIANT vProp
    )
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}



/* IADsClass methods */

STDMETHODIMP
CNDSClass::get_PrimaryInterface( THIS_ BSTR FAR *pbstrGUID )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_CLSID( THIS_ BSTR FAR *pbstrCLSID )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::put_CLSID( THIS_ BSTR bstrCLSID )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_OID( THIS_ BSTR FAR *pbstrOID )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::put_OID( THIS_ BSTR bstrOID )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_Abstract( THIS_ VARIANT_BOOL FAR *pfAbstract )
{
    if (_dwFlags & NDS_EFFECTIVE_CLASS) {
        *pfAbstract = VARIANT_FALSE;
    }else {
        *pfAbstract = VARIANT_TRUE;
    }
    RRETURN(S_OK);
}

STDMETHODIMP
CNDSClass::put_Abstract( THIS_ VARIANT_BOOL fAbstract )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_Auxiliary( THIS_ VARIANT_BOOL FAR *pfAuxiliary )
{
    *pfAuxiliary = VARIANT_FALSE;
    RRETURN(S_OK);
}

STDMETHODIMP
CNDSClass::put_Auxiliary( THIS_ VARIANT_BOOL fAuxiliary )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_MandatoryProperties( THIS_ VARIANT FAR *pvMandatoryProperties )
{
    HRESULT hr = S_OK;

    hr = MakeVariantFromPropList(
            _lpMandatoryAttributes,
            _dwNumberOfMandatoryAttributes,
            pvMandatoryProperties
            );
    RRETURN_EXP_IF_ERR(hr);
}

STDMETHODIMP
CNDSClass::put_MandatoryProperties( THIS_ VARIANT vMandatoryProperties )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_DerivedFrom( THIS_ VARIANT FAR *pvDerivedFrom )
{
    HRESULT hr = S_OK;

    hr = MakeVariantFromPropList(
            _lpSuperClasses,
            _dwNumberOfSuperClasses,
            pvDerivedFrom
            );
    RRETURN_EXP_IF_ERR(hr);
}

STDMETHODIMP
CNDSClass::put_DerivedFrom( THIS_ VARIANT vDerivedFrom )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_AuxDerivedFrom( THIS_ VARIANT FAR *pvAuxDerivedFrom )
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}

STDMETHODIMP
CNDSClass::put_AuxDerivedFrom( THIS_ VARIANT vAuxDerivedFrom )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_PossibleSuperiors( THIS_ VARIANT FAR *pvPossSuperiors)
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}

STDMETHODIMP
CNDSClass::put_PossibleSuperiors( THIS_ VARIANT vPossSuperiors )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_Containment( THIS_ VARIANT FAR *pvContainment )
{
    HRESULT hr = S_OK;

    hr = MakeVariantFromPropList(
            _lpContainmentClasses,
            _dwNumberOfContainmentClasses,
            pvContainment
            );
    RRETURN_EXP_IF_ERR(hr);
}

STDMETHODIMP
CNDSClass::put_Containment( THIS_ VARIANT vContainment )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_Container( THIS_ VARIANT_BOOL FAR *pfContainer )
{
    if (_dwFlags & NDS_CONTAINER_CLASS) {
        *pfContainer = VARIANT_TRUE;
    }else {
        *pfContainer = VARIANT_FALSE;
    }
    RRETURN(S_OK);
}

STDMETHODIMP
CNDSClass::put_Container( THIS_ VARIANT_BOOL fContainer )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_HelpFileName( THIS_ BSTR FAR *pbstrHelpFileName )
{
    if ( !pbstrHelpFileName )
        RRETURN_EXP_IF_ERR(E_ADS_BAD_PARAMETER);
    
    HRESULT hr;
    hr = ADsAllocString( _bstrHelpFileName, pbstrHelpFileName );
    RRETURN_EXP_IF_ERR(hr);
}

STDMETHODIMP
CNDSClass::put_HelpFileName( THIS_ BSTR bstrHelpFile )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::get_HelpFileContext( THIS_ long FAR *plHelpContext )
{
    if ( !plHelpContext )
        RRETURN_EXP_IF_ERR(E_ADS_BAD_PARAMETER);

    *plHelpContext = _lHelpFileContext;
    RRETURN(S_OK);
}

STDMETHODIMP
CNDSClass::put_HelpFileContext( THIS_ long lHelpContext )
{
    RRETURN_EXP_IF_ERR(E_ADS_PROPERTY_NOT_SUPPORTED);
}

STDMETHODIMP
CNDSClass::Qualifiers(THIS_ IADsCollection FAR* FAR* ppQualifiers)
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}



PPROPENTRY
CreatePropertyList(
    LPWSTR_LIST  lpStringList
    )
{
    PPROPENTRY pStart = NULL;
    PPROPENTRY pPropEntry = NULL;

    while (lpStringList) {

        pPropEntry = CreatePropertyEntry(
                            lpStringList->szString,
                            0
                            );

        if (!pPropEntry) {
            goto error;
        }

        pPropEntry->pNext = pStart;
        pStart = pPropEntry;

        lpStringList = lpStringList->Next;
    }

error:
    return(pStart);
}

HRESULT
MakeVariantFromPropList(
    PPROPENTRY pPropList,
    DWORD dwNumEntries,
    VARIANT * pVarList
    )
{
    SAFEARRAYBOUND sabNewArray;
    SAFEARRAY * pFilter = NULL;
    HRESULT hr = S_OK;
    DWORD dwSLBound = 0;
    DWORD dwSUBound = 0;
    DWORD i = 0;
    VARIANT v;

    VariantInit(pVarList);

    sabNewArray.cElements = dwNumEntries;
    sabNewArray.lLbound = 0;

    pFilter = SafeArrayCreate(
                    VT_VARIANT,
                    1,
                    &sabNewArray
                    );

    if (!pFilter) {
        hr = E_OUTOFMEMORY;
        BAIL_ON_FAILURE(hr);
    }

    for (i = dwSLBound; i < (dwSLBound + dwNumEntries); i++) {
        VariantInit(&v);
        V_VT(&v) = VT_BSTR;

        V_BSTR(&v) = SysAllocString(pPropList->pszPropName);

        hr = SafeArrayPutElement(
                pFilter,
                (long*)&i,
                (void *)&v
                );
        VariantClear(&v);
        BAIL_ON_FAILURE(hr);

        pPropList = pPropList->pNext;

    }

    V_VT(pVarList) = VT_ARRAY | VT_VARIANT;
    V_ARRAY(pVarList) = pFilter;

    RRETURN(S_OK);

error:

    if (pFilter) {
        SafeArrayDestroy(pFilter);
    }

    RRETURN_EXP_IF_ERR(hr);
}


STDMETHODIMP
CNDSClass::get_OptionalProperties( THIS_ VARIANT FAR *retval )
{
    HRESULT hr = S_OK;

    hr = MakeVariantFromPropList(
            _lpOptionalAttributes,
            _dwNumberOfOptionalAttributes,
            retval
            );
    RRETURN_EXP_IF_ERR(hr);
}

STDMETHODIMP
CNDSClass::put_OptionalProperties( THIS_ VARIANT vOptionalProperties )
{

    HRESULT hr = E_NOTIMPL;

    RRETURN_EXP_IF_ERR(hr);
}

STDMETHODIMP
CNDSClass::get_NamingProperties( THIS_ VARIANT FAR *retval )
{
    HRESULT hr = S_OK;

    hr = MakeVariantFromPropList(
            _lpNamingAttributes,
            _dwNumberOfNamingAttributes,
            retval
            );
    RRETURN_EXP_IF_ERR(hr);
}

STDMETHODIMP
CNDSClass::put_NamingProperties( THIS_ VARIANT vNamingProperties )
{
    RRETURN_EXP_IF_ERR(E_NOTIMPL);
}
