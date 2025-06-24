#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <filesystem>
#include <string>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sstream>
#include <cstdlib>
#include <iomanip>
#include <array>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h> // unlink
#include "matplotlibcpp.h"

namespace fs = std::filesystem;
namespace plt = matplotlibcpp;

using namespace std;
using namespace chrono;

// RAM disk size in megabytes
constexpr int RAMDISK_SIZE_MB = 2047;
const string VOLUME_NAME = "RAMDisk";
const string MOUNT_PATH = "/Volumes/" + VOLUME_NAME;

// Get the device ID for the RAM disk (e.g., /dev/disk3)
string create_ramdisk_device(int size_mb, bool verbose)
{
    int blocks = size_mb * 1024 * 1024 / 512;
    string cmd = "hdiutil attach -nomount ram://" + to_string(blocks);

    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        return "";

    char buffer[128];
    string result;
    while (fgets(buffer, sizeof(buffer), pipe))
    {
        result += buffer;
    }

    pclose(pipe);
    // Remove trailing spaces and newlines
    result.erase(result.find_last_not_of(" \n\r\t") + 1);
    if (verbose)
    {
        cout << "Created RAM disk device: " << result << endl;
    }
    // Ensure the result is a valid device path
    if (result.empty() || result.find("/dev/disk") == string::npos)
    {
        return "";
    }

    return result;
}

