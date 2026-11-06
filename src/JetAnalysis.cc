#include "JetAnalysis.h"

#include <cstdlib>

bool JetAnalysis::isPromptLepton(const Pythia8::Event& event, int i) {
    const int absId = std::abs(event[i].id());
    if (absId != 11 && absId != 13) return false;
    // Walk up the mother1 chain. Hadrons from hadronization or decays have
    // |status| 81-99; the beam protons (status -12) do not count.
    int j = event[i].mother1();
    for (int guard = 0; j > 0 && guard < event.size(); ++guard) {
      const int absStatus = std::abs(event[j].status());
      if (event[j].isHadron() && absStatus >= 81 && absStatus <= 99) return false;
      j = event[j].mother1();
    }
    return true;
}

void JetAnalysis::convertPythiaToPseudoJets(const Pythia8::Event& event, bool keepPromptLeptons) {
    // one event per call: start from an empty particle list and fresh counters
    clear();
    nRemovedInvisible_ = 0;
    nRemovedPromptLeptons_ = 0;
    for (int i = 0; i < event.size(); ++i) {
      if (!event[i].isFinal()) continue;
      if (!event[i].isVisible()) { ++nRemovedInvisible_; continue; }
      if (!keepPromptLeptons && isPromptLepton(event, i)) { ++nRemovedPromptLeptons_; continue; }
      fastjet::PseudoJet particle(event[i].px(), event[i].py(), event[i].pz(), event[i].e());
      particle.set_user_index(event[i].id());
      particles_.push_back(particle);
    }
}
void JetAnalysis::clusterJets(double R, double ptMin) {
    fastjet::JetDefinition jetDef(fastjet::antikt_algorithm, R);
    auto clusterSeq = std::make_unique<fastjet::ClusterSequence>(particles_, jetDef);
    jets_ = clusterSeq->inclusive_jets(ptMin);
    // The jets own the ClusterSequence: it is deleted when the last jet (or copy of one)
    // referring to it is destroyed, so constituents() works as long as a jet exists.
    // FastJet allows this only if at least one jet refers to it; otherwise delete it now.
    if (!jets_.empty()) clusterSeq.release()->delete_self_when_unused();
}
