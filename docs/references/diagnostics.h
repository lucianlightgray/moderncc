#ifndef CONJURE_DIAGNOSTICS_H
#define CONJURE_DIAGNOSTICS_H

#include "common.h"

#define ANSI_RESET "\033[0m"
#define ANSI_RED "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE "\033[34m"
#define ANSI_GRAY "\033[90m"
#define ANSI_WHITE "\033[97m"

enum {
    MAX_DIAGS = 1000,
};

typedef enum {
    DK_ERROR,
    DK_SYNTAX_ERROR,
    DK_TYPE_MISMATCH,
    DK_UNDEFINED_VARIABLE,

    DK_FLOW_ERROR,

    DK_REDECLARATION,

    DK_TYPE_CYCLE,

    DK_NON_EXHAUSTIVE_MATCH,

    DK_INVALID_TYPE,

    DK_MISSING_RETURN,

    DK_CONDITION_ERROR,

    DK_ITERATION_ERROR,

    DK_FFI_ERROR,

    DK_INFERENCE_ERROR,

    DK_UNINITIALIZED,

    DK_OVERLOAD_ERROR,

    DK_LINKER_ERROR,

    DK_VARIANT_ERROR,

    DK_COMPTIME_ERROR,

    DK_NOGC_VIOLATION,

    DK_WARNING,
    DK_DEPRECATED,
    DK_UNUSED,
    DK_UNREACHABLE,
    DK_REDUNDANT,

    DK_INFO,
} DiagnosticKind;

typedef struct {
    DiagnosticKind kind : 8;
    u32 len : 24;
    u32 pos;
    const char *message;
} Diagnostic;

typedef struct SourceFile SourceFile;

void printDiagnostic(SourceFile *file, Diagnostic diag);

Diagnostic *reportDiagnostic(SourceFile *file, DiagnosticKind kind, u32 pos,
                             u32 len, const char *message);

bool diagnosticKindIsError(DiagnosticKind kind);

void diagnosticFreeMessage(Diagnostic *diag);

i64 diagnosticLiveMessageCount(void);

#endif
