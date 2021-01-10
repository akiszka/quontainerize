#include <string> //std::string
#include <fstream> // std::ifstream
#include <algorithm> // std::remove_if
#include <iostream> // std::cerr

#include "config.hpp"

int Configuration::config_read(std::string filename) {
    std::ifstream cFile(filename);
    if (cFile.is_open()) {
        std::string line;
        while(getline(cFile, line)){
            line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
            if(line[0] == '#' || line.empty())
		continue;
            auto delimiterPos = line.find("=");
            auto name = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);
	    configuration[name] = std::stoi(value);
        }
        
    }
    else {
        std::cerr << "Couldn't open config file for reading.\n";
	return -1;
    }
    return 0;
}

int Configuration::config_check_range() {
    int range_begin = configuration["range_begin"];
    int range_end = configuration["range_end"];
    int discovery_port = configuration["discovery_port"];

    if (range_begin > range_end) {
	std::cerr << "The port range for containers is incorrect." << std::endl;
	return -1;
    }
    else if (discovery_port >= range_begin && discovery_port <= range_end) {
	std::cerr << "The discovery port must not be in the container port range." << std::endl;
	return -1;
    }
    
    return 0;
}

Configuration::Configuration(std::string filename) {
    if (config_read(filename) != 0) exit(EXIT_FAILURE);
    if (config_check_range() != 0) exit(EXIT_FAILURE);
}

int& Configuration::operator[](std::string index) {
    return configuration[index];
}
