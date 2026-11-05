#!/usr/bin/env python3
"""Checks of the Python module jetlibpy (review findings 2.3, 4.1-4.3).

Two steps, because the container has no ROOT reader for Python:
  1. inside the environment jetlibpy was built in (the DICE container):
       python3 checks/check_bindings.py --build build --workdir <dir>
     runs the checks and writes filtered trees plus their expected jet counts;
  2. on the host with uproot (.venv):
       .venv/bin/python checks/check_bindings.py --verify --workdir <dir>
     compares the written trees with the expected counts.
"""
import argparse
import json
import gc
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
results = []


def check(name, ok, detail=""):
    results.append((name, bool(ok)))
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f": {detail}" if detail else ""))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--build", default=os.path.join(HERE, "..", "build"))
    p.add_argument("--config", default=os.path.join(HERE, "..", "config", "ttbar.cfg"))
    p.add_argument("--workdir", required=True)
    p.add_argument("--verify", action="store_true", help="step 2: read the filtered trees with uproot")
    args = p.parse_args()
    if args.verify:
        return verify(args.workdir)
    sys.path.insert(0, os.path.abspath(args.build))
    import jetlibpy as j
    os.makedirs(args.workdir, exist_ok=True)
    os.chdir(args.workdir)

    gen = j.EventGenerator(os.path.abspath(args.config))
    gen.setSeed(11)
    gen.initialize()
    gen.generateEvents(20)
    jets = [jet for ev in gen.getJetCollection() for jet in ev]

    # 2.3: invalid filters are rejected when set
    ana = j.JetAnalyzer("filter.root")
    bad = ["Pt>1000", "pt=1000", "pt>1000abc", "pt>30&&", "(pt>30", "pt>30)", "pt>", "abs(eta<2", "||pt>1"]
    rejected = []
    for f in bad:
        try:
            ana.setFilter(f)
        except ValueError:
            rejected.append(f)
    check("invalid filters raise ValueError", rejected == bad, f"accepted: {sorted(set(bad) - set(rejected))}")

    # 2.3: filter results agree with a direct evaluation (counts checked by --verify)
    cases = {
        "pt > 40 && abs(eta) < 2": lambda x: x.pt() > 40 and abs(x.eta()) < 2,
        "(eta > 1 || eta < -1) && e >= 100": lambda x: (x.eta() > 1 or x.eta() < -1) and x.e() >= 100,
        "phi < 0": lambda x: x.phi() < 0,
    }
    expected = {}
    for k, (expr, fn) in enumerate(cases.items()):
        a = j.JetAnalyzer(f"filter_case{k}.root")
        a.setSelection(j.JetSelection())   # passes everything: only the filter acts
        a.setFilter(expr)
        a.begin()
        a.fill(jets, 0)
        a.finish()
        del a
        expected[f"filter_case{k}.root"] = {"filter": expr, "n_jets": sum(fn(x) for x in jets), "n_all": len(jets)}
    with open("bindings_expected.json", "w") as f:
        json.dump(expected, f, indent=1)
    check("filter cases written", all(0 < e["n_jets"] < e["n_all"] for e in expected.values()),
          ", ".join(f"{e['filter']!r}: {e['n_jets']}/{e['n_all']}" for e in expected.values()))

    # 4.1: constituents stay valid after the generator is deleted; fastjet::Error is readable
    n_before = [len(x.constituents()) for x in jets]
    del gen
    gc.collect()
    n_after = [len(x.constituents()) for x in jets]
    check("constituents valid after del EventGenerator", n_before == n_after and sum(n_after) > 0,
          f"{len(jets)} jets, {sum(n_after)} constituents")
    try:
        j.PseudoJet().constituents()
        msg = ""
    except RuntimeError as e:
        msg = str(e)
    check("fastjet::Error -> RuntimeError with message", msg.startswith("FastJet:"), msg)

    # 4.2: JetAnalysis is reusable and usable from Python
    def p4(pt, eta, phi):
        return j.PseudoJet(pt * math.cos(phi), pt * math.sin(phi), pt * math.sinh(eta), pt * math.cosh(eta))
    an = j.JetAnalysis()
    an.setParticles([p4(50, 0.1, 1.0), p4(30, 0.2, 1.1), p4(40, -1.5, -2.0)])
    an.clusterJets(0.4, 20.0)
    first = sorted(round(x.pt()) for x in an.getJets(0))
    an.setParticles([p4(25, 2.0, 0.5)])
    an.clusterJets(0.4, 20.0)
    second = [round(x.pt()) for x in an.getJets(0)]
    check("JetAnalysis reuse does not accumulate", first == [40, 80] and second == [25], f"{first}, {second}")

    # 4.3: phi convention [-pi, pi), as in the tree
    phis = [x.phi() for x in jets] + [c.phi() for x in jets for c in x.constituents()]
    q = j.PseudoJet(-1, -1, 0, 2)
    check("phi() in [-pi, pi)", all(-math.pi <= v < math.pi for v in phis) and q.phi() < 0
          and abs(q.phi_02pi() - q.phi() - 2 * math.pi) < 1e-12, f"phi(-1,-1) = {q.phi():.3f}")

    nfail = sum(not ok for _, ok in results)
    print(f"\n{len(results) - nfail}/{len(results)} checks passed")
    return 1 if nfail else 0


def verify(workdir):
    import uproot
    with open(os.path.join(workdir, "bindings_expected.json")) as f:
        expected = json.load(f)
    for name, e in expected.items():
        n = uproot.open(os.path.join(workdir, name))["JetTree"].num_entries
        check(f"filter {e['filter']!r} writes the expected jets", n == e["n_jets"], f"{n} written, {e['n_jets']} expected")
    nfail = sum(not ok for _, ok in results)
    print(f"\n{len(results) - nfail}/{len(results)} checks passed")
    return 1 if nfail else 0


if __name__ == "__main__":
    sys.exit(main())
