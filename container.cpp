#include <algorithm> // std::find
#include <iostream> // std::cout, std::cerr
#include <fstream> // std::ifstream
#include <filesystem> // std::filesystem
#include <map> // std::map

#include <sched.h> // clone
#include <sys/types.h> // waitpid
#include <sys/wait.h> // waitpid, SIGCHIL
#include <sys/mount.h> // mount, umonut
#include <unistd.h> // execv, chdir, mkdtemp
#include <cstdlib> // mkdtemp
#include "container.hpp"

Container::Container(std::string manifest_name, int port) {
    std::map<std::string, std::string> configuration;
    
    std::ifstream manifest(manifest_name);
    if (manifest.is_open()) {
        std::string line;
        while(getline(manifest, line)){
            line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
            if(line[0] == '#' || line.empty())
		continue;
            auto delimiterPos = line.find("=");
            auto name = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);
	    configuration[name] = value;
        }
        
    } else {
        std::cerr << "Couldn't open manifest file: " << manifest_name << ".\n";
	exit(EXIT_FAILURE);
    }
    
    clone_args.executable_name = configuration["executable"];
    clone_args.directory_path = configuration["dirname"];
    clone_args.mount = configuration["mount"] == "true" ? true : false;
    clone_args.direct = configuration["direct"] == "true" ? true : false;
    clone_args.port = port;
}

void Container::run() {
    char* stack = static_cast<char*>(malloc(4096)) + 4096;
    int namespaces = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWNS;// | CLONE_NEWNET
    
    pid_t p = clone(internal_exec, stack, SIGCHLD|namespaces, &clone_args);
    
    if (p == -1) {
	std::cerr << "Error while cloning the container." << std::endl;
        exit(EXIT_FAILURE);
    }
    
    std::cout << "Spawning a container (pid: " << p << ", executable name: \"" << clone_args.executable_name << "\", port: " << clone_args.port << ")." << std::endl;

    pid = p;
}

int Container::internal_exec(void* args) {
    clone_args_t* _clone_args = (clone_args_t*) args;
    // remove capabilities? or switch user
    
    if (!_clone_args->direct) {
	// non-direct container was requested
	// first, a temp directory will be created. then the container "image" will be mounted there using overlayfs
	std::string tmp_template = std::filesystem::temp_directory_path() / "qcontain_runXXXXXX";
	std::string workdir_template = std::filesystem::temp_directory_path() / "qcontain_workXXXXXX";
	
	char* tmpdir = mkdtemp(tmp_template.data());
	char* workdir = mkdtemp(workdir_template.data());
	
	std::string srcdir = std::filesystem::absolute(_clone_args->directory_path);
	
	if (NULL == tmpdir) {
	    std::cerr << "Cannot create a temporary directory." << std::endl;
	    exit(EXIT_FAILURE);
	}

	std::string mount_data = "lowerdir=" + srcdir + ",upperdir=" + tmpdir + ",workdir=" + workdir;
	if (mount("overlay", tmpdir, "overlay", 0, mount_data.data()) != 0) {
	    std::cerr << "Cannot mount read-only volume." << std::endl;
	    exit(EXIT_FAILURE);
	}
	
	if (chdir(tmpdir) != 0) {
	    std::cerr << "Cannot change directory to new root." << std::endl;
	    exit(EXIT_FAILURE);
	}
    } else {
	// if a direct container is not necessary, just go to the data directory
	if (chdir(_clone_args->directory_path.c_str()) != 0) {
	    std::cerr << "Cannot change directory to new root!" << std::endl;
	    exit(EXIT_FAILURE);
	}
    }
    
    // mount /dev and /proc if it was requested
    if (_clone_args->mount) {
	umount("./dev");
	umount("./proc");
    
	if ((mount("/dev", "./dev", NULL, MS_BIND, NULL) != 0) || (mount("proc", "./proc", "proc", 0, NULL))) {
	    std::cerr << "Cannot mount filesystems." << std::endl;
	    exit(EXIT_FAILURE);
	}
    }

    if (chroot(".") != 0) {
	std::cerr << "Cannnot chroot!" << std::endl;
	exit(EXIT_FAILURE);
    }
    
    const char* name = _clone_args->executable_name.c_str();
    const std::string port_str = std::to_string(_clone_args->port);
    
    char* const newargv[] = { (char*) port_str.c_str(), (char*) NULL };
    
    execv(name, newargv);
    std::cerr << "Starting a container failed." << std::endl;
    
    return 0;
}

void Container::wait() {
    waitpid(pid, NULL, 0);
}
