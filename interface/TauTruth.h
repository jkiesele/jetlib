#ifndef TauTruth_h
#define TauTruth_h

#include "Pythia8/Pythia.h"
#include "fastjet/PseudoJet.hh"
#include <vector>

// Generator-level tau leptons of one Pythia event, for matching jets to taus.
struct TauTruth {
    int pdgId = 0;              // +15 (tau-) or -15 (tau+)
    fastjet::PseudoJet p4;      // full tau four-momentum (last copy, before the decay)
    fastjet::PseudoJet visP4;   // sum of the visible final-state descendants (no neutrinos)
    // Decay mode: -11 (tau -> e nu nu), -13 (tau -> mu nu nu); for hadronic decays
    // 5 * (nProngs - 1) + nPi0, e.g. 0 = pi nu, 1 = pi pi0 nu, 10 = 3 pi nu, 11 = 3 pi pi0 nu.
    int decayMode = 0;
    int nProngs = 0;            // charged hadrons (pi+-, K+-) from the tau decay
    int nPi0 = 0;               // pi0 from the tau decay
};

// Finds all prompt decayed taus (the last copy of each tau, i.e. the one that decays; taus
// from hadron decays such as D_s -> tau nu are skipped) and fills their visible momentum
// and decay mode.
// Prongs and pi0s are counted among the tau's descendants, descending through intermediate
// resonances (rho, a1, omega, eta, ...) but not into pi+-, K+-, pi0 or neutral kaons:
// a K0S -> pi+ pi- does not add prongs, as in the usual decay-mode definitions.
std::vector<TauTruth> findTaus(const Pythia8::Event& event);

// Index of the tau whose visible momentum is closest to the jet within deltaR < maxDR, or -1.
// dR is set to that distance (or -1).
int matchTau(const fastjet::PseudoJet& jet, const std::vector<TauTruth>& taus, double maxDR, double& dR);

#endif
