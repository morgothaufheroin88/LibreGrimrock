#!/bin/sh
# Strips the inlined SharedPtr refcount bookkeeping and other noise from bindings.py output.
grep -v "0x811c9dc5\|0x1000193\|sm_refcount\|piVar1\[1\] = piVar1\[1\] + 1\|goto LAB\|piVar1 = (int \*)piVar1\[2\]\|while( true )\|HashMap<void\*,int>::insert\|^LAB_\|do {$\|^    }$\|if (piVar1 == (int \*)0x0) {\|^  } while\|^  do {\|CatchHandler\|^  [A-Za-z_:<>]* \*[a-zA-Z_0-9]*;$\|^  [A-Za-z_:<>]* \*\*[a-zA-Z_0-9]*;$\|local_1[048] = \|local_[0-9a-f]*\[[01]\] = 1;\|local_[0-9a-f]* = (uint \*)0x0;" "$@"
