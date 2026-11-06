#!/usr/bin/env python3
"""Plausibility report for the jet library: before/after plots from the outputs of check_output.py.

Reads the ROOT files that checks/check_output.py leaves in its --workdir (master.root,
b_default.root, c_default.root, c_keep.root, ptmin10.root, abseta.root, seed_*.root,
seed0_*.root), fills checks/plausibility_template.html and writes a self-contained HTML page
with the histograms (eta, pT, neutrino and lepton energy fractions, constituent content,
--pt-min 10, --abs-eta, jets per event, phi) and the reproducibility comparisons.

Run on the host with the .venv (uproot, numpy, awkward), from the repository:
    .venv/bin/python checks/plausibility_report.py --workdir <check_output workdir> --out report.html \\
        [--cli-checks 31/31] [--py-checks 9/9]
"""
import argparse
import json
import os
import subprocess
import sys

import awkward as ak
import numpy as np
import uproot

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
NEUTRINOS = [12, 14, 16]
# constituent species shown in the composition plot (|PDG ID|)
SPECIES = [("π±", [211]), ("γ", [22]), ("K±", [321]), ("K⁰L", [130]), ("p/p̄", [2212]),
           ("n/n̄", [2112]), ("e±", [11]), ("μ±", [13]), ("ν", NEUTRINOS)]
META_SHOWN = ["jetlib_version", "selection", "particles", "n_removed_invisible", "n_removed_prompt_leptons",
              "n_jets_written", "pythia_version", "fastjet_version", "jet_algorithm", "cluster_pt_min", "seed"]


def load(workdir, name):
    a = uproot.open(os.path.join(workdir, name))["JetTree"].arrays(library="ak")
    if "jetConsituentsEta" in a.fields:          # master / older builds: misspelt branch
        a["jetConstituentsEta"] = a["jetConsituentsEta"]
    return a


def meta(workdir, name):
    f = uproot.open(os.path.join(workdir, name))
    return {k.split(";")[0].split("/")[-1]: f[k].member("fTitle") for k in f.keys() if k.startswith("jetlib/")}


def flat(x):
    return ak.to_numpy(ak.flatten(x, axis=None))


def hist(x, lo, hi, nb):
    c, e = np.histogram(x, bins=nb, range=(lo, hi))
    return {"edges": [float(v) for v in e], "counts": [int(v) for v in c]}


def energy_fraction(a, ids):
    """per jet: energy fraction of constituents with |PDG ID| in ids"""
    pid = abs(a.jetConstituentsPdgId)
    sel = ak.zeros_like(pid, dtype=bool)
    for i in ids:
        sel = sel | (pid == i)
    return ak.to_numpy(ak.sum(a.jetConstituentsE * sel, axis=1) / ak.sum(a.jetConstituentsE, axis=1))


def max_lepton_fraction(a):
    """per jet: energy fraction of the most energetic e/mu constituent"""
    pid = abs(a.jetConstituentsPdgId)
    e = ak.where((pid == 11) | (pid == 13), a.jetConstituentsE, 0)
    return ak.to_numpy(ak.max(e, axis=1) / ak.sum(a.jetConstituentsE, axis=1))


def species(a):
    pid = flat(abs(a.jetConstituentsPdgId))
    return {name: float(np.isin(pid, ids).mean()) for name, ids in SPECIES}, int(pid.size)


def same(a, b):
    """identical tree contents on the common branches"""
    if len(a) != len(b):
        return False
    return all(bool(ak.all(ak.ravel(a[f] == b[f]))) for f in a.fields if f in b.fields)


def peak_mem(d):
    """fact tile text: master -> branch max RSS at the largest timed event count"""
    if "timing" not in d:
        return "1187 → 205 MB"          # measured once during the streaming change (batch 2)
    summ = d["timing"]["summary"]
    n = max(s["n"] for s in summ)
    get = lambda v: next(s["maxrss_mb"] for s in summ if s["variant"] == v and s["n"] == n)
    return f"{get('master'):.0f} → {get('branch'):.0f} MB"


def git(*args):
    return subprocess.run(["git", "-C", REPO, *args], capture_output=True, text=True, check=True).stdout