// Format and mount the RAM disk as a volume
bool mount_ramdisk(const string &device, const string &volume_name)
{
    string cmd = "diskutil erasevolume HFS+ " + volume_name + " " + device +
    " >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

// Unmount and eject the RAM disk
bool eject_ramdisk(const string &mount_path)
{
    string cmd = "diskutil eject \"" + mount_path + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

// Check if the RAM disk exists in /Volumes
bool is_ramdisk_mounted(const string &mount_path)
{
    return access(mount_path.c_str(), F_OK) == 0;
}

constexpr int FAILED_CREATE = 1;  // Failed to create RAM disk device
constexpr int FAILED_MOUNT = 2;   // Failed to mount RAM disk
constexpr int DISK_NOT_FOUND = 3; // RAM disk not found in /Volumes

int create_ramdisk(bool verbose)
{
    if (verbose)
    {
        cout << "Creating RAM disk..." << endl;
    }
    // create a device in RAM
    string device = create_ramdisk_device(RAMDISK_SIZE_MB, verbose);
    if (device.empty())
    {
        return FAILED_CREATE;
    }
    if (verbose)
    {
        cout << "Mounting RAM disk..." << endl;
    }
    if (!mount_ramdisk(device, VOLUME_NAME))
    {
        return FAILED_MOUNT;
    }
    // Wait a moment for the volume to appear in /Volumes
    for (int i = 0; i < 10; ++i)
    {
        if (is_ramdisk_mounted(MOUNT_PATH))
            break;
        usleep(100000); // 100 ms
    }
    // Check if the RAM disk is mounted
    if (!is_ramdisk_mounted(MOUNT_PATH))
    {
        return DISK_NOT_FOUND;
    }
    if (verbose)
    {
        cout << "RAM disk mounted at path: " << MOUNT_PATH << endl;
    }
    return 0;
}

constexpr int ERROR_EJECTING = 1;       // Error ejecting RAM disk
constexpr int RAMDISK_STILL_EXISTS = 2; // RAM disk still exists in /Volumes

int unmount_ramdisk(bool verbose) {
    // Unmount the RAM disk
    if (verbose) {
        cout << "Ejecting RAM disk..." << endl;
    }
    if (eject_ramdisk(MOUNT_PATH)) {
        if (verbose) {
            cout << "RAM disk ejected." << endl;
        }
    }
    else {
        return ERROR_EJECTING;
    }
    // Check if the RAM disk was successfully ejected
    if (is_ramdisk_mounted(MOUNT_PATH)) {
        return RAMDISK_STILL_EXISTS;
    }
    if (verbose) {
        cout << "RAM disk successfully removed." << endl;
    }
    return 0;
}

enum class Func { ReadWrite, FStream, MMap };

struct Config {
    size_t minSize = 1024 * 1024;      // 1M
    size_t maxSize = 10 * 1024 * 1024; // 10M
    size_t strideSize = 1024 * 1024;   // 1M
    size_t bufferSize = 1024 * 1024;   // 1M
    int iterations = 1;
    bool useRamDisk = false;
    bool plotGraph = false;
    Func function = Func::ReadWrite;
};

void printHelp(const char *programName) {
    cout << "Usage: " << programName << " [options]\n"
         << "Options:\n"
         << "  -min=<size[KMG]>, --min=<size[KMG]>  Minimum file size (default: 1M)\n"
         << "  -max=<size[KMG]>, --max=<size[KMG]>  Maximum file size (default: 10M)\n"
         << "  -s=<size[KMG]>, --s=<size[KMG]>      Stride size (default: 1M)\n"
         << "  -buf=<size[KMG]>, --buf=<size[KMG]>  Memory buffer size (default: 1M)\n"
         << "  -f=<[rw|mm|fs]>, --f=<[rw|mm|fs]>    Functions for file operation:\n"
         << "      rw - read/write\n"
         << "      mm - mmap\n"
         << "      fs - fstream\n"
         << "  -n=<N>, --n=<N>                      Number of iterations (default: 1)\n"
         << "  -r, --r                              Use RAM disk (default: not used)\n"
         << "  -p, --plot                           Enable plotting graph (default: off)\n"
         << "  -h, --help                           Show this help message and exit\n";
}

void plotGraph(vector<double> &sizes_mb, vector<double> &write_speeds,
    vector<double> &read_speeds, string &filename) {
    // Draw a plot
    plt::figure_size(800, 600);
    plt::named_plot("Write", sizes_mb, write_speeds, "r-");
    plt::named_plot("Read", sizes_mb, read_speeds, "b-");
    plt::xlabel("File size (MB)");
    plt::ylabel("Speed (MB/s)");
    plt::title("Disk Write/Read Speed vs File Size");
    plt::legend();
    plt::grid(true);
    // find maximum
    double max_write = *std::max_element(write_speeds.begin(), write_speeds.end());
    double max_read = *std::max_element(read_speeds.begin(), read_speeds.end());
    double y_max = std::max(max_write, max_read) * 1.1; // add some space on the top
    // set limits for Y axis from 0 to y_max
    plt::ylim(0.0, y_max);
    plt::save("speed_graph.png");
}

Func parseFunc(const string &str) {
    if (str.empty())
        throw invalid_argument("Empty function string");
    if (str == "rw")
        return Func::ReadWrite;
    if (str == "mm")
        return Func::MMap;
    if (str == "fs")
        return Func::FStream;
    throw invalid_argument("Wrong function string");
}

size_t parseSize(const string &str) {
    if (str.empty())
        throw invalid_argument("Empty size string");

    size_t multiplier = 1;
    char suffix = str.back();
    string numberPart = str;

    if (suffix == 'K' || suffix == 'M' || suffix == 'G') {
        numberPart = str.substr(0, str.size() - 1);
        switch (suffix) {
        case 'K':
            multiplier = 1024ULL;
            break;
        case 'M':
            multiplier = 1024ULL * 1024;
            break;
        case 'G':
            multiplier = 1024ULL * 1024 * 1024;
            break;
        }
    }

    size_t number;
    istringstream(numberPart) >> number;
    return number * multiplier;
}

string formatSize(size_t bytes) {
    ostringstream oss;
    oss << bytes << " B";

    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;

    oss << " (";
    bool first = true;

    if (gb >= 1.0) {
        oss << fixed << setprecision(2) << gb << " GiB";
        first = false;
    }
    if (mb >= 1.0 && mb < 1024.0) {
        if (!first)
            oss << ", ";
        oss << fixed << setprecision(2) << mb << " MiB";
        first = false;
    }
    if (kb >= 1.0 && kb < 1024.0) {
        if (!first)
            oss << ", ";
        oss << fixed << setprecision(2) << kb << " KiB";
    }

    oss << ")";
    return oss.str();
}

Config parseArgs(int argc, char *argv[]) {
    Config config;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "-r" || arg == "--r") {
            config.useRamDisk = true;
        }
        else if (arg == "-p" || arg == "--plot") {
            config.plotGraph = true;
        }
        else if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            exit(0);
        }
        else if (arg.find("-min=") == 0) {
            config.minSize = parseSize(arg.substr(5));
        }
        else if (arg.find("--min=") == 0) {
            config.minSize = parseSize(arg.substr(6));
        }
        else if (arg.find("-max=") == 0) {
            config.maxSize = parseSize(arg.substr(5));
        }
        else if (arg.find("--max=") == 0) {
            config.maxSize = parseSize(arg.substr(5));
        }
        else if (arg.find("-s=") == 0) {
            config.strideSize = parseSize(arg.substr(3));
        }
        else if (arg.find("--s=") == 0) {
            config.strideSize = parseSize(arg.substr(4));
        }
        else if (arg.find("-buf=") == 0) {
            config.bufferSize = parseSize(arg.substr(5));
        }
        else if (arg.find("--buf=") == 0) {
            config.bufferSize = parseSize(arg.substr(6));
        }
        else if (arg.find("-n=") == 0) {
            config.iterations = stoi(arg.substr(3));
        }
        else if (arg.find("--n=") == 0) {
            config.iterations = stoi(arg.substr(4));
        }
        else if (arg.find("-f=") == 0) {
            config.function = parseFunc(arg.substr(3));
        }        
        else if (arg.find("--f=") == 0) {
            config.function = parseFunc(arg.substr(4));
        }
        else {
            cerr << "Unknown argument: " << arg << "\n";
            printHelp(argv[0]);
            exit(1);
        }
    }

    if (config.minSize > config.maxSize) {
        throw invalid_argument("minSize cannot be greater than maxSize");
    }
    if (config.strideSize > (config.maxSize - config.minSize)) {
        throw invalid_argument("strideSize cannot be greater than (maxSize -"
            " minSize)");
    }

    return config;
}

