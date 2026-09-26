// compile-new-api.cpp - compile-only check (see tests/CMakeLists.txt): code that
// uses only the 4.0 API, including d[key], must build with no deprecation warning
// under -Werror=deprecated-declarations.
#include "Arduino.h"
#include "Dictionary.h"

int main() {
    Dictionary d;
    d.set("a", "1");
    d.set("n", 5u);
    String s = d["a"];
    String k = d.key(0);
    String v = d.value(0);
    return (int)(s.length() + k.length() + v.length() + d.has("a") + d.getInt("n"));
}
