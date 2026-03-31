#include "semantic.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALLEE_PREVIEW_HEAD 24
#define CALLEE_PREVIEW_TAIL 16

#define SEMANTIC_SPLIT_AGGREGATOR 1
#include "semantic_core_flow.inc"
#include "semantic_callable_rules.inc"
#include "semantic_scope_rules.inc"
#include "semantic_entry.inc"
#undef SEMANTIC_SPLIT_AGGREGATOR
