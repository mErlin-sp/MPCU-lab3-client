#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>
#include <fstream>

const int BUFFER_SIZE = 1024;

// Flag to indicate if the program should continue running
volatile sig_atomic_t interrupted = 0;

// Signal handler for SIGINT
void signal_handler(int signal) {
    std::cout << "Interrupt signal (" << signal << ") received.\n";
    interrupted = 1;
}

int parseMessageHeaders(const char *buffer, const char *&c, int bytes_received) {
    // Parse protocol
    std::string protocol;
    for (; *c != '#' && (c - &buffer[0]) < bytes_received; c++) {
        char cc = *c;
        protocol += cc;
    }
    if (protocol != "PROTO:1.4.8.8") {
        std::cerr << "Invalid protocol" << std::endl;
        return 1;
    }
    c++;

    // Parse command name
    std::string command; //(BUFFER_SIZE, 0x4)
    for (; *c != '#' && (c - &buffer[0]) < bytes_received; c++) {
        char cc = *c;
        command += cc;
    }
    c++;

    if (command.length() != bytes_received) {
        // Print parsed command
        std::cout << "Parsed command: " << command << std::endl;
    }

    // Parse status
    std::string status;
    for (; *c != '#' && (c - &buffer[0]) < bytes_received; c++) {
        char cc = *c;
        status += cc;
    }
    c++;

    if (status == "ERR") {
        std::cerr << "Server error" << std::endl;
        return 1;
    } else if (status != "OK") {
        std::cerr << "Unknown status: " << status << std::endl;
        return 1;
    }

    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << "./program <server_address> <server_port> <file_name> <max_file_size>"
                  << std::endl;
        return 1;
    }

    // Install signal handler for SIGINT
    std::signal(SIGINT, signal_handler);

    // Parse command line arguments
    uint32_t serverAddress = std::stoi(argv[1]);
    uint16_t serverPort = std::stoi(argv[2]);
    std::string fileName = argv[3];
    uint64_t maxFileSize = std::stoi(argv[4]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Error creating socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = serverAddress;
    server_addr.sin_port = serverPort;  //htons(PORT)

    if (connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        return 1;
    }

    std::cout << "Connected to server\n";

    try {

        // Sending NEW request
        std::string message = "PROTO:1.4.8.8#NEW#" + fileName;
        message += (char) 0x1C;
        message += '#';
        message += (char) 0x4;

        if (send(sock, message.c_str(), message.length(), 0) == -1) {
            perror("Send failed");
            throw std::runtime_error("Send failed");
        }

        // Receive NEW response
        char buffer[BUFFER_SIZE];
        int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_received == -1) {
            perror("Receive failed");
            throw std::runtime_error("Receive failed");
        }

        // Print the received data
        std::cout << "Received from server: ";
        std::cout.write(buffer, bytes_received);
        std::cout << 0x0 << std::endl;

        // Process received data (e.g., save to a file)
        char const *c = &buffer[0];
        if (parseMessageHeaders(&buffer[0], c, bytes_received) != 0) {
            throw std::runtime_error("Parse headers failed");
        }

        // Parse file name
        std::string _fileName;
        for (; *c != '#' && (c - &buffer[0]) < bytes_received; c++) {
            char cc = *c;
            _fileName += cc;
        }
        if (*(c - 1) != 0x1C) {
            throw std::runtime_error("Invalid filename");
        }
        _fileName.erase(_fileName.size() - 1);
        c++;

        if (fileName != _fileName) {
            throw std::runtime_error("Filename received from server doesnt match with that sent");
        }

        // Print file name
        std::cout << "Parsed filename: " << _fileName << std::endl;

        // Parse file size
        std::string _fileSize;
        for (; *c != '#' && (c - &buffer[0]) < bytes_received; c++) {
            char cc = *c;
            _fileSize += cc;
        }
        c++;

        if (std::stoi(_fileSize) > maxFileSize) {
            throw std::runtime_error("File is too big");
        }

        // Print file size
        std::cout << "Parsed filesize: " << _fileSize << std::endl;

        // Sending REC request
        message = "PROTO:1.4.8.8#REC#";

        if (send(sock, message.c_str(), message.length(), 0) == -1) {
            perror("Send failed");
            throw std::runtime_error("Send failed");
        }

        // Receive REC response
        bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_received == -1) {
            perror("Receive failed");
            throw std::runtime_error("Receive failed");
        }

        // Print the received data
        std::cout << "Received from server: ";
        std::cout.write(buffer, bytes_received);
        std::cout << 0x0 << std::endl;

        // Process received data (e.g., save to a file)
        c = &buffer[0];
        if (parseMessageHeaders(&buffer[0], c, bytes_received) != 0) {
            throw std::runtime_error("Parse headers failed");
        }

        // Write data to file
        std::ofstream outFile(fileName); // Create or open a file for writing

        if (!outFile.is_open()) {
            throw std::runtime_error("Failed to open file");
        }

        for (; *c != 0x4; c++) {
            if ((c - &buffer[0]) >= bytes_received) {
                std::cout << "End of buffer" << std::endl;

                // Receive REC response
                bytes_received = recv(sock, buffer, sizeof(buffer), 0);
                if (bytes_received == -1) {
                    perror("Receive failed");
                    throw std::runtime_error("Receive failed");
                }
                std::cout << "Bytes received: " << std::to_string(bytes_received) << std::endl;
                c = &buffer[0];
            }
            outFile << *c;
//        std::cout << *c << std::endl;
        }

        outFile.close(); // Close the file

        std::cout << "File created successfully" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }

    close(sock);

    return 0;
}
