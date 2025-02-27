/******************************************************************
   SNetScal.cpp -- WBEM provider class implementation

   MODULE:
        DhcpProv.dll

   DESCRIPTION:
        Contains: the definition of the DHCP_Subnet class,
        the static table of manageable objects.

   REVISION:
        08/03/98 - created

******************************************************************/
#include <stdafx.h>

#include "props.h"
#include "snetfn.h"
#include "resfn.h"
#include "srvfn.h"
#include "anetres.h"

// the name of the WBEM class
#define PROVIDER_NAME_DHCP_ASSOCIATIONSUBNETRESERVATION       "DHCP_SubnetReservation"

// the names of the properties
#define PROP_NAME_SUBNET        L"Subnet"
#define PROP_NAME_RESERVATION   L"Reservation"

// main class instantiation.
CDHCP_AssociationSubnetToReservation MyDHCP_Association_Subnet_Reservation (PROVIDER_NAME_DHCP_ASSOCIATIONSUBNETRESERVATION, PROVIDER_NAMESPACE_DHCP) ;

/*****************************************************************************
 *
 *  FUNCTION    :	CDHCP_AssociationSubnetToReservation::CDHCP_AssociationSubnetToReservation
 *
 *  DESCRIPTION :	Constructor
 *
 *  INPUTS      :	none
 *
 *  RETURNS     :	nothing
 *
 *  COMMENTS    :	Calls the Provider constructor.
 *
 *****************************************************************************/
CDHCP_AssociationSubnetToReservation::CDHCP_AssociationSubnetToReservation (const CHString& strName, LPCSTR pszNameSpace ) :
	Provider(strName, pszNameSpace)
{
}

/*****************************************************************************
 *
 *  FUNCTION    :	CDHCP_AssociationSubnetToReservation::~CDHCP_AssociationSubnetToReservation
 *
 *  DESCRIPTION :	Destructor
 *
 *  INPUTS      :	none
 *
 *  RETURNS     :	nothing
 *
 *  COMMENTS    : 
 *
 *****************************************************************************/
CDHCP_AssociationSubnetToReservation::~CDHCP_AssociationSubnetToReservation ()
{
}

/*****************************************************************************
*
*  FUNCTION    :	CDHCP_AssociationSubnetToReservation::EnumerateInstances
*
*  DESCRIPTION :	Returns all the instances of this class.
*
*  INPUTS      :	none
*
*  RETURNS     :	WBEM_S_NO_ERROR if successful
*
*  COMMENTS    :    Enumerates all this instances of this class. As there is only one
*                   DHCP Server per system, there is only one instance for this class
*
*****************************************************************************/
HRESULT CDHCP_AssociationSubnetToReservation::EnumerateInstances ( MethodContext* pMethodContext, long lFlags )
{
    CDHCP_Server_Parameters     srvParams;
    LPDHCP_MIB_INFO             pSrvMibInfo;

    if (srvParams.GetMibInfo(pSrvMibInfo, TRUE))
    {
        // loop through all the subnets configured on the local server
        for (int i = 0; i < pSrvMibInfo->Scopes; i++)
        {
            DWORD                   dwSubnet;
            DHCP_RESUME_HANDLE      ResumeHandle;

            dwSubnet = pSrvMibInfo->ScopeInfo[i].Subnet;

            CDHCP_Reservation_Parameters  resParams(dwSubnet, 0);

            ResumeHandle = 0;

            // for each subnet, loop through all the client buffers belonging to it
            do
            {
                HRESULT hRes = WBEM_S_NO_ERROR;

                // load the next buffer
				LONG t_Res = resParams.NextSubnetReservation(ResumeHandle) ;
                if ( t_Res < 0 )
                    return WBEM_E_FAILED;  // will fail here
				if ( t_Res == 0 )
					return WBEM_S_NO_ERROR ;

                // for the current buffer, loop through all the clients
                for (int j = 0; hRes == WBEM_S_NO_ERROR && j < resParams.m_pReservationInfoArray->NumElements; j++)
                {
                    // this is finally the info of the current client
                    LPDHCP_IP_RESERVATION_V4   pReservation = RESERVATION_CAST(resParams.m_pReservationInfoArray->Elements[j].Element.ReservedIp);
                    WCHAR                      wcsSubnet[256], wcsAddress[256]; // should be enough for holding 'xxx.yyy.zzz.uuu\0'

                    // build the str representation of the Subnet address
                    swprintf(wcsSubnet,  L"DHCP_Subnet.Address=\"%u.%u.%u.%u\"",
                                                        (dwSubnet & 0xff000000) >> 24,
                                                        (dwSubnet & 0x00ff0000) >> 16,
                                                        (dwSubnet & 0x0000ff00) >> 8,
                                                        (dwSubnet & 0x000000ff));

                    // build the str representation of the ClientIpAddress address
                    swprintf(wcsAddress, L"DHCP_Reservation.Subnet=\"%u.%u.%u.%u\",Address=\"%u.%u.%u.%u\"",
														(dwSubnet & 0xff000000) >> 24,
                                                        (dwSubnet & 0x00ff0000) >> 16,
                                                        (dwSubnet & 0x0000ff00) >> 8,
                                                        (dwSubnet & 0x000000ff) ,
														(pReservation->ReservedIpAddress & 0xff000000) >> 24,
                                                        (pReservation->ReservedIpAddress & 0x00ff0000) >> 16,
                                                        (pReservation->ReservedIpAddress & 0x0000ff00) >> 8,
                                                        (pReservation->ReservedIpAddress & 0x000000ff));

                    // we finally have everything we need for the creating one more instance
                    CInstance*  pInstance = CreateNewInstance(pMethodContext);

                    // initialize the instance with the key info and call LoadInstanceProperties for the rest of the info
                    if (pInstance->SetCHString(PROP_NAME_SUBNET, wcsSubnet) &&
                        pInstance->SetCHString(PROP_NAME_RESERVATION, wcsAddress))                      
                    {
                        hRes = Commit(pInstance);
                        // now everything relys on the err returned above.
                    }
                }

                // if there was an error above, bail
                if (hRes != WBEM_S_NO_ERROR)
                    return WBEM_E_FAILED;

            } while (ResumeHandle != 0);    // bail if ResumeHandle got back to 0 (the end was reached)
        }
    }

    return WBEM_S_NO_ERROR;
}

