#ifndef JetAnalysis_h
#define JetAnalysis_h

#include "fastjet/ClusterSequence.hh"
#include "fastjet/PseudoJet.hh"

#include "Pythia8/Pythia.h"
#include <vector>
#include <memory>

class JetAnalysis {
  public:
    JetAnalysis() {};
    ~JetAnalysis() {};

    // ptMin: minimum jet pT kept by the clustering step
    void clusterJets(double R, double ptMin = 20.0);
    std::vector<fastjet::PseudoJet> getJets(double ptMin) const {
      std::vector<fastjet::PseudoJet> selectedJets;
      for (const auto& jet : jets_) {
        if (jet.pt() >= ptMin) {
          selectedJets.push_back(jet);
        }
      }
      return selectedJets;
    }

    void convertPythiaToPseudoJets(const Pythia8::Event& event);

    void clear() {
      jets_.clear();
      particles_.clear();
    }

  private:
    std::vector<fastjet::PseudoJet> jets_, particles_;
    std::unique_ptr<fastjet::ClusterSequence> clusterSeq_;
};

#endif