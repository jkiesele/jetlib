#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "EventGenerator.h"
#include "JetAnalysis.h"
#include "JetAnalyzer.h"
#include "JetSelection.h"
#include "fastjet/PseudoJet.hh"

PYBIND11_MODULE(jetlibpy, m) {
    m.doc() = "Python bindings for jetlib";

    // Bind fastjet::PseudoJet
    pybind11::class_<fastjet::PseudoJet>(m, "PseudoJet")
        .def(pybind11::init<>())
        .def("pt", &fastjet::PseudoJet::pt)
        .def("eta", &fastjet::PseudoJet::eta)
        .def("phi", &fastjet::PseudoJet::phi)
        .def("rap", &fastjet::PseudoJet::rap)
        .def("m", &fastjet::PseudoJet::m)
        .def("e", &fastjet::PseudoJet::E)
        .def("px", &fastjet::PseudoJet::px)
        .def("py", &fastjet::PseudoJet::py)
        .def("pz", &fastjet::PseudoJet::pz)
        .def("constituents", &fastjet::PseudoJet::constituents);

    pybind11::class_<EventGenerator>(m, "EventGenerator")
        .def(pybind11::init<std::string>(), pybind11::arg("configFile") = "config.cfg")
        .def("initialize", &EventGenerator::initialize)
        .def("generateEvents", [](EventGenerator& g, size_t nEvents) { g.generateEvents(nEvents); },
             pybind11::arg("nEvents"), "Generate events and keep all jets for getJetCollection().")
        .def("getNAcceptedEvents", &EventGenerator::getNAcceptedEvents)
        .def("getJetCollection", &EventGenerator::getJetCollection)
        .def("setClusterPtMin", &EventGenerator::setClusterPtMin, pybind11::arg("ptMin"))
        .def("getClusterPtMin", &EventGenerator::getClusterPtMin)
        .def("setKeepPromptLeptons", &EventGenerator::setKeepPromptLeptons, pybind11::arg("keep"))
        .def("getKeepPromptLeptons", &EventGenerator::getKeepPromptLeptons)
        .def("getNRemovedInvisible", &EventGenerator::getNRemovedInvisible)
        .def("getNRemovedPromptLeptons", &EventGenerator::getNRemovedPromptLeptons)
        .def("getNFailedEvents", &EventGenerator::getNFailedEvents)
        .def("setSeed", &EventGenerator::setSeed, pybind11::arg("seed"),
             "Override the config seed (0 = time-based, max 900000000). Call before initialize().");

    pybind11::class_<JetSelection>(m, "JetSelection")
        .def(pybind11::init<>())
        .def_readwrite("ptMin", &JetSelection::ptMin)
        .def_readwrite("ptMax", &JetSelection::ptMax)
        .def_readwrite("etaMin", &JetSelection::etaMin)
        .def_readwrite("etaMax", &JetSelection::etaMax)
        .def_readwrite("absEta", &JetSelection::absEta)
        .def("validate", &JetSelection::validate)
        .def("passes", &JetSelection::pass)
        .def("describe", &JetSelection::describe)
        .def("__repr__", [](const JetSelection& s) { return "JetSelection(" + s.describe() + ")"; });

    pybind11::class_<JetAnalysis>(m, "JetAnalysis")
        .def(pybind11::init<>())
        .def("clusterJets", &JetAnalysis::clusterJets, pybind11::arg("R"), pybind11::arg("ptMin") = 20.0)
        .def("getJets", &JetAnalysis::getJets)
        .def("convertPythiaToPseudoJets", &JetAnalysis::convertPythiaToPseudoJets,
             pybind11::arg("event"), pybind11::arg("keepPromptLeptons") = false)
        .def("clear", &JetAnalysis::clear);

    pybind11::class_<JetAnalyzer>(m, "JetAnalyzer")
        .def(pybind11::init<std::string>(), pybind11::arg("outputFilename") = "jet_analysis.root")
        .def("analyze", &JetAnalyzer::analyze)
        .def("begin", &JetAnalyzer::begin)
        .def("fill", &JetAnalyzer::fill, pybind11::arg("jets"), pybind11::arg("eventIndex"))
        .def("finish", &JetAnalyzer::finish)
        .def("setFilter", &JetAnalyzer::setFilter)
        .def("setSelection", &JetAnalyzer::setSelection)
        .def("getSelection", &JetAnalyzer::getSelection);
}