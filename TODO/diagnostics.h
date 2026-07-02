#ifndef CONJURE_DIAGNOSTICS_H
#define CONJURE_DIAGNOSTICS_H

#include "common.h"

#define ANSI_RESET "\033[0m"
#define ANSI_RED "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE "\033[34m"
#define ANSI_GRAY "\033[90m"
#define ANSI_WHITE "\033[97m"

/// Maximum number of diagnostics a single SourceFile will retain
/// before the parser enters silent-bail mode. The limit exists so
/// that catastrophically broken files (pasted binary data, partial
/// writes, encoding errors) can't spam a million-entry diagnostic
/// list. Once the limit is hit, the parser emits one final
/// "too many errors" diagnostic and suppresses everything after.
enum {
  MAX_DIAGS = 1000,
};

typedef enum {
  DK_ERROR,              /// Generic error (try to avoid this)
  DK_SYNTAX_ERROR,       /// Used by the parser for invalid syntax
  DK_TYPE_MISMATCH,      /// Used by the type checker for type errors
  DK_UNDEFINED_VARIABLE, /// Used by the name resolver for undefined
                         /// variables
  DK_FLOW_ERROR,         /// Used for invalid control flow, like `break` or
                         /// `continue` outside a loop
  DK_REDECLARATION,      /// Used when a name is declared more than once in the
                         /// same scope
  DK_TYPE_CYCLE,         /// Used when a type contains itself by value, giving it
                         /// an infinite size
  DK_NON_EXHAUSTIVE_MATCH, /// Used when a `when` over an enum subject leaves
                           /// some variant unhandled and has no `else` arm
  DK_INVALID_TYPE, /// Used when a type expression is malformed, such as the
                   /// redundant nestings `!?T` and `?!T`
  DK_MISSING_RETURN, /// Used when a value-returning function can reach its end
                     /// without returning a value
  DK_CONDITION_ERROR, /// Used when a condition is not a `bool`, `?T`, or `!T`,
                      /// such as the condition of an `if` or `while`
  DK_ITERATION_ERROR, /// Used when a `for` loop's subject is not an iterable
                      /// type
  DK_FFI_ERROR,       /// Used for malformed foreign-function-interface
                      /// declarations, such as an `@extern` annotation with a
                      /// missing or non-string symbol name
  DK_INFERENCE_ERROR, /// Used when a type cannot be inferred from context, such
                      /// as a bare `{ ... }` literal with no expected type
  DK_UNINITIALIZED, /// Used when a value that must be explicitly initialized is
                    /// left zero-initialized, such as a non-nullable pointer
                    /// field omitted from a struct literal
  DK_OVERLOAD_ERROR, /// Used for a malformed `overload` compiler-hook
                     /// declaration, such as an unknown hook kind, a signature
                     /// that does not match the hook, or a hook declared for a
                     /// type defined in another module
  DK_LINKER_ERROR,   /// Used for a malformed `link` declaration, such as an
                     /// unknown link-kind prefix or a guard condition that is
                     /// not a compile-time boolean
  DK_VARIANT_ERROR,  /// Used when a `+tag` variant file overrides a symbol the
                     /// untagged base does not declare, or overrides one with a
                     /// mismatched kind or signature
  DK_COMPTIME_ERROR, /// Used when a `@comptime` function call cannot be folded
                     /// to a constant during analysis, such as an argument that
                     /// is not compile-time known or a body using a construct
                     /// the compile-time interpreter does not support
  DK_NOGC_VIOLATION, /// Used when a `@nogc` function performs or could perform a
                     /// garbage-collected allocation: a `new`, an `&T{...}` heap
                     /// literal, an allocating string operator, or a call to a
                     /// function that is neither `@nogc` nor `@extern`

  DK_WARNING,     /// Generic warning (try to avoid this)
  DK_DEPRECATED,  /// Used for deprecated features
  DK_UNUSED,      /// Used for unused variables, functions, or other code
  DK_UNREACHABLE, /// Used for unreachable code
  DK_REDUNDANT,   /// Used for redundant or duplicate modifiers, like
                  /// `static static int x` or a second `const` on the
                  /// same type

  DK_INFO, /// Generic informational message (try to avoid this)
} DiagnosticKind;

/// Provides information about source code during lexing, parsing,
/// and analysis. Used for errors, warnings, and informational messages.
typedef struct {
  DiagnosticKind kind : 8; /// Severity of the diagnostic.
  u32 len : 24; /// Length of the source code this diagnostic covers (in bytes).
  u32 pos;      /// Start position in the source code (in bytes).
  const char *message; /// Diagnostic message text (requires free).
} Diagnostic;

// Forward declaration, defined in source.h. Using `typedef struct`
// here (rather than plain `struct`) matches source.h's definition
// so the two declarations refer to the same type in every TU.
typedef struct SourceFile SourceFile;

/// Pretty-prints a diagnostic message to the console. Requires the
/// related source file content to provide context.
void printDiagnostic(SourceFile *file, Diagnostic diag);

/// Adds a diagnostic to the given source file's diagnostic list
/// and returns a pointer to the stored entry.
Diagnostic *reportDiagnostic(SourceFile *file, DiagnosticKind kind, u32 pos,
                             u32 len, const char *message);

/// Reports whether a diagnostic of this severity is an error that should fail a
/// build. Warnings and informational messages are not errors, so a build that
/// produces only those still succeeds.
bool diagnosticKindIsError(DiagnosticKind kind);

/// Frees a diagnostic's heap-allocated message and accounts for the release.
/// Safe to call when the message is NULL, and clears the message pointer so a
/// repeated call is a no-op. SourceFile teardown frees its diagnostics through
/// this function so the live-message count stays balanced.
void diagnosticFreeMessage(Diagnostic *diag);

/// Returns the number of diagnostic messages currently allocated but not yet
/// freed. reportDiagnostic increments this count and diagnosticFreeMessage
/// decrements it, so a correctly balanced workload returns the count to its
/// starting value. Leak tests assert on this to confirm that heap-allocated
/// messages are released across repeated analysis.
i64 diagnosticLiveMessageCount(void);

#endif
