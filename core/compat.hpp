#ifndef COMPAT_H
#define COMPAT_H

#include <fstream>
#include <string>

/* Large file aware functions. */
#ifdef _MSC_VER
#undef fseeko
#define fseeko _fseeki64
#undef ftello
#define ftello _ftelli64
#endif

/* Path delimiter. */
#ifdef _WIN32
#define PATH_DELIM 0x5C
#else
#define PATH_DELIM 0x2F
#endif

using namespace std;

istream& d2vgetline(istream& is, string& str);

#endif
