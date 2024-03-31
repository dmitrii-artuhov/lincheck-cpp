#!/usr/bin/bash
find runtime -iname *.h -o -iname *.cpp | xargs clang-format -style=file -i
find test -iname *.h -o -iname *.cpp | xargs clang-format -style=file -i
find codegen -iname *.h -o -iname *.cpp | xargs clang-format -style=file -i
