
using System;
using System.Runtime.InteropServices;
using System.Security;
using System.Runtime.Serialization;

namespace WbemClient_v1 {}
namespace WbemUtilities_v1 {}

namespace System.Management
{
	#region FreeThreadedInterfaces
    sealed class IWbemClassObjectFreeThreaded : IDisposable, ISerializable
    {
        const string DllName = "WMINet_Utils.dll";
        const string EntryPointName = "UFunc";
        const string SerializationBlobName = "flatWbemClassObject";
        static readonly string name = typeof(IWbemClassObjectFreeThreaded).FullName;
        public static Guid IID_IWbemClassObject = new Guid("DC12A681-737F-11CF-884D-00AA004B2E24");

        IntPtr pWbemClassObject = IntPtr.Zero;

        public IWbemClassObjectFreeThreaded(IntPtr pWbemClassObject)
        {
            // This instance will now own a single ref count on pWbemClassObject
            this.pWbemClassObject = pWbemClassObject;
        }

        public static implicit operator IntPtr(IWbemClassObjectFreeThreaded wbemClassObject)
        {
            if(null == wbemClassObject)
                return IntPtr.Zero;
            return wbemClassObject.pWbemClassObject;
        }

        public IWbemClassObjectFreeThreaded(SerializationInfo info, StreamingContext context)
        {
            Byte[] rg = info.GetValue(SerializationBlobName, typeof(Byte[])) as Byte[];
            if(null == rg)
                throw new SerializationException();

            DeserializeFromBlob(rg);
        }

        void ISerializable.GetObjectData(SerializationInfo info, StreamingContext context)
        {
            info.AddValue(SerializationBlobName, SerializeToBlob());
        }

