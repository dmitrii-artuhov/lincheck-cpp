#!/usr/bin/bash
find runtime -iname *.h -o -iname *.cpp | xargs clang-format -i
find test -iname *.h -o -iname *.cpp | xargs clang-format -i
find codegen -iname *.h -o -iname *.cpp | xargs clang-format -i
