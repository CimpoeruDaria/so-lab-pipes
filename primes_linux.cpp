#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

bool is_prime(int n) {
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

void run_child(int read_fd, int write_fd) {
    int range[2];
    if (read(read_fd, range, sizeof(range)) > 0) {
        int start = range[0];
        int end = range[1];
        for (int i = start; i <= end; ++i) {
            if (is_prime(i)) {
                write(write_fd, &i, sizeof(int));
            }
        }
    }
    int sentinel = -1;
    write(write_fd, &sentinel, sizeof(int));
    close(read_fd);
    close(write_fd);
}

int main() {
    const int NUM_PROCESSES = 10;
    const int CHUNK_SIZE = 1000;
    
    int p2c[NUM_PROCESSES][2];
    int c2p[NUM_PROCESSES][2];
    pid_t pids[NUM_PROCESSES];

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        if (pipe(p2c[i]) < 0 || pipe(c2p[i]) < 0) {
            perror("Eroare la crearea pipe-urilor");
            return 1;
        }

        pids[i] = fork();
        if (pids[i] < 0) {
            perror("Eroare la fork");
            return 1;
        }

        if (pids[i] == 0) {
            close(p2c[i][1]);
            close(c2p[i][0]);
            run_child(p2c[i][0], c2p[i][1]);
            _exit(0);
        } else {
            close(p2c[i][0]);
            close(c2p[i][1]);
            int range[2] = { i * CHUNK_SIZE + 1, (i + 1) * CHUNK_SIZE };
            write(p2c[i][1], range, sizeof(range));
            close(p2c[i][1]);
        }
    }

    int total_primes = 0;
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        std::cout << "\n--- Rezultate Proces Copil " << i 
                  << " (" << i * CHUNK_SIZE + 1 << " - " << (i + 1) * CHUNK_SIZE << ") ---\n";
        int val;
        while (read(c2p[i][0], &val, sizeof(int)) > 0) {
            if (val == -1) break;
            std::cout << val << " ";
            total_primes++;
        }
        std::cout << "\n";
        close(c2p[i][0]);
    }

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        waitpid(pids[i], nullptr, 0);
    }

    std::cout << "\nTotal numere prime gasite (1 - 10000): " << total_primes << " (Asteptat: 1229)\n";
    return 0;
}
