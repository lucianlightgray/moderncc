#!/usr/bin/env python3
"""Enforce tests/gate-contract.txt (T-lin-10003).

The most repeated defect in this tree is a measurement reporting success over an
empty or truncated subject: `flagsweep/accept` printed PASS with
`MCC=/nonexistent/mcc`; `spvgate` prints OK for a case that lowered nothing;
`gate-ledger.sh` scored twelve gates off a compile that never happened;
`rir/drop-ratchet` checked that its report file existed rather than that mcc
exited 0. The rule that yields is two-sided, and a gate needs both sides:

  FLOOR   the cell fails, rather than passes, when its subject is empty or
          truncated. Count where the thing becomes true, not where it is
          attempted.
  PROVER  some registered cell reverts the fix or perturbs the bank and shows
          this cell going red while naming its subject. Build the known-positive
          before believing the green.

Neither side can be inferred from a cell's name, so both are declared, per cell,
in tests/gate-contract.txt -- and this tool is what makes the declaration cost
something. It refuses a manifest that has quietly shrunk, a floor flag the
build no longer passes, a prover that is not registered, a prover that is an
`mcc_skip_test` echo stub, and a proof cell that no row claims. The `unfloored`
and `unproved` counts are ratchets: they are pinned on the command line and may
only ever fall.

  --build DIR       the build to read registrations from (ctest --show-only).
  --manifest FILE   tests/gate-contract.txt.
  --must-run FILE   tests/must-run.txt. Two ways in: every non-proof row of the
                    must-run manifest must appear here, so a cell the tree has
                    already said it must not lose cannot also be a gate nobody
                    declared; and every prover must appear THERE, so a proof
                    cannot silently stop being registered.
  --mutate WHAT     perturb the manifest in memory and expect to go red. This is
                    this cell's own known-positive; see ci/gate-contract-known-positive.

A cell registered as an `mcc_skip_test` echo stub on THIS host is host-skipped:
it has no command line to floor and no prover that can go red here, so this tool
reads nothing about it either way and counts it in neither ratchet. The same
holds for a row whose prover is stubbed here. That is a host verdict, not a
defect -- `ast/o0-baseline` on Darwin and `gpu/msl-slice-known-positive` on
Linux are the same situation wearing each platform's face, and a contract that
went red on either would be measuring which host ran it rather than whether the
gate can fail. The first version of this tool did go red on exactly that, on
Darwin, over five cells and one bug.

Exit 0 clean, 1 on any violation, 2 on a usage or parse problem. Never 77: a
contract that cannot be checked is a failure, not a skip.
"""

import argparse
import json
import re
import subprocess
import sys

FLOOR_MIN = re.compile(r"^min:(--[A-Za-z0-9][-A-Za-z0-9]*)$")
FLOOR_INTRINSIC = re.compile(r"^intrinsic:(.+)$")
PROVER_SELF = re.compile(r"^self:(.+)$")
UNFLOORED = "unfloored"
UNPROVED = "unproved"
PROOF_SUFFIX = "-known-positive"

MUTATIONS = ("drop-row", "relax-floor", "forge-prover", "unlisted-prover", "empty")


def die(msg):
    sys.stderr.write("gate-contract: %s\n" % msg)
    sys.exit(2)


def load_manifest(path):
    rows = []
    seen = {}
    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = [p.strip() for p in line.split("|")]
            if len(parts) != 4:
                die("%s:%d: expected 4 `|`-separated columns "
                    "(cell | floor | prover | why), got %d" % (path, lineno, len(parts)))
            cell, floor, prover, why = parts
            if not cell or not floor or not prover or not why:
                die("%s:%d: no column may be empty; the `why` is what makes an "
                    "`%s`/`%s` row an argument rather than a shrug"
                    % (path, lineno, UNFLOORED, UNPROVED))
            if cell in seen:
                die("%s:%d: %s is already declared at line %d"
                    % (path, lineno, cell, seen[cell]))
            seen[cell] = lineno
            rows.append({"cell": cell, "floor": floor, "prover": prover,
                         "why": why, "line": lineno})
    if not rows:
        die("%s: the manifest is empty, so it asserts nothing" % path)
    return rows


