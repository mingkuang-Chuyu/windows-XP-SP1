/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    traninfo.cpp

Abstract:

    Translation information of MSMQ 1.0 properties into MSMQ 2.0 attributes

Author:

    ronit hartmann ( ronith)

--*/
#include "ds_stdh.h"
#include "mqads.h"
#include "mqprops.h"
#include "mqattrib.h"
#include "_mqini.h"
#include "tranrout.h"
#include "xlatqm.h"
#include "traninfo.h"

#include "traninfo.tmh"

static WCHAR *s_FN=L"mqad/traninfo";
GUID guidNull = {0,0,0,{0,0,0,0,0,0,0,0}};

//----------------------------------------------------------
//  defaultVARIANT
//
//  This structure is equivalent in size and order of variables 
//  to MQPROPVARIANT.
//
//  MQPROPVARIANT contains a union, and the size first member of
//  the union is smaller than other members of the union.
//  Therefore MQPROVARIANT cannot be initialized at compile time
//  with union members other than the smallest one.
//
//----------------------------------------------------------
struct defaultVARIANT {
    VARTYPE vt;
    WORD wReserved1;
    WORD wReserved2;
    WORD wReserved3;
    ULONG_PTR l1;
    ULONG_PTR l2;
};

C_ASSERT(sizeof(defaultVARIANT) == sizeof(MQPROPVARIANT));
C_ASSERT(FIELD_OFFSET(defaultVARIANT, l1) == FIELD_OFFSET(MQPROPVARIANT, caub.cElems));
C_ASSERT(FIELD_OFFSET(defaultVARIANT, l2) == FIELD_OFFSET(MQPROPVARIANT, caub.pElems));

//
//      Default values for queue properties
//
const defaultVARIANT varDefaultQType = { VT_CLSID, 0,0,0, (LONG_PTR)&guidNull, 0};
const defaultVARIANT varDefaultQJournal = { VT_UI1, 0,0,0, DEFAULT_Q_JOURNAL, 0};
const defaultVARIANT varDefaultQQuota = { VT_UI4, 0,0,0, DEFAULT_Q_QUOTA, 0};
const defaultVARIANT varDefaultQBasePriority = { VT_I2, 0,0,0, DEFAULT_Q_BASEPRIORITY, 0};
const defaultVARIANT varDefaultQJQuota = { VT_UI4, 0,0,0, DEFAULT_Q_JOURNAL_QUOTA, 0};
const defaultVARIANT varDefaultQJLabel = { VT_LPWSTR, 0,0,0, (LONG_PTR)TEXT(""), 0};
const defaultVARIANT varDefaultQAuthenticate = { VT_UI1, 0,0,0, DEFAULT_Q_AUTHENTICATE, 0};
const defaultVARIANT varDefaultQPrivLevel = { VT_UI4, 0,0,0, DEFAULT_Q_PRIV_LEVEL, 0};
const defaultVARIANT varDefaultQTransaction = { VT_UI1, 0,0,0, DEFAULT_Q_TRANSACTION, 0};
const defaultVARIANT varDefaultQMulticastAddress = { VT_EMPTY, 0, 0, 0, 0, 0};

