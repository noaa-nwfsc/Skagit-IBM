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
    unsigned long runID;
    int status;
    float mortConstA;
    float mortConstC;
    runListingEntry(unsigned long runID, int status, float mortConstA, float mortConstC)
        : runID(runID), status(status), mortConstA(mortConstA), mortConstC(mortConstC) {}
};

bool readRunListings(int runListingsFd, std::string& header, std::vector<struct runListingEntry>& runListings) {
    runListings.clear();
    header.clear();

    if (lseek(runListingsFd, 0, SEEK_SET) == -1) {
        std::cerr << "Error seeking to beginning of run listing file: " << strerror(errno) << std::endl;
        return false;
    }

    struct stat st;
    if (fstat(runListingsFd, &st) == -1 || st.st_size == 0) {
        std::cerr << "Error reading run listing file or file is empty" << std::endl;
        return false;
    }

    std::string fileContent(st.st_size, '\0');
    ssize_t bytesRead = read(runListingsFd, &fileContent[0], st.st_size);

    if (bytesRead != st.st_size) {
        std::cerr << "Error reading complete run listing file" << std::endl;
        return false;
    }

    std::istringstream is(fileContent);

    std::string line;
    bool first = true;
    while (std::getline(is, line) && !line.empty()) {
        if (first) {
            first = false;
            header = line;
            continue;
        }
        std::vector<std::string> chunks = split(line, ',');
        if (chunks.size() != 4) {
            std::cerr << "Invalid CSV format in run listing file" << std::endl;
            return false;
        }

        try {
            unsigned long runID = std::stoi(chunks[0]);
            int status = std::stoi(chunks[1]);
            float mortConstA = std::stof(chunks[2]);
            float mortConstC = std::stof(chunks[3]);

            runListings.emplace_back(runID, status, mortConstA, mortConstC);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing run listing data: " << e.what() << std::endl;
            return false;
        }
    }
    return true;
}

bool writeRunListings(int runListingsFd, std::string& header, std::vector<struct runListingEntry>& runListings) {
    if (ftruncate(runListingsFd, 0) == -1) {
        std::cerr << "Error truncating run listing file" << std::endl;
        return false;
    }
    lseek(runListingsFd, 0, SEEK_SET);


    std::ostringstream ss;
    ss << header << std::endl;
    for (struct runListingEntry& entry : runListings) {
        ss << entry.runID << ',' << entry.status << ',' << entry.mortConstA << ',' << entry.mortConstC << std::endl;
    }

    std::string content = ss.str();
    ssize_t written = write(runListingsFd, content.c_str(), content.length());
    if (written != (ssize_t)content.length()) {
        std::cerr << "Error writing run listing file" << std::endl;
        return false;
    }
    return true;
}

int pickRun(int runListingsFd, Model *model) {
    lseek(runListingsFd, 0, SEEK_SET);

    std::vector<struct runListingEntry> runListings;
    std::string header;

    if (!readRunListings(runListingsFd, header, runListings)) {
        std::cerr << "Failed to read run listing file" << std::endl;
        return -1;
    }

    for (struct runListingEntry &entry : runListings) {
        if (entry.status == 0) {
            entry.status = 1;
            model->mortConstA = entry.mortConstA;
            model->mortConstC = entry.mortConstC;
            if (!writeRunListings(runListingsFd, header, runListings)) {
                std::cerr << "Failed to write updated run listing file" << std::endl;
                entry.status = 0;
                return -1;
            }

            std::cout << "Selected run ID: " << entry.runID << std::endl;
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
    int runListingFd = open(argv[1], O_RDWR);
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

    int runID = pickRun(runListingFd, m);

    flock(runListingFd, LOCK_UN);
    close(runListingFd);

    if (runID == -1) {
        std::cout << "No runs left in listing file, or other error encountered!" << std::endl;
        exit(0);
    }

    struct stat sb;
    if (stat(outputPath.c_str(), &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        mkdir(outputPath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    }

    // std::stringstream idMapFile;
    // idMapFile << outputPath << "/id_mapping_" << runID << ".nc";
    // m->saveNodeIdMapping(idMapFile.str());

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

#define QUICK_DEBUG_HACK
// #undef QUICK_DEBUG_HACK
#if defined(QUICK_DEBUG_HACK) && !defined(NDEBUG)
        if (m->time % 25 == 0) {
#else
        if (m->time % 330 == 0) {
#endif
            std::cout << "\rStep " << m->time << ": " << elapsed << "s elapsed; " << remainingStr << " remaining; " << m->livingIndividuals.size() << " living fish; " << m->exitedCount << " exited; " << m->deadCount << " dead" << std::endl;
            std::cout.flush();

#if !defined(QUICK_DEBUG_HACK) || defined(NDEBUG)
            std::cout << "Writing intermediary file at step: " << m->time << std::endl;
            std::stringstream interruptfile;
            interruptfile << outputPath << "/run_" << runID <<"_step_" << m->time << ".nc";
            m->saveState(interruptfile.str());
#endif
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
