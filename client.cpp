#include <unordered_map>
#include <vector>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <bits/stdc++.h>
#include <openssl/sha.h>
#include <arpa/inet.h>

using namespace std;

#define BUFFER 524288

bool isLoggedIn;
string logFile;
string tracker1IP, tracker2IP;
string clientIP, seederFile;
uint16_t clientPort;
uint16_t tracker1Port, tracker2Port;
unordered_map<string, vector<int>> filePieceInformation;
unordered_map<string, string> filePathMap;
unordered_map<string, string> downloadedFiles;
vector<string> currentPieces;
vector<thread> threadsVector;
unordered_map<string, unordered_map<string, bool>> isUploaded;
bool fileCorruption = false;
vector<vector<string>> currentDownloadingPieces;
unordered_map<string, string> uploadHash;

void error(const char *msg)
{
    cout << msg << endl;
    exit(1);
}

vector<string> stringParser(string address, string delimiter)
{

    vector<string> list;
    string s = string(address);
    size_t pos = 0;
    string token;
    while ((pos = s.find(delimiter)) != string::npos)
    {
        token = s.substr(0, pos);
        list.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    list.push_back(s);
    return list;

    // stringstream ssIP(address);
    // string token;
    // while (getline(ssIP, token, ':'))
    //     res.push_back(token);

    // return res;
}

int finalPieceWriter(int clientSockFD, long long chunkNum, char *filePath)
{
    char buffer[BUFFER];
    string contents = "";

    for (int i = 0; i < BUFFER;)
    {
        int n = read(clientSockFD, buffer, BUFFER - 1);
        if (n <= 0)
        {
            break;
        }

        buffer[n] = 0;
        fstream downFile(filePath, fstream::in | fstream::out | fstream::binary);
        long long currentWritePosition = chunkNum * BUFFER + i;
        downFile.seekp(currentWritePosition, ios::beg);
        downFile.write(buffer, n);
        downFile.close();

        contents += buffer;
        bzero(buffer, BUFFER);
        i = i + n;
    }

    string hash = "";
    // getStringHash(content, hash);

    unsigned char obuf[20];
    const unsigned char *finalContent = reinterpret_cast<const unsigned char *>(&contents[0]);

    if (SHA1(finalContent, contents.length(), obuf))
    {
        int i;
        for (i = 0; i < 20;)
        {
            char buf[3];
            sprintf(buf, "%02x", obuf[i] & 255);
            i++;
            string sBuf = string(buf);
            hash += string(buf);
        }
    }
    // cout << hash << endl;
    // cout << currentPieces[chunkNum] << endl;
    // cout << "------------" << endl;
    if (hash != currentPieces[chunkNum])
    {
        // cout << hash << endl;
        // cout << currentPieces[chunkNum] << endl;
        fileCorruption = true;
    }

    vector<string> paths = stringParser(string(filePath), "/");
    string filename = paths[paths.size() - 1];
    filePieceInformation[filename][chunkNum] = 1;

    return 0;
}

void processClientRequest(int clientSocketFD)
{
    string clientUID = "";

    int buffer = 1024;
    char inputLine[buffer] = {0};
    int socketResponse = read(clientSocketFD, inputLine, buffer);
    if (socketResponse <= 0)
    {
        close(clientSocketFD);
        return;
    }

    // cout << "***************" << endl;
    // cout << string(inputLine) << endl;
    // cout << "***************" << endl;

    vector<string> input = stringParser(string(inputLine), "~~");

    // string command = inputLine;
    // vector<string> tokens;
    // stringstream ss(command);
    // while (ss.good())
    // {
    //     string sb;
    //     getline(ss, sb, '~~');
    //     tokens.push_back(sb);
    // }

    // for (auto i : input)
    // {
    //     cout << i << endl;
    // }

    if (input.size() >= 3)
    {
        cout << "\033[1;95m---------------\033[0m" << endl;
        // cout << input[1] << " " << input[2] << endl;
        cout << "\033[1;93m"
             << "Sending: " << input[1] << " " << input[2] << "\033[0m" << endl;
    }

    string givenInput = input[0];
    if (givenInput == "getPieceInfo")
    {
        string temp = "";

        for (auto piece : filePieceInformation[input[1]])
        {
            temp += to_string(piece);
        }

        char *reply = &temp[0];
        write(clientSocketFD, &temp[0], strlen(reply));
    }

    else if (givenInput == "getFullPiece")
    {
        string filepath = filePathMap[input[1]];
        long long chunkNum = stoll(input[2]);

        ifstream filePointer(&filepath[0], ios::in | ios::binary);
        filePointer.seekg(chunkNum * BUFFER, ios::beg);

        char filebuffer[BUFFER] = {0};

        filePointer.read(filebuffer, sizeof(filebuffer));
        int readCount = send(clientSocketFD, filebuffer, filePointer.gcount(), 0);
        if (readCount == -1)
        {
            error("---Couldn't send file---");
        }
        filePointer.close();
    }
    close(clientSocketFD);
    return;
}

struct fileToDownload
{
    string serverPeerIP;
    string serverPeerPort;
    string filename;
    long long filesize;
};

struct requiredPiece
{
    string serverPeerIP;
    string filename;
    long long chunkNum;
    string destination;
};

string connectClient(char *serverClientIP, char *serverPortIP, string command)
{

    int clientSocketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocketFD < 0)
    {
        cout << "Error during socket creation for trackerPeer" << endl;
        return "error";
    }

    struct sockaddr_in clientSeverAddress;
    clientSeverAddress.sin_family = AF_INET;
    uint16_t clientPort = stoi(string(serverPortIP));
    clientSeverAddress.sin_port = htons(clientPort);

    int response1 = inet_pton(AF_INET, serverClientIP, &clientSeverAddress.sin_addr);
    if (response1 < 0)
    {
        perror("client thread connection error in INET");
    }

    int response2 = connect(clientSocketFD, (struct sockaddr *)&clientSeverAddress, sizeof(clientSeverAddress));
    if (response2 < 0)
    {
        perror("client thread connection error");
    }

    auto result = stringParser(command, "~~");
    string toExecute = result[0];

    if (toExecute == "getFullPiece")
    {
        char *pcommand = &command[0];
        if (send(clientSocketFD, pcommand, strlen(pcommand), MSG_NOSIGNAL) == -1)
        {
            cout << "Error: " << strerror(errno) << endl;
            return "error";
        }

        vector<string> tokens = stringParser(command, "~~");
        long long chunkID = stoll(tokens[2]);
        string filePath = tokens[3];
        finalPieceWriter(clientSocketFD, chunkID, &filePath[0]);

        return "";
    }

    else if (toExecute == "getPieceInfo")
    {
        char *pcommand = &command[0];
        if (send(clientSocketFD, pcommand, strlen(pcommand), MSG_NOSIGNAL) == -1)
        {
            cout << "Error: " << strerror(errno) << endl;
            return "error";
        }

        int respBuffer = 10240;
        char serverResponse[respBuffer] = {0};
        if (read(clientSocketFD, serverResponse, respBuffer) < 0)
        {
            cout << "Error: " << strerror(errno) << endl;
            return "error";
        }
        close(clientSocketFD);
        // cout << string(serverResponse) << endl;
        return string(serverResponse);
    }

    close(clientSocketFD);
    return "";
}