translateProp   QueueTranslateInfo[cQueueTranslateInfoSize] = {
// PROPID                | attribute-name               | vartype   | adstype                   | Translation routine                  | multivalue| InGC | default value                            | Set routine | Create routine | 
//-----------------------|------------------------------|-----------|---------------------------|--------------------------------------|-----------|------|------------------------------------------|-------------|----------------|
{PROPID_Q_INSTANCE       ,MQ_Q_INSTANCE_ATTRIBUTE       ,VT_CLSID   ,MQ_Q_INSTANCE_ADSTYPE      ,NULL                                  ,FALSE      ,TRUE  ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_TYPE           ,MQ_Q_TYPE_ATTRIBUTE           ,VT_CLSID   ,MQ_Q_TYPE_ADSTYPE          ,NULL                                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQType          ,NULL         ,NULL            },
{PROPID_Q_PATHNAME       ,NULL                          ,VT_LPWSTR  ,ADSTYPE_INVALID            ,MQADpRetrieveQueueName                ,FALSE      ,FALSE ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_JOURNAL        ,MQ_Q_JOURNAL_ATTRIBUTE        ,VT_UI1     ,MQ_Q_JOURNAL_ADSTYPE       ,NULL                                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQJournal       ,NULL         ,NULL            },
{PROPID_Q_QUOTA          ,MQ_Q_QUOTA_ATTRIBUTE          ,VT_UI4     ,MQ_Q_QUOTA_ADSTYPE         ,NULL                                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQQuota         ,NULL         ,NULL            },
{PROPID_Q_BASEPRIORITY   ,MQ_Q_BASEPRIORITY_ATTRIBUTE   ,VT_I2      ,MQ_Q_BASEPRIORITY_ADSTYPE  ,NULL                                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQBasePriority  ,NULL         ,NULL            },
{PROPID_Q_JOURNAL_QUOTA  ,MQ_Q_JOURNAL_QUOTA_ATTRIBUTE  ,VT_UI4     ,MQ_Q_JOURNAL_QUOTA_ADSTYPE ,NULL                                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQJQuota        ,NULL         ,NULL            },
{PROPID_Q_LABEL          ,MQ_Q_LABEL_ATTRIBUTE          ,VT_LPWSTR  ,MQ_Q_LABEL_ADSTYPE         ,NULL                                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQJLabel        ,NULL         ,NULL            },
{PROPID_Q_CREATE_TIME    ,MQ_Q_CREATE_TIME_ATTRIBUTE    ,VT_I4      ,MQ_Q_CREATE_TIME_ADSTYPE   ,NULL                                  ,FALSE      ,TRUE  ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_MODIFY_TIME    ,MQ_Q_MODIFY_TIME_ATTRIBUTE    ,VT_I4      ,MQ_Q_MODIFY_TIME_ADSTYPE   ,NULL                                  ,FALSE      ,TRUE  ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_AUTHENTICATE   ,MQ_Q_AUTHENTICATE_ATTRIBUTE   ,VT_UI1     ,MQ_Q_AUTHENTICATE_ADSTYPE  ,NULL                                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQAuthenticate  ,NULL         ,NULL            },
{PROPID_Q_PRIV_LEVEL     ,MQ_Q_PRIV_LEVEL_ATTRIBUTE     ,VT_UI4     ,MQ_Q_PRIV_LEVEL_ADSTYPE    ,NULL                                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQPrivLevel     ,NULL         ,NULL            },
{PROPID_Q_TRANSACTION    ,MQ_Q_TRANSACTION_ATTRIBUTE    ,VT_UI1     ,MQ_Q_TRANSACTION_ADSTYPE   ,NULL                                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQTransaction   ,NULL         ,NULL            },
{PROPID_Q_QMID           ,NULL                          ,VT_CLSID   ,ADSTYPE_INVALID            ,MQADpRetrieveQueueQMid                ,FALSE      ,FALSE ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_MASTERID       ,MQ_Q_MASTERID_ATTRIBUTE       ,VT_CLSID   ,MQ_Q_MASTERID_ADSTYPE      ,NULL                                  ,FALSE      ,TRUE  ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_SECURITY       ,MQ_Q_SECURITY_ATTRIBUTE       ,VT_BLOB    ,MQ_Q_SECURITY_ADSTYPE      ,NULL                                  ,FALSE      ,TRUE  ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_NT4ID          ,MQ_Q_NT4ID_ATTRIBUTE          ,VT_CLSID   ,MQ_Q_NT4ID_ADSTYPE         ,NULL                                  ,FALSE      ,TRUE  ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_DONOTHING      ,NULL                          ,VT_UI1     ,ADSTYPE_INVALID            ,NULL                                  ,FALSE      ,FALSE ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_FULL_PATH      ,MQ_Q_FULL_PATH_ATTRIBUTE      ,VT_LPWSTR  ,MQ_Q_FULL_PATH_ADSTYPE     ,NULL                                  ,FALSE      ,TRUE  ,NULL                                      ,NULL         ,NULL            },            
{PROPID_Q_NAME_SUFFIX    ,MQ_Q_NAME_EXT                 ,VT_LPWSTR  ,MQ_Q_NAME_EXT_ADSTYPE      ,NULL                                  ,FALSE      ,TRUE  ,NULL                                      ,NULL         ,NULL            },            
{PROPID_Q_OBJ_SECURITY   ,MQ_Q_SECURITY_ATTRIBUTE       ,VT_BLOB    ,MQ_Q_SECURITY_ADSTYPE      ,NULL                                  ,FALSE      ,TRUE  ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_SECURITY_INFORMATION  ,NULL                   ,VT_UI1     ,ADSTYPE_INVALID            ,MQADpRetrieveNothing                  ,FALSE      ,FALSE ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_PATHNAME_DNS   ,NULL                          ,VT_LPWSTR  ,ADSTYPE_INVALID            ,MQADpRetrieveQueueDNSName             ,FALSE      ,FALSE ,NULL                                      ,NULL         ,NULL            },
{PROPID_Q_MULTICAST_ADDRESS,MQ_Q_MULTICAST_ADDRESS      ,VT_LPWSTR  ,MQ_Q_MULTICAST_ADDRESS_ADSTYPE,NULL                               ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMulticastAddress,NULL       ,NULL            },
{PROPID_Q_ADS_PATH       ,NULL                          ,VT_LPWSTR  ,ADSTYPE_INVALID            ,MQADpRetrieveQueueADSPath             ,FALSE      ,FALSE ,NULL                                      ,NULL         ,NULL            }
};

//
//      Default values for machine properties
//

const defaultVARIANT varDefaultQMService = { VT_UI4, 0,0,0, DEFAULT_N_SERVICE, 0};
const defaultVARIANT varDefaultQMServiceRout = { VT_UI1, 0,0,0, DEFAULT_N_SERVICE, 0};    //[adsrv]
const defaultVARIANT varDefaultQMServiceDs   = { VT_UI1, 0,0,0, DEFAULT_N_SERVICE, 0};
const defaultVARIANT varDefaultQMServiceDep  = { VT_UI1, 0,0,0, DEFAULT_N_SERVICE, 0};
const defaultVARIANT varDefaultQMInFrs = { VT_CLSID|VT_VECTOR, 0,0,0, 0, 0};
const defaultVARIANT varDefaultQMOutFrs = { VT_CLSID|VT_VECTOR, 0,0,0, 0, 0};
const defaultVARIANT varDefaultQMQuota = { VT_UI4, 0,0,0, DEFAULT_QM_QUOTA, 0};
const defaultVARIANT varDefaultQMJQuota = { VT_UI4, 0,0,0, DEFAULT_QM_JOURNAL_QUOTA, 0};
const defaultVARIANT varDefaultQMForeign = { VT_UI1, 0,0,0, DEFAULT_QM_FOREIGN, 0};
const defaultVARIANT varDefaultQMOs = { VT_UI4, 0,0,0, DEFAULT_QM_OS, 0};
const defaultVARIANT varDefaultQMMType = { VT_LPWSTR, 0,0,0, (LONG_PTR)TEXT(""), 0};
const defaultVARIANT varDefaultQMSignPk = { VT_BLOB, 0,0,0, 0, 0};
const defaultVARIANT varDefaultQMEncryptPk = { VT_BLOB, 0,0,0, 0, 0};
const defaultVARIANT varDefaultQMSiteIDs = { VT_CLSID|VT_VECTOR, 0,0,0, 0, 0};
const defaultVARIANT varDefaultQMDescription = { VT_LPWSTR, 0,0,0, (LONG_PTR)TEXT(""), 0};


