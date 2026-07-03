# OPTIMIZE — DRY / helper-extraction findings

Duplication and helper-extraction candidates for the mcc codebase, mined
mechanically from a statement-pattern analysis of the C we own (`src/`, `tools/`,
`runtime/`) and cross-checked with a raw-body hash/similarity pass. Every item is
grounded in a query or grep you can re-run.

- **How this was produced:** `python3 analysis/build.py` tokenizes the tree,
  splits every function into linear *statement units*, normalizes them (locals →
  `V1,V2…`, literals → placeholders, **call names & struct fields kept**), and
  mines contiguous groups that recur. Two databases are kept for exploration:
  `analysis/statements.db` (per-function patterns) and `analysis/inlined.db`
  (call-inlined streams + cross-function patterns). See `analysis/README.md`.
  Findings below were then confirmed with a raw-text function-body hash
  (`/tmp/dupfn.py`-style: exact-identical bodies across files) and a token-multiset
  Jaccard near-dup pass — the ranking tool over-counts (it renames enum constants,
  so same-shaped `switch`es collide), so **every item here was read in source.**
- **Corpus** (re-analyzed at commit `22f4049c`): 77 files · 1,929 functions ·
  38,114 statements → 10,817 recurring per-function patterns (n = 3…12) and
  31,380 cross-boundary patterns.
- **Reproduce:** `python3 analysis/query.py {candidates,sweep,xfunc,show,xshow}`;
  cross-file dups: the SQL at the bottom of this file.
- **Caveat:** the parser is a pragmatic heuristic, not a full C frontend. Ranking
  is trustworthy for *where to look*; **confirm each site before refactoring.**

---

## Reproduce the cross-file dup query

```sql
-- analysis/statements.db : n-grams (n>=4) spanning >=3 distinct src/ files
SELECT g.id, g.n, g.count, COUNT(DISTINCT f.id) AS files
FROM ngram g
JOIN ngram_occ o ON o.ngram_id = g.id
JOIN func fn ON fn.id = o.func_id
JOIN file f ON f.id = fn.file_id
WHERE g.n >= 4 AND f.dir = 'src'
GROUP BY g.id HAVING files >= 3
ORDER BY files DESC, g.n DESC, g.count DESC;
```
Exact-identical bodies: hash `cparse.raw_text(fn['tokens'])` per function across
`src/**/*.c` and group; near-dups: token-multiset Jaccard ≥ 0.88.
