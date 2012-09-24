#ifndef WIN_TO_LINUX_H_
#define WIN_TO_LINUX_H_
#define TRUE								1
#define FALSE							0
#define true								1
#define false								0

typedef unsigned int DWORD;
typedef int bool;
typedef int BOOL;
typedef unsigned int UINT;

#if defined(WIN32) || defined(WIN64)
#define snprintf		_snprintf
#define sprintf		sprintf_s
#define access		_access
#define mkdir			_mkdir
#endif

#endif