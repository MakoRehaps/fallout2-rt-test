#ifndef INTERPRETER_EXTRA_H
#define INTERPRETER_EXTRA_H

// interpreter_extra.cc includes this header first. Keep a translation-unit
// marker alive so scripts.h can route script combat requests through the
// realtime co-op dispatcher without renaming scripts.cc's stock definitions.
#define LOCAL_COOP_INTERPRETER_EXTRA_TRANSLATION_UNIT 1

#include "interpreter.h"

namespace fallout {

void _intExtraClose_();
void _initIntExtra();
void intExtraUpdate();
void intExtraRemoveProgramReferences(Program* program);

} // namespace fallout

#endif /* INTERPRETER_EXTRA_H */