        public void Dispose()
        {
            if(pWbemClassObject != IntPtr.Zero)
            {
                Marshal.Release(pWbemClassObject);
                pWbemClassObject = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }

        ~IWbemClassObjectFreeThreaded()
        {
            Dispose();
        }

        void DeserializeFromBlob(Byte [] rg)
        {
            IntPtr hGlobal = IntPtr.Zero;
            UCOMIStream stream = null;
            try
            {
                // If something goes wrong, we want to make sure the object is invalidated
                pWbemClassObject = IntPtr.Zero;

                hGlobal = Marshal.AllocHGlobal(rg.Length);
                Marshal.Copy(rg, 0, hGlobal, rg.Length);
                stream = CreateStreamOnHGlobal(hGlobal, 0);
                pWbemClassObject = CoUnmarshalInterface(stream, ref IID_IWbemClassObject);
            }
            finally
            {
                if(stream != null)
                    Marshal.ReleaseComObject(stream);
                if(hGlobal != IntPtr.Zero)
                    Marshal.FreeHGlobal(hGlobal);
            }
        }

        Byte[] SerializeToBlob()
        {
            Byte [] rg = null;
            UCOMIStream stream = null;
            try
            {
                // Stream will own the HGlobal
                stream = CreateStreamOnHGlobal(IntPtr.Zero, 1);

                CoMarshalInterface(stream, ref IID_IWbemClassObject, pWbemClassObject, (UInt32)MSHCTX.MSHCTX_DIFFERENTMACHINE, IntPtr.Zero, (UInt32)MSHLFLAGS.MSHLFLAGS_TABLEWEAK);

                STATSTG statstg;
                stream.Stat(out statstg, (int)STATFLAG.STATFLAG_DEFAULT);
                rg = new Byte[statstg.cbSize];

                Marshal.Copy(GetHGlobalFromStream(stream), rg, 0, (int)statstg.cbSize);
            }
            finally
            {
                if(stream != null)
                    Marshal.ReleaseComObject(stream);
            }
            return rg;
        }

        // Interface methods
        public int GetQualifierSet_(out IWbemQualifierSetFreeThreaded ppQualSet)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            IntPtr pQualSet;
            int hResult = GetQualifierSet_f(3, pWbemClassObject, out pQualSet);
            if(hResult < 0)
                ppQualSet = null;
            else
                ppQualSet = new IWbemQualifierSetFreeThreaded(pQualSet);
            return hResult;
        }
        public int Get_(string wszName, Int32 lFlags, ref object pVal, ref Int32 pType, ref Int32 plFlavor)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            int hr = Get_f(4, pWbemClassObject, wszName, lFlags, ref pVal, ref pType, ref plFlavor);
            // There is a BUG in WMI where some instances (events and out params from method invocations)
            // do not have a __PATH property.  Unfortunately, GetNames says the object DOES have a __PATH
            // property.  Going under the assumption that __PATH should always exist, we make a slight fixup
            // if we detect a missing __PATH
            if(hr == (int)ManagementStatus.InvalidObject)
            {
                // We optimize the quick string comparison before trying the case insensitive comparison
                if(wszName == "__PATH" || wszName.ToLower() == "__path")
                {
                    hr = 0;
                    pType = (Int32)tag_CIMTYPE_ENUMERATION.CIM_STRING;
                    plFlavor = (Int32)tag_WBEM_FLAVOR_TYPE.WBEM_FLAVOR_ORIGIN_SYSTEM;
                    pVal = DBNull.Value;
                }
            }
            return hr;
        }
        public int Put_(string wszName, Int32 lFlags, ref object pVal, Int32 Type)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return Put_f(5, pWbemClassObject, wszName, lFlags, ref pVal, Type);
        }
        public int Delete_(string wszName)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return Delete_f(6, pWbemClassObject, wszName);
        }
        public int GetNames_(string wszQualifierName, Int32 lFlags, ref object pQualifierVal, out string[] pNames)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return GetNames_f(7, pWbemClassObject, wszQualifierName, lFlags, ref pQualifierVal, out pNames);
        }
        public int BeginEnumeration_(Int32 lEnumFlags)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return BeginEnumeration_f(8, pWbemClassObject, lEnumFlags);
        }
        public int Next_(Int32 lFlags, ref string strName, ref object pVal, ref Int32 pType, ref Int32 plFlavor)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return Next_f(9, pWbemClassObject, lFlags, ref strName, ref pVal, ref pType, ref plFlavor);
        }
        public int EndEnumeration_()
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return EndEnumeration_f(10, pWbemClassObject);
        }
        public int GetPropertyQualifierSet_(string wszProperty, out IWbemQualifierSetFreeThreaded ppQualSet)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            IntPtr pQualSet;
            int hResult = GetPropertyQualifierSet_f(11, pWbemClassObject, wszProperty, out pQualSet);
            if(hResult < 0)
                ppQualSet = null;
            else
                ppQualSet = new IWbemQualifierSetFreeThreaded(pQualSet);
            return hResult;
        }
        public int Clone_(out IWbemClassObjectFreeThreaded ppCopy)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            IntPtr pWbemClassObjectNew;
            int hResult = Clone_f(12, pWbemClassObject, out pWbemClassObjectNew);
            if(hResult < 0)
                ppCopy = null;
            else
                ppCopy = new IWbemClassObjectFreeThreaded(pWbemClassObjectNew);
            return hResult;
        }
        public int GetObjectText_(Int32 lFlags, out string pstrObjectText)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return GetObjectText_f(13, pWbemClassObject, lFlags, out pstrObjectText);
        }
        public int SpawnDerivedClass_(Int32 lFlags, out IWbemClassObjectFreeThreaded ppNewClass)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            IntPtr pWbemClassObjectNew;
            int hResult = SpawnDerivedClass_f(14, pWbemClassObject, lFlags, out pWbemClassObjectNew);
            if(hResult < 0)
                ppNewClass = null;
            else
                ppNewClass = new IWbemClassObjectFreeThreaded(pWbemClassObjectNew);
            return hResult;
        }
        public int SpawnInstance_(Int32 lFlags, out IWbemClassObjectFreeThreaded ppNewInstance)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            IntPtr pWbemClassObjectNew;
            int hResult = SpawnInstance_f(15, pWbemClassObject, lFlags, out pWbemClassObjectNew);
            if(hResult < 0)
                ppNewInstance = null;
            else
                ppNewInstance = new IWbemClassObjectFreeThreaded(pWbemClassObjectNew);
            return hResult;
        }
        public int CompareTo_(Int32 lFlags, IWbemClassObjectFreeThreaded pCompareTo)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return CompareTo_f(16, pWbemClassObject, lFlags, pCompareTo.pWbemClassObject);
        }
        public int GetPropertyOrigin_(string wszName, out string pstrClassName)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return GetPropertyOrigin_f(17, pWbemClassObject, wszName, out pstrClassName);
        }
        public int InheritsFrom_(string strAncestor)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return InheritsFrom_f(18, pWbemClassObject, strAncestor);
        }
        public int GetMethod_(string wszName, Int32 lFlags, out IWbemClassObjectFreeThreaded ppInSignature, out IWbemClassObjectFreeThreaded ppOutSignature)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            IntPtr pInSignature;
            IntPtr pOutSignature;
            int hResult = GetMethod_f(19, pWbemClassObject, wszName, lFlags, out pInSignature, out pOutSignature);
            ppInSignature = null;
            ppOutSignature = null;
            if(hResult >= 0)
            {
                // This can be NULL
                if(pInSignature != IntPtr.Zero)
                    ppInSignature = new IWbemClassObjectFreeThreaded(pInSignature);
                if(pOutSignature != IntPtr.Zero)
                    ppOutSignature = new IWbemClassObjectFreeThreaded(pOutSignature);
            }
            return hResult;
        }
        public int PutMethod_(string wszName, Int32 lFlags, IWbemClassObjectFreeThreaded pInSignature, IWbemClassObjectFreeThreaded pOutSignature)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return PutMethod_f(20, pWbemClassObject, wszName, lFlags, pInSignature, pOutSignature);
        }
        public int DeleteMethod_(string wszName)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return DeleteMethod_f(21, pWbemClassObject, wszName);
        }
        public int BeginMethodEnumeration_(Int32 lEnumFlags)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return BeginMethodEnumeration_f(22, pWbemClassObject, lEnumFlags);
        }
        public int NextMethod_(Int32 lFlags, out string pstrName, out IWbemClassObjectFreeThreaded ppInSignature, out IWbemClassObjectFreeThreaded ppOutSignature)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);

            // TODO: Provide overload which optionally only gets the name param
            IntPtr pInSignature;
            IntPtr pOutSignature;
            int hResult = NextMethod_f(23, pWbemClassObject, lFlags, out pstrName, out pInSignature, out pOutSignature);
            ppInSignature = null;
            ppOutSignature = null;
            if(hResult >= 0)
            {
                // This can be NULL
                if(pInSignature != IntPtr.Zero)
                    ppInSignature = new IWbemClassObjectFreeThreaded(pInSignature);
                if(pOutSignature != IntPtr.Zero)
                    ppOutSignature = new IWbemClassObjectFreeThreaded(pOutSignature);
            }
            return hResult;
        }
        public int EndMethodEnumeration_()
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return EndMethodEnumeration_f(24, pWbemClassObject);
        }
        public int GetMethodQualifierSet_(string wszMethod, out IWbemQualifierSetFreeThreaded ppQualSet)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            IntPtr pQualSet;
            int hResult = GetMethodQualifierSet_f(25, pWbemClassObject, wszMethod, out pQualSet);
            if(hResult < 0)
                ppQualSet = null;
            else
                ppQualSet = new IWbemQualifierSetFreeThreaded(pQualSet);
            return hResult;
        }
        public int GetMethodOrigin_(string wszMethodName, out string pstrClassName)
        {
            if(pWbemClassObject == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return GetMethodOrigin_f(26, pWbemClassObject, wszMethodName, out pstrClassName);
        }

        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int GetQualifierSet_f(int vFunc, IntPtr pWbemClassObject, [Out] out IntPtr ppQualSet);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int Get_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In][Out] ref object pVal, [In][Out] ref Int32 pType, [In][Out] ref Int32 plFlavor);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int Put_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In] ref object pVal, [In] Int32 Type);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int Delete_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int GetNames_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszQualifierName, [In] Int32 lFlags, [In] ref object pQualifierVal, [Out][MarshalAs(UnmanagedType.SafeArray, SafeArraySubType=VarEnum.VT_BSTR)]  out string[]   pNames);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int BeginEnumeration_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lEnumFlags);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int Next_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lFlags, [In][Out][MarshalAs(UnmanagedType.BStr)]  ref string   strName, [In][Out] ref object pVal, [In][Out] ref Int32 pType, [In][Out] ref Int32 plFlavor);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int EndEnumeration_f(int vFunc, IntPtr pWbemClassObject);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int GetPropertyQualifierSet_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszProperty, [Out] out IntPtr ppQualSet);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int Clone_f(int vFunc, IntPtr pWbemClassObject, [Out] out IntPtr ppCopy);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int GetObjectText_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrObjectText);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int SpawnDerivedClass_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lFlags, [Out] out IntPtr ppNewClass);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int SpawnInstance_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lFlags, [Out] out IntPtr ppNewInstance);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int CompareTo_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lFlags, [In] IntPtr pCompareTo);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int GetPropertyOrigin_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrClassName);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int InheritsFrom_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   strAncestor);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int GetMethod_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [Out]out IntPtr ppInSignature, [Out] out IntPtr ppOutSignature);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int PutMethod_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In] IntPtr pInSignature, [In] IntPtr pOutSignature);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int DeleteMethod_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int BeginMethodEnumeration_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lEnumFlags);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int NextMethod_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.BStr)] out string pstrName, [Out] out IntPtr ppInSignature, [Out] out IntPtr ppOutSignature);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int EndMethodEnumeration_f(int vFunc, IntPtr pWbemClassObject);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int GetMethodQualifierSet_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszMethod, [Out] out IntPtr ppQualSet);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int GetMethodOrigin_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszMethodName, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrClassName);

        enum STATFLAG 
        {
            STATFLAG_DEFAULT    = 0,
            STATFLAG_NONAME     = 1
        }

        enum MSHCTX
        {
            MSHCTX_LOCAL               = 0,
            MSHCTX_NOSHAREDMEM         = 1,
            MSHCTX_DIFFERENTMACHINE    = 2,
            MSHCTX_INPROC              = 3 
        }

        enum MSHLFLAGS 
        {
            MSHLFLAGS_NORMAL         = 0,
            MSHLFLAGS_TABLESTRONG    = 1,
            MSHLFLAGS_TABLEWEAK      = 2,
            MSHLFLAGS_NOPING         = 3
        }

        [DllImport("ole32.dll", PreserveSig=false)]
        static extern UCOMIStream CreateStreamOnHGlobal(IntPtr hGlobal, int fDeleteOnRelease);

        [DllImport("ole32.dll", PreserveSig=false)]
        static extern IntPtr GetHGlobalFromStream([In] UCOMIStream pstm);

        [DllImport("ole32.dll", PreserveSig=false)]
        static extern void CoGetMarshalSizeMax(
            [In] ref UInt32 ulSize,      //Pointer to the upper-bound value
            [In] ref Guid riid,         //Reference to the identifier of the interface
            [In] IntPtr Unk,      //Pointer to the interface to be marshaled
            [In] UInt32 dwDestContext,  //Destination process
            [In] IntPtr pvDestContext,   //Reserved for future use
            [In] UInt32 mshlflags       //Reason for marshaling
            );

        [DllImport("ole32.dll", PreserveSig=false)]
        static extern void CoMarshalInterface(
            [In] UCOMIStream pStm,        //Pointer to the stream used for marshaling
            [In] ref Guid riid,          //Reference to the identifier of the 
            [In] IntPtr Unk,      //Pointer to the interface to be marshaled
            [In] UInt32 dwDestContext,  //Destination process
            [In] IntPtr pvDestContext,   //Reserved for future use
            [In] UInt32 mshlflags       //Reason for marshaling
            );

        [DllImport("ole32.dll", PreserveSig=false)]
        static extern IntPtr CoUnmarshalInterface(
            [In] UCOMIStream pStm,  //Pointer to the stream
            [In] ref Guid riid     //Reference to the identifier of the interface
            );
    }

    sealed class IWbemQualifierSetFreeThreaded
    {
        const string DllName = "WMINet_Utils.dll";
        const string EntryPointName = "UFunc";
        const string SerializationBlobName = "flatWbemClassObject";
        static readonly string name = typeof(IWbemQualifierSetFreeThreaded).FullName;
        public static Guid IID_IWbemClassObject = new Guid("DC12A681-737F-11CF-884D-00AA004B2E24");

        IntPtr pWbemQualifierSet = IntPtr.Zero;
        public IWbemQualifierSetFreeThreaded(IntPtr pWbemQualifierSet)
        {
            // This instance will now own a single ref count on pWbemClassObject
            this.pWbemQualifierSet = pWbemQualifierSet;
        }

        public int Get_(string wszName, Int32 lFlags, ref object pVal, ref Int32 plFlavor)
        {
            if(pWbemQualifierSet == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return Get_f(3, pWbemQualifierSet, wszName, lFlags, ref pVal, ref plFlavor);
        }
        public int Put_(string wszName, ref object pVal, Int32 lFlavor)
        {
            if(pWbemQualifierSet == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return Put_f(4, pWbemQualifierSet, wszName, ref pVal, lFlavor);
        }
        public int Delete_(string wszName)
        {
            if(pWbemQualifierSet == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return Delete_f(5, pWbemQualifierSet, wszName);
        }
        public int GetNames_(Int32 lFlags, out string[] pNames)
        {
            if(pWbemQualifierSet == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return GetNames_f(6, pWbemQualifierSet, lFlags, out pNames);
        }
        public int BeginEnumeration_(Int32 lFlags)
        {
            if(pWbemQualifierSet == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return BeginEnumeration_f(7, pWbemQualifierSet, lFlags);
        }
        public int Next_(Int32 lFlags, out string pstrName, out object pVal, out Int32 plFlavor)
        {
            if(pWbemQualifierSet == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return Next_f(8, pWbemQualifierSet, lFlags, out pstrName, out pVal, out plFlavor);
        }
        public int EndEnumeration_()
        {
            if(pWbemQualifierSet == IntPtr.Zero)
                throw new ObjectDisposedException(name);
            return EndEnumeration_f(9, pWbemQualifierSet);
        }

        
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int Get_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In][Out] ref object pVal, [In][Out] ref Int32 plFlavor);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int Put_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] ref object pVal, [In] Int32 lFlavor);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int Delete_f(int vFunc, IntPtr pWbemClassObject, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int GetNames_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.SafeArray, SafeArraySubType=VarEnum.VT_BSTR)]  out string[]   pNames);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int BeginEnumeration_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lFlags);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int Next_f(int vFunc, IntPtr pWbemClassObject, [In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrName, [Out] out object pVal, [Out] out Int32 plFlavor);
        [SuppressUnmanagedCodeSecurity, DllImport(DllName, EntryPoint=EntryPointName)] static extern int EndEnumeration_f(int vFunc, IntPtr pWbemClassObject);
    }

    class MarshalWbemObject : ICustomMarshaler 
    {
        public static ICustomMarshaler GetInstance(string cookie)
        {
            return new MarshalWbemObject(cookie);
        }

        string cookie;
        MarshalWbemObject(string cookie)
        {
            this.cookie = cookie;
        }

        public void CleanUpManagedData(object obj) 
        {
        }

        public void CleanUpNativeData(IntPtr pObj) 
        {
//            Marshal.Release(pObj);
        }

        public int GetNativeDataSize() 
        {
            return -1; // not a value type, so use -1
        }

        public IntPtr MarshalManagedToNative(object obj) 
        {
            return (IntPtr)obj;
        }

        public object MarshalNativeToManaged(IntPtr pObj) 
        {
            return new IWbemClassObjectFreeThreaded(pObj);
        }
    }
	#endregion

    #region Interfaces
    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("DC12A681-737F-11CF-884D-00AA004B2E24")]
    [ComImport]
    interface IWbemClassObject_DoNotMarshal
    {
        [PreserveSig] int GetQualifierSet_([Out][MarshalAs(UnmanagedType.Interface)]  out IWbemQualifierSet_DoNotMarshal   ppQualSet);
        [PreserveSig] int Get_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In][Out] ref object pVal, [In][Out] ref Int32 pType, [In][Out] ref Int32 plFlavor);
        [PreserveSig] int Put_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In] ref object pVal, [In] Int32 Type);
        [PreserveSig] int Delete_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName);
        [PreserveSig] int GetNames_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszQualifierName, [In] Int32 lFlags, [In] ref object pQualifierVal, [Out][MarshalAs(UnmanagedType.SafeArray, SafeArraySubType=VarEnum.VT_BSTR)]  out string[]   pNames);
        [PreserveSig] int BeginEnumeration_([In] Int32 lEnumFlags);
        [PreserveSig] int Next_([In] Int32 lFlags, [In][Out][MarshalAs(UnmanagedType.BStr)]  ref string   strName, [In][Out] ref object pVal, [In][Out] ref Int32 pType, [In][Out] ref Int32 plFlavor);
        [PreserveSig] int EndEnumeration_();
        [PreserveSig] int GetPropertyQualifierSet_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszProperty, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemQualifierSet_DoNotMarshal   ppQualSet);
        [PreserveSig] int Clone_([Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppCopy);
        [PreserveSig] int GetObjectText_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrObjectText);
        [PreserveSig] int SpawnDerivedClass_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppNewClass);
        [PreserveSig] int SpawnInstance_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppNewInstance);
        [PreserveSig] int CompareTo_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pCompareTo);
        [PreserveSig] int GetPropertyOrigin_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrClassName);
        [PreserveSig] int InheritsFrom_([In][MarshalAs(UnmanagedType.LPWStr)]  string   strAncestor);
        [PreserveSig] int GetMethod_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppInSignature, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppOutSignature);
        [PreserveSig] int PutMethod_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pInSignature, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pOutSignature);
        [PreserveSig] int DeleteMethod_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName);
        [PreserveSig] int BeginMethodEnumeration_([In] Int32 lEnumFlags);
        [PreserveSig] int NextMethod_([In] Int32 lFlags, [In][Out][MarshalAs(UnmanagedType.BStr)]  ref string   pstrName, [In][Out][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject_DoNotMarshal   ppInSignature, [In][Out][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject_DoNotMarshal   ppOutSignature);
        [PreserveSig] int EndMethodEnumeration_();
        [PreserveSig] int GetMethodQualifierSet_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszMethod, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemQualifierSet_DoNotMarshal   ppQualSet);
        [PreserveSig] int GetMethodOrigin_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszMethodName, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrClassName);
    }

    [InterfaceTypeAttribute(0x0001)]
    [GuidAttribute("DC12A680-737F-11CF-884D-00AA004B2E24")]
    [TypeLibTypeAttribute(0x0200)]
    [ComImport]
    interface IWbemQualifierSet_DoNotMarshal
    {
        [PreserveSig] int Get_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In][Out] ref object pVal, [In][Out] ref Int32 plFlavor);
        [PreserveSig] int Put_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] ref object pVal, [In] Int32 lFlavor);
        [PreserveSig] int Delete_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName);
        [PreserveSig] int GetNames_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.SafeArray, SafeArraySubType=VarEnum.VT_BSTR)]  out string[]   pNames);
        [PreserveSig] int BeginEnumeration_([In] Int32 lFlags);
        [PreserveSig] int Next_([In] Int32 lFlags, [In][Out][MarshalAs(UnmanagedType.BStr)]  ref string   pstrName, [In][Out] ref object pVal, [In][Out] ref Int32 plFlavor);
        [PreserveSig] int EndEnumeration_();
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("DC12A687-737F-11CF-884D-00AA004B2E24")]
    [ComImport]
    interface IWbemLocator
    {
        [PreserveSig] int ConnectServer_([In][MarshalAs(UnmanagedType.BStr)]  string   strNetworkResource, [In][MarshalAs(UnmanagedType.BStr)]  string   strUser, [In][MarshalAs(UnmanagedType.BStr)]  string   strPassword, [In][MarshalAs(UnmanagedType.BStr)]  string   strLocale, [In] Int32 lSecurityFlags, [In][MarshalAs(UnmanagedType.BStr)]  string   strAuthority, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemServices   ppNamespace);
    }

    [GuidAttribute("44ACA674-E8FC-11D0-A07C-00C04FB68820")]
    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemContext
    {
        [PreserveSig] int Clone_([Out][MarshalAs(UnmanagedType.Interface)]  out IWbemContext   ppNewCopy);
        [PreserveSig] int GetNames_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.SafeArray, SafeArraySubType=VarEnum.VT_BSTR)]  out string[]   pNames);
        [PreserveSig] int BeginEnumeration_([In] Int32 lFlags);
        [PreserveSig] int Next_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrName, [Out] out object pValue);
        [PreserveSig] int EndEnumeration_();
        [PreserveSig] int SetValue_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In] ref object pValue);
        [PreserveSig] int GetValue_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [Out] out object pValue);
        [PreserveSig] int DeleteValue_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags);
        [PreserveSig] int DeleteAll_();
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("9556DC99-828C-11CF-A37E-00AA003240C7")]
    [ComImport]
    interface IWbemServices
    {
        [PreserveSig] int OpenNamespace_([In][MarshalAs(UnmanagedType.BStr)]  string   strNamespace, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][Out][MarshalAs(UnmanagedType.Interface)]  ref IWbemServices   ppWorkingNamespace, [In] IntPtr ppCallResult);
        [PreserveSig] int CancelAsyncCall_([In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pSink);
        [PreserveSig] int QueryObjectSink_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemObjectSink   ppResponseHandler);
        [PreserveSig] int GetObject_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(MarshalWbemObject))] out IWbemClassObjectFreeThreaded ppObject, [In] IntPtr ppCallResult);
        [PreserveSig] int GetObjectAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int PutClass_([In] IntPtr pObject, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr ppCallResult);
        [PreserveSig] int PutClassAsync_([In] IntPtr pObject, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int DeleteClass_([In][MarshalAs(UnmanagedType.BStr)]  string   strClass, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr ppCallResult);
        [PreserveSig] int DeleteClassAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strClass, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int CreateClassEnum_([In][MarshalAs(UnmanagedType.BStr)]  string   strSuperclass, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IEnumWbemClassObject   ppEnum);
        [PreserveSig] int CreateClassEnumAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strSuperclass, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int PutInstance_([In] IntPtr pInst, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr ppCallResult);
        [PreserveSig] int PutInstanceAsync_([In] IntPtr pInst, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int DeleteInstance_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr ppCallResult);
        [PreserveSig] int DeleteInstanceAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int CreateInstanceEnum_([In][MarshalAs(UnmanagedType.BStr)]  string   strFilter, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IEnumWbemClassObject   ppEnum);
        [PreserveSig] int CreateInstanceEnumAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strFilter, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int ExecQuery_([In][MarshalAs(UnmanagedType.BStr)]  string   strQueryLanguage, [In][MarshalAs(UnmanagedType.BStr)]  string   strQuery, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IEnumWbemClassObject   ppEnum);
        [PreserveSig] int ExecQueryAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strQueryLanguage, [In][MarshalAs(UnmanagedType.BStr)]  string   strQuery, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int ExecNotificationQuery_([In][MarshalAs(UnmanagedType.BStr)]  string   strQueryLanguage, [In][MarshalAs(UnmanagedType.BStr)]  string   strQuery, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IEnumWbemClassObject   ppEnum);
        [PreserveSig] int ExecNotificationQueryAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strQueryLanguage, [In][MarshalAs(UnmanagedType.BStr)]  string   strQuery, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int ExecMethod_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In][MarshalAs(UnmanagedType.BStr)]  string   strMethodName, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr pInParams, [Out][MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(MarshalWbemObject))] out IWbemClassObjectFreeThreaded ppOutParams, [In] IntPtr ppCallResult);
        [PreserveSig] int ExecMethodAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In][MarshalAs(UnmanagedType.BStr)]  string   strMethodName, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr pInParams, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("9556DC99-828C-11CF-A37E-00AA003240C7")]
    [ComImport]
    interface IWbemServices_Old
    {
        [PreserveSig] int OpenNamespace_([In][MarshalAs(UnmanagedType.BStr)]  string   strNamespace, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][Out][MarshalAs(UnmanagedType.Interface)]  ref IWbemServices   ppWorkingNamespace, [In] IntPtr ppCallResult);
        [PreserveSig] int CancelAsyncCall_([In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pSink);
        [PreserveSig] int QueryObjectSink_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemObjectSink   ppResponseHandler);
        [PreserveSig] int GetObject_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][Out][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject_DoNotMarshal   ppObject, [In] IntPtr ppCallResult);
        [PreserveSig] int GetObjectAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int PutClass_([In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pObject, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr ppCallResult);
        [PreserveSig] int PutClassAsync_([In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pObject, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int DeleteClass_([In][MarshalAs(UnmanagedType.BStr)]  string   strClass, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr ppCallResult);
        [PreserveSig] int DeleteClassAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strClass, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int CreateClassEnum_([In][MarshalAs(UnmanagedType.BStr)]  string   strSuperclass, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IEnumWbemClassObject   ppEnum);
        [PreserveSig] int CreateClassEnumAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strSuperclass, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int PutInstance_([In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pInst, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr ppCallResult);
        [PreserveSig] int PutInstanceAsync_([In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pInst, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int DeleteInstance_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In] IntPtr ppCallResult);
        [PreserveSig] int DeleteInstanceAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int CreateInstanceEnum_([In][MarshalAs(UnmanagedType.BStr)]  string   strFilter, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IEnumWbemClassObject   ppEnum);
        [PreserveSig] int CreateInstanceEnumAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strFilter, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int ExecQuery_([In][MarshalAs(UnmanagedType.BStr)]  string   strQueryLanguage, [In][MarshalAs(UnmanagedType.BStr)]  string   strQuery, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IEnumWbemClassObject   ppEnum);
        [PreserveSig] int ExecQueryAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strQueryLanguage, [In][MarshalAs(UnmanagedType.BStr)]  string   strQuery, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int ExecNotificationQuery_([In][MarshalAs(UnmanagedType.BStr)]  string   strQueryLanguage, [In][MarshalAs(UnmanagedType.BStr)]  string   strQuery, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IEnumWbemClassObject   ppEnum);
        [PreserveSig] int ExecNotificationQueryAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strQueryLanguage, [In][MarshalAs(UnmanagedType.BStr)]  string   strQuery, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
        [PreserveSig] int ExecMethod_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In][MarshalAs(UnmanagedType.BStr)]  string   strMethodName, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pInParams, [In][Out][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject_DoNotMarshal   ppOutParams, [In] IntPtr ppCallResult);
        [PreserveSig] int ExecMethodAsync_([In][MarshalAs(UnmanagedType.BStr)]  string   strObjectPath, [In][MarshalAs(UnmanagedType.BStr)]  string   strMethodName, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pInParams, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pResponseHandler);
    }

    [GuidAttribute("44ACA675-E8FC-11D0-A07C-00C04FB68820")]
    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemCallResult
    {
        [PreserveSig] int GetResultObject_([In] Int32 lTimeout, [Out][MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(MarshalWbemObject))] out IWbemClassObjectFreeThreaded ppResultObject);
        [PreserveSig] int GetResultString_([In] Int32 lTimeout, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrResultString);
        [PreserveSig] int GetResultServices_([In] Int32 lTimeout, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemServices   ppServices);
        [PreserveSig] int GetCallStatus_([In] Int32 lTimeout, [Out] out Int32 plStatus);
    }

    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("7C857801-7381-11CF-884D-00AA004B2E24")]
    [InterfaceTypeAttribute(0x0001)]
    [System.Security.SuppressUnmanagedCodeSecurity]
    [ComImport]
    interface IWbemObjectSink
    {
//      [PreserveSig] int Indicate_([In] Int32 lObjectCount, [In][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject   apObjArray);
//      [PreserveSig] int Indicate_([In] Int32 lObjectCount, [In][MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 1)] IWbemObjectAccess[] apObjArray);
        [PreserveSig] int Indicate_([In] Int32 lObjectCount, [In] IntPtr apObjArray);
        [PreserveSig] int SetStatus_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Error)]  Int32   hResult, [In][MarshalAs(UnmanagedType.BStr)]  string   strParam, [In] IntPtr pObjParam);
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("027947E1-D731-11CE-A357-000000000001")]
    [ComImport]
    interface IEnumWbemClassObject
    {
        [PreserveSig] int Reset_();
        [PreserveSig] int Next_([In] Int32 lTimeout, [In] UInt32 uCount, [Out][MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(MarshalWbemObject))] out IWbemClassObjectFreeThreaded apObjects, [Out] out UInt32 puReturned);
        [PreserveSig] int NextAsync_([In] UInt32 uCount, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pSink);
        [PreserveSig] int Clone_([Out][MarshalAs(UnmanagedType.Interface)]  out IEnumWbemClassObject   ppEnum);
        [PreserveSig] int Skip_([In] Int32 lTimeout, [In] UInt32 nCount);
    }

    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [GuidAttribute("C1E2D759-CABD-11D3-A11B-00105A1F515A")]
    [ComImport]
    interface IWbemRawSdAccessor
    {
        [PreserveSig] int Get_([In] Int32 lFlags, [In] UInt32 uBufSize, [Out] out UInt32 puSDSize, [In][Out] ref Byte pSD);
        [PreserveSig] int Put_([In] Int32 lFlags, [In] UInt32 uBufSize, [In] ref Byte pSD);
    }

    [InterfaceTypeAttribute(0x0001)]
    [GuidAttribute("B7B31DF9-D515-11D3-A11C-00105A1F515A")]
    [ComImport]
    interface IWbemShutdown
    {
        [PreserveSig] int Shutdown_([In] Int32 uReason, [In] UInt32 uMaxMilliseconds, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx);
    }

    [GuidAttribute("4212DC47-142E-4C6C-BC49-6CA232DD0959")]
    [InterfaceTypeAttribute(0x0001)]
    /*[ComConversionLossAttribute]*/
    [ComImport]
    interface IWbemCallStatus
    {
        [PreserveSig] int GetCallStatus_([In] UInt32 uFlags, [In] UInt32 lLocale, [Out][MarshalAs(UnmanagedType.Error)]  out Int32   phRes, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pszMsg, [In] ref System.Guid riid, [Out] IntPtr pObj);
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("BFBF883A-CAD7-11D3-A11B-00105A1F515A")]
    [ComImport]
    interface IWbemObjectTextSrc
    {
        [PreserveSig] int GetText_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pObj, [In] UInt32 uObjTextFormat, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.BStr)]  out string   strText);
        [PreserveSig] int CreateFromText_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.BStr)]  string   strText, [In] UInt32 uObjTextFormat, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   pNewObj);
    }

    [GuidAttribute("49353C9A-516B-11D1-AEA6-00C04FB68820")]
    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemObjectAccess
    {
        [PreserveSig] int GetQualifierSet_([Out][MarshalAs(UnmanagedType.Interface)]  out IWbemQualifierSet_DoNotMarshal   ppQualSet);
        [PreserveSig] int Get_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In][Out] ref object pVal, [In][Out] ref Int32 pType, [In][Out] ref Int32 plFlavor);
        [PreserveSig] int Put_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In] ref object pVal, [In] Int32 Type);
        [PreserveSig] int Delete_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName);
        [PreserveSig] int GetNames_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszQualifierName, [In] Int32 lFlags, [In] ref object pQualifierVal, [Out][MarshalAs(UnmanagedType.SafeArray, SafeArraySubType=VarEnum.VT_BSTR)]  out string[]   pNames);
        [PreserveSig] int BeginEnumeration_([In] Int32 lEnumFlags);
        [PreserveSig] int Next_([In] Int32 lFlags, [In][Out][MarshalAs(UnmanagedType.BStr)]  ref string   strName, [In][Out] ref object pVal, [In][Out] ref Int32 pType, [In][Out] ref Int32 plFlavor);
        [PreserveSig] int EndEnumeration_();
        [PreserveSig] int GetPropertyQualifierSet_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszProperty, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemQualifierSet_DoNotMarshal   ppQualSet);
        [PreserveSig] int Clone_([Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppCopy);
        [PreserveSig] int GetObjectText_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrObjectText);
        [PreserveSig] int SpawnDerivedClass_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppNewClass);
        [PreserveSig] int SpawnInstance_([In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppNewInstance);
        [PreserveSig] int CompareTo_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pCompareTo);
        [PreserveSig] int GetPropertyOrigin_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrClassName);
        [PreserveSig] int InheritsFrom_([In][MarshalAs(UnmanagedType.LPWStr)]  string   strAncestor);
        [PreserveSig] int GetMethod_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppInSignature, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppOutSignature);
        [PreserveSig] int PutMethod_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pInSignature, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pOutSignature);
        [PreserveSig] int DeleteMethod_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName);
        [PreserveSig] int BeginMethodEnumeration_([In] Int32 lEnumFlags);
        [PreserveSig] int NextMethod_([In] Int32 lFlags, [In][Out][MarshalAs(UnmanagedType.BStr)]  ref string   pstrName, [In][Out][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject_DoNotMarshal   ppInSignature, [In][Out][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject_DoNotMarshal   ppOutSignature);
        [PreserveSig] int EndMethodEnumeration_();
        [PreserveSig] int GetMethodQualifierSet_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszMethod, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemQualifierSet_DoNotMarshal   ppQualSet);
        [PreserveSig] int GetMethodOrigin_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszMethodName, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrClassName);
        [PreserveSig] int GetPropertyHandle_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszPropertyName, [Out] out Int32 pType, [Out] out Int32 plHandle);
        [PreserveSig] int WritePropertyValue_([In] Int32 lHandle, [In] Int32 lNumBytes, [In] ref Byte aData);
        [PreserveSig] int ReadPropertyValue_([In] Int32 lHandle, [In] Int32 lBufferSize, [Out] out Int32 plNumBytes, [Out] out Byte aData);
        [PreserveSig] int ReadDWORD_([In] Int32 lHandle, [Out] out UInt32 pdw);
        [PreserveSig] int WriteDWORD_([In] Int32 lHandle, [In] UInt32 dw);
        [PreserveSig] int ReadQWORD_([In] Int32 lHandle, [Out] out UInt64 pqw);
        [PreserveSig] int WriteQWORD_([In] Int32 lHandle, [In] UInt64 pw);
        [PreserveSig] int GetPropertyInfoByHandle_([In] Int32 lHandle, [Out][MarshalAs(UnmanagedType.BStr)]  out string   pstrName, [Out] out Int32 pType);
        [PreserveSig] int Lock_([In] Int32 lFlags);
        [PreserveSig] int Unlock_([In] Int32 lFlags);
    }

    [InterfaceTypeAttribute(0x0001)]
    [GuidAttribute("6DAF974E-2E37-11D2-AEC9-00C04FB68820")]
    [ComImport]
    interface IMofCompiler
    {
        [PreserveSig] int CompileFile_([In][MarshalAs(UnmanagedType.LPWStr)]  string   FileName, [In][MarshalAs(UnmanagedType.LPWStr)]  string   ServerAndNamespace, [In][MarshalAs(UnmanagedType.LPWStr)]  string   User, [In][MarshalAs(UnmanagedType.LPWStr)]  string   Authority, [In][MarshalAs(UnmanagedType.LPWStr)]  string   Password, [In] Int32 lOptionFlags, [In] Int32 lClassFlags, [In] Int32 lInstanceFlags, [In][Out] ref tag_CompileStatusInfo pInfo);
        [PreserveSig] int CompileBuffer_([In] Int32 BuffSize, [In] ref Byte pBuffer, [In][MarshalAs(UnmanagedType.LPWStr)]  string   ServerAndNamespace, [In][MarshalAs(UnmanagedType.LPWStr)]  string   User, [In][MarshalAs(UnmanagedType.LPWStr)]  string   Authority, [In][MarshalAs(UnmanagedType.LPWStr)]  string   Password, [In] Int32 lOptionFlags, [In] Int32 lClassFlags, [In] Int32 lInstanceFlags, [In][Out] ref tag_CompileStatusInfo pInfo);
        [PreserveSig] int CreateBMOF_([In][MarshalAs(UnmanagedType.LPWStr)]  string   TextFileName, [In][MarshalAs(UnmanagedType.LPWStr)]  string   BMOFFileName, [In][MarshalAs(UnmanagedType.LPWStr)]  string   ServerAndNamespace, [In] Int32 lOptionFlags, [In] Int32 lClassFlags, [In] Int32 lInstanceFlags, [In][Out] ref tag_CompileStatusInfo pInfo);
    }

    [GuidAttribute("1CFABA8C-1523-11D1-AD79-00C04FD8FDFF")]
    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IUnsecuredApartment
    {
        [PreserveSig] int CreateObjectStub_([In][MarshalAs(UnmanagedType.IUnknown)]  object   pObject, [Out][MarshalAs(UnmanagedType.IUnknown)]  out object   ppStub);
    }

    [GuidAttribute("EB87E1BC-3233-11D2-AEC9-00C04FB68820")]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemStatusCodeText
    {
        [PreserveSig] int GetErrorCodeText_([In][MarshalAs(UnmanagedType.Error)]  Int32   hRes, [In] UInt32 LocaleId, [In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.BStr)]  out string   MessageText);
        [PreserveSig] int GetFacilityCodeText_([In][MarshalAs(UnmanagedType.Error)]  Int32   hRes, [In] UInt32 LocaleId, [In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.BStr)]  out string   MessageText);
    }

    [GuidAttribute("C49E32C7-BC8B-11D2-85D4-00105A1F8304")]
    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemBackupRestore
    {
        [PreserveSig] int Backup_([In][MarshalAs(UnmanagedType.LPWStr)]  string   strBackupToFile, [In] Int32 lFlags);
        [PreserveSig] int Restore_([In][MarshalAs(UnmanagedType.LPWStr)]  string   strRestoreFromFile, [In] Int32 lFlags);
    }

    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("A359DEC5-E813-4834-8A2A-BA7F1D777D76")]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemBackupRestoreEx
    {
        [PreserveSig] int Backup_([In][MarshalAs(UnmanagedType.LPWStr)]  string   strBackupToFile, [In] Int32 lFlags);
        [PreserveSig] int Restore_([In][MarshalAs(UnmanagedType.LPWStr)]  string   strRestoreFromFile, [In] Int32 lFlags);
        [PreserveSig] int Pause_();
        [PreserveSig] int Resume_();
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("49353C99-516B-11D1-AEA6-00C04FB68820")]
    [ComImport]
    interface IWbemRefresher
    {
        [PreserveSig] int Refresh_([In] Int32 lFlags);
    }

    [GuidAttribute("2705C288-79AE-11D2-B348-00105A1F8177")]
    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemHiPerfEnum
    {
        [PreserveSig] int AddObjects_([In] Int32 lFlags, [In] UInt32 uNumObjects, [In] ref Int32 apIds, [In][MarshalAs(UnmanagedType.Interface)]  ref IWbemObjectAccess   apObj);
        [PreserveSig] int RemoveObjects_([In] Int32 lFlags, [In] UInt32 uNumObjects, [In] ref Int32 apIds);
        [PreserveSig] int GetObjects_([In] Int32 lFlags, [In] UInt32 uNumObjects, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemObjectAccess   apObj, [Out] out UInt32 puReturned);
        [PreserveSig] int RemoveAll_([In] Int32 lFlags);
    }

    [GuidAttribute("49353C92-516B-11D1-AEA6-00C04FB68820")]
    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemConfigureRefresher
    {
        [PreserveSig] int AddObjectByPath_([In][MarshalAs(UnmanagedType.Interface)]  IWbemServices   pNamespace, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszPath, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pContext, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppRefreshable, [In][Out] ref Int32 plId);
        [PreserveSig] int AddObjectByTemplate_([In][MarshalAs(UnmanagedType.Interface)]  IWbemServices   pNamespace, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pTemplate, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pContext, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemClassObject_DoNotMarshal   ppRefreshable, [In][Out] ref Int32 plId);
        [PreserveSig] int AddRefresher_([In][MarshalAs(UnmanagedType.Interface)]  IWbemRefresher   pRefresher, [In] Int32 lFlags, [In][Out] ref Int32 plId);
        [PreserveSig] int Remove_([In] Int32 lId, [In] Int32 lFlags);
        [PreserveSig] int AddEnum_([In][MarshalAs(UnmanagedType.Interface)]  IWbemServices   pNamespace, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszClassName, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pContext, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemHiPerfEnum   ppEnum, [In][Out] ref Int32 plId);
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("E246107B-B06E-11D0-AD61-00C04FD8FDFF")]
    [ComImport]
    interface IWbemUnboundObjectSink
    {
        [PreserveSig] int IndicateToConsumer_([In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pLogicalConsumer, [In] Int32 lNumObjects, [In][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject_DoNotMarshal   apObjects);
    }

    [InterfaceTypeAttribute(0x0001)]
    [GuidAttribute("CE61E841-65BC-11D0-B6BD-00AA003240C7")]
    [TypeLibTypeAttribute(0x0200)]
    [ComImport]
    interface IWbemPropertyProvider
    {
        [PreserveSig] int GetProperty_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.BStr)]  string   strLocale, [In][MarshalAs(UnmanagedType.BStr)]  string   strClassMapping, [In][MarshalAs(UnmanagedType.BStr)]  string   strInstMapping, [In][MarshalAs(UnmanagedType.BStr)]  string   strPropMapping, [Out] out object pvValue);
        [PreserveSig] int PutProperty_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.BStr)]  string   strLocale, [In][MarshalAs(UnmanagedType.BStr)]  string   strClassMapping, [In][MarshalAs(UnmanagedType.BStr)]  string   strInstMapping, [In][MarshalAs(UnmanagedType.BStr)]  string   strPropMapping, [In] ref object pvValue);
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("E245105B-B06E-11D0-AD61-00C04FD8FDFF")]
    [ComImport]
    interface IWbemEventProvider
    {
        [PreserveSig] int ProvideEvents_([In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pSink, [In] Int32 lFlags);
    }

    [GuidAttribute("580ACAF8-FA1C-11D0-AD72-00C04FD8FDFF")]
    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemEventProviderQuerySink
    {
        [PreserveSig] int NewQuery_([In] UInt32 dwId, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszQueryLanguage, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszQuery);
        [PreserveSig] int CancelQuery_([In] UInt32 dwId);
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("631F7D96-D993-11D2-B339-00105A1F4AAF")]
    [ComImport]
    interface IWbemEventProviderSecurity
    {
        [PreserveSig] int AccessCheck_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszQueryLanguage, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszQuery, [In] Int32 lSidLength, [In] ref Byte pSid);
    }

    [GuidAttribute("631F7D97-D993-11D2-B339-00105A1F4AAF")]
    [TypeLibTypeAttribute(0x0200)]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemProviderIdentity
    {
        [PreserveSig] int SetRegistrationObject_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pProvReg);
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("E246107A-B06E-11D0-AD61-00C04FD8FDFF")]
    [ComImport]
    interface IWbemEventConsumerProvider
    {
        [PreserveSig] int FindConsumer_([In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pLogicalConsumer, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemUnboundObjectSink   ppConsumer);
    }

    [InterfaceTypeAttribute(0x0001)]
    [GuidAttribute("17CF534A-D8A3-4AD0-AC92-5E3D01717151")]
    [TypeLibTypeAttribute(0x0200)]
    [ComImport]
    interface IWbemEventConsumerProviderEx
    {
        [PreserveSig] int FindConsumer_([In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pLogicalConsumer, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemUnboundObjectSink   ppConsumer);
        [PreserveSig] int ValidateSubscription_([In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pLogicalConsumer);
    }

    [GuidAttribute("1BE41571-91DD-11D1-AEB2-00C04FB68820")]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemProviderInitSink
    {
        [PreserveSig] int SetStatus_([In] Int32 lStatus, [In] Int32 lFlags);
    }

    [GuidAttribute("1BE41572-91DD-11D1-AEB2-00C04FB68820")]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemProviderInit
    {
        [PreserveSig] int Initialize_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszUser, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszNamespace, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszLocale, [In][MarshalAs(UnmanagedType.Interface)]  IWbemServices   pNamespace, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemProviderInitSink   pInitSink);
    }

    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("49353C93-516B-11D1-AEA6-00C04FB68820")]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemHiPerfProvider
    {
        [PreserveSig] int QueryInstances_([In][MarshalAs(UnmanagedType.Interface)]  IWbemServices   pNamespace, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszClass, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pCtx, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectSink   pSink);
        [PreserveSig] int CreateRefresher_([In][MarshalAs(UnmanagedType.Interface)]  IWbemServices   pNamespace, [In] Int32 lFlags, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemRefresher   ppRefresher);
        [PreserveSig] int CreateRefreshableObject_([In][MarshalAs(UnmanagedType.Interface)]  IWbemServices   pNamespace, [In][MarshalAs(UnmanagedType.Interface)]  IWbemObjectAccess   pTemplate, [In][MarshalAs(UnmanagedType.Interface)]  IWbemRefresher   pRefresher, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pContext, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemObjectAccess   ppRefreshable, [Out] out Int32 plId);
        [PreserveSig] int StopRefreshing_([In][MarshalAs(UnmanagedType.Interface)]  IWbemRefresher   pRefresher, [In] Int32 lId, [In] Int32 lFlags);
        [PreserveSig] int CreateRefreshableEnum_([In][MarshalAs(UnmanagedType.Interface)]  IWbemServices   pNamespace, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszClass, [In][MarshalAs(UnmanagedType.Interface)]  IWbemRefresher   pRefresher, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pContext, [In][MarshalAs(UnmanagedType.Interface)]  IWbemHiPerfEnum   pHiPerfEnum, [Out] out Int32 plId);
        [PreserveSig] int GetObjects_([In][MarshalAs(UnmanagedType.Interface)]  IWbemServices   pNamespace, [In] Int32 lNumObjects, [In][Out][MarshalAs(UnmanagedType.Interface)]  ref IWbemObjectAccess   apObj, [In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   pContext);
    }

    [InterfaceTypeAttribute(0x0001)]
    [GuidAttribute("1005CBCF-E64F-4646-BCD3-3A089D8A84B4")]
    [ComImport]
    interface IWbemDecoupledRegistrar
    {
        [PreserveSig] int Register_([In] Int32 a_Flags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   a_Context, [In][MarshalAs(UnmanagedType.LPWStr)]  string   a_User, [In][MarshalAs(UnmanagedType.LPWStr)]  string   a_Locale, [In][MarshalAs(UnmanagedType.LPWStr)]  string   a_Scope, [In][MarshalAs(UnmanagedType.LPWStr)]  string   a_Registration, [In][MarshalAs(UnmanagedType.IUnknown)]  object   a_Unknown);
        [PreserveSig] int UnRegister_();
    }

    [GuidAttribute("86336D20-CA11-4786-9EF1-BC8A946B42FC")]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemDecoupledBasicEventProvider
    {
        [PreserveSig] int Register_([In] Int32 a_Flags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   a_Context, [In][MarshalAs(UnmanagedType.LPWStr)]  string   a_User, [In][MarshalAs(UnmanagedType.LPWStr)]  string   a_Locale, [In][MarshalAs(UnmanagedType.LPWStr)]  string   a_Scope, [In][MarshalAs(UnmanagedType.LPWStr)]  string   a_Registration, [In][MarshalAs(UnmanagedType.IUnknown)]  object   a_Unknown);
        [PreserveSig] int UnRegister_();
        [PreserveSig] int GetSink_([In] Int32 a_Flags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   a_Context, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemObjectSink   a_Sink);
        [PreserveSig] int GetService_([In] Int32 a_Flags, [In][MarshalAs(UnmanagedType.Interface)]  IWbemContext   a_Context, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemServices   a_Service);
    }

    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("75ABD540-F492-4161-86A5-37FC8898F69E")]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemSecureObjectSink
    {
        [PreserveSig] int Indicate_([In] Int32 lObjectCount, [In][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject_DoNotMarshal   apObjArray);
        [PreserveSig] int SetStatus_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Error)]  Int32   hResult, [In][MarshalAs(UnmanagedType.BStr)]  string   strParam, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pObjParam);
        [PreserveSig] int IndicateWithSD_([In] Int32 lNumObjects, [In][MarshalAs(UnmanagedType.IUnknown)]  ref object   apObjects, [In] Int32 lSDLength, [In] ref Byte pSD);
    }

    [InterfaceTypeAttribute(0x0001)]
    [TypeLibTypeAttribute(0x0200)]
    [GuidAttribute("3AE0080A-7E3A-4366-BF89-0FEEDC931659")]
    [ComImport]
    interface IWbemEventSink
    {
        [PreserveSig] int Indicate_([In] Int32 lObjectCount, [In][MarshalAs(UnmanagedType.Interface)]  ref IWbemClassObject_DoNotMarshal   apObjArray);
        [PreserveSig] int SetStatus_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.Error)]  Int32   hResult, [In][MarshalAs(UnmanagedType.BStr)]  string   strParam, [In][MarshalAs(UnmanagedType.Interface)]  IWbemClassObject_DoNotMarshal   pObjParam);
        [PreserveSig] int IndicateWithSD_([In] Int32 lNumObjects, [In][MarshalAs(UnmanagedType.IUnknown)]  ref object   apObjects, [In] Int32 lSDLength, [In] ref Byte pSD);
        [PreserveSig] int SetSinkSecurity_([In] Int32 lSDLength, [In] ref Byte pSD);
        [PreserveSig] int IsActive_();
        [PreserveSig] int GetRestrictedSink_([In] Int32 lNumQueries, [In][MarshalAs(UnmanagedType.LPWStr)]  ref string   awszQueries, [In][MarshalAs(UnmanagedType.IUnknown)]  object   pCallback, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemEventSink   ppSink);
        [PreserveSig] int SetBatchingParameters_([In] Int32 lFlags, [In] UInt32 dwMaxBufferSize, [In] UInt32 dwMaxSendLatency);
    }

    [GuidAttribute("9AE62877-7544-4BB0-AA26-A13824659ED6")]
    /*[ComConversionLossAttribute]*/
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemPathKeyList
    {
        [PreserveSig] int GetCount_([Out] out UInt32 puKeyCount);
        [PreserveSig] int SetKey_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] UInt32 uFlags, [In] UInt32 uCimType, [In] IntPtr pKeyVal);
        [PreserveSig] int SetKey2_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] UInt32 uFlags, [In] UInt32 uCimType, [In] ref object pKeyVal);
        [PreserveSig] int GetKey_([In] UInt32 uKeyIx, [In] UInt32 uFlags, [In][Out] ref UInt32 puNameBufSize, [In][Out][MarshalAs(UnmanagedType.LPWStr)]  string   pszKeyName, [In][Out] ref UInt32 puKeyValBufSize, [In][Out] IntPtr pKeyVal, [Out] out UInt32 puApparentCimType);
        [PreserveSig] int GetKey2_([In] UInt32 uKeyIx, [In] UInt32 uFlags, [In][Out] ref UInt32 puNameBufSize, [In][Out][MarshalAs(UnmanagedType.LPWStr)]  string   pszKeyName, [In][Out] ref object pKeyValue, [Out] out UInt32 puApparentCimType);
        [PreserveSig] int RemoveKey_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszName, [In] UInt32 uFlags);
        [PreserveSig] int RemoveAllKeys_([In] UInt32 uFlags);
        [PreserveSig] int MakeSingleton_([In] SByte bSet);
        [PreserveSig] int GetInfo_([In] UInt32 uRequestedInfo, [Out] out UInt64 puResponse);
        [PreserveSig] int GetText_([In] Int32 lFlags, [In][Out] ref UInt32 puBuffLength, [In][Out][MarshalAs(UnmanagedType.LPWStr)]  string   pszText);
    }

    [GuidAttribute("3BC15AF2-736C-477E-9E51-238AF8667DCC")]
    [InterfaceTypeAttribute(0x0001)]
    [ComImport]
    interface IWbemPath
    {
        [PreserveSig] int SetText_([In] UInt32 uMode, [In][MarshalAs(UnmanagedType.LPWStr)]  string   pszPath);
        [PreserveSig] int GetText_([In] Int32 lFlags, [In][Out] ref UInt32 puBuffLength, [In][Out][MarshalAs(UnmanagedType.LPWStr)]  string   pszText);
        [PreserveSig] int GetInfo_([In] UInt32 uRequestedInfo, [Out] out UInt64 puResponse);
        [PreserveSig] int SetServer_([In][MarshalAs(UnmanagedType.LPWStr)]  string   Name);
        [PreserveSig] int GetServer_([In][Out] ref UInt32 puNameBufLength, [In][Out][MarshalAs(UnmanagedType.LPWStr)]  string   pName);
        [PreserveSig] int GetNamespaceCount_([Out] out UInt32 puCount);
        [PreserveSig] int SetNamespaceAt_([In] UInt32 uIndex, [In][MarshalAs(UnmanagedType.LPWStr)]  string   pszName);
        [PreserveSig] int GetNamespaceAt_([In] UInt32 uIndex, [In][Out] ref UInt32 puNameBufLength, [In][Out][MarshalAs(UnmanagedType.LPWStr)]  string   pName);
        [PreserveSig] int RemoveNamespaceAt_([In] UInt32 uIndex);
        [PreserveSig] int RemoveAllNamespaces_();
        [PreserveSig] int GetScopeCount_([Out] out UInt32 puCount);
        [PreserveSig] int SetScope_([In] UInt32 uIndex, [In][MarshalAs(UnmanagedType.LPWStr)]  string   pszClass);
        [PreserveSig] int SetScopeFromText_([In] UInt32 uIndex, [In][MarshalAs(UnmanagedType.LPWStr)]  string   pszText);
        [PreserveSig] int GetScope_([In] UInt32 uIndex, [In][Out] ref UInt32 puClassNameBufSize, [In][Out][MarshalAs(UnmanagedType.LPWStr)]  string   pszClass, [Out][MarshalAs(UnmanagedType.Interface)]  out IWbemPathKeyList   pKeyList);
        [PreserveSig] int GetScopeAsText_([In] UInt32 uIndex, [In][Out] ref UInt32 puTextBufSize, [In][Out][MarshalAs(UnmanagedType.LPWStr)]  string   pszText);
        [PreserveSig] int RemoveScope_([In] UInt32 uIndex);
        [PreserveSig] int RemoveAllScopes_();
        [PreserveSig] int SetClassName_([In][MarshalAs(UnmanagedType.LPWStr)]  string   Name);
        [PreserveSig] int GetClassName_([In][Out] ref UInt32 puBuffLength, [In][Out][MarshalAs(UnmanagedType.LPWStr)]  string   pszName);
        [PreserveSig] int GetKeyList_([Out][MarshalAs(UnmanagedType.Interface)]  out IWbemPathKeyList   pOut);
        [PreserveSig] int CreateClassPart_([In] Int32 lFlags, [In][MarshalAs(UnmanagedType.LPWStr)]  string   Name);
        [PreserveSig] int DeleteClassPart_([In] Int32 lFlags);
        [PreserveSig] int IsRelative_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszMachine, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszNamespace);
        [PreserveSig] int IsRelativeOrChild_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszMachine, [In][MarshalAs(UnmanagedType.LPWStr)]  string   wszNamespace, [In] Int32 lFlags);
        [PreserveSig] int IsLocal_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszMachine);
        [PreserveSig] int IsSameClassName_([In][MarshalAs(UnmanagedType.LPWStr)]  string   wszClass);
    }

    [GuidAttribute("81166F58-DD98-11D3-A120-00105A1F515A")]
    [InterfaceTypeAttribute(0x0001)]
    /*[ComConversionLossAttribute]*/
    [ComImport]
    interface IWbemQuery
    {
        [PreserveSig] int Empty_();
        [PreserveSig] int SetLanguageFeatures_([In] Int32 lFlags, [In] UInt32 uArraySize, [In] ref UInt32 puFeatures);
        [PreserveSig] int TestLanguageFeatures_([In][Out] ref UInt32 uArraySize, [Out] out UInt32 puFeatures);
        [PreserveSig] int Parse_([In][MarshalAs(UnmanagedType.LPWStr)]  string   pszLang, [In][MarshalAs(UnmanagedType.LPWStr)]  string   pszQuery, [In] UInt32 uFlags);
        [PreserveSig] int GetAnalysis_([In] UInt32 uAnalysisType, [In] UInt32 uFlags, [Out] IntPtr pAnalysis);
        [PreserveSig] int FreeMemory_([In] IntPtr pMem);
        [PreserveSig] int GetQueryInfo_([In] UInt32 uAnalysisType, [In] UInt32 uInfoId, [In] UInt32 uBufSize, [Out] IntPtr pDestBuf);
        [PreserveSig] int AttachClassDef_([In] ref System.Guid riid, [In] IntPtr pClassDef);
        [PreserveSig] int TestObject_([In] UInt32 uTestType, [In] UInt32 uFlags, [In] ref System.Guid riid, [In] IntPtr pObj);
        [PreserveSig] int StringTest_([In] UInt32 uTestType, [In][MarshalAs(UnmanagedType.LPWStr)]  string   pszTestStr, [In][MarshalAs(UnmanagedType.LPWStr)]  string   pszExpr);
    }
    #endregion

    #region Enums
    enum tag_WBEM_GENUS_TYPE
    {
        WBEM_GENUS_CLASS = unchecked((int)0x00000001),
        WBEM_GENUS_INSTANCE = unchecked((int)0x00000002),
    }

    enum tag_WBEM_CHANGE_FLAG_TYPE
    {
        WBEM_FLAG_CREATE_OR_UPDATE = unchecked((int)0x00000000),
        WBEM_FLAG_UPDATE_ONLY = unchecked((int)0x00000001),
        WBEM_FLAG_CREATE_ONLY = unchecked((int)0x00000002),
        WBEM_FLAG_UPDATE_COMPATIBLE = unchecked((int)0x00000000),
        WBEM_FLAG_UPDATE_SAFE_MODE = unchecked((int)0x00000020),
        WBEM_FLAG_UPDATE_FORCE_MODE = unchecked((int)0x00000040),
        WBEM_MASK_UPDATE_MODE = unchecked((int)0x00000060),
        WBEM_FLAG_ADVISORY = unchecked((int)0x00010000),
    }

    enum tag_WBEM_GENERIC_FLAG_TYPE
    {
        WBEM_FLAG_RETURN_IMMEDIATELY = unchecked((int)0x00000010),
        WBEM_FLAG_RETURN_WBEM_COMPLETE = unchecked((int)0x00000000),
        WBEM_FLAG_BIDIRECTIONAL = unchecked((int)0x00000000),
        WBEM_FLAG_FORWARD_ONLY = unchecked((int)0x00000020),
        WBEM_FLAG_NO_ERROR_OBJECT = unchecked((int)0x00000040),
        WBEM_FLAG_RETURN_ERROR_OBJECT = unchecked((int)0x00000000),
        WBEM_FLAG_SEND_STATUS = unchecked((int)0x00000080),
        WBEM_FLAG_DONT_SEND_STATUS = unchecked((int)0x00000000),
        WBEM_FLAG_ENSURE_LOCATABLE = unchecked((int)0x00000100),
        WBEM_FLAG_DIRECT_READ = unchecked((int)0x00000200),
        WBEM_FLAG_SEND_ONLY_SELECTED = unchecked((int)0x00000000),
        WBEM_RETURN_WHEN_COMPLETE = unchecked((int)0x00000000),
        WBEM_RETURN_IMMEDIATELY = unchecked((int)0x00000010),
        WBEM_MASK_RESERVED_FLAGS = unchecked((int)0x0001F000),
        WBEM_FLAG_USE_AMENDED_QUALIFIERS = unchecked((int)0x00020000),
        WBEM_FLAG_STRONG_VALIDATION = unchecked((int)0x00100000),
    }

    enum tag_WBEM_STATUS_TYPE
    {
        WBEM_STATUS_COMPLETE = unchecked((int)0x00000000),
        WBEM_STATUS_REQUIREMENTS = unchecked((int)0x00000001),
        WBEM_STATUS_PROGRESS = unchecked((int)0x00000002),
    }

    enum tag_WBEM_TIMEOUT_TYPE
    {
        WBEM_NO_WAIT = unchecked((int)0x00000000),
        WBEM_INFINITE = unchecked((int)0xFFFFFFFF),
    }

    enum tag_WBEM_CONDITION_FLAG_TYPE
    {
        WBEM_FLAG_ALWAYS = unchecked((int)0x00000000),
        WBEM_FLAG_ONLY_IF_TRUE = unchecked((int)0x00000001),
        WBEM_FLAG_ONLY_IF_FALSE = unchecked((int)0x00000002),
        WBEM_FLAG_ONLY_IF_IDENTICAL = unchecked((int)0x00000003),
        WBEM_MASK_PRIMARY_CONDITION = unchecked((int)0x00000003),
        WBEM_FLAG_KEYS_ONLY = unchecked((int)0x00000004),
        WBEM_FLAG_REFS_ONLY = unchecked((int)0x00000008),
        WBEM_FLAG_LOCAL_ONLY = unchecked((int)0x00000010),
        WBEM_FLAG_PROPAGATED_ONLY = unchecked((int)0x00000020),
        WBEM_FLAG_SYSTEM_ONLY = unchecked((int)0x00000030),
        WBEM_FLAG_NONSYSTEM_ONLY = unchecked((int)0x00000040),
        WBEM_MASK_CONDITION_ORIGIN = unchecked((int)0x00000070),
        WBEM_FLAG_CLASS_OVERRIDES_ONLY = unchecked((int)0x00000100),
        WBEM_FLAG_CLASS_LOCAL_AND_OVERRIDES = unchecked((int)0x00000200),
        WBEM_MASK_CLASS_CONDITION = unchecked((int)0x00000300),
    }
    enum tag_WBEM_FLAVOR_TYPE
    {
        WBEM_FLAVOR_DONT_PROPAGATE = unchecked((int)0x00000000),
        WBEM_FLAVOR_FLAG_PROPAGATE_TO_INSTANCE = unchecked((int)0x00000001),
        WBEM_FLAVOR_FLAG_PROPAGATE_TO_DERIVED_CLASS = unchecked((int)0x00000002),
        WBEM_FLAVOR_MASK_PROPAGATION = unchecked((int)0x0000000F),
        WBEM_FLAVOR_OVERRIDABLE = unchecked((int)0x00000000),
        WBEM_FLAVOR_NOT_OVERRIDABLE = unchecked((int)0x00000010),
        WBEM_FLAVOR_MASK_PERMISSIONS = unchecked((int)0x00000010),
        WBEM_FLAVOR_ORIGIN_LOCAL = unchecked((int)0x00000000),
        WBEM_FLAVOR_ORIGIN_PROPAGATED = unchecked((int)0x00000020),
        WBEM_FLAVOR_ORIGIN_SYSTEM = unchecked((int)0x00000040),
        WBEM_FLAVOR_MASK_ORIGIN = unchecked((int)0x00000060),
        WBEM_FLAVOR_NOT_AMENDED = unchecked((int)0x00000000),
        WBEM_FLAVOR_AMENDED = unchecked((int)0x00000080),
        WBEM_FLAVOR_MASK_AMENDED = unchecked((int)0x00000080),
    }

    enum tag_WBEM_QUERY_FLAG_TYPE
    {
        WBEM_FLAG_DEEP = unchecked((int)0x00000000),
        WBEM_FLAG_SHALLOW = unchecked((int)0x00000001),
        WBEM_FLAG_PROTOTYPE = unchecked((int)0x00000002),
    }

    enum tag_WBEM_SECURITY_FLAGS
    {
        WBEM_ENABLE = unchecked((int)0x00000001),
        WBEM_METHOD_EXECUTE = unchecked((int)0x00000002),
        WBEM_FULL_WRITE_REP = unchecked((int)0x00000004),
        WBEM_PARTIAL_WRITE_REP = unchecked((int)0x00000008),
        WBEM_WRITE_PROVIDER = unchecked((int)0x00000010),
        WBEM_REMOTE_ACCESS = unchecked((int)0x00000020),
        WBEM_RIGHT_SUBSCRIBE = unchecked((int)0x00000001),
        WBEM_RIGHT_PUBLISH = unchecked((int)0x00000001),
    }

    enum tag_WBEM_LIMITATION_FLAG_TYPE
    {
        WBEM_FLAG_EXCLUDE_OBJECT_QUALIFIERS = unchecked((int)0x00000010),
        WBEM_FLAG_EXCLUDE_PROPERTY_QUALIFIERS = unchecked((int)0x00000020),
    }

    enum tag_WBEM_TEXT_FLAG_TYPE
    {
        WBEM_FLAG_NO_FLAVORS = unchecked((int)0x00000001),
    }

    enum tag_WBEM_COMPARISON_FLAG
    {
        WBEM_COMPARISON_INCLUDE_ALL = unchecked((int)0x00000000),
        WBEM_FLAG_IGNORE_QUALIFIERS = unchecked((int)0x00000001),
        WBEM_FLAG_IGNORE_OBJECT_SOURCE = unchecked((int)0x00000002),
        WBEM_FLAG_IGNORE_DEFAULT_VALUES = unchecked((int)0x00000004),
        WBEM_FLAG_IGNORE_CLASS = unchecked((int)0x00000008),
        WBEM_FLAG_IGNORE_CASE = unchecked((int)0x00000010),
        WBEM_FLAG_IGNORE_FLAVOR = unchecked((int)0x00000020),
    }

    enum tag_WBEM_LOCKING
    {
        WBEM_FLAG_ALLOW_READ = unchecked((int)0x00000001),
    }

    enum tag_CIMTYPE_ENUMERATION
    {
        CIM_ILLEGAL = unchecked((int)0x00000FFF),
        CIM_EMPTY = unchecked((int)0x00000000),
        CIM_SINT8 = unchecked((int)0x00000010),
        CIM_UINT8 = unchecked((int)0x00000011),
        CIM_SINT16 = unchecked((int)0x00000002),
        CIM_UINT16 = unchecked((int)0x00000012),
        CIM_SINT32 = unchecked((int)0x00000003),
        CIM_UINT32 = unchecked((int)0x00000013),
        CIM_SINT64 = unchecked((int)0x00000014),
        CIM_UINT64 = unchecked((int)0x00000015),
        CIM_REAL32 = unchecked((int)0x00000004),
        CIM_REAL64 = unchecked((int)0x00000005),
        CIM_BOOLEAN = unchecked((int)0x0000000B),
        CIM_STRING = unchecked((int)0x00000008),
        CIM_DATETIME = unchecked((int)0x00000065),
        CIM_REFERENCE = unchecked((int)0x00000066),
        CIM_CHAR16 = unchecked((int)0x00000067),
        CIM_OBJECT = unchecked((int)0x0000000D),
        CIM_FLAG_ARRAY = unchecked((int)0x00002000),
    }

    enum tag_WBEM_BACKUP_RESTORE_FLAGS
    {
        WBEM_FLAG_BACKUP_RESTORE_DEFAULT = unchecked((int)0x00000000),
        WBEM_FLAG_BACKUP_RESTORE_FORCE_SHUTDOWN = unchecked((int)0x00000001),
    }

    enum tag_WBEM_REFRESHER_FLAGS
    {
        WBEM_FLAG_REFRESH_AUTO_RECONNECT = unchecked((int)0x00000000),
        WBEM_FLAG_REFRESH_NO_AUTO_RECONNECT = unchecked((int)0x00000001),
    }

    enum tag_WBEM_SHUTDOWN_FLAGS
    {
        WBEM_SHUTDOWN_UNLOAD_COMPONENT = unchecked((int)0x00000001),
        WBEM_SHUTDOWN_WMI = unchecked((int)0x00000002),
        WBEM_SHUTDOWN_OS = unchecked((int)0x00000003),
    }

    enum tag_WBEMSTATUS_FORMAT
    {
        WBEMSTATUS_FORMAT_NEWLINE = unchecked((int)0x00000000),
        WBEMSTATUS_FORMAT_NO_NEWLINE = unchecked((int)0x00000001),
    }

    enum tag_WBEMSTATUS
    {
        WBEM_NO_ERROR = unchecked((int)0x00000000),
        WBEM_S_NO_ERROR = unchecked((int)0x00000000),
        WBEM_S_SAME = unchecked((int)0x00000000),
        WBEM_S_FALSE = unchecked((int)0x00000001),
        WBEM_S_ALREADY_EXISTS = unchecked((int)0x00040001),
        WBEM_S_RESET_TO_DEFAULT = unchecked((int)0x00040002),
        WBEM_S_DIFFERENT = unchecked((int)0x00040003),
        WBEM_S_TIMEDOUT = unchecked((int)0x00040004),
        WBEM_S_NO_MORE_DATA = unchecked((int)0x00040005),
        WBEM_S_OPERATION_CANCELLED = unchecked((int)0x00040006),
        WBEM_S_PENDING = unchecked((int)0x00040007),
        WBEM_S_DUPLICATE_OBJECTS = unchecked((int)0x00040008),
        WBEM_S_ACCESS_DENIED = unchecked((int)0x00040009),
        WBEM_S_PARTIAL_RESULTS = unchecked((int)0x00040010),
        WBEM_S_NO_POSTHOOK = unchecked((int)0x00040011),
        WBEM_S_POSTHOOK_WITH_BOTH = unchecked((int)0x00040012),
        WBEM_S_POSTHOOK_WITH_NEW = unchecked((int)0x00040013),
        WBEM_S_POSTHOOK_WITH_STATUS = unchecked((int)0x00040014),
        WBEM_S_POSTHOOK_WITH_OLD = unchecked((int)0x00040015),
        WBEM_S_REDO_PREHOOK_WITH_ORIGINAL_OBJECT = unchecked((int)0x00040016),
        WBEM_S_SOURCE_NOT_AVAILABLE = unchecked((int)0x00040017),
        WBEM_E_FAILED = unchecked((int)0x80041001),
        WBEM_E_NOT_FOUND = unchecked((int)0x80041002),
        WBEM_E_ACCESS_DENIED = unchecked((int)0x80041003),
        WBEM_E_PROVIDER_FAILURE = unchecked((int)0x80041004),
        WBEM_E_TYPE_MISMATCH = unchecked((int)0x80041005),
        WBEM_E_OUT_OF_MEMORY = unchecked((int)0x80041006),
        WBEM_E_INVALID_CONTEXT = unchecked((int)0x80041007),
        WBEM_E_INVALID_PARAMETER = unchecked((int)0x80041008),
        WBEM_E_NOT_AVAILABLE = unchecked((int)0x80041009),
        WBEM_E_CRITICAL_ERROR = unchecked((int)0x8004100A),
        WBEM_E_INVALID_STREAM = unchecked((int)0x8004100B),
        WBEM_E_NOT_SUPPORTED = unchecked((int)0x8004100C),
        WBEM_E_INVALID_SUPERCLASS = unchecked((int)0x8004100D),
        WBEM_E_INVALID_NAMESPACE = unchecked((int)0x8004100E),
        WBEM_E_INVALID_OBJECT = unchecked((int)0x8004100F),
        WBEM_E_INVALID_CLASS = unchecked((int)0x80041010),
        WBEM_E_PROVIDER_NOT_FOUND = unchecked((int)0x80041011),
        WBEM_E_INVALID_PROVIDER_REGISTRATION = unchecked((int)0x80041012),
        WBEM_E_PROVIDER_LOAD_FAILURE = unchecked((int)0x80041013),
        WBEM_E_INITIALIZATION_FAILURE = unchecked((int)0x80041014),
        WBEM_E_TRANSPORT_FAILURE = unchecked((int)0x80041015),
        WBEM_E_INVALID_OPERATION = unchecked((int)0x80041016),
        WBEM_E_INVALID_QUERY = unchecked((int)0x80041017),
        WBEM_E_INVALID_QUERY_TYPE = unchecked((int)0x80041018),
        WBEM_E_ALREADY_EXISTS = unchecked((int)0x80041019),
        WBEM_E_OVERRIDE_NOT_ALLOWED = unchecked((int)0x8004101A),
        WBEM_E_PROPAGATED_QUALIFIER = unchecked((int)0x8004101B),
        WBEM_E_PROPAGATED_PROPERTY = unchecked((int)0x8004101C),
        WBEM_E_UNEXPECTED = unchecked((int)0x8004101D),
        WBEM_E_ILLEGAL_OPERATION = unchecked((int)0x8004101E),
        WBEM_E_CANNOT_BE_KEY = unchecked((int)0x8004101F),
        WBEM_E_INCOMPLETE_CLASS = unchecked((int)0x80041020),
        WBEM_E_INVALID_SYNTAX = unchecked((int)0x80041021),
        WBEM_E_NONDECORATED_OBJECT = unchecked((int)0x80041022),
        WBEM_E_READ_ONLY = unchecked((int)0x80041023),
        WBEM_E_PROVIDER_NOT_CAPABLE = unchecked((int)0x80041024),
        WBEM_E_CLASS_HAS_CHILDREN = unchecked((int)0x80041025),
        WBEM_E_CLASS_HAS_INSTANCES = unchecked((int)0x80041026),
        WBEM_E_QUERY_NOT_IMPLEMENTED = unchecked((int)0x80041027),
        WBEM_E_ILLEGAL_NULL = unchecked((int)0x80041028),
        WBEM_E_INVALID_QUALIFIER_TYPE = unchecked((int)0x80041029),
        WBEM_E_INVALID_PROPERTY_TYPE = unchecked((int)0x8004102A),
        WBEM_E_VALUE_OUT_OF_RANGE = unchecked((int)0x8004102B),
        WBEM_E_CANNOT_BE_SINGLETON = unchecked((int)0x8004102C),
        WBEM_E_INVALID_CIM_TYPE = unchecked((int)0x8004102D),
        WBEM_E_INVALID_METHOD = unchecked((int)0x8004102E),
        WBEM_E_INVALID_METHOD_PARAMETERS = unchecked((int)0x8004102F),
        WBEM_E_SYSTEM_PROPERTY = unchecked((int)0x80041030),
        WBEM_E_INVALID_PROPERTY = unchecked((int)0x80041031),
        WBEM_E_CALL_CANCELLED = unchecked((int)0x80041032),
        WBEM_E_SHUTTING_DOWN = unchecked((int)0x80041033),
        WBEM_E_PROPAGATED_METHOD = unchecked((int)0x80041034),
        WBEM_E_UNSUPPORTED_PARAMETER = unchecked((int)0x80041035),
        WBEM_E_MISSING_PARAMETER_ID = unchecked((int)0x80041036),
        WBEM_E_INVALID_PARAMETER_ID = unchecked((int)0x80041037),
        WBEM_E_NONCONSECUTIVE_PARAMETER_IDS = unchecked((int)0x80041038),
        WBEM_E_PARAMETER_ID_ON_RETVAL = unchecked((int)0x80041039),
        WBEM_E_INVALID_OBJECT_PATH = unchecked((int)0x8004103A),
        WBEM_E_OUT_OF_DISK_SPACE = unchecked((int)0x8004103B),
        WBEM_E_BUFFER_TOO_SMALL = unchecked((int)0x8004103C),
        WBEM_E_UNSUPPORTED_PUT_EXTENSION = unchecked((int)0x8004103D),
        WBEM_E_UNKNOWN_OBJECT_TYPE = unchecked((int)0x8004103E),
        WBEM_E_UNKNOWN_PACKET_TYPE = unchecked((int)0x8004103F),
        WBEM_E_MARSHAL_VERSION_MISMATCH = unchecked((int)0x80041040),
        WBEM_E_MARSHAL_INVALID_SIGNATURE = unchecked((int)0x80041041),
        WBEM_E_INVALID_QUALIFIER = unchecked((int)0x80041042),
        WBEM_E_INVALID_DUPLICATE_PARAMETER = unchecked((int)0x80041043),
        WBEM_E_TOO_MUCH_DATA = unchecked((int)0x80041044),
        WBEM_E_SERVER_TOO_BUSY = unchecked((int)0x80041045),
        WBEM_E_INVALID_FLAVOR = unchecked((int)0x80041046),
        WBEM_E_CIRCULAR_REFERENCE = unchecked((int)0x80041047),
        WBEM_E_UNSUPPORTED_CLASS_UPDATE = unchecked((int)0x80041048),
        WBEM_E_CANNOT_CHANGE_KEY_INHERITANCE = unchecked((int)0x80041049),
        WBEM_E_CANNOT_CHANGE_INDEX_INHERITANCE = unchecked((int)0x80041050),
        WBEM_E_TOO_MANY_PROPERTIES = unchecked((int)0x80041051),
        WBEM_E_UPDATE_TYPE_MISMATCH = unchecked((int)0x80041052),
        WBEM_E_UPDATE_OVERRIDE_NOT_ALLOWED = unchecked((int)0x80041053),
        WBEM_E_UPDATE_PROPAGATED_METHOD = unchecked((int)0x80041054),
        WBEM_E_METHOD_NOT_IMPLEMENTED = unchecked((int)0x80041055),
        WBEM_E_METHOD_DISABLED = unchecked((int)0x80041056),
        WBEM_E_REFRESHER_BUSY = unchecked((int)0x80041057),
        WBEM_E_UNPARSABLE_QUERY = unchecked((int)0x80041058),
        WBEM_E_NOT_EVENT_CLASS = unchecked((int)0x80041059),
        WBEM_E_MISSING_GROUP_WITHIN = unchecked((int)0x8004105A),
        WBEM_E_MISSING_AGGREGATION_LIST = unchecked((int)0x8004105B),
        WBEM_E_PROPERTY_NOT_AN_OBJECT = unchecked((int)0x8004105C),
        WBEM_E_AGGREGATING_BY_OBJECT = unchecked((int)0x8004105D),
        WBEM_E_UNINTERPRETABLE_PROVIDER_QUERY = unchecked((int)0x8004105F),
        WBEM_E_BACKUP_RESTORE_WINMGMT_RUNNING = unchecked((int)0x80041060),
        WBEM_E_QUEUE_OVERFLOW = unchecked((int)0x80041061),
        WBEM_E_PRIVILEGE_NOT_HELD = unchecked((int)0x80041062),
        WBEM_E_INVALID_OPERATOR = unchecked((int)0x80041063),
        WBEM_E_LOCAL_CREDENTIALS = unchecked((int)0x80041064),
        WBEM_E_CANNOT_BE_ABSTRACT = unchecked((int)0x80041065),
        WBEM_E_AMENDED_OBJECT = unchecked((int)0x80041066),
        WBEM_E_CLIENT_TOO_SLOW = unchecked((int)0x80041067),
        WBEM_E_NULL_SECURITY_DESCRIPTOR = unchecked((int)0x80041068),
        WBEM_E_TIMED_OUT = unchecked((int)0x80041069),
        WBEM_E_INVALID_ASSOCIATION = unchecked((int)0x8004106A),
        WBEM_E_AMBIGUOUS_OPERATION = unchecked((int)0x8004106B),
        WBEM_E_QUOTA_VIOLATION = unchecked((int)0x8004106C),
        WBEM_E_RESERVED_001 = unchecked((int)0x8004106D),
        WBEM_E_RESERVED_002 = unchecked((int)0x8004106E),
        WBEM_E_UNSUPPORTED_LOCALE = unchecked((int)0x8004106F),
        WBEM_E_HANDLE_OUT_OF_DATE = unchecked((int)0x80041070),
        WBEM_E_CONNECTION_FAILED = unchecked((int)0x80041071),
        WBEM_E_INVALID_HANDLE_REQUEST = unchecked((int)0x80041072),
        WBEM_E_PROPERTY_NAME_TOO_WIDE = unchecked((int)0x80041073),
        WBEM_E_CLASS_NAME_TOO_WIDE = unchecked((int)0x80041074),
        WBEM_E_METHOD_NAME_TOO_WIDE = unchecked((int)0x80041075),
        WBEM_E_QUALIFIER_NAME_TOO_WIDE = unchecked((int)0x80041076),
        WBEM_E_RERUN_COMMAND = unchecked((int)0x80041077),
        WBEM_E_DATABASE_VER_MISMATCH = unchecked((int)0x80041078),
        WBEM_E_VETO_DELETE = unchecked((int)0x80041079),
        WBEM_E_VETO_PUT = unchecked((int)0x8004107A),
        WBEM_E_INVALID_LOCALE = unchecked((int)0x80041080),
        WBEM_E_PROVIDER_SUSPENDED = unchecked((int)0x80041081),
        WBEM_E_SYNCHRONIZATION_REQUIRED = unchecked((int)0x80041082),
        WBEM_E_NO_SCHEMA = unchecked((int)0x80041083),
        WBEM_E_PROVIDER_ALREADY_REGISTERED = unchecked((int)0x80041084),
        WBEM_E_PROVIDER_NOT_REGISTERED = unchecked((int)0x80041085),
        WBEM_E_FATAL_TRANSPORT_ERROR = unchecked((int)0x80041086),
        WBEM_E_ENCRYPTED_CONNECTION_REQUIRED = unchecked((int)0x80041087),
        WBEM_E_PROVIDER_TIMED_OUT = unchecked((int)0x80041088),
        WBEM_E_NO_KEY = unchecked((int)0x80041089),
        WBEMESS_E_REGISTRATION_TOO_BROAD = unchecked((int)0x80042001),
        WBEMESS_E_REGISTRATION_TOO_PRECISE = unchecked((int)0x80042002),
        WBEMMOF_E_EXPECTED_QUALIFIER_NAME = unchecked((int)0x80044001),
        WBEMMOF_E_EXPECTED_SEMI = unchecked((int)0x80044002),
        WBEMMOF_E_EXPECTED_OPEN_BRACE = unchecked((int)0x80044003),
        WBEMMOF_E_EXPECTED_CLOSE_BRACE = unchecked((int)0x80044004),
        WBEMMOF_E_EXPECTED_CLOSE_BRACKET = unchecked((int)0x80044005),
        WBEMMOF_E_EXPECTED_CLOSE_PAREN = unchecked((int)0x80044006),
        WBEMMOF_E_ILLEGAL_CONSTANT_VALUE = unchecked((int)0x80044007),
        WBEMMOF_E_EXPECTED_TYPE_IDENTIFIER = unchecked((int)0x80044008),
        WBEMMOF_E_EXPECTED_OPEN_PAREN = unchecked((int)0x80044009),
        WBEMMOF_E_UNRECOGNIZED_TOKEN = unchecked((int)0x8004400A),
        WBEMMOF_E_UNRECOGNIZED_TYPE = unchecked((int)0x8004400B),
        WBEMMOF_E_EXPECTED_PROPERTY_NAME = unchecked((int)0x8004400C),
        WBEMMOF_E_TYPEDEF_NOT_SUPPORTED = unchecked((int)0x8004400D),
        WBEMMOF_E_UNEXPECTED_ALIAS = unchecked((int)0x8004400E),
        WBEMMOF_E_UNEXPECTED_ARRAY_INIT = unchecked((int)0x8004400F),
        WBEMMOF_E_INVALID_AMENDMENT_SYNTAX = unchecked((int)0x80044010),
        WBEMMOF_E_INVALID_DUPLICATE_AMENDMENT = unchecked((int)0x80044011),
        WBEMMOF_E_INVALID_PRAGMA = unchecked((int)0x80044012),
        WBEMMOF_E_INVALID_NAMESPACE_SYNTAX = unchecked((int)0x80044013),
        WBEMMOF_E_EXPECTED_CLASS_NAME = unchecked((int)0x80044014),
        WBEMMOF_E_TYPE_MISMATCH = unchecked((int)0x80044015),
        WBEMMOF_E_EXPECTED_ALIAS_NAME = unchecked((int)0x80044016),
        WBEMMOF_E_INVALID_CLASS_DECLARATION = unchecked((int)0x80044017),
        WBEMMOF_E_INVALID_INSTANCE_DECLARATION = unchecked((int)0x80044018),
        WBEMMOF_E_EXPECTED_DOLLAR = unchecked((int)0x80044019),
        WBEMMOF_E_CIMTYPE_QUALIFIER = unchecked((int)0x8004401A),
        WBEMMOF_E_DUPLICATE_PROPERTY = unchecked((int)0x8004401B),
        WBEMMOF_E_INVALID_NAMESPACE_SPECIFICATION = unchecked((int)0x8004401C),
        WBEMMOF_E_OUT_OF_RANGE = unchecked((int)0x8004401D),
        WBEMMOF_E_INVALID_FILE = unchecked((int)0x8004401E),
        WBEMMOF_E_ALIASES_IN_EMBEDDED = unchecked((int)0x8004401F),
        WBEMMOF_E_NULL_ARRAY_ELEM = unchecked((int)0x80044020),
        WBEMMOF_E_DUPLICATE_QUALIFIER = unchecked((int)0x80044021),
        WBEMMOF_E_EXPECTED_FLAVOR_TYPE = unchecked((int)0x80044022),
        WBEMMOF_E_INCOMPATIBLE_FLAVOR_TYPES = unchecked((int)0x80044023),
        WBEMMOF_E_MULTIPLE_ALIASES = unchecked((int)0x80044024),
        WBEMMOF_E_INCOMPATIBLE_FLAVOR_TYPES2 = unchecked((int)0x80044025),
        WBEMMOF_E_NO_ARRAYS_RETURNED = unchecked((int)0x80044026),
        WBEMMOF_E_MUST_BE_IN_OR_OUT = unchecked((int)0x80044027),
        WBEMMOF_E_INVALID_FLAGS_SYNTAX = unchecked((int)0x80044028),
        WBEMMOF_E_EXPECTED_BRACE_OR_BAD_TYPE = unchecked((int)0x80044029),
        WBEMMOF_E_UNSUPPORTED_CIMV22_QUAL_VALUE = unchecked((int)0x8004402A),
        WBEMMOF_E_UNSUPPORTED_CIMV22_DATA_TYPE = unchecked((int)0x8004402B),
        WBEMMOF_E_INVALID_DELETEINSTANCE_SYNTAX = unchecked((int)0x8004402C),
        WBEMMOF_E_INVALID_QUALIFIER_SYNTAX = unchecked((int)0x8004402D),
        WBEMMOF_E_QUALIFIER_USED_OUTSIDE_SCOPE = unchecked((int)0x8004402E),
        WBEMMOF_E_ERROR_CREATING_TEMP_FILE = unchecked((int)0x8004402F),
        WBEMMOF_E_ERROR_INVALID_INCLUDE_FILE = unchecked((int)0x80044030),
        WBEMMOF_E_INVALID_DELETECLASS_SYNTAX = unchecked((int)0x80044031),
    }

    enum tag_WMI_OBJ_TEXT
    {
        WMI_OBJ_TEXT_CIM_DTD_2_0 = unchecked((int)0x00000001),
        WMI_OBJ_TEXT_WMI_DTD_2_0 = unchecked((int)0x00000002),
        WMI_OBJ_TEXT_WMI_EXT1 = unchecked((int)0x00000003),
        WMI_OBJ_TEXT_WMI_EXT2 = unchecked((int)0x00000004),
        WMI_OBJ_TEXT_WMI_EXT3 = unchecked((int)0x00000005),
        WMI_OBJ_TEXT_WMI_EXT4 = unchecked((int)0x00000006),
        WMI_OBJ_TEXT_WMI_EXT5 = unchecked((int)0x00000007),
        WMI_OBJ_TEXT_WMI_EXT6 = unchecked((int)0x00000008),
        WMI_OBJ_TEXT_WMI_EXT7 = unchecked((int)0x00000009),
        WMI_OBJ_TEXT_WMI_EXT8 = unchecked((int)0x0000000A),
        WMI_OBJ_TEXT_WMI_EXT9 = unchecked((int)0x0000000B),
        WMI_OBJ_TEXT_WMI_EXT10 = unchecked((int)0x0000000C),
        WMI_OBJ_TEXT_LAST = unchecked((int)0x0000000D),
    }

    enum tag_WBEM_COMPILER_OPTIONS
    {
        WBEM_FLAG_CHECK_ONLY = unchecked((int)0x00000001),
        WBEM_FLAG_AUTORECOVER = unchecked((int)0x00000002),
        WBEM_FLAG_WMI_CHECK = unchecked((int)0x00000004),
        WBEM_FLAG_CONSOLE_PRINT = unchecked((int)0x00000008),
        WBEM_FLAG_DONT_ADD_TO_LIST = unchecked((int)0x00000010),
        WBEM_FLAG_SPLIT_FILES = unchecked((int)0x00000020),
        WBEM_FLAG_CONNECT_REPOSITORY_ONLY = unchecked((int)0x00000040),
    }

    enum tag_WBEM_PROVIDER_REQUIREMENTS_TYPE
    {
        WBEM_REQUIREMENTS_START_POSTFILTER = unchecked((int)0x00000000),
        WBEM_REQUIREMENTS_STOP_POSTFILTER = unchecked((int)0x00000001),
        WBEM_REQUIREMENTS_RECHECK_SUBSCRIPTIONS = unchecked((int)0x00000002),
    }

    enum tag_WBEM_EXTRA_RETURN_CODES
    {
        WBEM_S_INITIALIZED = unchecked((int)0x00000000),
        WBEM_S_LIMITED_SERVICE = unchecked((int)0x00043001),
        WBEM_S_INDIRECTLY_UPDATED = unchecked((int)0x00043002),
        WBEM_S_SUBJECT_TO_SDS = unchecked((int)0x00043003),
        WBEM_E_RETRY_LATER = unchecked((int)0x80043001),
        WBEM_E_RESOURCE_CONTENTION = unchecked((int)0x80043002),
    }

    enum tag_WBEM_PROVIDER_FLAGS
    {
        WBEM_FLAG_OWNER_UPDATE = unchecked((int)0x00010000),
    }

    enum tag_WBEM_INFORMATION_FLAG_TYPE
    {
        WBEM_FLAG_SHORT_NAME = unchecked((int)0x00000001),
        WBEM_FLAG_LONG_NAME = unchecked((int)0x00000002),
    }

    enum tag_WBEM_BATCH_TYPE
    {
        WBEM_FLAG_BATCH_IF_NEEDED = unchecked((int)0x00000000),
        WBEM_FLAG_MUST_BATCH = unchecked((int)0x00000001),
        WBEM_FLAG_MUST_NOT_BATCH = unchecked((int)0x00000002),
    }

    enum tag_WBEM_PATH_STATUS_FLAG
    {
        WBEMPATH_INFO_ANON_LOCAL_MACHINE = unchecked((int)0x00000001),
        WBEMPATH_INFO_HAS_MACHINE_NAME = unchecked((int)0x00000002),
        WBEMPATH_INFO_IS_CLASS_REF = unchecked((int)0x00000004),
        WBEMPATH_INFO_IS_INST_REF = unchecked((int)0x00000008),
        WBEMPATH_INFO_HAS_SUBSCOPES = unchecked((int)0x00000010),
        WBEMPATH_INFO_IS_COMPOUND = unchecked((int)0x00000020),
        WBEMPATH_INFO_HAS_V2_REF_PATHS = unchecked((int)0x00000040),
        WBEMPATH_INFO_HAS_IMPLIED_KEY = unchecked((int)0x00000080),
        WBEMPATH_INFO_CONTAINS_SINGLETON = unchecked((int)0x00000100),
        WBEMPATH_INFO_V1_COMPLIANT = unchecked((int)0x00000200),
        WBEMPATH_INFO_V2_COMPLIANT = unchecked((int)0x00000400),
        WBEMPATH_INFO_CIM_COMPLIANT = unchecked((int)0x00000800),
        WBEMPATH_INFO_IS_SINGLETON = unchecked((int)0x00001000),
        WBEMPATH_INFO_IS_PARENT = unchecked((int)0x00002000),
        WBEMPATH_INFO_SERVER_NAMESPACE_ONLY = unchecked((int)0x00004000),
        WBEMPATH_INFO_NATIVE_PATH = unchecked((int)0x00008000),
        WBEMPATH_INFO_WMI_PATH = unchecked((int)0x00010000),
        WBEMPATH_INFO_PATH_HAD_SERVER = unchecked((int)0x00020000),
    }

    enum tag_WBEM_PATH_CREATE_FLAG
    {
        WBEMPATH_CREATE_ACCEPT_RELATIVE = unchecked((int)0x00000001),
        WBEMPATH_CREATE_ACCEPT_ABSOLUTE = unchecked((int)0x00000002),
        WBEMPATH_CREATE_ACCEPT_ALL = unchecked((int)0x00000004),
        WBEMPATH_TREAT_SINGLE_IDENT_AS_NS = unchecked((int)0x00000008),
    }

    enum tag_WBEM_GET_TEXT_FLAGS
    {
        WBEMPATH_COMPRESSED = unchecked((int)0x00000001),
        WBEMPATH_GET_RELATIVE_ONLY = unchecked((int)0x00000002),
        WBEMPATH_GET_SERVER_TOO = unchecked((int)0x00000004),
        WBEMPATH_GET_SERVER_AND_NAMESPACE_ONLY = unchecked((int)0x00000008),
        WBEMPATH_GET_NAMESPACE_ONLY = unchecked((int)0x00000010),
        WBEMPATH_GET_ORIGINAL = unchecked((int)0x00000020),
    }

    enum tag_WBEM_GET_KEY_FLAGS
    {
        WBEMPATH_TEXT = unchecked((int)0x00000001),
        WBEMPATH_QUOTEDTEXT = unchecked((int)0x00000002),
    }

    enum WMIQ_ANALYSIS_TYPE
    {
        WMIQ_ANALYSIS_RPN_SEQUENCE = unchecked((int)0x00000001),
        WMIQ_ANALYSIS_ASSOC_QUERY = unchecked((int)0x00000002),
        WMIQ_ANALYSIS_PROP_ANALYSIS_MATRIX = unchecked((int)0x00000003),
        WMIQ_ANALYSIS_QUERY_TEXT = unchecked((int)0x00000004),
        WMIQ_ANALYSIS_RESERVED = unchecked((int)0x08000000),
    }

    enum __MIDL___MIDL_itf_wmi_0000_0001
    {
        WMIQ_ANALYSIS_RPN_SEQUENCE = unchecked((int)0x00000001),
        WMIQ_ANALYSIS_ASSOC_QUERY = unchecked((int)0x00000002),
        WMIQ_ANALYSIS_PROP_ANALYSIS_MATRIX = unchecked((int)0x00000003),
        WMIQ_ANALYSIS_QUERY_TEXT = unchecked((int)0x00000004),
        WMIQ_ANALYSIS_RESERVED = unchecked((int)0x08000000),
    }

    enum WMIQ_RPN_TOKEN_FLAGS
    {
        WMIQ_RPN_TOKEN_EXPRESSION = unchecked((int)0x00000001),
        WMIQ_RPN_TOKEN_AND = unchecked((int)0x00000002),
        WMIQ_RPN_TOKEN_OR = unchecked((int)0x00000003),
        WMIQ_RPN_TOKEN_NOT = unchecked((int)0x00000004),
        WMIQ_RPN_OP_UNDEFINED = unchecked((int)0x00000000),
        WMIQ_RPN_OP_EQ = unchecked((int)0x00000001),
        WMIQ_RPN_OP_NE = unchecked((int)0x00000002),
        WMIQ_RPN_OP_GE = unchecked((int)0x00000003),
        WMIQ_RPN_OP_LE = unchecked((int)0x00000004),
        WMIQ_RPN_OP_LT = unchecked((int)0x00000005),
        WMIQ_RPN_OP_GT = unchecked((int)0x00000006),
        WMIQ_RPN_OP_LIKE = unchecked((int)0x00000007),
        WMIQ_RPN_OP_ISA = unchecked((int)0x00000008),
        WMIQ_RPN_OP_ISNOTA = unchecked((int)0x00000009),
        WMIQ_RPN_LEFT_PROPERTY_NAME = unchecked((int)0x00000001),
        WMIQ_RPN_RIGHT_PROPERTY_NAME = unchecked((int)0x00000002),
        WMIQ_RPN_CONST2 = unchecked((int)0x00000004),
        WMIQ_RPN_CONST = unchecked((int)0x00000008),
        WMIQ_RPN_RELOP = unchecked((int)0x00000010),
        WMIQ_RPN_LEFT_FUNCTION = unchecked((int)0x00000020),
        WMIQ_RPN_RIGHT_FUNCTION = unchecked((int)0x00000040),
        WMIQ_RPN_GET_TOKEN_TYPE = unchecked((int)0x00000001),
        WMIQ_RPN_GET_EXPR_SHAPE = unchecked((int)0x00000002),
        WMIQ_RPN_GET_LEFT_FUNCTION = unchecked((int)0x00000003),
        WMIQ_RPN_GET_RIGHT_FUNCTION = unchecked((int)0x00000004),
        WMIQ_RPN_GET_RELOP = unchecked((int)0x00000005),
        WMIQ_RPN_NEXT_TOKEN = unchecked((int)0x00000001),
        WMIQ_RPN_FROM_UNARY = unchecked((int)0x00000001),
        WMIQ_RPN_FROM_PATH = unchecked((int)0x00000002),
        WMIQ_RPN_FROM_CLASS_LIST = unchecked((int)0x00000004),
    }

    enum __MIDL___MIDL_itf_wmi_0000_0002
    {
        WMIQ_RPN_TOKEN_EXPRESSION = unchecked((int)0x00000001),
        WMIQ_RPN_TOKEN_AND = unchecked((int)0x00000002),
        WMIQ_RPN_TOKEN_OR = unchecked((int)0x00000003),
        WMIQ_RPN_TOKEN_NOT = unchecked((int)0x00000004),
        WMIQ_RPN_OP_UNDEFINED = unchecked((int)0x00000000),
        WMIQ_RPN_OP_EQ = unchecked((int)0x00000001),
        WMIQ_RPN_OP_NE = unchecked((int)0x00000002),
        WMIQ_RPN_OP_GE = unchecked((int)0x00000003),
        WMIQ_RPN_OP_LE = unchecked((int)0x00000004),
        WMIQ_RPN_OP_LT = unchecked((int)0x00000005),
        WMIQ_RPN_OP_GT = unchecked((int)0x00000006),
        WMIQ_RPN_OP_LIKE = unchecked((int)0x00000007),
        WMIQ_RPN_OP_ISA = unchecked((int)0x00000008),
        WMIQ_RPN_OP_ISNOTA = unchecked((int)0x00000009),
        WMIQ_RPN_LEFT_PROPERTY_NAME = unchecked((int)0x00000001),
        WMIQ_RPN_RIGHT_PROPERTY_NAME = unchecked((int)0x00000002),
        WMIQ_RPN_CONST2 = unchecked((int)0x00000004),
        WMIQ_RPN_CONST = unchecked((int)0x00000008),
        WMIQ_RPN_RELOP = unchecked((int)0x00000010),
        WMIQ_RPN_LEFT_FUNCTION = unchecked((int)0x00000020),
        WMIQ_RPN_RIGHT_FUNCTION = unchecked((int)0x00000040),
        WMIQ_RPN_GET_TOKEN_TYPE = unchecked((int)0x00000001),
        WMIQ_RPN_GET_EXPR_SHAPE = unchecked((int)0x00000002),
        WMIQ_RPN_GET_LEFT_FUNCTION = unchecked((int)0x00000003),
        WMIQ_RPN_GET_RIGHT_FUNCTION = unchecked((int)0x00000004),
        WMIQ_RPN_GET_RELOP = unchecked((int)0x00000005),
        WMIQ_RPN_NEXT_TOKEN = unchecked((int)0x00000001),
        WMIQ_RPN_FROM_UNARY = unchecked((int)0x00000001),
        WMIQ_RPN_FROM_PATH = unchecked((int)0x00000002),
        WMIQ_RPN_FROM_CLASS_LIST = unchecked((int)0x00000004),
    }

    enum WMIQ_ASSOCQ_FLAGS
    {
        WMIQ_ASSOCQ_ASSOCIATORS = unchecked((int)0x00000001),
        WMIQ_ASSOCQ_REFERENCES = unchecked((int)0x00000002),
        WMIQ_ASSOCQ_RESULTCLASS = unchecked((int)0x00000004),
        WMIQ_ASSOCQ_ASSOCCLASS = unchecked((int)0x00000008),
        WMIQ_ASSOCQ_ROLE = unchecked((int)0x00000010),
        WMIQ_ASSOCQ_RESULTROLE = unchecked((int)0x00000020),
        WMIQ_ASSOCQ_REQUIREDQUALIFIER = unchecked((int)0x00000040),
        WMIQ_ASSOCQ_REQUIREDASSOCQUALIFIER = unchecked((int)0x00000080),
        WMIQ_ASSOCQ_CLASSDEFSONLY = unchecked((int)0x00000100),
        WMIQ_ASSOCQ_KEYSONLY = unchecked((int)0x00000200),
        WMIQ_ASSOCQ_SCHEMAONLY = unchecked((int)0x00000400),
        WMIQ_ASSOCQ_CLASSREFSONLY = unchecked((int)0x00000800),
    }

    enum __MIDL___MIDL_itf_wmi_0000_0003
    {
        WMIQ_ASSOCQ_ASSOCIATORS = unchecked((int)0x00000001),
        WMIQ_ASSOCQ_REFERENCES = unchecked((int)0x00000002),
        WMIQ_ASSOCQ_RESULTCLASS = unchecked((int)0x00000004),
        WMIQ_ASSOCQ_ASSOCCLASS = unchecked((int)0x00000008),
        WMIQ_ASSOCQ_ROLE = unchecked((int)0x00000010),
        WMIQ_ASSOCQ_RESULTROLE = unchecked((int)0x00000020),
        WMIQ_ASSOCQ_REQUIREDQUALIFIER = unchecked((int)0x00000040),
        WMIQ_ASSOCQ_REQUIREDASSOCQUALIFIER = unchecked((int)0x00000080),
        WMIQ_ASSOCQ_CLASSDEFSONLY = unchecked((int)0x00000100),
        WMIQ_ASSOCQ_KEYSONLY = unchecked((int)0x00000200),
        WMIQ_ASSOCQ_SCHEMAONLY = unchecked((int)0x00000400),
        WMIQ_ASSOCQ_CLASSREFSONLY = unchecked((int)0x00000800),
    }

    enum tag_WMIQ_LANGUAGE_FEATURES
    {
        WMIQ_LF1_BASIC_SELECT = unchecked((int)0x00000001),
        WMIQ_LF2_CLASS_NAME_IN_QUERY = unchecked((int)0x00000002),
        WMIQ_LF3_STRING_CASE_FUNCTIONS = unchecked((int)0x00000003),
        WMIQ_LF4_PROP_TO_PROP_TESTS = unchecked((int)0x00000004),
        WMIQ_LF5_COUNT_STAR = unchecked((int)0x00000005),
        WMIQ_LF6_ORDER_BY = unchecked((int)0x00000006),
        WMIQ_LF7_DISTINCT = unchecked((int)0x00000007),
        WMIQ_LF8_ISA = unchecked((int)0x00000008),
        WMIQ_LF9_THIS = unchecked((int)0x00000009),
        WMIQ_LF10_COMPEX_SUBEXPRESSIONS = unchecked((int)0x0000000A),
        WMIQ_LF11_ALIASING = unchecked((int)0x0000000B),
        WMIQ_LF12_GROUP_BY_HAVING = unchecked((int)0x0000000C),
        WMIQ_LF13_WMI_WITHIN = unchecked((int)0x0000000D),
        WMIQ_LF14_SQL_WRITE_OPERATIONS = unchecked((int)0x0000000E),
        WMIQ_LF15_GO = unchecked((int)0x0000000F),
        WMIQ_LF16_SINGLE_LEVEL_TRANSACTIONS = unchecked((int)0x00000010),
        WMIQ_LF17_QUALIFIED_NAMES = unchecked((int)0x00000011),
        WMIQ_LF18_ASSOCIATONS = unchecked((int)0x00000012),
        WMIQ_LF19_SYSTEM_PROPERTIES = unchecked((int)0x00000013),
        WMIQ_LF20_EXTENDED_SYSTEM_PROPERTIES = unchecked((int)0x00000014),
        WMIQ_LF21_SQL89_JOINS = unchecked((int)0x00000015),
        WMIQ_LF22_SQL92_JOINS = unchecked((int)0x00000016),
        WMIQ_LF23_SUBSELECTS = unchecked((int)0x00000017),
        WMIQ_LF24_UMI_EXTENSIONS = unchecked((int)0x00000018),
        WMIQ_LF25_DATEPART = unchecked((int)0x00000019),
        WMIQ_LF26_LIKE = unchecked((int)0x0000001A),
        WMIQ_LF27_CIM_TEMPORAL_CONSTRUCTS = unchecked((int)0x0000001B),
        WMIQ_LF28_STANDARD_AGGREGATES = unchecked((int)0x0000001C),
        WMIQ_LF29_MULTI_LEVEL_ORDER_BY = unchecked((int)0x0000001D),
        WMIQ_LF30_WMI_PRAGMAS = unchecked((int)0x0000001E),
        WMIQ_LF31_QUALIFIER_TESTS = unchecked((int)0x0000001F),
        WMIQ_LF32_SP_EXECUTE = unchecked((int)0x00000020),
        WMIQ_LF33_ARRAY_ACCESS = unchecked((int)0x00000021),
        WMIQ_LF34_UNION = unchecked((int)0x00000022),
        WMIQ_LF35_COMPLEX_SELECT_TARGET = unchecked((int)0x00000023),
        WMIQ_LF36_REFERENCE_TESTS = unchecked((int)0x00000024),
        WMIQ_LF37_SELECT_INTO = unchecked((int)0x00000025),
        WMIQ_LF38_BASIC_DATETIME_TESTS = unchecked((int)0x00000026),
        WMIQ_LF39_COUNT_COLUMN = unchecked((int)0x00000027),
        WMIQ_LF40_BETWEEN = unchecked((int)0x00000028),
        WMIQ_LF_LAST = unchecked((int)0x00000028),
    }

    enum tag_WMIQ_RPNQ_FEATURE
    {
        WMIQ_RPNF_WHERE_CLAUSE_PRESENT = unchecked((int)0x00000001),
        WMIQ_RPNF_QUERY_IS_CONJUNCTIVE = unchecked((int)0x00000002),
        WMIQ_RPNF_QUERY_IS_DISJUNCTIVE = unchecked((int)0x00000004),
        WMIQ_RPNF_PROJECTION = unchecked((int)0x00000008),
        WMIQ_RPNF_FEATURE_SELECT_STAR = unchecked((int)0x00000010),
        WMIQ_RPNF_EQUALITY_TESTS_ONLY = unchecked((int)0x00000020),
        WMIQ_RPNF_COUNT_STAR = unchecked((int)0x00000040),
        WMIQ_RPNF_QUALIFIED_NAMES_IN_SELECT = unchecked((int)0x00000080),
        WMIQ_RPNF_QUALIFIED_NAMES_IN_WHERE = unchecked((int)0x00000100),
        WMIQ_RPNF_PROP_TO_PROP_TESTS = unchecked((int)0x00000200),
        WMIQ_RPNF_ORDER_BY = unchecked((int)0x00000400),
        WMIQ_RPNF_ISA_USED = unchecked((int)0x00000800),
        WMIQ_RPNF_ISNOTA_USED = unchecked((int)0x00001000),
        WMIQ_RPNF_GROUP_BY_HAVING = unchecked((int)0x00002000),
        WMIQ_RPNF_WITHIN_INTERVAL = unchecked((int)0x00004000),
        WMIQ_RPNF_WITHIN_AGGREGATE = unchecked((int)0x00008000),
        WMIQ_RPNF_SYSPROP_CLASS = unchecked((int)0x00010000),
        WMIQ_RPNF_REFERENCE_TESTS = unchecked((int)0x00020000),
        WMIQ_RPNF_DATETIME_TESTS = unchecked((int)0x00040000),
        WMIQ_RPNF_ARRAY_ACCESS = unchecked((int)0x00080000),
        WMIQ_RPNF_QUALIFIER_FILTER = unchecked((int)0x00100000),
        WMIQ_RPNF_SELECTED_FROM_PATH = unchecked((int)0x00200000),
    }
    #endregion

    #region Structs
    struct tag_CompileStatusInfo
    {
        public Int32 lPhaseError;
        [MarshalAs(UnmanagedType.Error)] public   Int32 hRes;
        public Int32 ObjectNum;
        public Int32 FirstLine;
        public Int32 LastLine;
        public UInt32 dwOutFlags;
    }

    struct tag_SWbemQueryQualifiedName
    {
        /*[ComConversionLossAttribute]*/
        public UInt32 m_uVersion;
        public UInt32 m_uTokenType;
        public UInt32 m_uNameListSize;
        public IntPtr m_ppszNameList;
        /*[ComConversionLossAttribute]*/
        public Int32 m_bArraysUsed;
        public IntPtr m_pbArrayElUsed;
        /*[ComConversionLossAttribute]*/
        public IntPtr m_puArrayIndex;
        /*[ComConversionLossAttribute]*/
    }

    struct tag_SWbemRpnConst
    {
        public UInt64 unionhack;
    }

    struct tag_SWbemRpnQueryToken
    {
        /*[ComConversionLossAttribute]*/
        public UInt32 m_uVersion;
        public UInt32 m_uTokenType;
        public UInt32 m_uSubexpressionShape;
        public UInt32 m_uOperator;
        public IntPtr m_pRightIdent;
        /*[ComConversionLossAttribute]*/
        public IntPtr m_pLeftIdent;
        /*[ComConversionLossAttribute]*/
        public UInt32 m_uConstApparentType;
        public tag_SWbemRpnConst m_Const;
        public UInt32 m_uConst2ApparentType;
        public tag_SWbemRpnConst m_Const2;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszRightFunc;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszLeftFunc;
    }

    struct tag_SWbemRpnTokenList
    {
        public UInt32 m_uVersion;
        public UInt32 m_uTokenType;
        public UInt32 m_uNumTokens;
    }

    struct tag_SWbemRpnEncodedQuery
    {
        /*[ComConversionLossAttribute]*/
        public UInt32 m_uVersion;
        public UInt32 m_uTokenType;
        public UInt32 m_uParsedFeatureMask1;
        public UInt32 m_uParsedFeatureMask2;
        public UInt32 m_uDetectedArraySize;
        public IntPtr m_puDetectedFeatures;
        /*[ComConversionLossAttribute]*/
        public UInt32 m_uSelectListSize;
        public IntPtr m_ppSelectList;
        /*[ComConversionLossAttribute]*/
        public UInt32 m_uFromTargetType;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszOptionalFromPath;
        public UInt32 m_uFromListSize;
        public IntPtr m_ppszFromList;
        /*[ComConversionLossAttribute]*/
        public UInt32 m_uWhereClauseSize;
        public IntPtr m_ppRpnWhereClause;
        /*[ComConversionLossAttribute]*/
        public double m_dblWithinPolling;
        public double m_dblWithinWindow;
        public UInt32 m_uOrderByListSize;
        public IntPtr m_ppszOrderByList;
        /*[ComConversionLossAttribute]*/
        public IntPtr m_uOrderDirectionEl;
        /*[ComConversionLossAttribute]*/
    }

    struct tag_SWbemAnalysisMatrix
    {
        /*[ComConversionLossAttribute]*/
        public UInt32 m_uVersion;
        public UInt32 m_uMatrixType;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszProperty;
        public UInt32 m_uPropertyType;
        public UInt32 m_uEntries;
        public IntPtr m_pValues;
        /*[ComConversionLossAttribute]*/
        public IntPtr m_pbTruthTable;
        /*[ComConversionLossAttribute]*/
    }

    struct tag_SWbemAnalysisMatrixList
    {
        /*[ComConversionLossAttribute]*/
        public UInt32 m_uVersion;
        public UInt32 m_uMatrixType;
        public UInt32 m_uNumMatrices;
        public IntPtr m_pMatrices;
        /*[ComConversionLossAttribute]*/
    }

    struct tag_SWbemAssocQueryInf
    {
        public UInt32 m_uVersion;
        public UInt32 m_uAnalysisType;
        public UInt32 m_uFeatureMask;
        [MarshalAs(UnmanagedType.Interface)] public   IWbemPath m_pPath;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszPath;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszQueryText;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszResultClass;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszAssocClass;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszRole;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszResultRole;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszRequiredQualifier;
        [MarshalAs(UnmanagedType.LPWStr)] public   string m_pszRequiredAssocQualifier;
    }
    #endregion

    #region Co Classes
    [ClassInterfaceAttribute((short)0x0000)]
    [GuidAttribute("4590F811-1D3A-11D0-891F-00AA004B2E24")]
    [TypeLibTypeAttribute(0x0202)]
    [ComImport]
    class WbemLocator 
    {
    }

    [ClassInterfaceAttribute((short)0x0000)]
    [GuidAttribute("674B6698-EE92-11D0-AD71-00C04FD8FDFF")]
    [TypeLibTypeAttribute(0x0202)]
    [ComImport]
    class WbemContext 
    {
    }

    [ClassInterfaceAttribute((short)0x0000)]
    [GuidAttribute("49BD2028-1523-11D1-AD79-00C04FD8FDFF")]
    [TypeLibTypeAttribute(0x0002)]
    [ComImport]
    class UnsecuredApartment 
    {
    }

    [GuidAttribute("9A653086-174F-11D2-B5F9-00104B703EFD")]
    [ClassInterfaceAttribute((short)0x0000)]
    [TypeLibTypeAttribute(0x0002)]
    [ComImport]
    class WbemClassObject 
    {
    }

    [ClassInterfaceAttribute((short)0x0000)]
    [GuidAttribute("6DAF9757-2E37-11D2-AEC9-00C04FB68820")]
    [TypeLibTypeAttribute(0x0002)]
    [ComImport]
    class MofCompiler 
    {
    }

    [ClassInterfaceAttribute((short)0x0000)]
    [TypeLibTypeAttribute(0x0002)]
    [GuidAttribute("EB87E1BD-3233-11D2-AEC9-00C04FB68820")]
    [ComImport]
    class WbemStatusCodeText 
    {
    }

    [GuidAttribute("C49E32C6-BC8B-11D2-85D4-00105A1F8304")]
    [ClassInterfaceAttribute((short)0x0000)]
    [TypeLibTypeAttribute(0x0002)]
    [ComImport]
    class WbemBackupRestore 
    {
    }

    [ClassInterfaceAttribute((short)0x0000)]
    [GuidAttribute("C71566F2-561E-11D1-AD87-00C04FD8FDFF")]
    [TypeLibTypeAttribute(0x0202)]
    [ComImport]
    class WbemRefresher 
    {
    }

    [ClassInterfaceAttribute((short)0x0000)]
    [TypeLibTypeAttribute(0x0202)]
    [GuidAttribute("8D1C559D-84F0-4BB3-A7D5-56A7435A9BA6")]
    [ComImport]
    class WbemObjectTextSrc 
    {
    }

    [ClassInterfaceAttribute((short)0x0000)]
    [GuidAttribute("CB8555CC-9128-11D1-AD9B-00C04FD8FDFF")]
    [TypeLibTypeAttribute(0x0002)]
    [ComImport]
    class WbemAdministrativeLocator 
    {
    }

    [TypeLibTypeAttribute(0x0002)]
    [ClassInterfaceAttribute((short)0x0000)]
    [GuidAttribute("CD184336-9128-11D1-AD9B-00C04FD8FDFF")]
    [ComImport]
    class WbemAuthenticatedLocator 
    {
    }

    [TypeLibTypeAttribute(0x0002)]
    [ClassInterfaceAttribute((short)0x0000)]
    [GuidAttribute("443E7B79-DE31-11D2-B340-00104BCC4B4A")]
    [ComImport]
    class WbemUnauthenticatedLocator 
    {
    }

    [GuidAttribute("4CFC7932-0F9D-4BEF-9C32-8EA2A6B56FCB")]
    [TypeLibTypeAttribute(0x0002)]
    [ClassInterfaceAttribute((short)0x0000)]
    [ComImport]
    class WbemDecoupledRegistrar 
    {
    }

    [GuidAttribute("F5F75737-2843-4F22-933D-C76A97CDA62F")]
    [TypeLibTypeAttribute(0x0002)]
    [ClassInterfaceAttribute((short)0x0000)]
    [ComImport]
    class WbemDecoupledBasicEventProvider 
    {
    }

    [ClassInterfaceAttribute((short)0x0000)]
    [GuidAttribute("CF4CC405-E2C5-4DDD-B3CE-5E7582D8C9FA")]
    [TypeLibTypeAttribute(0x0202)]
    [ComImport]
    class WbemDefPath 
    {
    }

    [GuidAttribute("EAC8A024-21E2-4523-AD73-A71A0AA2F56A")]
    [ClassInterfaceAttribute((short)0x0000)]
    [TypeLibTypeAttribute(0x0002)]
    [ComImport]
    class WbemQuery 
    {
    }
    #endregion
}