void fs_write_file(const string &filename, const vector<unsigned char> &buffer,
    const size_t size_bytes) {
    ofstream file(filename, ios::binary);
    size_t written = 0;
    while (written < size_bytes) {
        size_t to_write = min(buffer.size(), size_bytes - written);
        file.write(reinterpret_cast<const char *>(buffer.data()), to_write);
        written += to_write;
    }
    file.close();
}

uint8_t fs_read_file(const string &filename, vector<unsigned char> &buffer,
    const size_t size_bytes) {
    ifstream file(filename, ios::binary);
    uint8_t checksum = 0;
    size_t read = 0;
    while (read < size_bytes) {
        size_t to_read = min(buffer.size(), size_bytes - read);
        file.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
        read += to_read;
        for(size_t i = 0; i < buffer.size(); i++) {
            checksum ^= buffer[i];
        }
    }
    file.close();
    return checksum;
}

void mm_write_file(const string &filename, vector<unsigned char> &buffer,
    const size_t size_bytes) {
    // MMap write test
    // Open a file ans set zero size
    int fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        throw string_view("mm_write_file/open");
    }

    // Caching off
    if (fcntl(fd, F_NOCACHE, 1) < 0) {
        perror("fcntl F_NOCACHE");
        throw string_view("mm_write_file/fcntl");
    }

    if (ftruncate(fd, size_bytes) != 0) {
        perror("ftruncate");
        close(fd);
        throw string_view("mm_write_file/ftruncate");;
    }
    // mmap
    void* map = mmap(nullptr, size_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        throw string_view("mm_write_file/mmap");;
    }
    size_t count = size_bytes / buffer.size();
    // --- Write + msync ---
    for (size_t i = 0; i < count; ++i) {
        uint8_t* curr_p = static_cast<uint8_t*>(map) + i * buffer.size();
        std::memcpy(curr_p, buffer.data(), buffer.size());
        // Synchronizing
        // it is possible to use MS_ASYNC
        if (msync(curr_p, buffer.size(), MS_SYNC) != 0) {
            perror("msync");
            throw string_view("mm_write_file/msync");;
        }
    }
    munmap(map, size_bytes);
    close(fd);
}