translateProp   MachineTranslateInfo[cMachineTranslateInfoSize] = {
// PROPID                | attribute-name               | vartype           | adstype                   | Translation routine                    | multivalue| InGC | default value                         | Set routine            | Create routine          |
//-----------------------|------------------------------|-------------------|---------------------------|----------------------------------------|-----------|------|---------------------------------------|------------------------|-------------------------|
{PROPID_QM_SITE_ID       ,NULL                          ,VT_CLSID           ,ADSTYPE_INVALID            ,MQADpRetrieveMachineSite                ,FALSE      ,FALSE ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_MACHINE_ID    ,MQ_QM_ID_ATTRIBUTE            ,VT_CLSID           ,MQ_QM_ID_ADSTYPE           ,NULL                                    ,FALSE      ,TRUE  ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_PATHNAME      ,MQ_QM_PATHNAME_ATTRIBUTE      ,VT_LPWSTR          ,ADSTYPE_INVALID            ,MQADpRetrieveMachineName                ,FALSE      ,FALSE ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_OUTFRS        ,NULL                          ,VT_CLSID|VT_VECTOR ,ADSTYPE_INVALID            ,MQADpRetrieveMachineOutFrs              ,FALSE      ,FALSE ,NULL                                   ,MQADpSetMachineOutFrss  ,MQADpCreateMachineOutFrss},
{PROPID_QM_INFRS         ,NULL                          ,VT_CLSID|VT_VECTOR ,ADSTYPE_INVALID            ,MQADpRetrieveMachineInFrs               ,FALSE      ,FALSE ,NULL                                   ,MQADpSetMachineInFrss   ,MQADpCreateMachineInFrss },
{PROPID_QM_SERVICE       ,MQ_QM_SERVICE_ATTRIBUTE       ,VT_UI4             ,ADSTYPE_INVALID,            MQADpRetrieveQMService                  ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMService   ,MQADpSetMachineService  ,NULL                     },  
{PROPID_QM_OLDSERVICE    ,MQ_QM_SERVICE_ATTRIBUTE       ,VT_UI4             ,MQ_QM_SERVICE_ADSTYPE      ,NULL                                    ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMService   ,MQADpSetMachineService  ,NULL                     },
{PROPID_QM_SERVICE_DSSERVER   ,MQ_QM_SERVICE_DSSERVER_ATTRIBUTE    ,VT_UI1,MQ_QM_SERVICE_DSSERVER_ADSTYPE   ,NULL                                ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMServiceDs ,MQADpSetMachineServiceDs,NULL                     },
{PROPID_QM_SERVICE_ROUTING    ,MQ_QM_SERVICE_ROUTING_ATTRIBUTE     ,VT_UI1,MQ_QM_SERVICE_ROUTING_ADSTYPE    ,NULL                                ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMServiceRout,MQADpSetMachineServiceRout,NULL                  },
{PROPID_QM_SERVICE_DEPCLIENTS ,MQ_QM_SERVICE_DEPCLIENTS_ATTRIBUTE  ,VT_UI1,MQ_QM_SERVICE_DEPCLIENTS_ADSTYPE ,NULL                                ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMServiceDep  ,NULL                  ,NULL                     },
{PROPID_QM_MASTERID      ,MQ_QM_MASTERID_ATTRIBUTE      ,VT_CLSID           ,MQ_QM_MASTERID_ADSTYPE     ,NULL						             ,FALSE      ,TRUE  ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_QUOTA         ,MQ_QM_QUOTA_ATTRIBUTE         ,VT_UI4             ,MQ_QM_QUOTA_ADSTYPE        ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultQMQuota     ,NULL                    ,NULL                     },
{PROPID_QM_JOURNAL_QUOTA ,MQ_QM_JOURNAL_QUOTA_ATTRIBUTE ,VT_UI4             ,MQ_QM_JOURNAL_QUOTA_ADSTYPE,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultQMJQuota    ,NULL                    ,NULL                     },
{PROPID_QM_MACHINE_TYPE  ,MQ_QM_MACHINE_TYPE_ATTRIBUTE  ,VT_LPWSTR          ,MQ_QM_MACHINE_TYPE_ADSTYPE ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultQMMType     ,NULL                    ,NULL                     },
{PROPID_QM_CREATE_TIME   ,MQ_QM_CREATE_TIME_ATTRIBUTE   ,VT_I4              ,MQ_QM_CREATE_TIME_ADSTYPE  ,NULL                                    ,FALSE      ,FALSE ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_MODIFY_TIME   ,MQ_QM_MODIFY_TIME_ATTRIBUTE   ,VT_I4              ,MQ_QM_MODIFY_TIME_ADSTYPE  ,NULL                                    ,FALSE      ,FALSE ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_FOREIGN       ,MQ_QM_FOREIGN_ATTRIBUTE       ,VT_UI1             ,MQ_QM_FOREIGN_ADSTYPE      ,NULL                                    ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMForeign   ,NULL                    ,NULL                     },
{PROPID_QM_OS            ,MQ_QM_OS_ATTRIBUTE            ,VT_UI4             ,MQ_QM_OS_ADSTYPE           ,NULL                                    ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMOs        ,NULL                    ,NULL                     },
{PROPID_QM_SECURITY      ,MQ_QM_SECURITY_ATTRIBUTE      ,VT_BLOB            ,MQ_QM_SECURITY_ADSTYPE     ,NULL                                    ,FALSE      ,TRUE  ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_SIGN_PK       ,MQ_QM_SIGN_PK_ATTRIBUTE       ,VT_BLOB            ,MQ_QM_SIGN_PK_ADSTYPE      ,NULL                                    ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMSignPk    ,NULL                    ,NULL                     },
{PROPID_QM_ENCRYPT_PK    ,NULL                          ,VT_BLOB            ,ADSTYPE_INVALID            ,MQADpRetrieveMachineEncryptPk           ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMEncryptPk ,MQADpSetMachineEncryptPk,MQADpCreateMachineEncryptPk},
{PROPID_QM_FULL_PATH     ,MQ_QM_FULL_PATH_ATTRIBUTE     ,VT_LPWSTR          ,MQ_QM_FULL_PATH_ADSTYPE    ,NULL                                    ,FALSE      ,TRUE  ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_SITE_IDS      ,MQ_QM_SITES_ATTRIBUTE         ,VT_CLSID|VT_VECTOR ,MQ_QM_SITES_ADSTYPE        ,NULL                                    ,TRUE       ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMSiteIDs   ,NULL                    ,NULL                     },
{PROPID_QM_OUTFRS_DN     ,MQ_QM_OUTFRS_ATTRIBUTE        ,VT_LPWSTR|VT_VECTOR,MQ_QM_OUTFRS_ADSTYPE       ,NULL                                    ,TRUE       ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMOutFrs    ,NULL                    ,NULL                     },
{PROPID_QM_INFRS_DN      ,MQ_QM_INFRS_ATTRIBUTE         ,VT_LPWSTR|VT_VECTOR,MQ_QM_OUTFRS_ADSTYPE       ,NULL                                    ,TRUE       ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMInFrs     ,NULL                    ,NULL                     },
{PROPID_QM_NT4ID         ,MQ_QM_NT4ID_ATTRIBUTE         ,VT_CLSID           ,MQ_QM_NT4ID_ADSTYPE        ,NULL                                    ,FALSE      ,TRUE  ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_DONOTHING     ,NULL                          ,VT_UI1             ,ADSTYPE_INVALID            ,NULL                                    ,FALSE      ,FALSE ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_OBJ_SECURITY  ,MQ_QM_SECURITY_ATTRIBUTE      ,VT_BLOB            ,MQ_QM_SECURITY_ADSTYPE     ,NULL                                    ,FALSE      ,TRUE  ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_SECURITY_INFORMATION   ,NULL                 ,VT_UI1             ,ADSTYPE_INVALID            ,MQADpRetrieveNothing                    ,FALSE      ,FALSE ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_ENCRYPT_PKS   ,MQ_QM_ENCRYPT_PK_ATTRIBUTE    ,VT_BLOB            ,MQ_QM_ENCRYPT_PK_ADSTYPE   ,NULL                                    ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMEncryptPk ,NULL                    ,NULL                     },
{PROPID_QM_SIGN_PKS      ,MQ_QM_SIGN_PK_ATTRIBUTE       ,VT_BLOB            ,MQ_QM_SIGN_PK_ADSTYPE      ,NULL                                    ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMSignPk    ,NULL                    ,NULL                     },
{PROPID_QM_PATHNAME_DNS  ,NULL                          ,VT_LPWSTR          ,ADSTYPE_INVALID            ,MQADpRetrieveMachineDNSName             ,FALSE      ,FALSE ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_WORKGROUP_ID  ,MQ_QM_WORKGROUP_ID_ATTRIBUTE  ,VT_CLSID           ,MQ_QM_WORKGROUP_ID_ADSTYPE ,NULL                                    ,FALSE      ,TRUE  ,NULL                                   ,NULL                    ,NULL                     },
{PROPID_QM_DESCRIPTION   ,MQ_QM_DESCRIPTION_ATTRIBUTE   ,VT_LPWSTR          ,MQ_QM_DESCRIPTION_ADSTYPE  ,NULL                                    ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultQMDescription ,NULL                  ,NULL                     }
};   

 

 
//
//      Default values for enterprise properties
//