def load_must_run(path):
    names = []
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            names.append(line.split("|")[0].strip())
    if not names:
        die("%s: the must-run manifest is empty, so the population this "
            "contract is checked over is empty too" % path)
    return names


def registrations(build):
    out = subprocess.run(["ctest", "--test-dir", build, "-N", "--show-only=json-v1"],
                         capture_output=True, text=True)
    if out.returncode != 0:
        die("ctest --show-only failed in %s:\n%s" % (build, out.stderr))
    try:
        data = json.loads(out.stdout)
    except ValueError as exc:
        die("cannot parse ctest --show-only=json-v1: %s" % exc)
    tests = {}
    for t in data.get("tests", []):
        tests[t["name"]] = [str(c) for c in t.get("command", [])]
    if not tests:
        die("%s registers no tests at all, so nothing here can be checked" % build)
    return tests


def simulate_host_skip_proved(tests, rows, want):
    """Stub the first `want` prover-carrying gates that really run here.

    The named-cell form cannot be used by a registered cell: the gates that
    host-skip on Darwin are not registered at all on Windows, so naming them
    would die there rather than check anything. Picking them by property
    instead makes the same assertion on every platform."""
    picked = []
    for r in rows:
        if len(picked) == want:
            break
        cell = r["cell"]
        if r["prover"] == UNPROVED or cell not in tests:
            continue
        if is_echo_stub(tests[cell]):
            continue
        picked.append(cell)
    if len(picked) < want:
        die("--simulate-host-skip-proved %d: only %d prover-carrying gate(s) "
            "run for real on this host, so the simulation would be over a "
            "smaller subject than it claims" % (want, len(picked)))
    for cell in picked:
        tests[cell] = ["cmake", "-E", "echo",
                       "%s: simulated mcc_skip_test stub" % cell]
    return tests, picked


def simulate_host_skip(tests, cells, manifest_cells):
    for cell in cells:
        if cell not in manifest_cells:
            die("--simulate-host-skip names %s, which no manifest row declares; "
                "a simulation over a cell this contract says nothing about "
                "proves nothing" % cell)
        if cell not in tests:
            die("--simulate-host-skip names %s, which this build does not "
                "register; there is nothing here to skip" % cell)
        tests[cell] = ["cmake", "-E", "echo",
                       "%s: simulated mcc_skip_test stub" % cell]
    return tests


def is_echo_stub(cmd):
    for i, c in enumerate(cmd):
        if c == "-E" and i + 1 < len(cmd) and cmd[i + 1] == "echo":
            return True
    return False


def floor_arg_value(cmd, flag):
    for i, c in enumerate(cmd):
        if c == flag:
            return cmd[i + 1] if i + 1 < len(cmd) else None
        if c.startswith(flag + "="):
            return c[len(flag) + 1:]
    return None


