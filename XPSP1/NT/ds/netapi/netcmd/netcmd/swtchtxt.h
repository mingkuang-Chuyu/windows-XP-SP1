/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1987-1990          **/
/********************************************************************/


#define SW_YES          TEXT("/YES")
#define SW_NO           TEXT("/NO")
#define SW_HELP         TEXT("/HELP")
#define SW_SYNTAX       TEXT("/?")

#define SW_ADD          TEXT("/ADD")
#define SW_DELETE       TEXT("/DELETE")
#define SW_REMARK       TEXT("/REMARK")
#define SW_COMMENT      TEXT("/COMMENT")
#define SW_CACHE        TEXT("/CACHE")

#define SW_COUNT        TEXT("/COUNT")
#define SW_REVERSE      TEXT("/REVERSE")
#define SW_SERVICE      TEXT("/SERVICE")

#define SW_OPTIONS      TEXT("/OPTIONS")
#define SW_PRIORITY     TEXT("/PRIORITY")
#define SW_ROUTE        TEXT("/ROUTE")
#define SW_PURGE        TEXT("/PURGE")
#define SW_DOMAIN       TEXT("/DOMAIN")
#define SW_PERSISTENT   TEXT("/PERSISTENT")
#define SW_NETWARE      TEXT("/FPNW")
#define SW_RANDOM       TEXT("/RANDOM")
#define SW_RTSDOMAIN    TEXT("/RTSDOMAIN")
#define SW_SETSNTP      TEXT("/SETSNTP")
#define SW_QUERYSNTP    TEXT("/QUERYSNTP")


#define SW_ALERTER_SIZALERTBUF  TEXT("/SIZALERTBUF")
#define SW_NETLOGON_CENTRALIZED TEXT("/CENTRALIZED")
#define SW_NETLOGON_PULSE   TEXT("/PULSE")
#define SW_NETLOGON_RANDOMIZE   TEXT("/RANDOMIZE")
#define SW_NETLOGON_SYNCHRONIZE TEXT("/UPDATE")
#define SW_NETLOGON_SCRIPTS TEXT("/SCRIPTS")

#define SW_COMM_PURGE       TEXT("/PURGE")
#define SW_COMM_OPTIONS     TEXT("/OPTIONS")
#define SW_COMM_PRIORITY    TEXT("/PRIORITY")
#define SW_COMM_ROUTE       TEXT("/ROUTE")

#define SW_WKSTA_CHARCOUNT  TEXT("/CHARCOUNT")
#define SW_WKSTA_CHARTIME   TEXT("/CHARTIME")
#define SW_WKSTA_CHARWAIT   TEXT("/CHARWAIT")
#define SW_WKSTA_COMPUTERNAME   TEXT("/COMPUTERNAME")
#define SW_WKSTA_KEEPCONN   TEXT("/KEEPCONN")
#define SW_WKSTA_KEEPSEARCH TEXT("/KEEPSEARCH")
#define SW_WKSTA_LOGONSERVER    TEXT("/LOGONSERVER")
#define SW_WKSTA_MAILSLOTS  TEXT("/MAILSLOTS")
#define SW_WKSTA_MAXERRORLOG    TEXT("/MAXERRORLOG")
#define SW_WKSTA_MAXTHREADS TEXT("/MAXTHREADS")
#define SW_WKSTA_MAXWRKCACHE    TEXT("/MAXWRKCACHE")
#define SW_WKSTA_NUMALERTS  TEXT("/NUMALERTS")
#define SW_WKSTA_NUMCHARBUF TEXT("/NUMCHARBUF")
#define SW_WKSTA_NUMDGRAMBUF    TEXT("/NUMDGRAMBUF")
#define SW_WKSTA_NUMSERVICES    TEXT("/NUMSERVICES")
#define SW_WKSTA_NUMWORKBUF TEXT("/NUMWORKBUF")
#define SW_WKSTA_OTHDOMAINS TEXT("/OTHDOMAINS")
#define SW_WKSTA_PRIMARYDOMAIN  TEXT("/DOMAIN")
#define SW_WKSTA_PRINTBUFTIME   TEXT("/PRINTBUFTIME")
#define SW_WKSTA_SESSTIMEOUT    TEXT("/SESSTIMEOUT")
#define SW_WKSTA_SIZCHARBUF TEXT("/SIZCHARBUF")
#define SW_WKSTA_SIZERROR   TEXT("/SIZERROR")
#define SW_WKSTA_SIZWORKBUF TEXT("/SIZWORKBUF")
#define SW_WKSTA_WRKHEURISTICS  TEXT("/WRKHEURISTICS")
#define SW_WKSTA_WRKNETS    TEXT("/WRKNETS")
#define SW_WKSTA_WRKSERVICES    TEXT("/WRKSERVICES")