const defaultVARIANT varDefaultENameStyle = { VT_UI1, 0,0,0, DEFAULT_E_NAMESTYLE, 0};
const defaultVARIANT varDefaultECspName = { VT_LPWSTR, 0,0,0, (LONG_PTR)DEFAULT_E_DEFAULTCSP, 0};
const defaultVARIANT varDefaultELongLive = { VT_UI4, 0,0,0, MSMQ_DEFAULT_LONG_LIVE, 0};
const defaultVARIANT varDefaultEVersion = { VT_UI2, 0,0,0, DEFAULT_E_VERSION, 0};
const defaultVARIANT varDefaultEInterval1 = { VT_UI2, 0,0,0, DEFAULT_S_INTERVAL1, 0};
const defaultVARIANT varDefaultEInterval2 = { VT_UI2, 0,0,0, DEFAULT_S_INTERVAL2, 0};
                                                                                                                                                                                                                         
translateProp   EnterpriseTranslateInfo[cEnterpriseTranslateInfoSize] = {
// PROPID                | attribute-name               | vartype           | adstype                    | Translation routine                    | multivalue| InGC | default value                         | Set routine | Create routine |
//-----------------------|------------------------------|-------------------|----------------------------|----------------------------------------|-----------|------|---------------------------------------|-------------|----------------|
{PROPID_E_NAME           ,NULL                          ,VT_LPWSTR          ,ADSTYPE_INVALID             ,MQADpRetrieveEnterpriseName             ,FALSE      ,FALSE ,NULL                                   ,NULL         ,NULL            },
{PROPID_E_NAMESTYLE      ,MQ_E_NAMESTYLE_ATTRIBUTE      ,VT_UI1             ,MQ_E_NAMESTYLE_ADSTYPE      ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultENameStyle  ,NULL         ,NULL            },
{PROPID_E_CSP_NAME       ,MQ_E_CSP_NAME_ATTRIBUTE       ,VT_LPWSTR          ,MQ_E_CSP_NAME_ADSTYPE       ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultECspName    ,NULL         ,NULL            },
{PROPID_E_S_INTERVAL1    ,MQ_E_INTERVAL1                ,VT_UI2             ,MQ_E_INTERVAL1_ADSTYPE      ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultEInterval1  ,NULL         ,NULL            },
{PROPID_E_S_INTERVAL2    ,MQ_E_INTERVAL2                ,VT_UI2             ,MQ_E_INTERVAL2_ADSTYPE      ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultEInterval2  ,NULL         ,NULL            },
{PROPID_E_ID             ,MQ_E_ID_ATTRIBUTE             ,VT_CLSID           ,MQ_E_ID_ADSTYPE             ,NULL                                    ,FALSE      ,FALSE ,NULL                                   ,NULL         ,NULL            },
{PROPID_E_LONG_LIVE      ,MQ_E_LONG_LIVE_ATTRIBUTE      ,VT_UI4             ,MQ_E_LONG_LIVE_ADSTYPE      ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultELongLive   ,NULL         ,NULL            },
{PROPID_E_VERSION        ,MQ_E_VERSION_ATTRIBUTE        ,VT_UI2             ,MQ_E_VERSION_ADSTYPE        ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultEVersion    ,NULL         ,NULL            },
{PROPID_E_NT4ID          ,MQ_E_NT4ID_ATTRIBUTE          ,VT_CLSID           ,MQ_E_NT4ID_ADSTYPE          ,NULL                                    ,FALSE      ,FALSE ,NULL                                   ,NULL         ,NULL            },
{PROPID_E_SECURITY       ,MQ_E_SECURITY_ATTRIBUTE       ,VT_BLOB            ,MQ_E_SECURITY_ADSTYPE       ,NULL                                    ,FALSE      ,FALSE ,NULL                                   ,NULL         ,NULL            }
};


