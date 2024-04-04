#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>
#include <cstring>
#include <fstream>

const std::string PROTOCOL_VERSION = "1.4.8.8";

// Flag to indicate if the program should continue running
volatile sig_atomic_t interrupted = 0;

// Signal handler for SIGINT
void signal_handler(int signal) {
    std::cout << "Interrupt signal (" << signal << ") received.\n";
    interrupted = 1;
}

int parse_error(const char *buffer, char *&c, int bytes_received, std::string &error_msg) {
    // Parse error code
    std::string error_code;
    for (; *c != '#' && (c - &buffer[0]) < bytes_received; c++) {
        char cc = *c;
        error_code += cc;
    }
    c++;

    // Parse error message
    for (; *c != '#' && (c - &buffer[0]) < bytes_received; c++) {
        char cc = *c;
        error_msg += cc;
    }
    c++;

    return std::stoi(error_code);
}

int parse_headers(const char *buffer, char *&c, int bytes_received, std::string &error_msg) {
    // Parse protocol
    std::string protocol;
    for (; *c != '#' && (c - &buffer[0]) < bytes_received; c++) {
        char cc = *c;
        protocol += cc;
    }
    if (protocol != "PROTO:" + PROTOCOL_VERSION) {
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

    if (command == "ERR") {
        std::cerr << "Server error" << std::endl;
        return parse_error(buffer, c, bytes_received, error_msg);
    }


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

    if (status != "OK") {
        if (status == "ERR") {
            std::cerr << "Server error" << std::endl;
            return parse_error(buffer, c, bytes_received, error_msg);
        }

        std::cerr << "Unknown status: " << status << std::endl;
        return 1;
    }

    return 0;
}

size_t read(char *buffer, size_t buffer_length, int sock) {
    char *c = &buffer[0];
    bool eot = false;

    while (!interrupted && !eot) {
        char recv_buffer[buffer_length];
        char *cc = &recv_buffer[0];

        int bytes_received = recv(sock, recv_buffer, (&buffer[0] + buffer_length) - c, 0);
        if (bytes_received == -1) {
            perror("Receive failed");
            throw std::runtime_error("Receive failed");
        }

        // Print the received data
        std::cout << "Bytes received: " << bytes_received << std::endl;
//        std::cout << "Receive Buffer: ";
//        std::cout.write(recv_buffer, bytes_received);
//        std::cout << std::endl;


        for (; (cc - &recv_buffer[0]) < bytes_received; cc++) {
//            std::cout << cc[0] << std::endl;
            if (c - &buffer[0] > buffer_length) {
                throw std::runtime_error("Buffer overflow");
            }

            *c = *cc;
            c++;

            if (*cc == 0x4) {
                std::cout << "EOT" << std::endl;
                *c = '\0';
                eot = true;
                std::cout << "Receive End" << std::endl;
                break;
            }
        }
        std::cout << "Receive Buffer End" << std::endl;
        if (c - &buffer[0] >= buffer_length) {
            return buffer_length;
        }
    }
    return c - &buffer[0];
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << "./program <server_address> <server_port> <file_name> <max_file_size>"
                  << std::endl;
        return 1;
    }

    // Install signal handler for SIGINT
    std::signal(SIGINT, signal_handler);

    // Parse command line arguments
    uint32_t serverAddress = std::stoi(argv[1]);
    uint16_t serverPort = std::stoi(argv[2]);
    std::string fileName = argv[3];
    uint64_t max_file_size = std::stoi(argv[4]);

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
        const int BUFFER_SIZE = 1024;

        char buffer[BUFFER_SIZE];
        char *c = &buffer[0];

        // Sending NEW request
        sprintf(buffer, "PROTO:%s#NEW#%s\x1c#\x4", PROTOCOL_VERSION.c_str(), fileName.c_str());

        if (send(sock, buffer, strlen(buffer), 0) == -1) {
            perror("Send failed");
            throw std::runtime_error("Send failed");
        }

        // Receive NEW response
//        while (!interrupted && *c != '\0') {
//            char recv_buffer[BUFFER_SIZE];
//            char *cc = &recv_buffer[0];
//
//            int bytes_received = recv(sock, recv_buffer, BUFFER_SIZE, 0);
//            if (bytes_received == -1) {
//                perror("Receive failed");
//                throw std::runtime_error("Receive failed");
//            }
//
//            // Print the received data
//            std::cout << "Receive Buffer: ";
//            std::cout.write(recv_buffer, bytes_received);
//            std::cout << std::endl;
//
//
//            for (; (cc - &recv_buffer[0]) < bytes_received; cc++) {
//                std::cout << cc[0] << std::endl;
//                if (c - &buffer[0] > BUFFER_SIZE) {
//                    throw std::runtime_error("Buffer overflow");
//                }
//
//                *c = *cc;
//                c++;
//
//                if (*cc == 0x4) {
//                    std::cout << "EOT" << std::endl;
//                    *c = '\0';
//                    std::cout << "Receive End" << std::endl;
//                    break;
//                }
//            }
//            std::cout << "Receive Buffer End" << std::endl;
//        }

        // Receive NEW response
        read(buffer, BUFFER_SIZE, sock);

        // Print the full received data
        std::cout << "Received from server: " << buffer << std::endl;

        // Process received data (e.g., save to a file)
        c = &buffer[0];
        std::string error_msg;
        if (int error_code = parse_headers(&buffer[0], c, strlen(buffer), error_msg); error_code != 0) {
            if (error_code >= 100) {
                std::cerr << "Server error" << std::endl;
                std::cerr << "Error code: " << error_code << std::endl;
                std::cerr << "Error msg: " << error_msg << std::endl;
                throw std::runtime_error(error_msg);
            }
            throw std::runtime_error("Parse headers failed");
        }

        // Parse file name
        std::string _fileName;
        for (; *c != '#' && (c - &buffer[0]) < strlen(buffer); c++) {
            _fileName += c[0];
        }
        if (*(c - 1) != 0x1c) {
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
        std::string _file_size;
        for (; *c != '#' && (c - &buffer[0]) < strlen(buffer); c++) {
            _file_size += c[0];
        }
        c++;

        if (std::stoi(_file_size) > max_file_size) {
            throw std::runtime_error("File is too big");
        }

        // Print file size
        std::cout << "Parsed filesize: " << _file_size << std::endl;

        // Sending REC request
        c = &buffer[0];

        sprintf(buffer, "PROTO:%s#REC#\x4", PROTOCOL_VERSION.c_str());

        if (send(sock, buffer, strlen(buffer), 0) == -1) {
            perror("Send failed");
            throw std::runtime_error("Send failed");
        }

        // Receive REC response
        size_t bytes_received = read(buffer, BUFFER_SIZE, sock);

        // Print the full received data
        std::cout << "Received from server: " << buffer << std::endl;

        // Process received data (e.g., save to a file)
        c = &buffer[0];
        if (int error_code = parse_headers(&buffer[0], c, strlen(buffer), error_msg); error_code != 0) {
            if (error_code >= 100) {
                std::cerr << "Server error" << std::endl;
                std::cerr << "Error code: " << error_code << std::endl;
                std::cerr << "Error msg: " << error_msg << std::endl;
                throw std::runtime_error(error_msg);
            }
            throw std::runtime_error("Parse headers failed");
        }

        // Write data to file
        std::ofstream out_file(fileName); // Create or open a file for writing

        if (!out_file.is_open()) {
            throw std::runtime_error("Failed to open file");
        }

        u_int64_t total_bytes = 0;
        for (; *c != 0x4; c++) {
            if ((c - &buffer[0]) >= bytes_received) {
                std::cout << "End of buffer" << std::endl;

                // Receive REC response
                bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
                if (bytes_received == -1) {
                    perror("Receive failed");
                    throw std::runtime_error("Receive failed");
                }
                std::cout << "Bytes received: " << bytes_received << std::endl;
//                std::cout << "Received from server: ";
//                std::cout.write(buffer, bytes_received);
//                std::cout << std::endl;
                c = &buffer[0];
            }
            out_file << *c;
            total_bytes++;
//            std::cout << "c: " << *c << std::endl;
        }

        out_file.close(); // Close the file

        std::cout << "File created successfully" << std::endl;
        std::cout << "Total bytes: " << total_bytes << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }

    close(sock);

    return 0;
}
