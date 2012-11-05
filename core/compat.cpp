#include <fstream>
#include <string>

#include "compat.hpp"

using namespace std;

/* Replacement function for getline that removes any trailing \r. */
istream& d2vgetline(istream& is, string& str)
{
    string tmp;

    str.clear();

    getline(is, tmp);

    if (tmp.size() != 0 && tmp.at(tmp.length() - 1) == 0x0D)
        tmp.erase(tmp.length() - 1);

    str = tmp;

    return is;
}
