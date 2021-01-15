#include <iostream> // std::cout
#include <filesystem> // std::filesystem::path
#include "CLI11.hpp"
#include "container.hpp"

int main(int argc, char** argv) {
    CLI::App mainapp{"The Quontainerize container engine"};
    mainapp.require_subcommand(1);

    // ---- RUN SUBCOMMAND
    
    CLI::App* run = mainapp.add_subcommand("run", "Run a container");

    std::string image;
    run->add_option("image", image, "The image to execute");
    
    std::vector<std::string> command;
    run->add_option("command", command, "The command to run");

    bool mount{true};
    run->add_flag("--mount,!--no-mount", mount, "Mount /proc, /dev and /sys");
    
    run->callback([&]() {
	std::filesystem::path image_path("./containers");
	image_path /= image;
	Container container(command, image_path.string(), mount);
	container.run();
	container.wait();
    });
    
    // ---- IMAGES SUBCOMMAND
    
    CLI::App* images = mainapp.add_subcommand("images", "List all container images");

    images->callback([&]() {
	std::cout << "images" << std::endl;
    });
    
    CLI11_PARSE(mainapp, argc, argv);
    
    return 0;
}
