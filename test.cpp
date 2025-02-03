#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>
#include <cstring>
#include <atomic>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cmath>

template <typename T>
class shared_memory {
public:
    struct shm_header {
        std::atomic<int> n; // length of shared_memory
        double start_ts;    // start timestamp
        double interval;    // interval between timestamps
        int limit;          // limit on number of elements
    };

    constexpr static double EPS = 1e-6;

public:
    static std::unique_ptr<shared_memory> create(const char* shm_name, int n = (1 << 15), double interval = 1.0) {
        size_t file_size = sizeof(shm_header) + sizeof(T) * n;

        // Try to open the POSIX shared memory object first to check if it exists
        int shm_fd = shm_open(shm_name, O_RDONLY, 0666);
        if (shm_fd != -1) {
            close(shm_fd);
            throw std::runtime_error("Shared memory already exists");
        }

        // If it does not exist, create it
        shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open failed");
            throw std::runtime_error("Failed to create shared memory");
        }

        // Set size of the shared memory object
        if (ftruncate(shm_fd, file_size) == -1) {
            perror("ftruncate failed");
            close(shm_fd);
            throw std::runtime_error("Failed to set size of shared memory");
        }

        // Map the shared memory object into the process's address space
        void* mem = mmap(0, file_size, PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (mem == MAP_FAILED) {
            perror("mmap failed");
            close(shm_fd);
            throw std::runtime_error("Failed to map shared memory");
        }
        close(shm_fd); // No longer needed after mapping

        shm_header* header = static_cast<shm_header*>(mem);
        header->n = 0;
        header->start_ts = 0;
        header->interval = interval;
        header->limit = n;

        std::cout << header->n << ' ' << header->start_ts << ' ' << header->interval << ' ' << header->limit << '\n';

        return std::make_unique<shared_memory>(shm_name, mem, file_size);
    }

    void append(const T& data) {
        T* array = get_header();

        if (header_->n == 0) {
            header_->start_ts = data.ts();
        } else if (fabs(array[header_->n - 1].ts() + header_->interval - data.ts()) > EPS) {
            throw std::runtime_error("There is a gap between new data and historical data.");
        } else if (header_->n + 1 == header_->limit) {
            throw std::runtime_error("You need to expand the shm.");
        }

        array[header_->n] = data;
        header_->n++;
    }

    int get_index(double timestamp) const {
        if (header_->start_ts == 0) {
            throw std::runtime_error("This is an empty shm.");
        }
        return static_cast<int>((timestamp - header_->start_ts) / header_->interval);
    }

    T read(size_t index) const {
        if (index >= header_->n || index < 0) {
            throw std::out_of_range("Index out of range");
        }

        const T* array = get_header();
        return array[index];
    }

    ~shared_memory() {
        if (mem_) {
            munmap(mem_, file_size_);
            shm_unlink(shm_name_); // Remove the shared memory segment when done
        }
    }

    shared_memory(const char* shm_name, void* mem, size_t file_size)
        : shm_name_(shm_name), mem_(mem), file_size_(file_size), header_(static_cast<shm_header*>(mem)) {}

private:
    const char* shm_name_;
    void* mem_;
    size_t file_size_;
    shm_header* header_;

    T* get_header() {
        return reinterpret_cast<T*>(static_cast<char*>(mem_) + sizeof(shm_header));
    }

    const T* get_header() const {
        return reinterpret_cast<const T*>(static_cast<const char*>(mem_) + sizeof(shm_header));
    }
};

struct data {
    double ts_;
    int v_[10];

    double ts() const { return ts_; }
};

int main() {
    try {
        auto shm = std::move(shared_memory<data>::create("/test_shm"));
        std::string opt;
        while (std::cin >> opt) {
            if (opt == "break") {
                break;
            } else if (opt == "append") {
                double ts;
                char v[10];
                std::cin >> ts >> v;
                shm->append(data{ts, v});
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}