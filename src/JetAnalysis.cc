#include "JetAnalysis.h"

void JetAnalysis::convertPythiaToPseudoJets(const Pythia8::Event& event) {
    for (int i = 0; i < event.size(); ++i) {
      if (!event[i].isFinal()) continue;
      fastjet::PseudoJet particle(event[i].px(), event[i].py(), event[i].pz(), event[i].e());
      particle.set_user_index(event[i].id());
      particles_.push_back(particle);
    }
}
void JetAnalysis::clusterJets(double R, double ptMin) {
    
    fastjet::JetDefinition jetDef(fastjet::antikt_algorithm, R);
    clusterSeq_ = std::make_unique<fastjet::ClusterSequence>(particles_, jetDef);
    jets_ = clusterSeq_->inclusive_jets(ptMin);
}
