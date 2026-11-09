#include "TauTruth.h"

#include <cstdlib>

namespace {
// Counts pi+- / K+- (prongs), pi0, and e / mu among the decay products of particle i.
void countProducts(const Pythia8::Event& event, int i, int& nProngs, int& nPi0, int& nE, int& nMu) {
    for (int d : event[i].daughterList()) {
        const int a = std::abs(event[d].id());
        if (a == 211 || a == 321) ++nProngs;
        else if (a == 111) ++nPi0;
        else if (a == 11) ++nE;
        else if (a == 13) ++nMu;
        else if (a == 130 || a == 310 || a == 311 || a == 22 || a == 12 || a == 14 || a == 16) continue;
        else if (!event[d].isFinal()) countProducts(event, d, nProngs, nPi0, nE, nMu);
    }
}

// True if a hadron from hadronization or decays (|status| 81-99) is among the mother1 ancestors,
// as in JetAnalysis::isPromptLepton; beam protons do not count.
bool fromHadron(const Pythia8::Event& event, int i) {
    int j = event[i].mother1();
    for (int guard = 0; j > 0 && guard < event.size(); ++guard) {
        const int absStatus = std::abs(event[j].status());
        if (event[j].isHadron() && absStatus >= 81 && absStatus <= 99) return true;
        j = event[j].mother1();
    }
    return false;
}

// Adds the visible final-state descendants of particle i.
void addVisible(const Pythia8::Event& event, int i, fastjet::PseudoJet& sum) {
    for (int d : event[i].daughterList()) {
        if (event[d].isFinal()) {
            if (event[d].isVisible())
                sum += fastjet::PseudoJet(event[d].px(), event[d].py(), event[d].pz(), event[d].e());
        } else {
            addVisible(event, d, sum);
        }
    }
}
}  // namespace

std::vector<TauTruth> findTaus(const Pythia8::Event& event) {
    std::vector<TauTruth> taus;
    for (int i = 0; i < event.size(); ++i) {
        if (std::abs(event[i].id()) != 15) continue;
        if (event[i].isFinal()) continue;                  // undecayed tau: none in standard settings
        if (event[i].iBotCopyId() != i) continue;          // only the last copy, which decays
        if (fromHadron(event, i)) continue;                // e.g. D_s -> tau nu inside a heavy-flavour jet
        TauTruth t;
        t.pdgId = event[i].id();
        t.p4 = fastjet::PseudoJet(event[i].px(), event[i].py(), event[i].pz(), event[i].e());
        t.visP4 = fastjet::PseudoJet(0, 0, 0, 0);
        addVisible(event, i, t.visP4);
        int nE = 0, nMu = 0;
        countProducts(event, i, t.nProngs, t.nPi0, nE, nMu);
        if (nE > 0 && t.nProngs == 0) t.decayMode = -11;
        else if (nMu > 0 && t.nProngs == 0) t.decayMode = -13;
        else t.decayMode = 5 * (t.nProngs - 1) + t.nPi0;
        taus.push_back(t);
    }
    return taus;
}

int matchTau(const fastjet::PseudoJet& jet, const std::vector<TauTruth>& taus, double maxDR, double& dR) {
    int best = -1;
    dR = -1;
    for (size_t k = 0; k < taus.size(); ++k) {
        if (taus[k].visP4.pt() <= 0) continue;
        const double d = jet.delta_R(taus[k].visP4);
        if (d < maxDR && (best < 0 || d < dR)) { best = static_cast<int>(k); dR = d; }
    }
    return best;
}
