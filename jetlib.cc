#include "EventGenerator.h"
#include "JetAnalyzer.h"
#include <iostream>
#include "ArgumentParser.h"
#include <algorithm>
#include <sstream>
#include "fastjet/ClusterSequence.hh"
#include "fastjet/PseudoJet.hh"
#include "JetlibVersion.h"

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

int run(const ArgumentParser& argParser, const std::string& commandLine) {
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
    const long effectiveSeed = eventGen.getSeed();
    std::cout << "Random seed: " << effectiveSeed;
    if (seed == 0) std::cout << " (drawn by jetlib; reproduce with --seed " << effectiveSeed << ")";
    std::cout << std::endl;

    // Stream: each event is selected and written while its ClusterSequence is alive,
    // then freed, so memory does not grow with the number of events.
    jetAnalyzer.setSelection(selection);
    jetAnalyzer.begin();
    eventGen.generateEvents(nEvents, [&](const std::vector<fastjet::PseudoJet>& jets, size_t iEvent) {
        if (debug) printEventInfo(jets, iEvent);
        jetAnalyzer.fill(jets, static_cast<long long>(iEvent));
    });
    // provenance, stored in the "jetlib" directory of the output file
    auto str = [](double v) { std::ostringstream s; s << v; return s.str(); };
    jetAnalyzer.setMetadata("jetlib_version", JETLIB_GIT_VERSION);
    jetAnalyzer.setMetadata("command_line", commandLine);
    jetAnalyzer.setMetadata("config_file", configFile);
    jetAnalyzer.setMetadata("config", eventGen.getConfigText());
    jetAnalyzer.setMetadata("seed", std::to_string(effectiveSeed));
    jetAnalyzer.setMetadata("seed_requested", seed == 0 ? "0 (seed drawn by jetlib)" : std::to_string(seed));
    jetAnalyzer.setMetadata("pythia_version", str(eventGen.getPythiaVersion()));
    jetAnalyzer.setMetadata("fastjet_version", fastjet::fastjet_version_string());
    jetAnalyzer.setMetadata("jet_algorithm", "anti-kt, R = " + str(EventGenerator::kJetR));
    jetAnalyzer.setMetadata("cluster_pt_min", str(clusterPtMin));
    jetAnalyzer.setMetadata("selection", selection.describe());
    jetAnalyzer.setMetadata("particles", std::string("visible final state; invisibles removed; prompt e/mu ") +
                                         (keepPromptLeptons ? "kept" : "removed"));
    jetAnalyzer.setMetadata("n_events_requested", std::to_string(nEvents));
    jetAnalyzer.setMetadata("n_events_accepted", std::to_string(eventGen.getNAcceptedEvents()));
    jetAnalyzer.setMetadata("n_failed_next", std::to_string(eventGen.getNFailedEvents()));
    jetAnalyzer.setMetadata("n_removed_invisible", std::to_string(eventGen.getNRemovedInvisible()));
    jetAnalyzer.setMetadata("n_removed_prompt_leptons", std::to_string(eventGen.getNRemovedPromptLeptons()));
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
        std::string commandLine;
        for (int i = 0; i < argc; ++i) commandLine += (i ? " " : "") + std::string(argv[i]);
        return run(argParser, commandLine);
    } catch (const fastjet::Error& e) {
        std::cerr << "Error (FastJet): " << e.message() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 1;
}
