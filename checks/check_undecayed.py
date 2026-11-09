#!/usr/bin/env python3
"""Checks for the removal of undecayed unstable particles (failed Pythia decays).

1. Regression: default ttbar config, same seed, new build vs a reference build without the
   removal. Failed decays are rare (1 in 600k ttbar events in the first libraries), so the
   JetTree must be identical.
2. Stress test: ttbar with "HadronLevel:Decay = off", so that every unstable hadron stays in the
   final state. The reference build writes unstable constituents; the new build must write only
   gamma, e, mu, pi+-, K+-, K0L, p, n (and antiparticles), count the removed particles, and every
   jet must still equal the sum of its constituents.

Example (from the repository root):
  .venv/bin/python checks/check_undecayed.py --new-bin build/jetlib \\
      --ref-bin /ceph/.../ref_build/jetlib --workdir /ceph/jkiesele/temp_data/jetlib/undecayed
Exit code 0 means all checks passed.
"""
import argparse
import os
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import uproot

WRAP = "apptainer exec -B /ceph -B /work /ceph/jkiesele/containers/dice_latest.sif"
STABLE = {11, 13, 22, 211, 321, 130, 2212, 2112}
CFG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "config", "ttbar.cfg")


def run(binary, cfg, out, n, seed, wrap):
    cmd = shlex.split(wrap) + [os.path.abspath(binary), "-c", cfg, "-f", out, "-n", str(n), "--seed", str(seed),
                               "--pt-min", "20", "--pt-max", "200"]
    with open(out + ".log", "w") as log:
        rc = subprocess.call(cmd, stdout=log, stderr=subprocess.STDOUT, cwd=os.path.dirname(out))
    if rc != 0:
        sys.exit(f"jetlib failed ({rc}), see {out}.log")
    return out


def arrays(p):
    return uproot.open(p)["JetTree"].arrays(library="np")


def identical(a, b):
    return a.keys() == b.keys() and all(len(a[k]) == len(b[k]) and all(np.array_equal(x, y) for x, y in zip(a[k], b[k])) for k in a)


def unstable(a):
    pdg = np.abs(np.concatenate(a["jetConstituentsPdgId"]))
    return int((~np.isin(pdg, list(STABLE))).sum()), len(pdg)


def sum_ok(a):
    pt = np.array([np.hypot(x.sum(), y.sum()) for x, y in zip(a["jetConstituentsPx"], a["jetConstituentsPy"])])
    e = np.array([x.sum() for x in a["jetConstituentsE"]])
    jpt = np.array([x[0] for x in a["jetPt"]])
    je = np.array([x[0] for x in a["jetEnergy"]])
    return float(max(np.max(np.abs(pt / jpt - 1)), np.max(np.abs(e / je - 1))))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--new-bin", required=True)
    p.add_argument("--ref-bin", required=True)
    p.add_argument("--workdir", required=True)
    p.add_argument("--wrap", default=WRAP)
    p.add_argument("--nevents", type=int, default=2000, help="events of the regression test")
    args = p.parse_args()
    w = os.path.abspath(args.workdir)
    os.makedirs(w, exist_ok=True)
    nodecay = os.path.join(w, "ttbar_nodecay.cfg")
    with open(CFG) as f, open(nodecay, "w") as g:
        g.write(f.read().rstrip("\n") + "\nHadronLevel:Decay = off\n")
    cfg = os.path.abspath(CFG)
    jobs = [(args.ref_bin, cfg, f"{w}/reg_ref.root", args.nevents, 4242), (args.new_bin, cfg, f"{w}/reg_new.root", args.nevents, 4242),
            (args.ref_bin, nodecay, f"{w}/nodecay_ref.root", 300, 4243), (args.new_bin, nodecay, f"{w}/nodecay_new.root", 300, 4243)]
    with ThreadPoolExecutor(4) as ex:
        list(ex.map(lambda j: run(*j, args.wrap), jobs))
    ok = True

    def check(name, cond, detail):
        nonlocal ok
        ok &= bool(cond)
        print(f"[{'PASS' if cond else 'FAIL'}] {name}: {detail}")

    a, b = arrays(f"{w}/reg_ref.root"), arrays(f"{w}/reg_new.root")
    check("default ttbar identical to the reference build", identical(a, b), f"{len(b['jetPt'])} jets, seed 4242")
    ra, rb = arrays(f"{w}/nodecay_ref.root"), arrays(f"{w}/nodecay_new.root")
    ua, na = unstable(ra)
    ub, nb = unstable(rb)
    meta = uproot.open(f"{w}/nodecay_new.root")["jetlib/n_removed_undecayed"].member("fTitle")
    nrem = int(meta.split()[0])
    check("decays off: reference build has unstable constituents (test is sensitive)", ua > 0, f"{ua}/{na}")
    check("decays off: new build has none", ub == 0 and nb > 0, f"{ub}/{nb}")
    check("decays off: removed particles counted", nrem > 0, meta[:80] + ("..." if len(meta) > 80 else ""))
    dev = sum_ok(rb)
    check("decays off: jet = sum of constituents", dev < 1e-9, f"max rel. dev. {dev:.1e}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
