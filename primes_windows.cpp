#include <windows.h>
#include <iostream>
#include <string>

bool is_prime(int n) {
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

void run_child(HANDLE hRead, HANDLE hWrite) {
    int range[2];
    DWORD bytesRead, bytesWritten;

    if (ReadFile(hRead, range, sizeof(range), &bytesRead, NULL) && bytesRead == sizeof(range)) {
        int start = range[0];
        int end = range[1];
        for (int i = start; i <= end; ++i) {
            if (is_prime(i)) {
                WriteFile(hWrite, &i, sizeof(int), &bytesWritten, NULL);
            }
        }
    }
    int sentinel = -1;
    WriteFile(hWrite, &sentinel, sizeof(int), &bytesWritten, NULL);
    CloseHandle(hRead);
    CloseHandle(hWrite);
}

int main(int argc, char* argv[]) {
    // Daca programul ruleaza in modul proces copil
    if (argc == 4 && std::string(argv[1]) == "--child") {
        HANDLE hRead = (HANDLE)(uintptr_t)std::stoull(argv[2]);
        HANDLE hWrite = (HANDLE)(uintptr_t)std::stoull(argv[3]);
        run_child(hRead, hWrite);
        return 0;
    }

    // Modul Parinte
    const int NUM_PROCESSES = 10;
    const int CHUNK_SIZE = 1000;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE; // Permite mostenirea pipe-urilor
    sa.lpSecurityDescriptor = NULL;

    HANDLE p2c_read[NUM_PROCESSES], p2c_write[NUM_PROCESSES];
    HANDLE c2p_read[NUM_PROCESSES], c2p_write[NUM_PROCESSES];
    PROCESS_INFORMATION pi[NUM_PROCESSES];

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        CreatePipe(&p2c_read[i], &p2c_write[i], &sa, 0);
        CreatePipe(&c2p_read[i], &c2p_write[i], &sa, 0);

        // Capetele neutilizate de copil nu trebuie mostenite
        SetHandleInformation(p2c_write[i], HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(c2p_read[i], HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi[i], sizeof(pi[i]));

        // Transmitem descriptorii pipe-urilor ca parametri in linia de comanda
        std::string cmd = std::string(exePath) + " --child " + 
                          std::to_string((uintptr_t)p2c_read[i]) + " " + 
                          std::to_string((uintptr_t)c2p_write[i]);

        CreateProcessA(NULL, &cmd[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi[i]);

        CloseHandle(p2c_read[i]);
        CloseHandle(c2p_write[i]);

        // Trimitem intervalul de 1000 numere copilului
        int range[2] = { i * CHUNK_SIZE + 1, (i + 1) * CHUNK_SIZE };
        DWORD bytesWritten;
        WriteFile(p2c_write[i], range, sizeof(range), &bytesWritten, NULL);
        CloseHandle(p2c_write[i]);
    }

    // Colectam rezultatele de la cele 10 procese
    int total_primes = 0;
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        std::cout << "\n--- Rezultate Proces Copil " << i 
                  << " (" << i * CHUNK_SIZE + 1 << " - " << (i + 1) * CHUNK_SIZE << ") ---\n";
        int val;
        DWORD bytesRead;
        while (ReadFile(c2p_read[i], &val, sizeof(int), &bytesRead, NULL) && bytesRead > 0) {
            if (val == -1) break;
            std::cout << val << " ";
            total_primes++;
        }
        std::cout << "\n";
        CloseHandle(c2p_read[i]);
    }

    // Asteptam terminarea tuturor proceselor copil
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        WaitForSingleObject(pi[i].hProcess, INFINITE);
        CloseHandle(pi[i].hProcess);
        CloseHandle(pi[i].hThread);
    }

    std::cout << "\nTotal numere prime gasite (1 - 10000): " << total_primes << " (Asteptat: 1229)\n";
    return 0;
}