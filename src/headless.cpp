#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <chrono>
#include <csignal>
#include <cmath>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include "model.h"
#include "load.h"

sig_atomic_t halt = 0;

void handleInterrupt(int p) {
    halt = 1;
}

bool acceptCommand(Model *m) {
    std::cout << "Enter command: [\"exit\" to terminate, \"save <filename>\" to save model state, \"resume\" to continue]" << std::endl;
    while (true) {
        std::cout << "> ";
        std::string line;
        if (std::getline(std::cin, line)) {
            if (line.substr(0, 4) == "exit") {
                std::cout << "Really exit? [y/N]: ";
                if (std::getline(std::cin, line)) {
                    if (line == "y") {
                        return true;
                    } else {
                        continue;
                    }
                }
            } else if (line.substr(0, 4) == "save") {
                m->saveState(line.substr(5));
                std::cout << "State saved to " << line.substr(5) << std::endl;
            } else if (line.substr(0, 6) == "resume") {
                return false;
            } else {
                std::cout << "Unrecognized command" << std::endl;
            }
        }
    }
}

struct runListingEntry {
    int runID;
    int status;
    float mortConstA;
    float mortConstC;
    runListingEntry(int runID, int status, float mortConstA, float mortConstC)
        : runID(runID), status(status), mortConstA(mortConstA), mortConstC(mortConstC) {}
};

void readRunListings(std::string& runListingPath, std::string& header, std::vector<struct runListingEntry>& runListings) {
    std::ifstream is(runListingPath);
    std::string line;
    bool first = true;
    while (std::getline(is, line) && !line.empty()) {
        if (first) {
            first = false;
            header = line;
            continue;
        }
        std::vector<std::string> chunks = split(line, ',');
        runListings.emplace_back(std::stoi(chunks[0]), std::stoi(chunks[1]), std::stof(chunks[2]), std::stof(chunks[3]));
    }
}

void writeRunListings(std::string& runListingPath, std::string& header, std::vector<struct runListingEntry>& runListings) {
    std::ofstream os(runListingPath);
    os << header << std::endl;
    for (struct runListingEntry& entry : runListings) {
        os << entry.runID << ',' << entry.status << ',' << entry.mortConstA << ',' << entry.mortConstC << std::endl;
    }
}

int pickRun(std::string& runListingPath, Model *model) {
    std::vector<struct runListingEntry> runListings;
    std::string header;
    readRunListings(runListingPath, header, runListings);
    for (struct runListingEntry &entry : runListings) {
        if (entry.status == 0) {
            entry.status = 1;
            model->mortConstA = entry.mortConstA;
            model->mortConstC = entry.mortConstC;
            writeRunListings(runListingPath, header, runListings);
            return entry.runID;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    std::string configPath = "default_config_env_from_file.json";
    if (argc < 3) {
        std::cerr << "Too few arguments, aborting (need run listing file and output directory)" << std::endl;
        exit(1);
    }
    std::string runListingPath(argv[1]);
    std::string outputPath(argv[2]);
    if (argc > 3) {
        configPath = std::string(argv[3]);
    }

    // Access the run listing file to get this run's parameters
    int runListingFd = open(argv[1], O_RDONLY);
    if (runListingFd == -1) {
        std::cerr << "Couldn't open run listing file, aborting!" << std::endl;
        exit(1);
    }

    if (flock(runListingFd, LOCK_EX) == -1) {
        std::cerr << "Couldn't lock run listing file, aborting!" << std::endl;
        exit(1);
    }

    std::cout << "Configuring model..." << std::endl;
    Model *m = modelFromConfig(configPath);

    int runID = pickRun(runListingPath, m);

    flock(runListingFd, LOCK_UN);
    close(runListingFd);

    if (runID == -1) {
        std::cout << "No runs left in listing file!" << std::endl;
        exit(0);
    }

    struct stat sb;
    if (stat(outputPath.c_str(), &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        mkdir(outputPath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    }

    std::stringstream idMapFile;
    idMapFile << outputPath << "/id_mapping_" << runID << ".nc";
    m->saveNodeIdMapping(idMapFile.str());

    std::stringstream hydroMappingFile;
    hydroMappingFile << outputPath << "/hydro_mapping_" << runID << ".csv";
    m->saveHydroMapping(hydroMappingFile.str());

    std::stringstream ss;
    ss << outputPath << "/output_" << runID << ".nc";
    std::cout << "Sample data will be saved to " << ss.str() << std::endl;

    void (*prevHandler)(int);
    prevHandler = signal(SIGINT, handleInterrupt);
    double totalElapsed = 0.0;
    const long TOTAL_STEPS = 166*24;
    while (m->time < TOTAL_STEPS) {
        auto start = std::chrono::steady_clock::now();
        m->masterUpdate();
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end-start).count();
        totalElapsed += elapsed;
        double remaining = (totalElapsed/((double) m->time)) * (double) (TOTAL_STEPS - m->time);
        std::string remainingStr = "";
        if (remaining > 60*60) {
            int hrs = floor(remaining/(60*60));
            remainingStr += std::to_string(hrs)+"h";
            remaining -= hrs*60*60;
        }
        if (remaining > 60) {
            int min = floor(remaining/60);
            remainingStr += std::to_string(min)+"m";
            remaining -= min*60;
        }
        int sec = floor(remaining);
        remainingStr += std::to_string(sec)+"s";

        if (halt) {
            halt = 0;
            std::cout << std::endl << "Interrupted at step " << m->time << "; " << totalElapsed << "s elapsed since start" << std::endl;
            bool shouldExit = acceptCommand(m);
            if (shouldExit) {
                return 0;
            }
        }
        if (m->time % 330 == 0) {
            std::cout << "\rStep " << m->time << ": " << elapsed << "s elapsed; " << remainingStr << " remaining; " << m->livingIndividuals.size() << " living fish; " << m->exitedCount << " exited; " << m->deadCount << " dead";
            std::cout.flush();
            std::cout << std::endl << "Writing intermediary file at step: " << m->time << std::endl;
            std::stringstream interruptfile;
            interruptfile << outputPath << "/run_" << runID <<"_step_" << m->time << ".nc";
            m->saveState(interruptfile.str());
        }
    }

    std::cout << std::endl << "Finished at step " << m->time << "; " << totalElapsed << "s elapsed since start" << std::endl;

    std::stringstream ss2;
    ss2 << outputPath << "/summary_" << runID << ".nc";

    m->saveSummary(ss2.str());
    //std::cout << "Summary statistics saved to summary.nc" << std::endl;
    m->saveSampleData(ss.str());

    std::stringstream th;
    th << outputPath << "/taggedhist_" << runID << ".nc";
    m->saveTaggedHistories(th.str());
    delete m;
}