/*****************************************************************************
*
*  FUNCTION    :	CDHCP_AssociationSubnetToReservation::GetObject
*
*  DESCRIPTION :	Find a single instance based on the key properties for the
*					class. 
*
*  INPUTS      :	A pointer to a CInstance object containing the key properties. 
*
*  RETURNS     :	WBEM_S_NO_ERROR if the instance can be found
*			WBEM_E_NOT_FOUND if the instance described by the key properties 
* 				could not be found
*			WBEM_E_FAILED if the instance could be found but another error 
*				occurred. 
*
*  COMMENTS    :
*
*****************************************************************************/
HRESULT CDHCP_AssociationSubnetToReservation::GetObject ( CInstance* pInstance, long lFlags )
{
    return LoadInstanceProperties(pInstance)? WBEM_S_NO_ERROR : WBEM_E_NOT_FOUND;
}

/*****************************************************************************
*
*  FUNCTION    :	CDHCP_AssociationSubnetToReservation::ExecQuery
*
*  DESCRIPTION :	You are passed a method context to use in the creation of 
*			instances that satisfy the query, and a CFrameworkQuery 
*			which describes the query.  Create and populate all 
*			instances which satisfy the query.  CIMOM will post - 
*			filter the query for you, you may return more instances 
*			or more properties than are requested and CIMOM 
*			will filter out any that do not apply.
*
*
*  INPUTS      : 
*
*  RETURNS     :	WBEM_E_PROVIDER_NOT_CAPABLE if not supported for this class
*			WBEM_E_FAILED if the query failed
*			WBEM_S_NO_ERROR if query was successful 
*
*  COMMENTS    : TO DO: Most providers will not need to implement this method.  If you don't, cimom 
*			will call your enumerate function to get all the instances and perform the 
*			filtering for you.  Unless you expect SIGNIFICANT savings from implementing 
*			queries, you should remove this entire method.
*
*****************************************************************************/
HRESULT CDHCP_AssociationSubnetToReservation::ExecQuery (MethodContext *pMethodContext, CFrameworkQuery& Query, long lFlags)
{
	 return (WBEM_E_PROVIDER_NOT_CAPABLE);
}

/*****************************************************************************
*
*  FUNCTION    : CDHCP_AssociationSubnetToReservation::PutInstance
*
*  DESCRIPTION :	PutInstance should be used in provider classes that can write
*			instance information back to the hardware or software.
*			For example: Win32_Environment will allow a PutInstance of a new
*			environment variable, because environment variables are "software"
*			related.   However, a class like Win32_MotherboardDevice will not
*			allow editing of the bus speed.   Since by default PutInstance 
*			returns WBEM_E_PROVIDER_NOT_CAPABLE, this function is placed here as a
*			skeleton, but can be removed if not used.
*
*  INPUTS      : 
*
*  RETURNS     :	WBEM_E_PROVIDER_NOT_CAPABLE if PutInstance is not available
*			WBEM_E_FAILED if there is an error delivering the instance
*			WBEM_E_INVALID_PARAMETER if any of the instance properties 
*				are incorrect.
*			WBEM_S_NO_ERROR if instance is properly delivered
*
*  COMMENTS    : TO DO: If you don't intend to support writing to your provider, remove this method.
*
*****************************************************************************/
HRESULT CDHCP_AssociationSubnetToReservation::PutInstance ( const CInstance &Instance, long lFlags)
{
	 return (WBEM_E_PROVIDER_NOT_CAPABLE);
}

