/*    INTERN.h
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * EXT  designates a global var which is defined in perl.h
 * dEXT designates a global var which is defined in another
 *      file, so we can't count on finding it in perl.h
 *      (this practice should be avoided).
 */
#undef EXT
#undef dEXT
#undef EXTCONST
#undef dEXTCONST
#if defined(VMS) && !defined(__GNUC__)
    /* Suppress portability warnings from DECC for VMS-specific extensions */
#  ifdef __DECC
#    pragma message disable (GLOBALEXT,NOSHAREEXT,READONLYEXT)
#  endif
#  define EXT globaldef {"$GLOBAL_RW_VARS"} noshare
#  define dEXT globaldef {"$GLOBAL_RW_VARS"} noshare
#  define EXTCONST globaldef {"$GLOBAL_RO_VARS"} readonly
#  define dEXTCONST globaldef {"$GLOBAL_RO_VARS"} readonly
#else
#  ifdef __cplusplus
#   define EXT
#   define dEXT
#   define EXTCONST extern const
#   define dEXTCONST const
#else
#  define EXT
#  define dEXT
#  define EXTCONST const
#  define dEXTCONST const
#endif
#endif

#undef INIT
#define INIT(x) = x

#define DOINIT