uint8_t mm_read_file(const string &filename, vector<unsigned char> &buffer,
    const size_t size_bytes) {
    // --- MMap Read ---
    // Open a file
    int fd = open(filename.c_str(), O_RDONLY, 0644);
    if (fd < 0) {
        perror("open");
        throw string_view("mm_write_file/open");;
    }
    // Caching off
    if (fcntl(fd, F_NOCACHE, 1) < 0) {
        perror("fcntl F_NOCACHE");
        throw string_view("mm_write_file/fcntl");
    }
    // mmap
    void* map = mmap(nullptr, size_bytes, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        throw string_view("mm_write_file/mmap");
    }
    size_t count = size_bytes / buffer.size();
    uint8_t checksum = 0;

    for (size_t i = 0; i < count; i++) {
        uint8_t* ptr = static_cast<uint8_t*>(map) + i * buffer.size();
        for (size_t j = 0; j < buffer.size(); j++) {
            checksum ^= ptr[j];
        }
    }
    munmap(map, size_bytes);
    close(fd);
    return checksum;
}

void rw_write_file(const string &filename, vector<unsigned char> &buffer,
    const size_t size_bytes) {

    // ==== Write ====
    int fd = open(filename.c_str(), O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        perror("open write");
        throw string_view("rw_write_file/open");
    }

    // Disable caching on macOS
    fcntl(fd, F_NOCACHE, 1);

    for (size_t written = 0; written < size_bytes; written += buffer.size()) {
        if (write(fd, buffer.data(), buffer.size()) != buffer.size()) {
            perror("write");
            close(fd);
            throw string_view("rw_write_file/write");
        }
    }
    fsync(fd);
    close(fd);
}

unsigned char rw_read_file(const string &filename, vector<unsigned char> &buffer,
const size_t size_bytes) {
    // ==== Read ====
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("open read");
        throw string_view("rw_read_file/open");
    }

    fcntl(fd, F_NOCACHE, 1);
    unsigned char sink = 0;
    for (size_t read_bytes = 0; read_bytes < size_bytes;
        read_bytes += buffer.size()) {
        if (read(fd, buffer.data(), buffer.size()) != buffer.size()) {
            perror("read");
            close(fd);
            throw string_view("rw_read_file/read");
        }
        sink ^= buffer[0];
    }
    close(fd);
    return sink;
}

void cleanup(vector<string>& filenames) {
    for (const auto &filename : filenames) {
        unlink(filename.c_str());
    }
}

string_view func_name(Func func) {
    switch(func) {
        case Func::ReadWrite: return "read/write";
        case Func::FStream: return "fstream";
        case Func::MMap: return "mmap";
    }
}