#define SW_INTERNAL_IGNSVC  TEXT("/IGNSVC")

#define SW_PRINT_AFTER      TEXT("/AFTER")
#define SW_PRINT_DELETE     TEXT("/DELETE")
#define SW_PRINT_FIRST      TEXT("/FIRST")
#define SW_PRINT_HOLD       TEXT("/HOLD")
#define SW_PRINT_LAST       TEXT("/LAST")
#define SW_PRINT_OPTIONS    TEXT("/OPTIONS")
#define SW_PRINT_PARMS      TEXT("/PARMS")
#define SW_PRINT_PRIORITY   TEXT("/PRIORITY")
#define SW_PRINT_PROCESSOR  TEXT("/PROCESSOR")
#define SW_PRINT_PURGE      TEXT("/PURGE")
#define SW_PRINT_RELEASE    TEXT("/RELEASE")
#define SW_PRINT_REMARK     TEXT("/REMARK")
#define SW_PRINT_ROUTE      TEXT("/ROUTE")
#define SW_PRINT_SEPARATOR  TEXT("/SEPARATOR")
#define SW_PRINT_UNTIL      TEXT("/UNTIL")
#define SW_PRINT_DRIVER     TEXT("/DRIVER")

#define SW_SHARE_COMM       TEXT("/COMM")
#define SW_SHARE_DELETE     TEXT("/DELETE")
#define SW_SHARE_PERMISSIONS    TEXT("/PERMISSIONS")
#define SW_SHARE_PRINT      TEXT("/PRINT")
#define SW_SHARE_REMARK     TEXT("/REMARK")
#define SW_SHARE_UNLIMITED  TEXT("/UNLIMITED")
#define SW_SHARE_USERS      TEXT("/USERS")

#define SW_USE_COMM     TEXT("/COMM")
#define SW_USE_USER     TEXT("/USER")
#define SW_USE_SMARTCARD    TEXT("/SMARTCARD")
#define SW_USE_SAVECRED     TEXT("/SAVECRED")
#define SW_USE_HOME     TEXT("/HOME")
#define SW_USE_DELETE       TEXT("/DELETE")
#define SW_USE_PERSISTENT   TEXT("/PERSISTENT")

#define SW_NETRUN_MAXRUNS   TEXT("/MAXRUNS")
#define SW_NETRUN_RUNPATH   TEXT("/RUNPATH")

#define SW_USER_ACTIVE      TEXT("/ACTIVE")
#define SW_USER_COUNTRYCODE TEXT("/COUNTRYCODE")
#define SW_USER_EXPIRES     TEXT("/EXPIRES")
#define SW_USER_ENABLESCRIPT    TEXT("/ENABLESCRIPT")
#define SW_USER_FULLNAME    TEXT("/FULLNAME")
#define SW_USER_HOMEDIR     TEXT("/HOMEDIR")
#define SW_USER_LOGONSERVER TEXT("/LOGONSERVER")
#define SW_USER_MAXSTORAGE  TEXT("/MAXSTORAGE")
#define SW_USER_PARMS       TEXT("/PARMS")
#define SW_USER_PASSWORDREQ TEXT("/PASSWORDREQ")
#define SW_USER_PASSWORDCHG TEXT("/PASSWORDCHG")
#define SW_USER_SCRIPTPATH  TEXT("/SCRIPTPATH")
#define SW_USER_TIMES       TEXT("/TIMES")
#define SW_USER_USERCOMMENT TEXT("/USERCOMMENT")
#define SW_USER_WORKSTATIONS    TEXT("/WORKSTATIONS")
#define SW_USER_PROFILEPATH TEXT("/PROFILEPATH")

#if defined(NTENV)
#define SW_SRV_SRVCOMMENT   TEXT("/SRVCOMMENT")
#define SW_SRV_AUTODISCONNECT   TEXT("/AUTODISCONNECT")
#define SW_SRV_MAXUSERS     TEXT("/USERS")
#define SW_SRV_SRVANNOUNCE  TEXT("/ANNOUNCE")
#define SW_SRV_SRVANNDELTA  TEXT("/ANNDELTA")
#define SW_SRV_MAXSESSOPENS TEXT("/SESSOPENS")
#define SW_SRV_NUMREQBUF    TEXT("/MAXWORKITEMS")
#define SW_SRV_SIZREQBUF    TEXT("/SIZREQBUF")
#define SW_SRV_NUMBIGBUF    TEXT("/RAWWORKITEMS")
#define SW_SRV_SRVHIDDEN    TEXT("/HIDDEN")
#define SW_SRV_DEBUG        TEXT("/DEBUG")