const defaultVARIANT varDefaultLGatesDN = { VT_LPWSTR|VT_VECTOR, 0,0,0, 0, 0};
const defaultVARIANT varDefaultLGates = { VT_CLSID|VT_VECTOR, 0,0,0, 0, 0};
const defaultVARIANT varDefaultLDescription = { VT_LPWSTR, 0,0,0, (LONG_PTR)TEXT(""), 0};

translateProp   SiteLinkTranslateInfo[cSiteLinkTranslateInfoSize] = {
// PROPID                | attribute-name               | vartype           | adstype                   | Translation routine                    | multivalue| InGC | default value                  | Set routine           | Creare routine          |
//-----------------------|------------------------------|-------------------|---------------------------|----------------------------------------|-----------|------|--------------------------------|-----------------------|-------------------------|
{PROPID_L_NEIGHBOR1      ,NULL                          ,VT_CLSID           ,ADSTYPE_INVALID            ,MQADpRetrieveLinkNeighbor1              ,FALSE      ,FALSE ,NULL                            ,MQADpSetLinkNeighbor1  ,MQADpCreateLinkNeighbor1},
{PROPID_L_NEIGHBOR2      ,NULL                          ,VT_CLSID           ,ADSTYPE_INVALID            ,MQADpRetrieveLinkNeighbor2              ,FALSE      ,FALSE ,NULL                            ,MQADpSetLinkNeighbor2  ,MQADpCreateLinkNeighbor2},
{PROPID_L_COST           ,NULL                          ,VT_UI4             ,ADSTYPE_INVALID            ,MQADpRetrieveLinkCost                   ,FALSE      ,FALSE ,NULL                            ,MQADpSetLinkCost       ,MQADpCreateLinkCost     },
{PROPID_L_ID             ,MQ_L_ID_ATTRIBUTE             ,VT_CLSID           ,MQ_L_ID_ADSTYPE            ,NULL                                    ,FALSE      ,FALSE ,NULL                            ,NULL                   ,NULL                     },
{PROPID_L_GATES_DN       ,MQ_L_SITEGATES_ATTRIBUTE      ,VT_LPWSTR|VT_VECTOR,MQ_L_SITEGATES_ADSTYPE     ,NULL                                    ,TRUE       ,FALSE ,(MQPROPVARIANT*)&varDefaultLGatesDN,NULL                ,NULL                     },
{PROPID_L_NEIGHBOR1_DN   ,MQ_L_NEIGHBOR1_ATTRIBUTE      ,VT_LPWSTR          ,MQ_L_NEIGHBOR1_ADSTYPE     ,NULL                                    ,FALSE      ,FALSE ,NULL                            ,NULL                   ,NULL                     },
{PROPID_L_NEIGHBOR2_DN   ,MQ_L_NEIGHBOR2_ATTRIBUTE      ,VT_LPWSTR          ,MQ_L_NEIGHBOR2_ADSTYPE     ,NULL                                    ,FALSE      ,FALSE ,NULL                            ,NULL                   ,NULL                     },
{PROPID_L_DESCRIPTION    ,MQ_L_DESCRIPTION_ATTRIBUTE    ,VT_LPWSTR          ,MQ_L_DESCRIPTION_ADSTYPE   ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultLDescription,NULL            ,NULL                     },
{PROPID_L_FULL_PATH      ,MQ_L_FULL_PATH_ATTRIBUTE      ,VT_LPWSTR          ,MQ_L_FULL_PATH_ADSTYPE     ,NULL                                    ,FALSE      ,TRUE  ,NULL                            ,NULL                   ,NULL                     },            
{PROPID_L_ACTUAL_COST    ,MQ_L_COST_ATTRIBUTE           ,VT_UI4             ,MQ_L_COST_ADSTYPE          ,NULL                                    ,FALSE      ,FALSE ,NULL                            ,NULL                   ,NULL                     },
{PROPID_L_GATES          ,NULL                          ,VT_CLSID|VT_VECTOR ,ADSTYPE_INVALID            ,MQADpRetrieveLinkGates                  ,TRUE       ,FALSE ,(MQPROPVARIANT*)&varDefaultLGates,NULL                  ,NULL                     }
};


