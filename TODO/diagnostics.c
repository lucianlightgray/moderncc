#include "diagnostics.h"
#include "sourcefile.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

//
// MARK: Fatal errors
//

_Noreturn void panic(const char *fmt, ...) {
  fflush(stdout);
  fputs("internal compiler error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(EXIT_FAILURE);
}

//
// MARK: ANSI color codes
//

static const char *diagnosticKindStrings[] = {
    [DK_ERROR] = "error",
    [DK_SYNTAX_ERROR] = "syntax error",
    [DK_TYPE_MISMATCH] = "type mismatch",
    [DK_UNDEFINED_VARIABLE] = "undefined variable",
    [DK_FLOW_ERROR] = "control flow error",
    [DK_REDECLARATION] = "redeclaration",
    [DK_TYPE_CYCLE] = "recursive type",
    [DK_NON_EXHAUSTIVE_MATCH] = "non-exhaustive match",
    [DK_INVALID_TYPE] = "invalid type",
    [DK_MISSING_RETURN] = "missing return",
    [DK_CONDITION_ERROR] = "condition error",
    [DK_ITERATION_ERROR] = "iteration error",
    [DK_FFI_ERROR] = "FFI error",
    [DK_INFERENCE_ERROR] = "inference error",
    [DK_UNINITIALIZED] = "uninitialized value",
    [DK_OVERLOAD_ERROR] = "overload error",
    [DK_LINKER_ERROR] = "linker error",
    [DK_VARIANT_ERROR] = "variant error",
    [DK_COMPTIME_ERROR] = "compile-time evaluation error",
    [DK_NOGC_VIOLATION] = "allocation in a @nogc function",
    [DK_WARNING] = "warning",
    [DK_DEPRECATED] = "deprecated code",
    [DK_UNUSED] = "unused code",
    [DK_UNREACHABLE] = "unreachable code",
    [DK_REDUNDANT] = "redundant modifier",
    [DK_INFO] = "info",
};

//
// MARK: Helpers
//

/// Return the terminal width, or 80 as a fallback.
static int termWidth(void) {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
  return 80;
#else
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
    return ws.ws_col;
  return 80;
#endif
}

/// Return the ANSI color escape for a diagnostic severity.
static const char *sevColor(DiagnosticKind kind) {
  if (kind >= DK_INFO)
    return ANSI_BLUE;
  if (kind >= DK_WARNING)
    return ANSI_YELLOW;
  return ANSI_RED;
}

bool diagnosticKindIsError(DiagnosticKind kind) { return kind < DK_WARNING; }

/// Get a pointer to the start of line `lineIdx` in the source, and
/// compute its length (excluding the trailing newline/NUL).
static void getSourceLine(SourceFile *file, u32 lineIdx, const char **out,
                          u32 *outLen) {
  u32 start = file->lineStarts.items[lineIdx];
  u32 end;
  if (lineIdx + 1 < file->lineStarts.len)
    end = file->lineStarts.items[lineIdx + 1];
  else
    end = file->srcLen;

  // Strip trailing newline / carriage return.
  while (end > start &&
         (file->src[end - 1] == '\n' || file->src[end - 1] == '\r'))
    end--;

  *out = file->src + start;
  *outLen = end - start;
}

/// Return true if a source line is blank (all whitespace).
static bool isBlankLine(const char *line, u32 len) {
  for (u32 i = 0; i < len; i++) {
    if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
      return false;
  }
  return true;
}

/// Count leading tabs in a line.
static u32 countLeadingTabs(const char *line, u32 len) {
  u32 n = 0;
  for (u32 i = 0; i < len && line[i] == '\t'; i++)
    n++;
  return n;
}

/// Print a message with word wrapping at `maxwidth` columns.
static void printWrapped(const char *msg, int maxwidth) {
  int col = 0;
  const char *p = msg;

  while (*p) {
    // Skip leading spaces between words.
    while (*p == ' ')
      p++;
    if (!*p)
      break;

    // Find end of current word.
    const char *wordStart = p;
    while (*p && *p != ' ')
      p++;
    int wordLen = (int)(p - wordStart);

    // If adding this word would overflow, start a new line.
    if (col > 0 && col + 1 + wordLen > maxwidth) {
      fputc('\n', stdout);
      col = 0;
    }

    if (col > 0) {
      fputc(' ', stdout);
      col++;
    }

    fwrite(wordStart, 1, wordLen, stdout);
    col += wordLen;
  }

  if (col > 0)
    fputc('\n', stdout);
}

//
// MARK: Pretty-print
//

void printDiagnostic(SourceFile *file, Diagnostic diag) {
  int maxwidth = termWidth();
  const char *color = sevColor(diag.kind);
  const char *kindStr = diagnosticKindStrings[diag.kind];

  // Resolve line/column for start and end of the diagnostic span.
  // LineCol is 1-based for user-facing output; the rest of this
  // function operates on 0-based line/column indices to match
  // `file->lineStarts` so the existing caret/gutter arithmetic keeps
  // working unchanged.
  LineCol start = findLineColumn(file, diag.pos);
  LineCol end = findLineColumn(file, diag.pos + diag.len);
  u32 sl = start.line - 1, sc = start.col - 1;
  u32 el = end.line - 1, ec = end.col - 1;

  // Header: "syntax error at path/file.cjr:4:10 ..."
  printf("%s%s at %s:%u:%u ...%s\n\n", color, kindStr, file->path, start.line,
         start.col, ANSI_RESET);

  // --- Determine context range (up to 2 non-blank lines before/after) ---
  const u32 contextWant = 2;

  u32 startLine = sl;
  u32 foundBefore = 0;
  for (u32 i = sl; i > 0 && foundBefore < contextWant;) {
    i--;
    const char *ln;
    u32 lnLen;
    getSourceLine(file, i, &ln, &lnLen);
    if (!isBlankLine(ln, lnLen)) {
      foundBefore++;
      startLine = i;
    }
  }

  u32 endLine = sl;
  u32 foundAfter = 0;
  for (u32 i = sl + 1; i < file->lineStarts.len && foundAfter < contextWant;
       i++) {
    const char *ln;
    u32 lnLen;
    getSourceLine(file, i, &ln, &lnLen);
    if (!isBlankLine(ln, lnLen)) {
      foundAfter++;
      endLine = i;
    }
  }

  // Line number gutter width.
  char numBuf[16];
  int gutterWidth = snprintf(numBuf, sizeof(numBuf), "%u", endLine + 1);

  // --- Print context + error lines ---
  bool inBlankSeq = false;
  bool blankPrinted = false;

  for (u32 i = startLine; i <= endLine; i++) {
    const char *ln;
    u32 lnLen;
    getSourceLine(file, i, &ln, &lnLen);

    bool blank = isBlankLine(ln, lnLen);

    // Collapse consecutive blank lines into " ⋮ ".
    if (blank && i != sl) {
      if (!inBlankSeq) {
        inBlankSeq = true;
        blankPrinted = false;
      }
      continue;
    }
    if (inBlankSeq && !blankPrinted) {
      printf(ANSI_GRAY "%*s ⋮ " ANSI_RESET "\n", gutterWidth, "");
      blankPrinted = true;
    }
    inBlankSeq = false;

    // Copy the line into a mutable buffer for truncation.
    char lineBuf[2048];
    u32 copyLen =
        lnLen < sizeof(lineBuf) - 4 ? lnLen : (u32)sizeof(lineBuf) - 4;
    memcpy(lineBuf, ln, copyLen);
    lineBuf[copyLen] = '\0';

    // Account for leading tabs (each tab renders as 4 spaces).
    u32 leadingTabs = countLeadingTabs(ln, lnLen);

    // Replace tabs with spaces for display.
    char expandBuf[2048];
    u32 ei = 0;
    for (u32 j = 0; j < copyLen && ei < sizeof(expandBuf) - 4; j++) {
      if (lineBuf[j] == '\t') {
        expandBuf[ei++] = ' ';
        expandBuf[ei++] = ' ';
        expandBuf[ei++] = ' ';
        expandBuf[ei++] = ' ';
      } else {
        expandBuf[ei++] = lineBuf[j];
      }
    }
    expandBuf[ei] = '\0';
    u32 expandLen = ei;

    // Adjusted columns for the error line (accounting for tab expansion).
    u32 adjSc = sc;
    u32 adjEc = ec;
    if (i == sl) {
      adjSc += leadingTabs * 3; // tab is 4 chars, already counted 1
      adjEc += leadingTabs * 3;
      if (el > sl)
        adjEc = expandLen > 0 ? expandLen - 1 : 0;
    }

    // Horizontal truncation: if the error starts beyond the visible area,
    // shift the view to show the error in context.
    char displayBuf[2048];
    u32 displayLen = expandLen;
    u32 dispSc = adjSc;
    u32 dispEc = adjEc;

    if (i == sl && adjSc > (u32)maxwidth - (u32)gutterWidth - 10) {
      int prefixLen =
          snprintf(displayBuf, sizeof(displayBuf), "... (col %u) ... ", sc + 1);
      u32 skip = adjSc;
      if (skip < expandLen) {
        u32 remaining = expandLen - skip;
        u32 room = (u32)sizeof(displayBuf) - prefixLen - 1;
        u32 toCopy = remaining < room ? remaining : room;
        memcpy(displayBuf + prefixLen, expandBuf + skip, toCopy);
        displayBuf[prefixLen + toCopy] = '\0';
        displayLen = prefixLen + toCopy;
      } else {
        displayBuf[prefixLen] = '\0';
        displayLen = prefixLen;
      }
      dispSc = (u32)prefixLen;
      dispEc = (dispEc > adjSc) ? dispEc - adjSc + (u32)prefixLen : dispSc + 1;
    } else {
      memcpy(displayBuf, expandBuf, expandLen + 1);
    }

    // Right truncation: if the line is still too long, cut it.
    int lineRoom = maxwidth - gutterWidth - 3; // " | " takes 3 chars
    if (lineRoom < 20)
      lineRoom = 20;
    if ((int)displayLen > lineRoom) {
      displayLen = lineRoom - 3;
      memcpy(displayBuf + displayLen, "...", 4);
      displayLen += 3;
      if (i == sl && dispEc > displayLen)
        dispEc = displayLen;
    }

    // Print the line.
    if (i == sl) {
      // Error line: bright source, colored carets.
      printf(ANSI_GRAY "%*u | " ANSI_RESET ANSI_WHITE "%s" ANSI_RESET "\n",
             gutterWidth, i + 1, displayBuf);

      // Caret line.
      u32 caretStart = dispSc + gutterWidth + 3; // gutter + " | "
      u32 caretCount = (dispEc > dispSc) ? dispEc - dispSc : 1;
      if (caretCount > 200)
        caretCount = 200;

      printf("%s", color);
      for (u32 c = 0; c < caretStart; c++)
        fputc(' ', stdout);
      for (u32 c = 0; c < caretCount; c++)
        fputc('^', stdout);
      printf(ANSI_RESET "\n");
    } else {
      // Context line: dimmed.
      printf(ANSI_GRAY "%*u | %s" ANSI_RESET "\n", gutterWidth, i + 1,
             displayBuf);
    }
  }

  // Trailing blank-sequence ellipsis.
  if (inBlankSeq && !blankPrinted)
    printf(ANSI_GRAY "%*s ⋮ " ANSI_RESET "\n", gutterWidth, "");

  // Diagnostic message with word wrapping.
  fputc('\n', stdout);
  printWrapped(diag.message, maxwidth);
  printf("\n\n");
}

//
// MARK: Diagnostic storage
//

/// Running count of heap-allocated diagnostic messages that have not yet been
/// freed. Maintained by reportDiagnostic and diagnosticFreeMessage so leak
/// tests can confirm the count returns to its starting value.
static i64 gLiveMessageCount = 0;

i64 diagnosticLiveMessageCount(void) { return gLiveMessageCount; }

void diagnosticFreeMessage(Diagnostic *diag) {
  if (!diag->message)
    return;
  free((void *)diag->message);
  diag->message = NULL;
  gLiveMessageCount--;
}

Diagnostic *reportDiagnostic(SourceFile *file, DiagnosticKind kind, u32 pos,
                             u32 len, const char *message) {
  // A diagnostic always highlights at least one source character so
  // the pretty-printer's caret underline has something to point at.
  // Zero-length spans from older call sites get promoted to one.
  if (len == 0)
    len = 1;

  // When a one-character span would reach past the end of the file
  // (the typical "unexpected EOF" case, reported at offset srcLen),
  // scoot the span back so it lands on the file's final byte.  This
  // keeps the caret inside the source instead of dangling off the
  // end where there is nothing to underline.
  if (len == 1 && pos >= file->srcLen && file->srcLen > 0)
    pos = file->srcLen - 1;

  // Copy the message so sourceFileFree can unconditionally free it.
  // Callers typically pass string literals from diagnostic tables; the
  // Diagnostic struct's contract is that `message` is heap-allocated.
  char *owned = NULL;
  if (message) {
    usize n = strlen(message);
    owned = malloc(n + 1);
    if (!owned)
      panic("Failed to allocate memory for a diagnostic message.");
    memcpy(owned, message, n + 1);
    gLiveMessageCount++;
  }

  listAppend(file->diagnostics, ((Diagnostic){
                                   .kind = kind,
                                   .pos = pos,
                                   .len = len,
                                   .message = owned,
                               }));

  return &file->diagnostics.items[file->diagnostics.len - 1];
}