#else /* !NTENV */

#define SW_SRV_SRVCOMMENT   TEXT("/SRVCOMMENT")
#define SW_SRV_ACCESSALERT  TEXT("/ACCESSALERT")
#define SW_SRV_ALERTNAMES   TEXT("/ALERTNAMES")
#define SW_SRV_ALERTSCHED   TEXT("/ALERTSCHED")
#define SW_SRV_AUTODISCONNECT   TEXT("/AUTODISCONNECT")
#define SW_SRV_DISKALERT    TEXT("/DISKALERT")
#define SW_SRV_ERRORALERT   TEXT("/ERRORALERT")
#define SW_SRV_LOGONALERT   TEXT("/LOGONALERT")
#define SW_SRV_MAXAUDITLOG  TEXT("/MAXAUDITLOG")
#define SW_SRV_NETIOALERT   TEXT("/NETIOALERT")
#define SW_SRV_SRVHIDDEN    TEXT("/SRVHIDDEN")

#define SW_SRV_MAXUSERS     TEXT("/MAXUSERS")
#define SW_SRV_SECURITY     TEXT("/SECURITY")
#define SW_SRV_AUDITING     TEXT("/AUDITING")
#define SW_SRV_NOAUDITING   TEXT("/NOAUDITING")
#define SW_SRV_NUMADMIN     TEXT("/NUMADMIN")
#define SW_SRV_SRVNETS      TEXT("/SRVNETS")
#define SW_SRV_SRVSERVICES  TEXT("/SRVSERVICES")
#define SW_SRV_SRVANNOUNCE  TEXT("/SRVANNOUNCE")
#define SW_SRV_SRVANNDELTA  TEXT("/SRVANNDELTA")
#define SW_SRV_GUESTACCT    TEXT("/GUESTACCT")
#define SW_SRV_USERPATH     TEXT("/USERPATH")
#define SW_SRV_MAXSHARES    TEXT("/MAXSHARES")
#define SW_SRV_MAXSESSOPENS TEXT("/MAXSESSOPENS")
#define SW_SRV_MAXSESSREQS  TEXT("/MAXSESSREQS")
#define SW_SRV_NUMREQBUF    TEXT("/NUMREQBUF")
#define SW_SRV_SIZREQBUF    TEXT("/SIZREQBUF")
#define SW_SRV_NUMBIGBUF    TEXT("/NUMBIGBUF")
#define SW_SRV_SRVHEURISTICS    TEXT("/SRVHEURISTICS")
#define SW_SRV_IGNSVC       TEXT("/IGNSVC")
#define SW_SRV_AUTOPROFILE  TEXT("/AUTOPROFILE")
#define SW_SRV_AUTOPATH         TEXT("/AUTOPATH")

#endif

#define SW_ACCESS_GRANT     TEXT("/GRANT")
#define SW_ACCESS_CHANGE    TEXT("/CHANGE")
#define SW_ACCESS_REVOKE    TEXT("/REVOKE")
#define SW_ACCESS_TRAIL     TEXT("/TRAIL")
#define SW_ACCESS_TREE      TEXT("/TREE")
#define SW_ACCESS_SUCCESS   TEXT("/SUCCESS")
#define SW_ACCESS_FAILURE   TEXT("/FAILURE")

#define SW_ACCESS_OPEN      TEXT("OPEN")
#define SW_ACCESS_WRITE     TEXT("WRITE")
#define SW_ACCESS_DELETE    TEXT("DELETE")
#define SW_ACCESS_ACL       TEXT("ACL")
#define SW_ACCESS_NONE      TEXT("NONE")
#define SW_ACCESS_ALL       TEXT("ALL")

#define SW_DEV_RESTART      TEXT("/RESTART")

#define SW_STATS_CLEAR      TEXT("/CLEAR")

#define SW_ADMIN_COMMAND    TEXT("/COMMAND")

#define SW_FS_GREY      TEXT("/MONOCHROME")

#define SW_MESSAGE_BROADCAST    TEXT("/BROADCAST")