const defaultVARIANT varDefaultUserSignCert = { VT_BLOB, 0,0,0, 0, 0};
const defaultVARIANT varDefaultUserDigest = { VT_VECTOR | VT_CLSID, 0,0,0,0,0};

translateProp   UserTranslateInfo[ cUserTranslateInfoSize] = {
// PROPID                | attribute-name               | vartype           | adstype                   | Translation routine                    | multivalue| InGC | default value                          | Set routine | Create routine |
//-----------------------|------------------------------|-------------------|---------------------------|----------------------------------------|-----------|------|----------------------------------------|-------------|----------------|
{PROPID_U_SID            ,MQ_U_SID_ATTRIBUTE            ,VT_BLOB            ,MQ_U_SID_ADSTYPE           ,NULL                                    ,FALSE      ,TRUE  ,NULL                                    ,NULL         ,NULL            },
{PROPID_U_SIGN_CERT      ,MQ_U_SIGN_CERT_ATTRIBUTE      ,VT_BLOB            ,MQ_U_SIGN_CERT_ADSTYPE     ,NULL                                    ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultUserSignCert ,NULL         ,NULL            },
{PROPID_U_DIGEST         ,MQ_U_DIGEST_ATTRIBUTE         ,VT_CLSID|VT_VECTOR ,MQ_U_DIGEST_ADSTYPE        ,NULL                                    ,TRUE       ,TRUE  ,(MQPROPVARIANT*)&varDefaultUserDigest   ,NULL         ,NULL            },
{PROPID_U_ID             ,MQ_U_ID_ATTRIBUTE             ,VT_CLSID           ,MQ_U_ID_ADSTYPE            ,NULL                                    ,FALSE      ,TRUE  ,NULL                                    ,NULL         ,NULL            }
};

translateProp   MQUserTranslateInfo[cMQUserTranslateInfoSize] = {
// PROPID                | attribute-name               | vartype           | adstype                   | Translation routine                    | multivalue| InGC | default value                          | Set routine | Create routine |
//-----------------------|------------------------------|-------------------|---------------------------|----------------------------------------|-----------|------|----------------------------------------|-------------|----------------|
{PROPID_MQU_SID          ,MQ_MQU_SID_ATTRIBUTE          ,VT_BLOB            ,MQ_MQU_SID_ADSTYPE         ,NULL                                     ,FALSE     ,TRUE  ,NULL                                    ,NULL         ,NULL            },
{PROPID_MQU_SIGN_CERT    ,MQ_MQU_SIGN_CERT_ATTRIBUTE    ,VT_BLOB            ,MQ_MQU_SIGN_CERT_ADSTYPE   ,NULL                                     ,FALSE     ,TRUE  ,(MQPROPVARIANT*)&varDefaultUserSignCert ,NULL         ,NULL            },
{PROPID_MQU_DIGEST       ,MQ_MQU_DIGEST_ATTRIBUTE       ,VT_CLSID|VT_VECTOR ,MQ_MQU_DIGEST_ADSTYPE      ,NULL                                     ,TRUE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultUserDigest   ,NULL         ,NULL            },
{PROPID_MQU_ID           ,MQ_MQU_ID_ATTRIBUTE           ,VT_CLSID           ,MQ_MQU_ID_ADSTYPE          ,NULL                                     ,FALSE     ,TRUE  ,NULL                                    ,NULL         ,NULL            },
{PROPID_MQU_SECURITY     ,MQ_MQU_SECURITY_ATTRIBUTE     ,VT_BLOB            ,MQ_MQU_SECURITY_ADSTYPE    ,NULL                                     ,FALSE     ,TRUE  ,NULL                                    ,NULL         ,NULL            }
};

