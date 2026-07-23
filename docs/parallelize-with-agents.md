---
name: parallelize-with-agents
description: Default to spawning agents to parallelize independent work; overlap is fine
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 6da1abd0-6b46-4030-914d-2ce0fa01c5cd
---

The user wants independent work parallelized via subagents by default — "even just mostly, overlap is okay."

**Why:** throughput. Serial deep-dives waste wall-clock when sub-tasks don't depend on each other.

**How to apply:** when a turn has independent chunks — verifying a change across multiple build configs (embedjit / shadow / default), mapping several subsystems, building+testing separate dirs, researching N topics — dispatch them as concurrent agents in a single message rather than doing them one-by-one. Keep the tight sequential probe loops (debugging one failing test) inline, but fan out the independent verification/research/build legs. Use distinct build dirs per agent to avoid conflicts (e.g. one agent on cmake-build-shadow while the main loop uses cmake-build-embedjit). See [[build-dir-prefix]].