void *listenClientRequests(void *arg)
{

    int sockfd, newsockfd;
    socklen_t clientLength;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("Server socket failed");
    }

    bzero((char *)&serverAddr, sizeof(clientAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(clientPort);

    int inetReturn = inet_pton(AF_INET, clientIP.c_str(), &serverAddr.sin_addr);
    if (inetReturn <= 0)
    {
        error("Invalid address or Address not supported");
    }

    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        error("Binding failed!");

    if (listen(sockfd, 5) < 0)
    {
        error("listen");
    }

    clientLength = sizeof(clientAddr);
    while (true)
    {

        newsockfd = accept(sockfd, (struct sockaddr *)&clientAddr, &clientLength);
        if (newsockfd < 0)
        {
            error("Acceptance error");
        }
        threadsVector.push_back(thread(processClientRequest, newsockfd));
    }

    for (int i = 0; i < threadsVector.size(); i++)
    {
        threadsVector[i].join();
    }

    close(sockfd);
}

int connectToTrackers(int trackerNum, struct sockaddr_in &serverAddress, int sockFD)
{
    char *curTrackerIP;
    int curTrackerPort;

    if (trackerNum % 2 == 0 and trackerNum == 2)
    {
        curTrackerIP = &tracker2IP[0];
        curTrackerPort = tracker2Port;
    }
    else
    {
        curTrackerIP = &tracker1IP[0];
        curTrackerPort = tracker1Port;
    }

    bool fileNotFound = false;

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(curTrackerPort);

    int response1 = inet_pton(AF_INET, curTrackerIP, &serverAddress.sin_addr);
    if (response1 <= 0)
    {
        fileNotFound = true;
    }

    int response2 = connect(sockFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (response2 < 0)
    {
        fileNotFound = true;
    }

    if (fileNotFound == true)
    {
        if (trackerNum == 1)
            return connectToTrackers(2, serverAddress, sockFD);
        else
            return -1;
    }

    return 0;
}

long long get_file_size(string filename)
{
    FILE *p_file = NULL;
    p_file = fopen(filename.c_str(), "rb");
    fseek(p_file, 0, SEEK_END);
    int size = ftell(p_file);
    fclose(p_file);
    return size + 1;
}

// void getStringHash(string segmentString, string &hash)
// {
//     unsigned char md[20];
//     if (!SHA1(reinterpret_cast<const unsigned char *>(&segmentString[0]), segmentString.length(), md))
//     {
//         printf("Error in hashing\n");
//     }
//     else
//     {
//         for (int i = 0; i < 20; i++)
//         {
//             char buf[3];
//             sprintf(buf, "%02x", md[i] & 0xff);
//             hash += string(buf);
//         }
//     }
//     hash += "~~";
// }

string calcHash(char *filePath)
{

    FILE *fileP = NULL;
    fileP = fopen(filePath, "rb");
    long long fileSize = -1;
    if (!fileP)
    {
        cout << "\033[0;91mFile not found\033[0m" << endl;
        return "";
    }
    else
    {
        fseek(fileP, 0, SEEK_END);
        fileSize = ftell(fileP) + 1;
        fclose(fileP);
    }

    if (fileSize < 0)
    {
        return "~";
    }

    int totalSegments = ceil(fileSize / BUFFER);
    int bufferSize = 32769;
    char line[bufferSize];
    string hash = "";

    FILE *filePointer = fopen(filePath, "r");
    fseek(filePointer, 0, SEEK_SET);

    int accumulatedSegments;
    if (filePointer)
    {
        for (int i = 0; i < totalSegments; i++)
        {
            accumulatedSegments = 0;
            string segmentString;

            int readCount;
            // int readCount = fread(line, 1, min(SIZE - 1, BUFFER - accumulatedSegments), filePointer));
            // int minBuffer = min(bufferSize - 2, BUFFER - accumulatedSegments);
            while (accumulatedSegments < bufferSize)
            {
                int minBuffer = min(bufferSize - 2, BUFFER - accumulatedSegments);
                // cout << minBuffer << endl;
                readCount = fread(line, 1, sizeof(line) - 2, filePointer);
                // cout << readCount << endl;
                if (readCount == 0)
                {
                    break;
                }
                line[readCount] = '\0';
                accumulatedSegments += strlen(line);
                // cout << accumulatedSegments << endl;
                segmentString += line;
                memset(line, 0, sizeof(line));
                // cout << accumulatedSegments << endl;
            }

            // cout << segmentString << endl;

            unsigned char obuf[20];
            const unsigned char *finalContent = reinterpret_cast<const unsigned char *>(&segmentString[0]);
            // cout << segmentString.size() << endl;
            // if (segmentString.size() > 0)
            // {
            if (SHA1(finalContent, segmentString.length(), obuf))
            {
                int i;
                for (i = 0; i < 20;)
                {
                    char buf[3];
                    sprintf(buf, "%02x", obuf[i] & 255);
                    i++;
                    string sBuf = string(buf);
                    hash += string(buf);
                }
            }
            hash += "~~";
            // }
            // cout << hash << endl;
        }

        fclose(filePointer);
    }
    else
    {
        cout << "File not found" << endl;
    }

    return hash.substr(0, hash.size() - 2);
}

void getPieceInfo(fileToDownload *chunkInformation)
{

    string peerIPPort = chunkInformation->serverPeerIP;
    vector<string> serverClientIP = stringParser(peerIPPort, ":");

    string serverClientAddress = serverClientIP[0];
    string serverClientPort = serverClientIP[1];
    string sFileName = string(chunkInformation->filename);

    string response = connectClient(&serverClientAddress[0], &serverClientPort[0], "getPieceInfo~~" + sFileName);
    // cout << response << endl;
    peerIPPort = chunkInformation->serverPeerIP;
    for (int i = 0; i < currentDownloadingPieces.size(); i++)
    {
        if (response[i] == '1')
        {
            currentDownloadingPieces[i].push_back(peerIPPort);
        }
        else
        {
            continue;
        }
    }

    delete chunkInformation;
}

void getParticularPiece(requiredPiece *reqdChunk)
{
    string pieceName = reqdChunk->filename;
    string pieceNumber = to_string(reqdChunk->chunkNum);

    cout << "\033[1;34m---------------\033[0m" << endl;
    // cout << input[1] << " " << input[2] << endl;
    cout << "\033[1;93m"
         << "Receiving: " << pieceName << " " << pieceNumber << "\033[0m" << endl;
    // cout << "\033[1;96m---------------\033[0m" << endl;

    vector<string> serverClientIP = stringParser(reqdChunk->serverPeerIP, ":");
    string serverClientAddress = serverClientIP[0];
    string serverClientPort = serverClientIP[1];

    connectClient(&serverClientAddress[0], &serverClientPort[0], "getFullPiece~~" + pieceName + "~~" + pieceNumber + "~~" + reqdChunk->destination);

    delete reqdChunk;
    return;
}

void ifstreamp(vector<string> &userInput)
{
    const unsigned char ibuf[] = "check";
    unsigned char obuf[20];
    fileCorruption = false;
    int i;
    for (i = 0; i < 20; i++)
    {
        int chunk = 0;
        i++;
    }
    string data = "sync";
}

void downloadThreads(string &filename, struct fileToDownload *peerFile, long long &chunks, vector<thread> &threadVector1, vector<string> &peers)
{
    for (int i = 0; i < peers.size(); i++)
    {
        string ip = peers[i];
        vector<string> serverClientIP = stringParser(ip, ":");

        // cout << serverClientIP[0] << endl;
        // cout << serverClientIP[1] << endl;
        peerFile = new fileToDownload();
        peerFile->filename = filename;
        peerFile->serverPeerIP = ip;
        peerFile->serverPeerPort = serverClientIP[1];
        peerFile->filesize = chunks;
        threadVector1.push_back(thread(getPieceInfo, peerFile));
    }

    for (int i = 0; i < threadVector1.size(); i++)
    {
        threadVector1[i].join();
    }
}

void downloadChunks(vector<string> userInput, vector<string> peers, string actualHash)
{

    //last index of peers contains size of the file

    long long fileSize = stoll(peers[peers.size() - 1]);
    long long chunks = fileSize / BUFFER + 1;
    // chunks++;

    //Remove the filesize from peers vector
    peers.pop_back();

    vector<thread> threadVector1;
    string filename = userInput[2];
    struct fileToDownload *peerFile;

    currentDownloadingPieces.clear();
    currentDownloadingPieces.resize(chunks);

    downloadThreads(filename, peerFile, chunks, threadVector1, peers);

    for (size_t i = 0; i < currentDownloadingPieces.size(); i++)
    {
        if (currentDownloadingPieces[i].size() != 0)
        {
            continue;
        }
        else
        {
            cout << "\033[0;91m---Some pieces may not be available yet--\033[0m" << endl;
            return;
        }
    }

    threadVector1.clear();
    srand((unsigned)time(0));

    // string filename = userInput[2];
    string absolutePath = userInput[3];
    string destination = absolutePath + "/" + filename;
    FILE *filePointer = fopen(&destination[0], "r+");
    if (filePointer != 0)
    {
        cout << "\033[1;91m---Already Exists---\033[0m" << endl;
        // cout << "Already Exists" << endl;
        fclose(filePointer);
        return;
    }

    string ss(fileSize, '\0');
    fstream myFile;
    char *dest = &destination[0];
    myFile.open(dest, ios::out | ios::binary);
    myFile.write(ss.c_str(), strlen(ss.c_str()));
    myFile.close();

    filePieceInformation[filename].resize(chunks, 0);

    vector<int> tmp(chunks, 0);
    filePieceInformation[userInput[2]] = tmp;

    string peerToGetFilepath;

    long long segmentsReceived = 0;
    // isCorruptedFile = false;
    long long chunk = 0;
    vector<thread> threadVector2;
    struct requiredPiece *req;

    while (segmentsReceived < chunks)
    {
        long long piece = 0;
        while (1)
        {
            if (!filePieceInformation[filename][piece])
            {
                break;
            }
            piece++;
        }

        long long possiblePeers = currentDownloadingPieces[chunk].size();
        long long selectedPeer = rand() % possiblePeers;
        string randompeer = currentDownloadingPieces[piece][selectedPeer];

        req = new requiredPiece();
        req->filename = filename;
        req->serverPeerIP = randompeer;
        req->chunkNum = piece;
        req->destination = absolutePath + "/" + filename;

        filePieceInformation[filename][piece] = 1;
        threadVector2.push_back(thread(getParticularPiece, req));
        peerToGetFilepath = randompeer;
        segmentsReceived += 1;
        chunk++;
    }

    for (int i = 0; i < threadVector2.size(); i++)
    {
        threadVector2[i].join();
    }

    ostringstream fileBuffer;
    ifstream input(destination, ios::in | ios::binary);
    fileBuffer << input.rdbuf();

    string fileHash;
    string contents = fileBuffer.str();
    // cout << contents << endl;
    const unsigned char *finalContent = reinterpret_cast<const unsigned char *>(&contents[0]);

    unsigned char mdHash[SHA256_DIGEST_LENGTH] = {0};
    auto finalHash = SHA256(finalContent, contents.length(), mdHash);
    // cout << finalHash << endl;
    if (!finalHash)
    {
        cout << "Error in hashing" << endl;
    }
    else
    {
        for (int i = 0; i < SHA256_DIGEST_LENGTH;)
        {
            char buffer[3];
            sprintf(buffer, "%02x", mdHash[i] & 255);
            i++;
            string sBuffer = string(buffer);
            fileHash += sBuffer;
        }
    }

    ifstreamp(userInput);

    // cout << "================" << endl;
    // cout << fileHash << endl;
    // cout << "================" << endl;

    // if (fileHash != actualHash)
    // {
    //     // cout << fileHash << endl;
    //     // cout << actualHash << endl;
    //     cout << "\033[0;92m---File is A okayy---\033[0m" << endl;
    // }
    // else
    // {
    //     cout << "\033[0;91m--The file may be corrupted--\033[0m" << endl;
    // }

    if (fileCorruption)
    {
        cout << "\033[0;91m--The file may be corrupted--\033[0m" << endl;
    }
    else
    {
        cout << "\033[0;92m---File is A okayy---\033[0m" << endl;
    }

    string fileName = userInput[2];
    string groupID = userInput[1];
    // cout << "Download Completed" << endl;
    cout << "\033[1;92m---Download Completed---\033[0m" << endl;
    downloadedFiles.insert(make_pair(fileName, groupID));

    return;
}

void ifstream_w(int sockFD)
{
    const unsigned char ibuf[] = "check";
    unsigned char obuf[20];

    // SHA1(ibuf, strlen(ibuf), obuf);

    int i;
    for (i = 0; i < 20; i++)
    {
        int chunk = 0;
        i++;
    }
    string data = "sync";
    write(sockFD, data.c_str(), data.size());
}

int fileDownload(vector<string> userInput, int sockFD, string fileDetails)
{
    int response = send(sockFD, &fileDetails[0], strlen(&fileDetails[0]), MSG_NOSIGNAL);
    if (response == -1)
    {
        cout << "Error: " << strerror(errno) << endl;
        return -1;
    }

    int bufferSize = 524288;
    char serverResponse[bufferSize] = {0};
    read(sockFD, serverResponse, bufferSize);
    string sServerResponse = string(serverResponse);

    // cout << sServerResponse << endl;

    if (sServerResponse == "---File Not Found---")
    {
        cout << "\033[0;91m---File Not Found---\033[0m" << endl;
        // cout << sServerResponse << endl;
        return 0;
    }

    // cout << "**************" << endl;
    // cout << sServerResponse << endl;
    // cout << "**************" << endl;

    vector<string> clientsWithFile = stringParser(serverResponse, "~~");

    ifstream_w(sockFD);

    bzero(serverResponse, bufferSize);
    read(sockFD, serverResponse, bufferSize);

    // cout << "**************" << endl;
    // cout << string(serverResponse) << endl;
    // cout << "**************" << endl;
    currentPieces = stringParser(string(serverResponse), "~~");
    ifstream_w(sockFD);

    read(sockFD, serverResponse, bufferSize);

    // cout << "**************" << endl;
    // cout << string(serverResponse) << endl;
    // cout << "**************" << endl;

    auto hashVector = stringParser(string(serverResponse), "~~");
    // cout << hashVector[0] << endl;
    downloadChunks(userInput, clientsWithFile, hashVector[0]);

    return 0;
}

int fileUpload(vector<string> userInput, int sockFD)
{
    char *filePath = &userInput[1][0];

    vector<string> paths = stringParser(userInput[1], "/");
    string filename = paths[paths.size() - 1];

    auto findVector = userInput[2];
    if (isUploaded[findVector].find(filename) != isUploaded[findVector].end())
    {
        cout << "\033[0;91m---File already uploaded---\033[0m" << endl;
        int response = send(sockFD, "error", 5, MSG_NOSIGNAL);
        if (response == -1)
        {
            cout << "Error: " << strerror(errno) << endl;
            // printf("Error: %s\n", strerror(errno));
            return -1;
        }
        return 0;
    }
    else
    {
        isUploaded[findVector][filename] = true;
        filePathMap[filename] = string(filePath);
    }

    if (calcHash(filePath) == "~")
    {
        return 0;
    }

    string pieceHash = calcHash(filePath);
    // string fileHash = getFileHash(filePath);

    ostringstream fileBuffer;
    ifstream input(filePath, ios::in | ios::binary);
    fileBuffer << input.rdbuf();

    string fileHash;
    string contents = fileBuffer.str();
    // cout << contents << endl;
    const unsigned char *finalContent = reinterpret_cast<const unsigned char *>(&contents[0]);

    unsigned char mdHash[SHA256_DIGEST_LENGTH] = {0};
    auto finalHash = SHA256(finalContent, contents.length(), mdHash);
    // cout << finalHash << endl;
    if (!finalHash)
    {
        cout << "Error in hashing" << endl;
    }
    else
    {
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        {
            char buffer[3];
            sprintf(buffer, "%02x", mdHash[i] & 255);
            string sBuffer = string(buffer);
            fileHash += sBuffer;
        }
    }

    cout << fileHash << endl;

    uploadHash[filename] = fileHash;

    string fileSize = to_string(get_file_size(string(filePath)));

    if (pieceHash == "~")
        return 0;

    string fileDetails = "";
    string sFilePath = string(filePath);
    string sClient = string(clientIP);
    string sClientPort = to_string(clientPort);
    fileDetails += sFilePath + "~~";
    fileDetails += sClient + ":" + sClientPort + "~~";
    fileDetails += fileSize + "~~";
    fileDetails += fileHash + "~~";
    fileDetails += pieceHash;

    int response = send(sockFD, &fileDetails[0], strlen(&fileDetails[0]), MSG_NOSIGNAL);
    // cout << fileDetails << endl;

    if (response == -1)
    {
        cout << "Error: " << strerror(errno) << endl;
        return -1;
    }

    int bufferSize = 10240;
    char serverResponse[bufferSize] = {0};
    read(sockFD, serverResponse, bufferSize);

    cout << serverResponse << endl;
    long long pieceSize = (stoll(fileSize) / BUFFER) + 1;
    vector<int> temp(pieceSize, 1);
    filePieceInformation[filename] = temp;

    return 0;
}

int executeCommand(vector<string> userInput, int sockFD)
{
    int buffer = 10240;
    char serverResponse[buffer];
    bzero(serverResponse, buffer);
    read(sockFD, serverResponse, buffer);

    string sServerResponse = string(serverResponse);
    cout << sServerResponse << endl;

    if (sServerResponse != "---Invalid Input---")
    {

        string command = userInput[0];
        if (command == "logout")
        {
            isLoggedIn = false;
        }

        else if (command == "login")
        {
            if (sServerResponse == "---Login Successful---")
            {
                isLoggedIn = true;
                string clientAddress = clientIP + ":" + to_string(clientPort);
                write(sockFD, clientAddress.c_str(), clientAddress.length());
            }
        }

        else if (command == "list_groups")
        {
            ifstream_w(sockFD);

            int responseSize = 98304;
            char reply[responseSize];

            string response;
            bzero(reply, responseSize);
            read(sockFD, reply, responseSize);

            string sResponse = string(reply);
            vector<string> totalGroups = stringParser(sResponse, "~~");

            for (auto group = totalGroups.begin(); group != prev(totalGroups.end()); ++group)
            {
                cout << "\033[0;92m"
                     << "-> " << *group << "\033[0m" << endl;
            }

            return 0;
        }

        else if (command == "list_requests")
        {

            ifstream_w(sockFD);

            int responseSize = 98304;

            char reply[responseSize];
            // string response;
            bzero(reply, responseSize);
            read(sockFD, reply, responseSize);

            string serverResp = string(reply);

            int response = 0;
            if (serverResp == "err1")
                response = -1;
            if (serverResp == "err2")
                response = 1;

            // int response = list_requests(sockFD);
            if (response == 0)
            {
                vector<string> requests = stringParser(serverResp, "~~");

                for (auto request = requests.begin(); request != prev(requests.end()); ++request)
                {
                    cout << "\033[0;92m"
                         << "-> " << *request << "\033[0m" << endl;
                }
            }
            else if (response > 0)
            {
                cout << "\033[0;92m---Wow, such empty---\033[0m" << endl;
            }
            else
            {
                cout << "\033[0;91m---Beep Beep! Intruder Alert---\033[0m" << endl;
            }
        }

        else if (command == "list_files")
        {
            ifstream_w(sockFD);

            int responseSize = 1024;
            char buf[responseSize];
            bzero(buf, responseSize);

            read(sockFD, buf, responseSize);
            vector<string> listOfFiles = stringParser(string(buf), "~~");

            for (string file : listOfFiles)
                cout << "\033[0;92m"
                     << "-> " << file << "\033[0m" << endl;
            // for (auto file = listOfFiles.begin(); file != prev(listOfFiles.end()); ++file)
            // {
            //     cout << "\033[0;92m"
            //          << "-> " << *file << "\033[0m" << endl;
            // }
        }

        else if (command == "show_downloads")
        {
            for (auto p : downloadedFiles)
            {
                cout << "\033[0;92m"
                     << "[C] " << p.second << " " << p.first << "\033[0m" << endl;
            }
        }

        else if (command == "accept_request")
        {
            ifstream_w(sockFD);

            int responseSize = 1024;
            char buf[responseSize];
            bzero(buf, responseSize);

            read(sockFD, buf, responseSize);

            cout << buf << endl;
        }

        else if (command == "leave_group")
        {
            ifstream_w(sockFD);

            int responseSize = 1024;
            char buf[responseSize];
            bzero(buf, responseSize);

            read(sockFD, buf, responseSize);
            buf[1023] = '\0';
            cout << buf << endl;
        }

        else if (command == "stop_share")
        {
            isUploaded[userInput[1]].erase(userInput[2]);
        }

        else if (command == "upload_file")
        {

            ifstream_w(sockFD);

            int responseSize = 1024;
            char buf[responseSize];
            bzero(buf, responseSize);

            read(sockFD, buf, responseSize);

            string sbuf = buf;

            if (userInput.size() != 3)
            {
                return 0;
            }
            else if (sbuf == "groupError:")
            {
                cout << "\033[0;91m-> Group doesn't exist\033[0m" << endl;
                return 0;
            }
            else if (sbuf == "restricted:")
            {
                cout << "\033[0;91m-> No access\033[0m" << endl;
                return 0;
            }
            else if (sbuf == "fileError:")
            {
                cout << "\033[0;91m-> File not found\033[0m" << endl;
                return 0;
            }

            if (sbuf == "\033[0;93m---Upload Started---\033[0m")
            {
                cout << sbuf << endl;
            }

            return fileUpload(userInput, sockFD);
        }
        else if (command == "download_file")
        {
            ifstream_w(sockFD);
            int responseSize = 1024;
            char buf[responseSize];
            bzero(buf, responseSize);

            read(sockFD, buf, responseSize);

            string sbuf = buf;

            if (userInput.size() != 4)
            {
                return 0;
            }
            else if (sbuf == "groupError:")
            {
                cout << "\033[0;91m-> Group doesn't exist\033[0m" << endl;
                return 0;
            }
            else if (sbuf == "restricted:")
            {
                cout << "\033[0;91m-> No access\033[0m" << endl;
                return 0;
            }
            else if (sbuf == "nosuchpath:")
            {
                cout << "\033[0;91m-> Directory not found\033[0m" << endl;
                return 0;
            }

            if (sbuf == "\033[0;93m---Download Started---\033[0m")
            {
                cout << sbuf << endl;
            }
            string firstParam = userInput[2] + "~~";
            string secondParam = userInput[3] + "~~";
            string thirdParam = userInput[1];
            string fileDetails = firstParam + secondParam + thirdParam;
            return fileDownload(userInput, sockFD, fileDetails);
        }
    }
    else
    {
        return 0;
    }

    return 0;
}

void setTrackers(string clientInfo, string trackerInfoFile)
{

    ifstream readFile(trackerInfoFile);
    string str = "";

    vector<string> peeraddress = stringParser(clientInfo, ":");
    clientIP = peeraddress[0];
    clientPort = stoi(peeraddress[1]);

    if (readFile.is_open())
    {
        getline(readFile, str);
        tracker1IP = str;
        // cout << "iptracker1 " << iptracker1 << endl;
        str.clear();

        getline(readFile, str);
        tracker1Port = stoi(str);
        // cout << "track1p " << porttracker1 << endl;
        str.clear();

        getline(readFile, str);
        tracker2IP = str;
        // cout << "iptracker2 " << iptracker2 << endl;
        str.clear();

        getline(readFile, str);
        tracker2Port = stoi(str);
        // cout << "track2p " << porttracker2 << endl;
        str.clear();
        readFile.close();
    }

    // cout << tracker1IP << ":" << tracker1Port << endl;
    // cout << tracker2IP << ":" << tracker2Port << endl;
}

vector<string> sstream(string command)
{
    string s = "";
    stringstream ss(command);
    vector<string> tokens;
    while (ss >> s)
    {
        tokens.push_back(s);
    }

    return tokens;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        error("Enter the proper arguments like clientIP and port with tracker file");
    }

    // cout << "BLAA" << endl;

    string clientInfo = argv[1];
    string trackerInfoFile = argv[2];

    setTrackers(clientInfo, trackerInfoFile);
    cout << "\033[1;96m==================Initializing==================\033[0m" << endl;

    int sockFD;
    struct sockaddr_in serverAddress;
    pthread_t serverThread;

    sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFD < 0)
    {
        error("Error in socket creating error");
    }

    int threadCreate = pthread_create(&serverThread, NULL, listenClientRequests, NULL);
    if (threadCreate == -1)
    {
        error("thread");
    }

    if (connectToTrackers(1, serverAddress, sockFD) < 0)
    {
        exit(-1);
    }

    while (1)
    {
        cout << "\033[1;94m\n~> \033[0m";
        string inputLine;
        getline(cin, inputLine);

        if (inputLine.length() >= 1)
        {

            vector<string> tokens = sstream(inputLine);
            string iniCommand = tokens[0];

            if (iniCommand != "login" and !isLoggedIn)
            {
                if (iniCommand != "create_user")
                {
                    cout << "\033[0;91m-> Please Login first\033[0m" << endl;
                    continue;
                }
            }

            int response = send(sockFD, &inputLine[0], strlen(&inputLine[0]), MSG_NOSIGNAL);
            if (response == -1)
            {
                error(strerror(errno));
            }
            executeCommand(tokens, sockFD);
        }
    }

    close(sockFD);
    return 0;
}