const defaultVARIANT varDefaultSForeign = { VT_UI1, 0,0,0, 0, 0};
const defaultVARIANT varDefaultSInterval1 = { VT_UI2, 0,0,0, DEFAULT_S_INTERVAL1, 0};
const defaultVARIANT varDefaultSInterval2 = { VT_UI2, 0,0,0, DEFAULT_S_INTERVAL2, 0};


translateProp   SiteTranslateInfo[cSiteTranslateInfoSize] = {
// PROPID                | attribute-name               | vartype           | adstype                   | Translation routine                    | multivalue| InGC | default value                     | Set routine | Create routine |
//-----------------------|------------------------------|-------------------|---------------------------|----------------------------------------|-----------|------|-----------------------------------|-------------|----------------|
{PROPID_S_PATHNAME       ,MQ_S_NAME_ATTRIBUTE           ,VT_LPWSTR          ,MQ_S_NAME_ADSTYPE          ,NULL                                    ,FALSE      ,FALSE ,NULL                               ,NULL         ,NULL            },
{PROPID_S_SITEID         ,MQ_S_ID_ATTRIBUTE             ,VT_CLSID           ,MQ_S_ID_ADSTYPE            ,NULL                                    ,FALSE      ,FALSE ,NULL                               ,NULL         ,NULL            },
{PROPID_S_INTERVAL1      ,MQ_S_INTERVAL1                ,VT_UI2             ,MQ_S_INTERVAL1_ADSTYPE     ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultSInterval1,NULL       ,NULL            },
{PROPID_S_INTERVAL2      ,MQ_S_INTERVAL2                ,VT_UI2             ,MQ_S_INTERVAL2_ADSTYPE     ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultSInterval2,NULL       ,NULL            },
{PROPID_S_SECURITY       ,MQ_S_SECURITY_ATRRIBUTE       ,VT_BLOB            ,MQ_S_SECURITY_ADSTYPE      ,NULL                                    ,FALSE      ,FALSE ,NULL                               ,NULL         ,NULL            },
{PROPID_S_FULL_NAME      ,MQ_S_FULL_NAME_ATTRIBUTE      ,VT_LPWSTR          ,MQ_S_FULL_NAME_ADSTYPE     ,NULL                                    ,FALSE      ,FALSE ,NULL                               ,NULL         ,NULL            },
{PROPID_S_NT4_STUB       ,MQ_S_NT4_STUB_ATTRIBUTE       ,VT_UI2             ,MQ_S_NT4_STUB_ADSTYPE      ,NULL                                    ,FALSE      ,FALSE ,NULL                               ,NULL         ,NULL            },
{PROPID_S_FOREIGN        ,MQ_S_FOREIGN_ATTRIBUTE        ,VT_UI1             ,MQ_S_FOREIGN_ADSTYPE       ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultSForeign,NULL         ,NULL            },                                         
{PROPID_S_DONOTHING      ,NULL                          ,VT_UI1             ,ADSTYPE_INVALID            ,NULL                                    ,FALSE      ,FALSE ,NULL                               ,NULL         ,NULL            },
{PROPID_S_SECURITY_INFORMATION  ,NULL                   ,VT_UI1             ,ADSTYPE_INVALID            ,MQADpRetrieveNothing                    ,FALSE      ,FALSE ,NULL                               ,NULL         ,NULL            }
};


translateProp   ServerTranslateInfo[cServerTranslateInfoSize] = {
// PROPID                | attribute-name               | vartype           | adstype                   | Translation routine                    | multivalue| default value | Set routine | Create routine |
//-----------------------|------------------------------|-------------------|---------------------------|----------------------------------------|-----------|---------------|-------------|----------------|
{PROPID_SRV_NAME         ,MQ_SRV_NAME_ATTRIBUTE         ,VT_LPWSTR          ,MQ_SRV_NAME_ADSTYPE        ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SRV_ID           ,MQ_SRV_ID_ATTRIBUTE           ,VT_CLSID           ,MQ_SRV_ID_ADSTYPE          ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            },                              
{PROPID_SRV_FULL_PATH    ,MQ_SRV_FULL_PATH_ATTRIBUTE    ,VT_LPWSTR          ,MQ_SRV_FULL_PATH_ADSTYPE   ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            }                              
};

