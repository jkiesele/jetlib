#!/usr/bin/env python3
"""Acceptance checks for the jetlib selection, seed and particle-content changes.

Runs jetlib binaries (inside the DICE container by default) and compares the
ROOT outputs with uproot on the host. Exit code 0 means all checks passed.

Binaries:
  --final-bin   build of the branch under test (all changes)
  --b-bin       build of commit B (seed option), before the particle-content
                change (C) and the new default eta range (D); used for the
                "defaults identical to master" check, because C and D
                intentionally change the default output
  --master-bin  unchanged master build

Example (from the repository root, with the venv that has uproot and numpy):
  .venv/bin/python checks/check_output.py \\
      --final-bin build/jetlib \\
      --b-bin /ceph/.../builds/commitB/build/jetlib \\
      --master-bin /ceph/.../builds/master/build/jetlib \\
      --workdir /ceph/jkiesele/temp_data/jetlib/checks
"""
import argparse
import os
import shlex
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import uproot

DEFAULT_WRAP = "apptainer exec -B /ceph -B /work /ceph/jkiesele/containers/dice_latest.sif"
SEED = 12345


def parse_args():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--final-bin", required=True)
    p.add_argument("--b-bin", required=True)
    p.add_argument("--master-bin", required=True)
    p.add_argument("--ref-bin", help="optional earlier build of this branch; its default output (same seed) "
                                     "must be identical to --final-bin (regression check for refactors)")
    p.add_argument("--config", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "config", "ttbar.cfg"))
    p.add_argument("--workdir", required=True, help="scratch directory for outputs")
    p.add_argument("--nevents", type=int, default=2000)
    p.add_argument("--jobs", type=int, default=8, help="parallel jetlib runs (<= 16)")
    p.add_argument("--wrap", default=DEFAULT_WRAP, help="command prefix to run jetlib ('' to run directly)")
    return p.parse_args()


class Runner:
    def __init__(self, args):
        self.wrap = shlex.split(args.wrap)
        self.workdir = os.path.abspath(args.workdir)
        self.config = os.path.abspath(args.config)
        self.n = args.nevents
        os.makedirs(self.workdir, exist_ok=True)

    def run(self, binary, extra, out=None, config=None, n=None):
        """Run jetlib; returns (returncode, combined output, output path)."""
        cmd = [os.path.abspath(binary), "-c", config or self.config, "-n", str(n if n is not None else self.n)]
        path = None
        if out:
            path = os.path.join(self.workdir, out)
            cmd += ["-f", path]
        cmd += extra
        r = subprocess.run(self.wrap + cmd, cwd=self.workdir, capture_output=True, text=True)
        log = r.stdout + r.stderr
        if out:
            with open(path + ".log", "w") as f:
                f.write(" ".join(cmd) + "\n" + log)
        return r.returncode, log, path


def read(path):
    return uproot.open(path)["JetTree"].arrays(library="np")


# branches renamed since older builds: old name -> new name
RENAMED = {"jetConsituentsEta": "jetConstituentsEta"}


def same_contents(p1, p2, subset_of=None):
    """True if both trees have identical contents. With subset_of=p2, only the branches
    of p2 are compared and p1 may have additional ones (for older reference builds);
    branches of p2 that were renamed since (RENAMED) are compared under their new name."""
    a, b = read(p1), read(p2)
    if subset_of is not None:
        b = {(RENAMED.get(k, k) if RENAMED.get(k) in a else k): v for k, v in b.items()}
    if subset_of is None and sorted(a) != sorted(b):
        return False
    if subset_of is not None and not set(b) <= set(a):
        return False
    for k in b:
        if len(a[k]) != len(b[k]):
            return False
        if not all(np.array_equal(x, y) for x, y in zip(a[k], b[k])):
            return False
    return True


def flat(a, key):
    return np.concatenate(a[key]) if len(a[key]) else np.array([])


def lepton_dominated_fraction(a):
    """Fraction of jets in which one e/mu carries more than half the jet energy."""
    n = len(a["jetConstituentsE"])
    if n == 0:
        return 0.0
    cnt = 0
    for pid, e in zip(a["jetConstituentsPdgId"], a["jetConstituentsE"]):
        lep = (np.abs(pid) == 11) | (np.abs(pid) == 13)
        if lep.any() and e[lep].max() > 0.5 * e.sum():
            cnt += 1
    return cnt / n


results = []


def check(name, ok, detail):
    results.append((name, bool(ok), detail))
    print(f"[{'PASS' if ok else 'FAIL'}] {name}: {detail}", flush=True)


