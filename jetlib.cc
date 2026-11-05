#include "EventGenerator.h"
#include "JetAnalyzer.h"
#include <iostream>
#include "ArgumentParser.h"
#include <algorithm>

void printEventInfo(const JetCollection& jetCollection) {
    for (size_t i = 0; i < jetCollection.size(); ++i) {
        std::cout << " *************\n";
        std::cout << " Event " << i << " has " << jetCollection[i].size() << " jets." << std::endl;
        for (size_t j = 0; j < jetCollection[i].size(); ++j) {
            const auto& jet = jetCollection[i][j];
            std::cout << "  Jet " << j + 1 << ": pt = " << jet.pt() 
                      << ", eta = " << jet.eta() 
                      << ", phi = " << jet.phi() << std::endl;
            for (int k = 0; k < jet.constituents().size(); ++k) {
                const auto& constituent = jet.constituents()[k];
                std::cout << "    Constituent " << k + 1 << ": pt = " << constituent.pt() 
                          << ", eta = " << constituent.eta() 
                          << ", phi = " << constituent.phi() << std::endl;
            }
        }
        std::cout << " *************\n";
    }
}

int main(int argc, char** argv) {

    ArgumentParser argParser;
    if (!argParser.parse(argc, argv)) {
        return 1;
    } 
    argParser.parse(argc, argv);
    bool debug = argParser.isDebug();
    size_t nEvents = argParser.getNEvents();
    std::string configFile = argParser.getConfigFile();
    std::string outputFilename = argParser.getFilename();
    const JetSelection& selection = argParser.getSelection();
    // the clustering threshold must never cut harder than the selection
    const double clusterPtMin = std::min(20.0, selection.ptMin);
    std::cout << "Jet selection: " << selection.describe() << std::endl;
    std::cout << "Clustering pT threshold: " << clusterPtMin << " GeV" << std::endl;

    EventGenerator eventGen(configFile);
    eventGen.setClusterPtMin(clusterPtMin);
    eventGen.initialize();
    eventGen.generateEvents(nEvents);
    const JetCollection& jets = eventGen.getJetCollection();
    if (debug) {
        printEventInfo(jets);
    }
    JetAnalyzer jetAnalyzer(outputFilename);
    jetAnalyzer.setSelection(selection);
    jetAnalyzer.analyze(jets);
    return 0;
}
