#include "EventGenerator.h"
#include "JetAnalyzer.h"
#include <iostream>
#include "ArgumentParser.h"
#include <algorithm>

void printEventInfo(const std::vector<fastjet::PseudoJet>& jets, size_t i) {
        std::cout << " *************\n";
        std::cout << " Event " << i << " has " << jets.size() << " jets." << std::endl;
        for (size_t j = 0; j < jets.size(); ++j) {
            const auto& jet = jets[j];
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

int run(const ArgumentParser& argParser) {
    bool debug = argParser.isDebug();
    size_t nEvents = argParser.getNEvents();
    std::string configFile = argParser.getConfigFile();
    std::string outputFilename = argParser.getFilename();
    const JetSelection& selection = argParser.getSelection();
    // the clustering threshold must never cut harder than the selection
    const double clusterPtMin = std::min(20.0, selection.ptMin);
    std::cout << "Jet selection: " << selection.describe() << std::endl;
    std::cout << "Clustering pT threshold: " << clusterPtMin << " GeV" << std::endl;
    const long seed = argParser.getSeed();
    std::cout << "Random seed: " << seed
              << (seed == 0 ? " (time-based, 1 s resolution: jobs started in the same second produce"
                              " identical events; use distinct --seed values for parallel jobs)" : "")
              << std::endl;

    const bool keepPromptLeptons = argParser.keepPromptLeptons();
    std::cout << "Particles clustered: visible final state; neutrinos/invisibles removed; prompt e/mu "
              << (keepPromptLeptons ? "kept" : "removed") << std::endl;

    // open the output first, so an unusable path fails before the generation
    JetAnalyzer jetAnalyzer(outputFilename);

    EventGenerator eventGen(configFile);
    eventGen.setKeepPromptLeptons(keepPromptLeptons);
    eventGen.setClusterPtMin(clusterPtMin);
    eventGen.setSeed(seed);
    eventGen.initialize();

    // Stream: each event is selected and written while its ClusterSequence is alive,
    // then freed, so memory does not grow with the number of events.
    jetAnalyzer.setSelection(selection);
    jetAnalyzer.begin();
    eventGen.generateEvents(nEvents, [&](const std::vector<fastjet::PseudoJet>& jets, size_t iEvent) {
        if (debug) printEventInfo(jets, iEvent);
        jetAnalyzer.fill(jets);
    });
    jetAnalyzer.finish();
    std::cout << "Removed before clustering: " << eventGen.getNRemovedInvisible() << " invisible particles, "
              << eventGen.getNRemovedPromptLeptons() << " prompt leptons" << std::endl;
    std::cout << "Generated " << eventGen.getNAcceptedEvents() << " events ("
              << eventGen.getNFailedEvents() << " failed pythia.next() calls retried)" << std::endl;
    return 0;
}

int main(int argc, char** argv) {
    ArgumentParser argParser;
    if (!argParser.parse(argc, argv)) {
        return argParser.helpRequested() ? 0 : 1;
    }
    try {
        return run(argParser);
    } catch (const fastjet::Error& e) {
        std::cerr << "Error (FastJet): " << e.message() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 1;
}