def main():
    args = parse_args()
    if args.jobs > 16:
        sys.exit("use at most 16 parallel jobs")
    R = Runner(args)

    # master reference: unchanged master, seed set via a temporary config copy
    seeded_cfg = os.path.join(R.workdir, f"ttbar_seed{SEED}.cfg")
    with open(R.config) as f:
        lines = [f"Random:seed = {SEED}" if l.strip().startswith("Random:seed") else l.rstrip("\n") for l in f]
    with open(seeded_cfg, "w") as f:
        f.write("\n".join(lines) + "\n")

    s = ["--seed", str(SEED)]
    jobs = {
        "master":    (args.master_bin, [], seeded_cfg),
        "b_default": (args.b_bin, s, None),
        "seed_a":    (args.final_bin, s, None),
        "seed_b":    (args.final_bin, s, None),
        "abseta":    (args.final_bin, s + ["--abs-eta", "--eta-min", "1.7", "--eta-max", "2.6", "--pt-min", "30"], None),
        "ptmin10":   (args.final_bin, s + ["--pt-min", "10"], None),
        "c_default": (args.final_bin, s, None),
        "c_keep":    (args.final_bin, s + ["--keep-prompt-leptons"], None),
    }
    if args.ref_bin:
        jobs["ref_default"] = (args.ref_bin, s, None)
    t0 = time.time()
    with ThreadPoolExecutor(args.jobs) as ex:
        futs = {k: ex.submit(R.run, b, e, f"{k}.root", c) for k, (b, e, c) in jobs.items()}
        out = {k: f.result() for k, f in futs.items()}
    # --seed 0 runs must start in different seconds (Pythia's time seed has 1 s resolution)
    out["seed0_a"] = R.run(args.final_bin, ["--seed", "0"], "seed0_a.root")
    time.sleep(1.5)
    out["seed0_b"] = R.run(args.final_bin, ["--seed", "0"], "seed0_b.root")
    print(f"runs finished in {time.time() - t0:.0f} s, outputs in {R.workdir}\n")

    for k, (rc, log, _) in out.items():
        if rc != 0:
            check(f"run {k}", False, f"exit {rc}: {log[-300:]}")

    p = {k: v[2] for k, v in out.items()}

    # 1. defaults identical to master (build before change C)
    m = read(p["master"])
    check("defaults == master (commit B, same seed)", same_contents(p["master"], p["b_default"]),
          f"{len(m['jetPt'])} jets in master")

    # 2. reproducibility
    check("same --seed twice identical", same_contents(p["seed_a"], p["seed_b"]), f"--seed {SEED}")
    check("--seed 0 twice differs", not same_contents(p["seed0_a"], p["seed0_b"]), "runs started >1 s apart")

    # 3. |eta| window
    a = read(p["abseta"])
    pt, eta = flat(a, "jetPt"), flat(a, "jetEta")
    inside = np.all((pt > 30) & (np.abs(eta) > 1.7) & (np.abs(eta) < 2.6))
    npos, nneg = int(np.sum(eta > 0)), int(np.sum(eta < 0))
    n = npos + nneg
    balanced = n > 0 and abs(npos - nneg) < 3 * np.sqrt(n)  # 3 sigma binomial
    check("--abs-eta window: all jets inside", inside and n > 0,
          f"{n} jets, pt min {pt.min():.2f}, |eta| in [{np.abs(eta).min():.3f}, {np.abs(eta).max():.3f}]")
    check("--abs-eta window: both eta signs balanced", balanced,
          f"eta>0: {npos}, eta<0: {nneg}, |diff| = {abs(npos - nneg)} < 3*sqrt(n) = {3 * np.sqrt(n):.1f}")

    # 4. pt-min below the old clustering threshold
    a = read(p["ptmin10"])
    pt = flat(a, "jetPt")
    low = int(np.sum((pt > 10) & (pt < 20)))
    check("--pt-min 10 gives jets with 10 < pt < 20", low > 0 and np.all(pt > 10),
          f"{low} of {len(pt)} jets in (10, 20) GeV, pt min {pt.min():.2f}")

    # default selection of the final build: pt > 30 && -3 < eta < 3
    a = read(p["c_default"])
    pt, eta = flat(a, "jetPt"), flat(a, "jetEta")
    npos, nneg = int(np.sum(eta > 0)), int(np.sum(eta < 0))
    check("final build default selection pt > 30 && -3 < eta < 3",
          len(pt) > 0 and np.all((pt > 30) & (eta > -3) & (eta < 3)) and abs(npos - nneg) < 3 * np.sqrt(len(pt)),
          f"{len(pt)} jets, pt min {pt.min():.2f}, eta in [{eta.min():.3f}, {eta.max():.3f}], eta>0: {npos}, eta<0: {nneg}")

    if args.ref_bin:
        check("final == reference build (default args, same seed, reference branches)",
              same_contents(p["c_default"], p["ref_default"], subset_of=True), f"reference {args.ref_bin}")

    # bookkeeping branches and provenance (review 1.8, 5.3)
    t = uproot.open(p["c_default"])
    a = read(p["c_default"])
    ev, ji = a["eventIndex"], a["jetIndex"]
    pt = np.array([x[0] for x in a["jetPt"]])
    ranks_ok = True
    for e in np.unique(ev):
        m = ev == e
        r = ji[m]
        ranks_ok &= sorted(r) == list(range(m.sum())) and bool(np.all(np.diff(pt[m][np.argsort(r)]) <= 0))
    check("eventIndex/jetIndex consistent", bool(np.all(np.diff(ev) >= 0)) and ev.max() < R.n and ranks_ok,
          f"{len(np.unique(ev))} events with jets, eventIndex max {ev.max()}, jetIndex = pT rank 0..n-1 per event")
    meta = {k: t["jetlib/" + k].member("fTitle") for k in t["jetlib"].keys(cycle=False)}
    prov_ok = (meta.get("seed") == str(SEED) and meta.get("n_jets_written") == str(len(pt))
               and meta.get("n_events_accepted") == str(R.n) and "Top:gg2ttbar" in meta.get("config", "")
               and meta.get("jetlib_version", "unknown") != "unknown")
    check("provenance stored", prov_ok,
          f"version {meta.get('jetlib_version')}, seed {meta.get('seed')}, n_jets_written {meta.get('n_jets_written')}, "
          f"{len(meta)} entries")

    # 5. particle content (change C)
    k = read(p["c_keep"])
    nu = lambda arr: int(sum(np.isin(np.abs(x), [12, 14, 16]).sum() for x in arr["jetConstituentsPdgId"]))
    check("no neutrinos in output", nu(a) == 0 and nu(k) == 0,
          f"neutrino constituents: default {nu(a)}, keep-prompt-leptons {nu(k)}")
    fd, fk = lepton_dominated_fraction(a), lepton_dominated_fraction(k)
    check("prompt leptons removed by default, kept with --keep-prompt-leptons", fd < 0.01 and fk > 0.02 and fk > 5 * fd,
          f"jets with one e/mu carrying >50% of E: default {100 * fd:.2f}%, keep {100 * fk:.2f}%")

    # 6. invalid arguments
    bad = [["--pt-min", "-1"], ["--pt-min", "50", "--pt-max", "40"], ["--eta-min", "2", "--eta-max", "1"],
           ["--abs-eta", "--eta-max", "-1"], ["--eta-min", "abc"], ["--pt-max"],
           ["--seed", "-1"], ["--seed", "900000001"], ["--seed", "1.5"], ["--seed"],
           ["-n", "-1"], ["-n", "abc"], ["-n", "0"], ["foo.rootx"]]
    for b in bad:
        rc, log, _ = R.run(args.final_bin, b, n=1)
        msg = next((l for l in log.splitlines() if l.startswith("Error")), "")
        check(f"invalid args {' '.join(b)}", rc != 0 and msg, f"exit {rc}: {msg}")

    # 7. failure modes (review 1.4, 2.1, 6.2)
    typo_cfg = os.path.join(R.workdir, "typo.cfg")
    with open(R.config) as f:
        typo = f.read().replace("Top:gg2ttbar", "Top:gg2ttbarr")
    with open(typo_cfg, "w") as f:
        f.write(typo)
    rc, log, _ = R.run(args.final_bin, s, config=typo_cfg)
    msg = next((l for l in log.splitlines() if l.startswith("Error")), "")
    check("config typo fails", rc != 0 and "did not accept" in msg, f"exit {rc}: {msg}")
    t = time.time()
    rc, log, _ = R.run(args.final_bin, s + ["-f", "/nonexistent/dir/x.root"])
    dt = time.time() - t
    msg = next((l for l in log.splitlines() if l.startswith("Error")), "")
    check("unwritable output fails before generation", rc != 0 and msg and dt < 30,
          f"exit {rc} after {dt:.0f} s (n = {R.n}): {msg}")
    rc, log, _ = R.run(args.final_bin, ["--help"], n=1)
    check("--help exits 0", rc == 0 and "Usage" in log, f"exit {rc}")

    nfail = sum(not ok for _, ok, _ in results)
    print(f"\n{len(results) - nfail}/{len(results)} checks passed")
    return 1 if nfail else 0


if __name__ == "__main__":
    sys.exit(main())