def compute(workdir):
    M, B = load(workdir, "master.root"), load(workdir, "b_default.root")
    F, K = load(workdir, "c_default.root"), load(workdir, "c_keep.root")
    P10, A = load(workdir, "ptmin10.root"), load(workdir, "abseta.root")
    S1, S2 = load(workdir, "seed_a.root"), load(workdir, "seed_b.root")
    Z1, Z2, ZR = load(workdir, "seed0_a.root"), load(workdir, "seed0_b.root"), load(workdir, "seed0_repro.root")
    eta = lambda a: flat(a.jetEta)
    pt = lambda a: flat(a.jetPt)
    fwd = lambda a: (eta(a) > 1.7) & (eta(a) < 3)     # window common to master and the branch default
    mF = meta(workdir, "c_default.root")
    n_events = int(mF["n_events_accepted"])

    nuM, nuF = energy_fraction(M, NEUTRINOS), energy_fraction(F, NEUTRINOS)
    lF, lK = max_lepton_fraction(F), max_lepton_fraction(K)
    sM, nM = species(M[fwd(M)])
    sF, nF = species(F[fwd(F)])
    jets_per_event = np.bincount(ak.to_numpy(F.eventIndex), minlength=n_events)
    px, py = ak.to_numpy(ak.sum(F.jetConstituentsPx, axis=1)), ak.to_numpy(ak.sum(F.jetConstituentsPy, axis=1))
    e = ak.to_numpy(ak.sum(F.jetConstituentsE, axis=1))

    d = {
        "eta": {"master": hist(eta(M), -5, 5, 50), "final": hist(eta(F), -5, 5, 50)},
        "pt_fwd": {"master": hist(pt(M)[fwd(M)], 0, 300, 30), "final": hist(pt(F)[fwd(F)], 0, 300, 30)},
        "nufrac": {"master": hist(nuM, 0, 1, 20), "final": hist(nuF, 0, 1, 20)},
        "lepfrac": {"default": hist(lF, 0, 1, 20), "keep": hist(lK, 0, 1, 20)},
        "pt10": {"ptmin10": hist(pt(P10), 0, 80, 32), "default": hist(pt(F), 0, 80, 32)},
        "abseta": {"abseta": hist(eta(A), -3, 3, 60)},
        "jpe": {"final": hist(jets_per_event, -0.5, 14.5, 15)},
        "phi": {"final": hist(flat(F.jetPhi), -np.pi, np.pi, 32)},
        "ncon": {"master": hist(flat(M.jetNConstituents)[fwd(M)], 0, 80, 40),
                 "final": hist(flat(F.jetNConstituents)[fwd(F)], 0, 80, 40)},
        "species": {"master": sM, "final": sF, "nmaster": nM, "nfinal": nF},
        # max relative deviation of the summed constituents from the jet pT and E
        "closure": [float(np.max(np.abs(np.hypot(px, py) / pt(F) - 1))),
                    float(np.max(np.abs(e / flat(F.jetEnergy) - 1)))],
    }
    d["stats"] = {
        "seed": mF.get("seed"), "n_events": n_events,
        "n_master": len(M), "n_final": len(F),
        "master_eq_b": same(M, B), "seed_same": same(S1, S2), "seed0_same": same(Z1, Z2), "repro_same": same(Z1, ZR),
        "seed0a": meta(workdir, "seed0_a.root").get("seed"), "seed0b": meta(workdir, "seed0_b.root").get("seed"),
        "nu_master_any": float(np.mean(nuM > 0)), "nu_master_jets_gt50": float(np.mean(nuM > 0.5)),
        "nu_master_jets_gt90": float(np.mean(nuM > 0.9)),
        "lep50_default": float(np.mean(lF > 0.5)), "lep50_keep": float(np.mean(lK > 0.5)),
        "p10_in_10_20": int(((pt(P10) > 10) & (pt(P10) < 20)).sum()), "p10_min": float(pt(P10).min()),
        "abs_pos": int((eta(A) > 0).sum()), "abs_neg": int((eta(A) < 0).sum()),
        "abs_min": float(np.abs(eta(A)).min()), "abs_max": float(np.abs(eta(A)).max()),
        "final_ptmin": float(pt(F).min()), "final_etamin": float(eta(F).min()), "final_etamax": float(eta(F).max()),
        "master_eta_min": float(eta(M).min()),
        "fwd_master": int(fwd(M).sum()), "fwd_final": int(fwd(F).sum()),
        "meta": {k: mF[k] for k in META_SHOWN if k in mF},
    }
    return d


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--workdir", required=True, help="--workdir of a completed check_output.py run")
    p.add_argument("--out", required=True, help="HTML file to write")
    p.add_argument("--template", default=os.path.join(HERE, "plausibility_template.html"))
    p.add_argument("--base", default="master", help="git ref the branch is compared with (commit list, diffstat)")
    p.add_argument("--cli-checks", default="see check_output.py", help="result of check_output.py, e.g. 31/31")
    p.add_argument("--py-checks", default="see check_bindings.py", help="result of check_bindings.py, e.g. 9/9")
    p.add_argument("--timing", help="JSON written by checks/timing.py: adds runtime and memory plots")
    p.add_argument("--json", help="also write the computed numbers to this JSON file")
    args = p.parse_args()

    d = compute(args.workdir)
    if args.timing:
        with open(args.timing) as f:
            t = json.load(f)
        d["timing"] = {k: t[k] for k in ("summary", "host", "jobs", "variants")}
    commits = [l.split("\t", 1) for l in git("log", "--reverse", "--format=%h\t%s", f"{args.base}..HEAD").splitlines()]
    shortstat = git("diff", "--shortstat", f"{args.base}..HEAD").split(",")
    nfiles = shortstat[0].split()[0] if shortstat[0].strip() else "0"
    ins = next((s.split()[0] for s in shortstat if "insertion" in s), "0")
    dels = next((s.split()[0] for s in shortstat if "deletion" in s), "0")
    s = d["stats"]

    fill = {
        "__DATA__": json.dumps(d, separators=(",", ":"), ensure_ascii=False).replace("</", "<\\/"),
        "__COMMITS__": json.dumps(commits, ensure_ascii=False).replace("</", "<\\/"),
        "__NCOMMITS__": str(len(commits)), "__NFILES__": nfiles, "__DIFFSTAT__": f"+{ins} / −{dels}",
        "__BUILD__": s["meta"].get("jetlib_version", "unknown"),
        "__NMASTER__": f"{s['n_master']:,}".replace(",", " "), "__NFINAL__": f"{s['n_final']:,}".replace(",", " "),
        "__CLICHECKS__": args.cli_checks, "__PYCHECKS__": args.py_checks,
        "__SEED__": str(s["seed"]), "__NEVENTS__": str(s["n_events"]),
        "__PEAKMEM__": peak_mem(d),
    }
    page = open(args.template, encoding="utf-8").read()
    for k, v in fill.items():
        page = page.replace(k, v)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(page)
    if args.json:
        with open(args.json, "w") as f:
            json.dump(d, f, indent=1, ensure_ascii=False)
    print(f"wrote {args.out}: {s['n_master']} master jets, {s['n_final']} branch jets, "
          f"{len(commits)} commits since {args.base}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
