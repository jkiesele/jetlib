#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "EventGenerator.h"
#include "JetAnalysis.h"
#include "JetAnalyzer.h"
#include "JetSelection.h"
#include "fastjet/PseudoJet.hh"
#include "fastjet/Error.hh"

PYBIND11_MODULE(jetlibpy, m) {
    m.doc() = "Python bindings for jetlib";

    // fastjet::Error does not derive from std::exception; without this it would arrive
    // in Python as "Caught an unknown exception!".
    pybind11::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p) std::rethrow_exception(p);
        } catch (const fastjet::Error& e) {
            PyErr_SetString(PyExc_RuntimeError, ("FastJet: " + e.message()).c_str());
        }
    });

    // Bind fastjet::PseudoJet
    pybind11::class_<fastjet::PseudoJet>(m, "PseudoJet")
        .def(pybind11::init<>())
        .def(pybind11::init<double, double, double, double>(),
             pybind11::arg("px"), pybind11::arg("py"), pybind11::arg("pz"), pybind11::arg("E"))
        .def("user_index", &fastjet::PseudoJet::user_index, "PDG ID for particles/constituents from jetlib")
        .def("set_user_index", &fastjet::PseudoJet::set_user_index, pybind11::arg("index"))
        .def("pt", &fastjet::PseudoJet::pt)
        .def("eta", &fastjet::PseudoJet::eta)
        // phi in [-pi, pi), as in the output tree (FastJet's own phi() is in [0, 2pi))
        .def("phi", &fastjet::PseudoJet::phi_std, "Azimuth in [-pi, pi), as in the output tree")
        .def("phi_02pi", &fastjet::PseudoJet::phi_02pi, "Azimuth in [0, 2pi)")
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
        .def("getConfigText", &EventGenerator::getConfigText)
        .def("getPythiaVersion", &EventGenerator::getPythiaVersion)
        .def("getJetCollection", &EventGenerator::getJetCollection)
        .def("setClusterPtMin", &EventGenerator::setClusterPtMin, pybind11::arg("ptMin"))
        .def("getClusterPtMin", &EventGenerator::getClusterPtMin)
        .def("setKeepPromptLeptons", &EventGenerator::setKeepPromptLeptons, pybind11::arg("keep"))
        .def("getKeepPromptLeptons", &EventGenerator::getKeepPromptLeptons)
        .def("getNRemovedInvisible", &EventGenerator::getNRemovedInvisible)
        .def("getNRemovedPromptLeptons", &EventGenerator::getNRemovedPromptLeptons)
        .def("getNRemovedUndecayed", &EventGenerator::getNRemovedUndecayed)
        .def("getRemovedUndecayed", &EventGenerator::getRemovedUndecayed)
        .def("getNFailedEvents", &EventGenerator::getNFailedEvents)
        .def("setSeed", &EventGenerator::setSeed, pybind11::arg("seed"),
             "Override the config seed (0 = jetlib draws one, max 900000000). Call before initialize().")
        .def("getSeed", &EventGenerator::getSeed, "Seed actually used (after initialize()); -1 = Pythia default seed")
        .def("getRequestedSeed", &EventGenerator::getRequestedSeed);

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
        .def("getJets", &JetAnalysis::getJets, pybind11::arg("ptMin"))
        // convertPythiaToPseudoJets is not bound: Pythia8::Event is not available in Python.
        .def("setParticles", &JetAnalysis::setParticles, pybind11::arg("particles"),
             "Particles (PseudoJet(px, py, pz, E)) to cluster; replaces previous particles and jets.")
        .def("getParticles", &JetAnalysis::getParticles)
        .def("clear", &JetAnalysis::clear);

    pybind11::class_<JetAnalyzer>(m, "JetAnalyzer")
        .def(pybind11::init<std::string>(), pybind11::arg("outputFilename") = "jet_analysis.root")
        .def("analyze", &JetAnalyzer::analyze)
        .def("begin", &JetAnalyzer::begin)
        .def("fill", pybind11::overload_cast<const std::vector<fastjet::PseudoJet>&, long long>(&JetAnalyzer::fill),
             pybind11::arg("jets"), pybind11::arg("eventIndex"))
        .def("finish", &JetAnalyzer::finish)
        .def("setMetadata", &JetAnalyzer::setMetadata, pybind11::arg("name"), pybind11::arg("value"))
        .def("setFilter", &JetAnalyzer::setFilter)
        .def("setSelection", &JetAnalyzer::setSelection)
        .def("getSelection", &JetAnalyzer::getSelection);
}