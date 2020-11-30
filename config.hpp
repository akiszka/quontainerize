#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <map> // std::map
#include <string> // std::string

class Configuration {
    std::map<std::string, int> configuration;
    
    int config_read(std::string filename);
    int config_check_range();
public:
    Configuration(std::string filename);

    int& operator[](std::string index);
    const int& operator[](std::string index) const = delete;
};

#endif // CONFIG_HPP