def mutate(rows, what):
    if what == "empty":
        return []
    out = [dict(r) for r in rows]
    if what == "drop-row":
        for i, r in enumerate(out):
            if r["prover"] not in (UNPROVED,) and not PROVER_SELF.match(r["prover"]):
                del out[i]
                return out
        die("drop-row: no proved row to drop")
    if what == "relax-floor":
        for r in out:
            if FLOOR_MIN.match(r["floor"]):
                r["floor"] = UNFLOORED
                return out
        die("relax-floor: no `min:` row to relax")
    if what == "forge-prover":
        for r in out:
            if r["prover"] == UNPROVED:
                r["prover"] = "no/such-cell" + PROOF_SUFFIX
                return out
        die("forge-prover: no unproved row to forge a prover onto")
    if what == "unlisted-prover":
        for r in out:
            if r["prover"] == UNPROVED:
                r["prover"] = "mcc_build"
                return out
        die("unlisted-prover: no unproved row to point at an unlisted cell")
    die("unknown mutation %r (want one of %s)" % (what, ", ".join(MUTATIONS)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", required=True)
    ap.add_argument("--manifest", required=True)
    ap.add_argument("--must-run", required=True)
    ap.add_argument("--min-rows", type=int, default=1)
    ap.add_argument("--min-proved", type=int, default=1)
    ap.add_argument("--max-unfloored", type=int, default=None)
    ap.add_argument("--max-unproved", type=int, default=None)
    ap.add_argument("--mutate", choices=MUTATIONS, default=None)
    ap.add_argument("--simulate-host-skip", default=None)
    ap.add_argument("--simulate-host-skip-proved", type=int, default=None)
    a = ap.parse_args()

    rows = load_manifest(a.manifest)
    if a.mutate:
        rows = mutate(rows, a.mutate)
    must_run = load_must_run(a.must_run)
    must_run_set = set(must_run)
    tests = registrations(a.build)
    if a.simulate_host_skip:
        want = [c.strip() for c in a.simulate_host_skip.split(",") if c.strip()]
        if not want:
            die("--simulate-host-skip was given no cell names")
        tests = simulate_host_skip(tests, want, set(r["cell"] for r in rows))
    if a.simulate_host_skip_proved is not None:
        if a.simulate_host_skip_proved < 1:
            die("--simulate-host-skip-proved needs a positive count; %d would "
                "simulate nothing" % a.simulate_host_skip_proved)
        tests, picked = simulate_host_skip_proved(tests, rows,
                                                  a.simulate_host_skip_proved)
        print("gate-contract: simulating this host skipping %d prover-carrying "
              "gate(s): %s" % (len(picked), ", ".join(picked)))

    bad = []
    declared = set(r["cell"] for r in rows)
    claimed_provers = set()
    unfloored = []
    unproved = []
    host_skipped = []
    host_skipped_proved = []
    stubbed = []
    proved = 0
    floored = 0

    for name in must_run:
        if name.endswith(PROOF_SUFFIX):
            continue
        if name not in declared:
            bad.append("%s is a tests/must-run.txt row and is declared in no "
                       "gate-contract row, so nothing states whether it has a "
                       "floor or a prover" % name)

    for r in rows:
        cell, floor, prover = r["cell"], r["floor"], r["prover"]
        if cell not in tests:
            bad.append("%s (line %d) is declared here and is not registered by "
                       "this build; the gate it describes is gone"
                       % (cell, r["line"]))
            continue
        cmd = tests[cell]
        if floor == UNFLOORED:
            unfloored.append(cell)
        if prover == UNPROVED:
            unproved.append(cell)

        if is_echo_stub(cmd):
            host_skipped.append(cell)
            if prover == UNPROVED:
                continue
            if PROVER_SELF.match(prover):
                host_skipped_proved.append(cell)
                continue
            names = [p.strip() for p in prover.split(",") if p.strip()]
            if not names:
                bad.append("%s: the prover column is empty" % cell)
                continue
            ok = True
            for p in names:
                claimed_provers.add(p)
                if p not in tests:
                    bad.append("%s names %s as its known-positive and this "
                               "build does not register it; the proof is gone "
                               "and the green means nothing. %s is host-skipped "
                               "here, which excuses running the proof, not "
                               "declaring one that exists" % (cell, p, cell))
                    ok = False
                elif p not in must_run_set:
                    bad.append("%s names %s as its known-positive and %s is not "
                               "in tests/must-run.txt, so the proof can stop "
                               "being registered without anything noticing"
                               % (cell, p, p))
                    ok = False
            if ok:
                host_skipped_proved.append(cell)
            continue

        m = FLOOR_MIN.match(floor)
        if m:
            flag = m.group(1)
            got = floor_arg_value(cmd, flag)
            if got is None:
                bad.append("%s declares its floor as `%s` and this build's "
                           "command line does not pass it, so the subject is "
                           "unbounded below" % (cell, flag))
            else:
                try:
                    n = int(got)
                except ValueError:
                    bad.append("%s passes %s %r, which is not an integer floor"
                               % (cell, flag, got))
                    n = 0
                if n < 1:
                    bad.append("%s passes %s %s; a floor of %s cannot reject an "
                               "empty subject" % (cell, flag, got, got))
                else:
                    floored += 1
        elif FLOOR_INTRINSIC.match(floor):
            floored += 1
        elif floor == UNFLOORED:
            pass
        else:
            bad.append("%s: floor %r is not `min:--flag`, `intrinsic:<why>` or "
                       "`%s`" % (cell, floor, UNFLOORED))

        if prover == UNPROVED:
            pass
        elif PROVER_SELF.match(prover):
            proved += 1
        else:
            names = [p.strip() for p in prover.split(",") if p.strip()]
            if not names:
                bad.append("%s: the prover column is empty" % cell)
                continue
            ok = True
            for p in names:
                claimed_provers.add(p)
                if p not in tests:
                    bad.append("%s names %s as its known-positive and this "
                               "build does not register it; the proof is gone "
                               "and the green means nothing" % (cell, p))
                    ok = False
                    continue
                if is_echo_stub(tests[p]):
                    stubbed.append("%s (prover %s)" % (cell, p))
                    ok = False
                    continue
                if p not in must_run_set:
                    bad.append("%s names %s as its known-positive and %s is not "
                               "in tests/must-run.txt, so the proof can stop "
                               "being registered without anything noticing"
                               % (cell, p, p))
                    ok = False
            if ok:
                proved += 1

    for name, cmd in sorted(tests.items()):
        if not name.endswith(PROOF_SUFFIX):
            continue
        if is_echo_stub(cmd):
            continue
        if name not in claimed_provers:
            bad.append("%s is a registered known-positive that no gate-contract "
                       "row claims, so whatever it proves is not attached to a "
                       "gate" % name)

    if len(rows) < a.min_rows:
        bad.append("the manifest declares %d gate(s) and at least %d were "
                   "expected; a contract that covers nothing reports green over "
                   "nothing" % (len(rows), a.min_rows))
    carry = proved + len(stubbed) + len(host_skipped_proved)
    if carry < a.min_proved:
        bad.append("%d gate(s) carry a prover (%d verified here, %d with a "
                   "prover stubbed on this host, %d host-skipped gates) and at "
                   "least %d were expected"
                   % (carry, proved, len(stubbed), len(host_skipped_proved),
                      a.min_proved))
    if a.max_unfloored is not None and len(unfloored) > a.max_unfloored:
        bad.append("%d gate(s) are `%s` and the ratchet is %d: %s"
                   % (len(unfloored), UNFLOORED, a.max_unfloored,
                      ", ".join(sorted(unfloored)[:8])))
    if a.max_unproved is not None and len(unproved) > a.max_unproved:
        bad.append("%d gate(s) are `%s` and the ratchet is %d: %s"
                   % (len(unproved), UNPROVED, a.max_unproved,
                      ", ".join(sorted(unproved)[:8])))

    for b in bad:
        print("gate-contract: " + b)
    if bad:
        print("gate-contract: %d violation(s) over %d declared gate(s)"
              % (len(bad), len(rows)))
        return 1
    print("gate-contract: OK -- %d gate(s) declared, %d floored (%d %s), "
          "%d proved (%d %s), %d known-positive cell(s) all claimed"
          % (len(rows), floored, len(unfloored), UNFLOORED,
             proved, len(unproved), UNPROVED, len(claimed_provers)))
    if host_skipped or stubbed:
        print("gate-contract: %d gate(s) are mcc_skip_test stubs on this host "
              "and %d carry a prover that is, so this host proves nothing about "
              "them and does not pretend to. The %d of them that declare a "
              "prover still count toward --min-proved, which is a property of "
              "the manifest and not of the host, so the pin holds identically "
              "on all three. A `min:` floor is verified against this build's "
              "command line, so a stub has none to verify and is not counted "
              "as floored here"
              % (len(host_skipped), len(stubbed), len(host_skipped_proved)))
        for name in sorted(host_skipped)[:12]:
            print("gate-contract:   host-skipped: %s%s"
                  % (name, " (declares a prover; counted)"
                     if name in host_skipped_proved else ""))
        for name in sorted(stubbed)[:12]:
            print("gate-contract:   stubbed prover: %s" % name)
    if a.max_unfloored is not None and len(unfloored) < a.max_unfloored:
        print("gate-contract: the %s ratchet is %d and the count is %d; lower it"
              % (UNFLOORED, a.max_unfloored, len(unfloored)))
    if a.max_unproved is not None and len(unproved) < a.max_unproved:
        print("gate-contract: the %s ratchet is %d and the count is %d; lower it"
              % (UNPROVED, a.max_unproved, len(unproved)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
