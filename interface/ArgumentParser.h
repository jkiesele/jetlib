#ifndef ArgumentParser_h
#define ArgumentParser_h

#include <string>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include "JetSelection.h"

class ArgumentParser {
  private:
    std::string filename_;
    std::string configFile_;
    bool debug_;
    int nEvents_;
    JetSelection selection_;

    // Parse a floating-point option value; the whole string must be a finite number.
    static bool parseDouble(const std::string& opt, const char* text, double& out) {
        std::string s(text);
        try {
            size_t pos = 0;
            out = std::stod(s, &pos);
            if (pos == s.size() && std::isfinite(out)) return true;
        } catch (const std::exception&) {}
        std::cerr << "Error: " << opt << " expects a number, got '" << s << "'" << std::endl;
        return false;
    }

    // Default selection: reproduces the former hardcoded filter "eta > 1.7 && pt > 30".
    static JetSelection defaultSelection() {
        JetSelection sel;
        sel.ptMin = 30.0;
        sel.etaMin = 1.7;
        return sel;
    }
    
  public:
    ArgumentParser() : filename_("output.root"), configFile_("../config/ttbar.cfg"), debug_(false), nEvents_(1), selection_(defaultSelection()) {}
    
    bool parse(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "--nEvents" || arg == "-n") {
                if (i + 1 < argc) {
                    nEvents_ = std::stoi(argv[++i]);
                } else {
                    std::cerr << "Error: " << arg << " requires a number argument" << std::endl;
                    return false;
                }
            }
            else if (arg == "--config" || arg == "-c") {
                if (i + 1 < argc) {
                    configFile_ = argv[++i];
                    std::cout << "Config file specified: " << configFile_ << std::endl;
                } else {
                    std::cerr << "Error: " << arg << " requires a filename argument" << std::endl;
                    return false;
                }
            }
            else if (arg == "--pt-min" || arg == "--pt-max" || arg == "--eta-min" || arg == "--eta-max") {
                if (i + 1 >= argc) {
                    std::cerr << "Error: " << arg << " requires a number argument" << std::endl;
                    return false;
                }
                double v;
                if (!parseDouble(arg, argv[++i], v)) return false;
                if (arg == "--pt-min") selection_.ptMin = v;
                else if (arg == "--pt-max") selection_.ptMax = v;
                else if (arg == "--eta-min") selection_.etaMin = v;
                else selection_.etaMax = v;
            }
            else if (arg == "--abs-eta") {
                selection_.absEta = true;
            }
            else if (arg == "--debug" || arg == "-d") {
                debug_ = true;
            }
            else if (arg == "--filename" || arg == "-f") {
                if (i + 1 < argc) {
                    filename_ = argv[++i];
                } else {
                    std::cerr << "Error: " << arg << " requires a filename argument" << std::endl;
                    return false;
                }
            }
            else if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return false;
            }
            else if (arg.find(".root") != std::string::npos) {
                // If it looks like a root file, treat it as filename
                filename_ = arg;
            }
            else {
                std::cerr << "Unknown argument: " << arg << std::endl;
                printUsage(argv[0]);
                return false;
            }
        }
        try {
            selection_.validate();
        } catch (const std::invalid_argument& e) {
            std::cerr << "Error: invalid jet selection: " << e.what() << std::endl;
            return false;
        }
        return true;
    }
    
    void printUsage(const char* program_name) {
        std::cout << "Usage: " << program_name << " [options]" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -c, --config <file>      Configuration file for Pythia (default: config/ttbar.cfg)" << std::endl;
        std::cout << "  -f, --filename <file>    Output ROOT filename (default: output.root)" << std::endl;
        std::cout << "  -n, --nEvents <number>   Number of events to generate (default: 1)" << std::endl;
        std::cout << "  -d, --debug              Enable debug mode" << std::endl;
        std::cout << std::endl;
        std::cout << "Jet selection (all cuts strict; default: pt > 30 && eta > 1.7):" << std::endl;
        std::cout << "      --pt-min <GeV>       Minimum jet pT (default: 30, must be >= 0)" << std::endl;
        std::cout << "      --pt-max <GeV>       Maximum jet pT (default: none)" << std::endl;
        std::cout << "      --eta-min <value>    Minimum jet eta (default: 1.7)" << std::endl;
        std::cout << "      --eta-max <value>    Maximum jet eta (default: none)" << std::endl;
        std::cout << "      --abs-eta            Apply the eta cuts to |eta| (default: signed eta)" << std::endl;
        std::cout << "  The clustering threshold is min(20 GeV, pt-min), so it never cuts harder than --pt-min." << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << program_name << " -c myconfig.cfg -f myoutput.root -n 100" << std::endl;
  
    }
    
    const std::string& getConfigFile() const { return configFile_; }
    const std::string& getFilename() const { return filename_; }
    bool isDebug() const { return debug_; }
    int getNEvents() const { return nEvents_; }
    const JetSelection& getSelection() const { return selection_; }
};

#endif
