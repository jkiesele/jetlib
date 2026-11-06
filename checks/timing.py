#!/usr/bin/env python3
"""Runtime and memory of the master build vs the current build, on the same events.

Each variant is run for every event count and repetition with the same seed per repetition
(master gets the seed through a temporary config copy, as in check_output.py). Wall time,
CPU time (user + sys) and peak memory (max RSS) of the jetlib process are measured inside the
container by a small Python wrapper (resource.getrusage of the child). The runs of one
(event count, repetition) use identical seeds, and all jobs are interleaved, so load changes
on a shared machine affect the variants alike.

Variants:
  master              unchanged master build, its default selection (eta > 1.7, pt > 30)
  branch              current build, default arguments (-3 < eta < 3, pt > 30)
  branch_master_sel   current build with --eta-min 1.7 --eta-max 100 (about master's selection,
                      without neutrinos and prompt leptons), separates the selection effect

Run on the host (uses the .venv for reading the outputs):
    .venv/bin/python checks/timing.py --master-bin <master>/build/jetlib --final-bin build/jetlib \\
        --workdir <scratch dir> --out timing.json [--sizes 1000 4000] [--reps 3] [--jobs 6]
Keep --jobs moderate on a shared machine: with 12 parallel runs, wall times showed a 6% spread
between variants that disappeared with 6 (CPU times agreed in both cases).
The JSON can be passed to plausibility_report.py --timing.
"""
import argparse
import concurrent.futures as cf
import json
import os
import shlex
import statistics
import subprocess
import sys

import uproot

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_WRAP = "apptainer exec -B /ceph -B /work /ceph/jkiesele/containers/dice_latest.sif"
# runs argv[1:] with output to files, prints wall/CPU/maxRSS of that child as JSON
MEASURE = r"""
import json, resource, subprocess, sys, time
log = open(sys.argv[1], "w")
t = time.perf_counter()
rc = subprocess.run(sys.argv[2:], stdout=log, stderr=subprocess.STDOUT).returncode
wall = time.perf_counter() - t
ru = resource.getrusage(resource.RUSAGE_CHILDREN)
print(json.dumps({"rc": rc, "wall": wall, "user": ru.ru_utime, "sys": ru.ru_stime, "maxrss_mb": ru.ru_maxrss / 1024}))
"""
VARIANTS = {
    "master": {"bin": "master", "extra": []},
    "branch": {"bin": "final", "extra": []},
    "branch_master_sel": {"bin": "final", "extra": ["--eta-min", "1.7", "--eta-max", "100"]},
}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--master-bin", required=True)
    p.add_argument("--final-bin", required=True)
    p.add_argument("--config", default=os.path.join(HERE, "..", "config", "ttbar.cfg"))
    p.add_argument("--workdir", required=True)
    p.add_argument("--out", required=True, help="JSON file with all runs and the summary")
    p.add_argument("--sizes", type=int, nargs="+", default=[1000, 4000])
    p.add_argument("--reps", type=int, default=3)
    p.add_argument("--jobs", type=int, default=6, help="parallel runs (<= 16)")
    p.add_argument("--wrap", default=DEFAULT_WRAP)
    args = p.parse_args()
    if args.jobs > 16:
        sys.exit("--jobs must be <= 16")
    wd = os.path.abspath(args.workdir)
    os.makedirs(wd, exist_ok=True)
    wrap = shlex.split(args.wrap)
    config = os.path.abspath(args.config)
    bins = {"master": os.path.abspath(args.master_bin), "final": os.path.abspath(args.final_bin)}
    base_cfg = open(config).read()

    jobs = []
    for rep in range(args.reps):
        seed = 1000 + rep
        master_cfg = os.path.join(wd, f"master_seed{seed}.cfg")
        with open(master_cfg, "w") as f:
            f.write(base_cfg + f"\nRandom:setSeed = on\nRandom:seed = {seed}\n")
        for n in args.sizes:
            for name, v in VARIANTS.items():
                tag = f"{name}_n{n}_r{rep}"
                out = os.path.join(wd, tag + ".root")
                if v["bin"] == "master":
                    cmd = [bins["master"], "-c", master_cfg]
                else:
                    cmd = [bins["final"], "-c", config, "--seed", str(seed)] + v["extra"]
                cmd += ["-n", str(n), "-f", out]
                jobs.append({"variant": name, "n": n, "rep": rep, "seed": seed, "out": out,
                             "cmd": wrap + ["python3", "-c", MEASURE, out + ".log"] + cmd})
    # largest runs first, so the pool drains evenly
    jobs.sort(key=lambda j: -j["n"])

    def run(j):
        r = subprocess.run(j["cmd"], capture_output=True, text=True, cwd=wd)
        line = next((l for l in r.stdout.splitlines() if l.startswith("{")), None)
        res = json.loads(line) if line else {"rc": -1}
        if res.get("rc") == 0:
            res["n_jets"] = uproot.open(j["out"])["JetTree"].num_entries
        print(f"{j['variant']:18s} n={j['n']:5d} rep={j['rep']} -> {res}", flush=True)
        return {k: j[k] for k in ("variant", "n", "rep", "seed")} | res

    with cf.ThreadPoolExecutor(args.jobs) as ex:
        runs = list(ex.map(run, jobs))

    bad = [r for r in runs if r.get("rc") != 0]
    summary = []
    for n in args.sizes:
        for name in VARIANTS:
            rs = [r for r in runs if r["variant"] == name and r["n"] == n and r.get("rc") == 0]
            if not rs:
                continue
            med = lambda k: statistics.median(r[k] for r in rs)
            summary.append({"variant": name, "n": n, "reps": len(rs),
                            "wall": med("wall"), "cpu": statistics.median(r["user"] + r["sys"] for r in rs),
                            "cpu_min": min(r["user"] + r["sys"] for r in rs), "cpu_max": max(r["user"] + r["sys"] for r in rs),
                            "maxrss_mb": med("maxrss_mb"), "n_jets": med("n_jets")})
    host = subprocess.run(["bash", "-c", "lscpu | sed -n 's/^Model name: *//p'; nproc"], capture_output=True, text=True).stdout.split("\n")
    with open(args.out, "w") as f:
        json.dump({"runs": runs, "summary": summary, "variants": {k: v["extra"] for k, v in VARIANTS.items()},
                   "host": {"cpu": host[0].strip(), "nproc": host[1].strip()}, "jobs": args.jobs,
                   "config": config}, f, indent=1)
    for s in summary:
        print(f"{s['variant']:18s} n={s['n']:5d}: wall {s['wall']:7.1f} s  cpu {s['cpu']:7.1f} s "
              f"({1000 * s['cpu'] / s['n']:.1f} ms/event)  maxRSS {s['maxrss_mb']:6.0f} MB  jets {s['n_jets']:.0f}")
    if bad:
        print(f"{len(bad)} runs failed", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