translateProp   SettingTranslateInfo[cSettingTranslateInfoSize] = {
// PROPID                | attribute-name               | vartype           | adstype                   | Translation routine                    | multivalue| default value | Set routine | Create routine |
//-----------------------|------------------------------|-------------------|---------------------------|----------------------------------------|-----------|---------------|-------------|----------------|
{PROPID_SET_NAME         ,MQ_SET_NAME_ATTRIBUTE         ,VT_LPWSTR          ,MQ_SET_NAME_ADSTYPE        ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SET_OLDSERVICE    ,MQ_SET_SERVICE_ATTRIBUTE      ,VT_UI4             ,MQ_SET_SERVICE_ADSTYPE     ,NULL                                   ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SET_SERVICE_ROUTING   ,MQ_SET_SERVICE_ROUTING_ATTRIBUTE   ,VT_UI1 ,MQ_SET_SERVICE_ROUTING_ADSTYPE     ,NULL                              ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SET_SERVICE_DSSERVER  ,MQ_SET_SERVICE_DSSERVER_ATTRIBUTE  ,VT_UI1 ,MQ_SET_SERVICE_DSSERVER_ADSTYPE    ,NULL                              ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SET_SERVICE_DEPCLIENTS,MQ_SET_SERVICE_DEPCLIENTS_ATTRIBUTE,VT_UI1 ,MQ_SET_SERVICE_DEPCLIENTS_ADSTYPE  ,NULL                              ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SET_QM_ID        ,MQ_SET_QM_ID_ATTRIBUTE        ,VT_CLSID           ,MQ_SET_QM_ID_ADSTYPE       ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SET_APPLICATION  ,MQ_SET_APPLICATION_ATTRIBUTE  ,VT_LPWSTR          ,MQ_SET_QM_ID_ADSTYPE       ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SET_FULL_PATH    ,MQ_SET_FULL_PATH_ATTRIBUTE    ,VT_LPWSTR          ,MQ_SET_FULL_PATH_ADSTYPE   ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            },          
{PROPID_SET_NT4          ,MQ_SET_NT4_ATTRIBUTE          ,VT_UI4             ,MQ_SET_NT4_ADSTYPE         ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SET_MASTERID     ,MQ_SET_MASTERID_ATTRIBUTE     ,VT_CLSID           ,MQ_SET_MASTERID_ADSTYPE    ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            },
{PROPID_SET_SITENAME     ,MQ_SET_SITENAME_ATTRIBUTE     ,VT_LPWSTR          ,MQ_SET_SITENAME_ADSTYPE    ,NULL                                    ,FALSE      ,NULL           ,NULL         ,NULL            }
};

const defaultVARIANT varDefaultComVersion = { VT_LPWSTR, 0,0,0, (LONG_PTR)TEXT(""), 0};

translateProp ComputerTranslateInfo[cComputerTranslateInfoSize] = {
// PROPID                   | attribute-name                   | vartype           | adstype                       | Translation routine                    | multivalue| InGC | default value                          | Set routine | Create routine |
//--------------------------|----------------------------------|-------------------|-------------------------------|----------------------------------------|-----------|------|----------------------------------------|-------------|----------------|
{PROPID_COM_FULL_PATH       ,MQ_COM_FULL_PATH_ATTRIBUTE        ,VT_LPWSTR          ,MQ_COM_FULL_PATH_ADSTYPE       ,NULL                                    ,FALSE      ,FALSE ,NULL                                    ,NULL         ,NULL            },
{PROPID_COM_SAM_ACCOUNT     ,MQ_COM_SAM_ACCOUNT_ATTRIBUTE      ,VT_LPWSTR          ,MQ_COM_SAM_ACCOUNT_ADSTYPE     ,NULL                                    ,FALSE      ,FALSE ,NULL                                    ,NULL         ,NULL            },
{PROPID_COM_CONTAINER       ,NULL                              ,VT_LPWSTR          ,ADSTYPE_INVALID                ,NULL                                    ,FALSE      ,FALSE ,NULL                                    ,NULL         ,NULL            },
{PROPID_COM_ACCOUNT_CONTROL ,MQ_COM_ACCOUNT_CONTROL_ATTRIBUTE  ,VT_UI4             ,MQ_COM_ACCOUNT_CONTROL_ADSTYPE ,NULL                                    ,FALSE      ,FALSE ,NULL                                    ,NULL         ,NULL            },
{PROPID_COM_DNS_HOSTNAME    ,MQ_COM_DNS_HOSTNAME_ATTRIBUTE     ,VT_LPWSTR          ,MQ_COM_DNS_HOSTNAME_ADSTYPE    ,NULL                                    ,FALSE      ,FALSE ,NULL                                    ,NULL         ,NULL            },
{PROPID_COM_SID             ,MQ_COM_SID_ATTRIBUTE              ,VT_BLOB            ,MQ_COM_SID_ADSTYPE             ,NULL                                    ,FALSE      ,FALSE ,NULL                                    ,NULL         ,NULL            },
{PROPID_COM_SIGN_CERT       ,MQ_COM_SIGN_CERT_ATTRIBUTE        ,VT_BLOB            ,MQ_COM_SIGN_CERT_ADSTYPE       ,NULL                                    ,FALSE      ,TRUE  ,(MQPROPVARIANT*)&varDefaultUserSignCert ,NULL         ,NULL            },
{PROPID_COM_DIGEST          ,MQ_COM_DIGEST_ATTRIBUTE           ,VT_CLSID|VT_VECTOR ,MQ_COM_DIGEST_ADSTYPE          ,NULL                                    ,TRUE       ,TRUE  ,(MQPROPVARIANT*)&varDefaultUserDigest   ,NULL         ,NULL            },
{PROPID_COM_ID              ,MQ_COM_ID_ATTRIBUTE               ,VT_CLSID           ,MQ_COM_ID_ADSTYPE              ,NULL                                    ,FALSE      ,TRUE  ,NULL                                    ,NULL         ,NULL            },
{PROPID_COM_VERSION         ,MQ_COM_VERSION_ATTRIBUTE          ,VT_LPWSTR          ,MQ_COM_VERSION_ADSTYPE         ,NULL                                    ,FALSE      ,FALSE ,(MQPROPVARIANT*)&varDefaultComVersion   ,NULL         ,NULL            }
};

//-----------------------------------------------------
// Helper macro to get the number of elements in a static array
//
//-----------------------------------------------------
#define ARRAY_SIZE(array)   (sizeof(array)/sizeof(array[0]))