/*****************************************************************************
*
*  FUNCTION    :	CDHCP_AssociationSubnetToReservation::DeleteInstance
*
*  DESCRIPTION :	DeleteInstance, like PutInstance, actually writes information
*			to the software or hardware.   For most hardware devices, 
*			DeleteInstance should not be implemented, but for software
*			configuration, DeleteInstance implementation is plausible.
*			Like PutInstance, DeleteInstance returns WBEM_E_PROVIDER_NOT_CAPABLE from
*			inside Provider::DeleteInstance (defined in Provider.h).  So, if
*			you choose not to implement DeleteInstance, remove this function
*			definition and the declaration from DHCP_Server_Scalars.h 
*
*  INPUTS      : 
*
*  RETURNS     :	WBEM_E_PROVIDER_NOT_CAPABLE if DeleteInstance is not available.
*			WBEM_E_FAILED if there is an error deleting the instance.
*			WBEM_E_INVALID_PARAMETER if any of the instance properties 
*				are incorrect.
*			WBEM_S_NO_ERROR if instance is properly deleted.
*
*  COMMENTS    : TO DO: If you don't intend to support deleting instances, remove this method.
*
*****************************************************************************/
HRESULT CDHCP_AssociationSubnetToReservation::DeleteInstance ( const CInstance &Instance, long lFlags )
{
	 return (WBEM_E_PROVIDER_NOT_CAPABLE);
}

/*****************************************************************************
*
*  FUNCTION    :	CDHCP_AssociationSubnetToReservation::ExecMethod
*
*  DESCRIPTION : 	Override this function to provide support for methods.  
*			A method is an entry point for the user of your provider 
*			to request your class perform some function above and 
*			beyond a change of state.  (A change of state should be 
*			handled by PutInstance() )
*
*  INPUTS      :	A pointer to a CInstance containing the instance the method was executed against.
*			A string containing the method name
*			A pointer to the CInstance which contains the IN parameters.
*			A pointer to the CInstance to contain the OUT parameters.
*			A set of specialized method flags
*
*  RETURNS     :	WBEM_E_PROVIDER_NOT_CAPABLE if not implemented for this class
*			WBEM_S_NO_ERROR if method executes successfully
*			WBEM_E_FAILED if error occurs executing method 
*
*  COMMENTS    : TO DO: If you don't intend to support Methods, remove this method.
*
*****************************************************************************/
HRESULT CDHCP_AssociationSubnetToReservation::ExecMethod ( const CInstance& Instance,
						const BSTR bstrMethodName,
						CInstance *pInParams,
						CInstance *pOutParams,
						long lFlags)
{
    return WBEM_E_PROVIDER_NOT_CAPABLE;
}


/*****************************************************************************
*
*  FUNCTION    :	CDHCP_AssociationSubnetToReservation::LoadInstanceProperties
*
*  RETURNS     :	TRUE if the values for all the properties was loaded successfully,
*           FALSE otherwise.
*
*  COMMENTS    :    It loops through the Server_Property table, calling the GET functions.
*
*****************************************************************************/
BOOL CDHCP_AssociationSubnetToReservation::LoadInstanceProperties(CInstance* pInstance)
{
	CHString        str;
    char            *pAddress;
    DHCP_IP_ADDRESS dwSubnet, dwAddress;

    // at this point, the key information should be provided by the pInstance
    if (!pInstance->GetCHString (PROP_NAME_SUBNET, str ) ||
        !inet_wstodw(str, dwSubnet) || 
        !pInstance->GetCHString ( PROP_NAME_RESERVATION, str) ||
        !(pAddress = strchr ( str.GetBuffer(0), ',' )) || 
        !inet_wstodw(pAddress, dwAddress))
        return FALSE;

	CDHCP_Subnet_Parameters         subnetParams(dwSubnet) ;
    CDHCP_Reservation_Parameters    reservationParams(dwSubnet, dwAddress);
    LPDHCP_SUBNET_INFO              pSubnetInfo;
	LPDHCP_IP_RESERVATION_V4        pReservationInfo;

	return subnetParams.GetSubnetInfo(pSubnetInfo, TRUE) && reservationParams.GetReservationInfo(pReservationInfo, TRUE);
}