int main(int argc, char *argv[]) {
    try {
        Config config = parseArgs(argc, argv);
        string_view no_yes[] = {"no", "yes"};
        cout << "Configuration:\n";
        cout << "  Functoins:     " << func_name(config.function) << '\n';
        cout << "  Min size:      " << formatSize(config.minSize) << '\n';
        cout << "  Max size:      " << formatSize(config.maxSize) << '\n';
        cout << "  Stride size:   " << formatSize(config.strideSize) << '\n';
        cout << "  Memory buffer: " << formatSize(config.bufferSize) << '\n';
        cout << "  Iterations:    " << config.iterations << '\n';
        cout << "  Use RAM disk:  " << no_yes[config.useRamDisk] << '\n';
        cout << "  Plot graph:    " << no_yes[config.plotGraph] << '\n';

        bool verbose = false;
        // if RAM disk is enabled, mount RAM disk
        string mount_path;
        if (config.useRamDisk) {
            int err = create_ramdisk(verbose);
            switch (err) {
            case FAILED_CREATE: // Failed to create RAM disk device
                cerr << "Failed to create RAM disk device!" << endl;
                return 1;
            case FAILED_MOUNT: // Failed to mount RAM disk
                cerr << "Failed to mount RAM disk!" << endl;
                return 1;
            case DISK_NOT_FOUND: // RAM disk not found in /Volumes
                cerr << "RAM disk not found in /Volumes!" << endl;
                return 1;
            }
            mount_path = MOUNT_PATH;
        }
        else {
            mount_path = ".";
        }
        // RAM disk mounted
        //---------------------------------------
        vector<double> sizes_mb;
        vector<double> write_speeds;
        vector<double> read_speeds;

        auto test_write = array<function<void(const string&,
        vector<unsigned char>&, const size_t)>, 3>{rw_write_file, fs_write_file,
            mm_write_file}[int(config.function)];

        auto test_read = array<function<uint8_t(const string&,
            vector<unsigned char>&, const size_t)>, 3>{rw_read_file, fs_read_file,
                mm_read_file}[int(config.function)];

        for (size_t size_bytes = config.minSize; size_bytes <= config.maxSize;
            size_bytes += config.strideSize) {
                vector<string> filenames;
                filenames.reserve(config.iterations);
                size_t size_mb = size_bytes / (1 << 20);
                time_point<high_resolution_clock> end_write,
                    start_write,
                    end_read,
                    start_read;
                try
                {
                    // Write test
                    vector<unsigned char> wr_buffer(config.bufferSize);
                    memset(wr_buffer.data(), 0x55, config.bufferSize);
                    start_write = high_resolution_clock::now();
                    try {
                        for (int i = 0; i < config.iterations; ++i) {
                            string filename = mount_path + "/testfile_" + to_string(i) +
                            ".bin";
                            cout << "filename: " << filename << "\n";
                            test_write(filename, wr_buffer, size_bytes);
                            filenames.push_back(filename);
                        }
                    }
                    catch (string_view msg) {
                        cout << "Error ocuured at " << msg << endl;
                        throw 0;
                    }
                    end_write = high_resolution_clock::now();

                    // Read test
                    vector<unsigned char> rd_buffer(config.bufferSize);

                    unsigned char sink = 0;
                    start_read = high_resolution_clock::now();
                    for (const auto &filename : filenames) {
                        // to prevent an optimization
                        sink ^= test_read(filename, rd_buffer, size_bytes);
                    }
                    end_read = high_resolution_clock::now();
                    cout << "sink = " << (int)sink << endl;
                }
                catch (...) {
                    cleanup(filenames);
                    exit(1);
                }
            cleanup(filenames);

            duration<double> write_duration = end_write - start_write;
            duration<double> read_duration = end_read - start_read;

            double total_mb = static_cast<double>(size_mb * config.iterations);
            double write_speed = total_mb / write_duration.count();
            double read_speed = total_mb / read_duration.count();

            sizes_mb.push_back(static_cast<double>(size_mb));
            write_speeds.push_back(write_speed);
            read_speeds.push_back(read_speed);

            cout << "Size: " << size_mb << " MB | Write: " << write_speed <<
            " MB/s | Read: " << read_speed << " MB/s\n";
        }

        if (config.plotGraph) {
            string filename("speed_graph.png");
            plotGraph(sizes_mb, write_speeds, read_speeds, filename);
            cout << "The plot saved in " << filename << endl;
        }

        if (config.useRamDisk) {
            int err = unmount_ramdisk(verbose);
            switch (err) {
            case ERROR_EJECTING: // Error ejecting RAM disk
                cerr << "Error ejecting RAM disk" << endl;
                return 1;
            case RAMDISK_STILL_EXISTS: // RAM disk still exists in /Volumes
                cerr << "RAM disk still exists in /Volumes" << endl;
                return 1;
            }
        }
    }
    catch (const exception &e) {
        cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
