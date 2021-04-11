#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

#ifdef _MSC_VER 
// not #if defined(_WIN32) || defined(_WIN64)
// because we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define MAXPATHLEN MAX_PATH
#endif

#if defined __WIN32__ || defined _WIN32 || defined _Windows
#if !defined S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFDIR) == _S_IFDIR)
#endif
#endif