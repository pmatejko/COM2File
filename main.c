#include <Windows.h>
#include <stdio.h>
#include <time.h>

#define BUFFER_SIZE 1025

HANDLE comPortHandle;
HANDLE resultsFileHandle;


void safeExit() {
    CloseHandle(comPortHandle);
    CloseHandle(resultsFileHandle);
}

void setCOMPortHandle(char * comPortName) {
    comPortHandle = CreateFile(comPortName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (comPortHandle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error opening COM port %s\n", comPortName);
        exit(EXIT_FAILURE);
    }
}

void setResultsFileHandle(char * resultsFileName) {
    resultsFileHandle = CreateFile(resultsFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (resultsFileHandle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error opening file %s\n", resultsFileName);
        exit(EXIT_FAILURE);
    }
}

void setNewCOMState() {
    DCB serialParameters = {0};
    serialParameters.DCBlength = sizeof(serialParameters);

    if (GetCommState(comPortHandle, &serialParameters) == FALSE) {
        perror("Error getting current COM state\n");
        exit(1);
    }

    serialParameters.BaudRate = CBR_9600;
    serialParameters.ByteSize = 8;
    serialParameters.StopBits = ONESTOPBIT;
    serialParameters.Parity = NOPARITY;

    if (SetCommState(comPortHandle, &serialParameters) == FALSE) {
        fprintf(stderr, "Error setting new COM state\n");
        exit(EXIT_FAILURE);
    }
}

void setNewCOMTimeouts() {
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 15;
    timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    if (SetCommTimeouts(comPortHandle, &timeouts) == FALSE) {
        fprintf(stderr, "Error setting new COM timeouts\n");
        exit(EXIT_FAILURE);
    }
}

void setNewCOMMask() {
    if (SetCommMask(comPortHandle, EV_RXCHAR) == FALSE) { // awakes when at least one character is received
        fprintf(stderr, "Error setting new COM mask\n");
        exit(EXIT_FAILURE);
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void com2FileLoop() {
    char buffer[BUFFER_SIZE] = {};
    char * reading;
    DWORD bytesRead;
    DWORD bytesWritten;
    DWORD awakingEvent;

    for (;;) {
        if (WaitCommEvent(comPortHandle, &awakingEvent, NULL) != FALSE) {
            do {
                ReadFile(comPortHandle, buffer, sizeof(char) * BUFFER_SIZE - 1, &bytesRead, NULL);
                buffer[bytesRead] = '\0';

                reading = strtok(buffer, "\n");
                while (reading != NULL) {
                    WriteFile(resultsFileHandle, reading, sizeof(char) * strlen(reading), &bytesWritten, NULL);

                    time_t currentEpoch = time(NULL);
                    size_t timeStringSize = (size_t) (snprintf(NULL, 0, "%ld", currentEpoch) + 3); // ',' '\n' '\0'
                    char timeString[timeStringSize] = {};
                    snprintf(timeString, timeStringSize, ",%ld\n", currentEpoch);

                    WriteFile(resultsFileHandle, timeString, sizeof(char) * (timeStringSize - 1), &bytesWritten, NULL);

                    reading = strtok(NULL, "\n");
                }
            } while (bytesRead > 0);
        }
    }
}
#pragma clang diagnostic pop


void main(int argc, char * argv[]) {
    char * comPortName;
    char * resultsFileName;

    if (argc != 3) {
        printf("Use program like this:\n"
                       "com2file <name_of_com_port> <name_of_results_file>\n");
        exit(EXIT_SUCCESS);
    } else {
        comPortName = argv[1];
        resultsFileName = argv[2];
    }

    setCOMPortHandle(comPortName);
    setResultsFileHandle(resultsFileName);

    atexit(safeExit);

    setNewCOMState();
    setNewCOMTimeouts();
    setNewCOMMask();

    printf("\nStarted listening for data\n");
    com2FileLoop();
}
