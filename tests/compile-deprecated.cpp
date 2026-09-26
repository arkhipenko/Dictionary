// compile-deprecated.cpp - compile-only check (see tests/CMakeLists.txt).
// Every call below is a 3.x API that 4.0 keeps as a deprecated wrapper. Built with
// -Werror=deprecated-declarations it must FAIL; built the same way with
// _DICT_NO_DEPRECATION_WARNINGS it must compile.
#include "Arduino.h"
#include "Dictionary.h"

int main() {
    Dictionary d;
    d.insert("a", "1");
    d("b", "2");
    String s = d.search("a");
    bool e = d("a");
    String k = d(0);
    String v = d[0];
    return (int)(s.length() + e + k.length() + v.length());
}
