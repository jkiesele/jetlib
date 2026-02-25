#ifndef ArgumentParser_h
#define ArgumentParser_h

#include <string>
#include <iostream>

class ArgumentParser {
  private:
    std::string filename_;
    std::string configFile_;
    bool debug_;
    int nEvents_;
    
  public:
    ArgumentParser() : filename_("output.root"), configFile_("../config/ttbar.cfg"), debug_(false), nEvents_(1) {}
    
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
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << program_name << " -c myconfig.cfg -f myoutput.root -n 100" << std::endl;
  
    }
    
    const std::string& getConfigFile() const { return configFile_; }
    const std::string& getFilename() const { return filename_; }
    bool isDebug() const { return debug_; }
    int getNEvents() const { return nEvents_; }
};

#endif