#define SW_ACCOUNTS_FORCELOGOFF TEXT("/FORCELOGOFF")
#define SW_ACCOUNTS_UNIQUEPW    TEXT("/UNIQUEPW")
#define SW_ACCOUNTS_MINPWLEN    TEXT("/MINPWLEN")
#define SW_ACCOUNTS_MINPWAGE    TEXT("/MINPWAGE")
#define SW_ACCOUNTS_MAXPWAGE    TEXT("/MAXPWAGE")
#define SW_ACCOUNTS_ROLE    TEXT("/ROLE")
#define SW_ACCOUNTS_PRIMARY TEXT("/PRIMARY")
#define SW_ACCOUNTS_LOCKOUT TEXT("/LOCKOUT")
#define SW_ACCOUNTS_SYNCH   TEXT("/SYNCHRONIZE")
#define SW_ACCOUNTS_LOCKOUT_THRESHOLD TEXT("/LOCKOUTTHRESHOLD")
#define SW_ACCOUNTS_LOCKOUT_DURATION  TEXT("/LOCKOUTDURATION")
#define SW_ACCOUNTS_LOCKOUT_WINDOW    TEXT("/LOCKOUTWINDOW")


#define SW_REPL_REPL         TEXT("/REPLICATE")
#define SW_REPL_EXPPATH      TEXT("/EXPORTPATH")
#define SW_REPL_IMPPATH      TEXT("/IMPORTPATH")
#define SW_REPL_EXPLIST      TEXT("/EXPORTLIST")
#define SW_REPL_IMPLIST      TEXT("/IMPORTLIST")
#define SW_REPL_TRYUSER      TEXT("/TRYUSER")
#define SW_REPL_LOGON        TEXT("/LOGON")
#define SW_REPL_PASSWD       TEXT("/PASSWORD")
#define SW_REPL_SYNCH        TEXT("/INTERVAL")
#define SW_REPL_PULSE        TEXT("/PULSE")
#define SW_REPL_GUARD        TEXT("/GUARDTIME")
#define SW_REPL_RANDOM       TEXT("/RANDOM")

#define SW_RIPL_RPL1            TEXT("/RPL1")
#define SW_RIPL_RPL2            TEXT("/RPL2")
#define SW_RIPL_RPL3            TEXT("/RPL3")
#define SW_RIPL_RPL4            TEXT("/RPL4")
#define SW_RIPL_RPL5            TEXT("/RPL5")
#define SW_RIPL_RPL6            TEXT("/RPL6")
#define SW_RIPL_RPL7            TEXT("/RPL7")
#define SW_RIPL_RPL8            TEXT("/RPL8")
#define SW_RIPL_RPL9            TEXT("/RPL9")
#define SW_RIPL_RPL10           TEXT("/RPL10")
#define SW_RIPL_RPL11           TEXT("/RPL11")
#define SW_RIPL_RPL12           TEXT("/RPL12")
#define SW_RIPL_RPLDIR          TEXT("/RPLDIR")
#define SW_RIPL_MAXTHREADS      TEXT("/MAXTHREADS")
#define SW_RIPL_AUDITINGFILE        TEXT("/AUDITINGFILE")
#define SW_RIPL_RPLCHECKSUM     TEXT("/RPLCHECKSUM")
#define SW_RIPL_FORCEDEFAULTBOOT    TEXT("/FORCEDEFAULTBOOT")
#define SW_RIPL_RPLLOGONKBD     TEXT("/RPLLOGONKBD")
#define SW_RIPL_RPLFILES        TEXT("/RPLFILES")
#define SW_RIPL_BINFILES        TEXT("/BINFILES")
#define SW_RIPL_TMPFILES        TEXT("/TMPFILES")
#define SW_RIPL_XNSSAP          TEXT("/XNSSAP")

#define SW_COPY_FROM        TEXT("/FROM")
#define SW_COPY_TO      TEXT("/TO")
#define SW_COPY_PASSWORD    TEXT("/PASSWORD")

#define SW_TIME_DOMAIN       TEXT("/DOMAIN")
#define SW_TIME_SET      TEXT("/SET")

#define SW_ALIAS_MODE       TEXT("/WHEN")
#define SW_ALIAS_PRIORITY   TEXT("/PRIORITY")

#define SW_UPS_BATTERYTIME  TEXT("/BATTERYTIME")
#define SW_UPS_CMDFILE      TEXT("/CMDFILE")
#define SW_UPS_DEVICENAME   TEXT("/DEVICENAME")
#define SW_UPS_MESSDELAY    TEXT("/MESSDELAY")
#define SW_UPS_MESSTIME     TEXT("/MESSTIME")
#define SW_UPS_RECHARGE     TEXT("/RECHARGE")
#define SW_UPS_SIGNALS      TEXT("/SIGNALS")
#define SW_UPS_VOLTLEVELS   TEXT("/VOLTLEVELS")

#define SW_COMPUTER_JOIN    TEXT("/JOIN")
#define SW_COMPUTER_LEAVE   TEXT("/LEAVE")
#define SW_NETWORK          TEXT("/NETWORK")
