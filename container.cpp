#include <algorithm> // std::find
#include <iostream> // std::cout, std::cerr
#include <fstream> // std::ifstream
#include <filesystem> // std::filesystem
#include <map> // std::map

#include <sched.h> // clone
#include <sys/types.h> // waitpid, gete{uid,gid}
#include <sys/wait.h> // waitpid, SIGCHIL
#include <sys/mount.h> // mount, umonut
#include <fcntl.h> // openat, AT_FWCWD
#include <unistd.h> // execve, chdir, mkdtemp, gete{uid,gid}, sethostname
#include <sys/syscall.h> // For SYS_xxx definitions
#include <cstdlib> // mkdtemp, system
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
    clone_args.port = port;
    clone_args.euid = geteuid();
    clone_args.egid = getegid();
}

void Container::run() {
    char* stack = static_cast<char*>(malloc(4096)) + 4096;
    int namespaces = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWNS | CLONE_NEWUSER;// | CLONE_NEWNET
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
    
    // map the current user and group to root
    {
	std::string uid_map = "0 " + std::to_string(_clone_args->euid) + " 1";
	std::string gid_map = "0 " + std::to_string(_clone_args->egid) + " 1";

	int fd = openat(AT_FDCWD, "/proc/self/uid_map", O_WRONLY);
	if (write(fd, uid_map.data(), uid_map.length()) != uid_map.length()) {
	    std::cerr << "Cannnot set uid map!" << std::endl;
	    exit(EXIT_FAILURE);
	}
	close(fd);

	fd = openat(AT_FDCWD, "/proc/self/setgroups", O_WRONLY);
	if (write(fd, "deny", 4) != 4) {
	    std::cerr << "Cannnot set gid map (stage 1)!" << std::endl;
	    exit(EXIT_FAILURE);
	}
	close(fd);
	
	fd = openat(AT_FDCWD, "/proc/self/gid_map", O_WRONLY);
	if (write(fd, gid_map.data(), gid_map.length()) != gid_map.length()) {
	    std::cerr << "Cannnot set gid map (stage 2)!" << std::endl;
	    exit(EXIT_FAILURE);
	}
	close(fd);
    }

    // first, a temp directory will be created. then the container "image" will be mounted there using fuse-overlayfs. this directory the container's rootfs.
    {
	std::string upperdir_template = std::filesystem::temp_directory_path() / "quont_runXXXXXX";
	std::string workdir_template = std::filesystem::temp_directory_path() / "quont_workXXXXXX";
	
	char* upperdir = mkdtemp(upperdir_template.data());
	char* workdir = mkdtemp(workdir_template.data());
	
	std::string srcdir = _clone_args->directory_path;

	if (nullptr == upperdir || nullptr == workdir) {
	    std::cerr << "Cannot create a temporary directory." << std::endl;
	    exit(EXIT_FAILURE);
	}

	std::string mount_data = "lowerdir=" + srcdir + ",upperdir=" + upperdir + ",workdir=" + workdir;
	std::string command = "bash -c '";
	command += "fuse-overlayfs -o " + mount_data + " " + upperdir + "'";

	if (system(command.data()) != 0) { // this is disgusting, i'm probably going to hell for this
	    std::cerr << "Cannot mount read-only volume." << std::endl;
	    exit(EXIT_FAILURE);
	}
	
	if (chdir(upperdir) != 0) {
	    std::cerr << "Cannot change directory to new root." << std::endl;
	    exit(EXIT_FAILURE);
	}
    }

    // mount /proc if it was requested
    // TODO: mount /dev and /sys (possibly filtered)
    if (_clone_args->mount) {
	if (mount("proc", "./proc", "proc", 0, NULL) != 0) {
	    std::cerr << "Cannot mount filesystems." << std::endl;
	    exit(EXIT_FAILURE);
	}
    }

    // change the fs root to the current directory (this prevents chroot escape, see: pivot_root(2) manpage)
    if (syscall(SYS_pivot_root, ".", ".") != 0 ||
	umount2(".", MNT_DETACH) != 0) {
	std::cerr << "Cannnot pivot root!" << std::endl;
	exit(EXIT_FAILURE);
    }
    
    if (chroot(".") != 0) {
	std::cerr << "Cannnot chroot!" << std::endl;
	exit(EXIT_FAILURE);
    }

    // set the hostname
    {
	std::string hostname = "qont";
	sethostname(hostname.data(), hostname.length());
    }
    
    // finally, run the desired program within the container
    {
	const char* name = _clone_args->executable_name.c_str();
	const std::string port_str = std::to_string(_clone_args->port);
    
	char* const newargv[] = { (char*) port_str.data(), NULL };
	char* const env[] = { NULL };

	execve(name, newargv, env);
	std::cerr << "Starting the contained program failed." << std::endl;
    }
    return 0;
}

void Container::wait() {
    waitpid(pid, NULL, 0);
}
