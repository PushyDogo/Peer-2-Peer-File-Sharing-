#include <bits/stdc++.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

using namespace std;

string tracker1IP;
string tracker2IP;
string curTrackerIP;
string seederFileName;
uint16_t tracker1Port;
uint16_t tracker2Port;
uint16_t curTrackerPort;

unordered_map<string, string> loginCredentails;
unordered_map<string, bool> isLoggedIn;
unordered_map<string, string> fileSize;
unordered_map<string, string> groupAdminMap;

#define BUFFER 524288
#define SIZE 32768

vector<string> allGroups;
string client_UID = "";
string client_GID = "";
unordered_map<string, set<string>> groupMemberMap;
unordered_map<string, set<string>> grpPendngRequests;

vector<int> userIDS;

unordered_map<string, string> userToPortMap;
unordered_map<string, string> piecewiseHash;
unordered_map<string, string> fullHash;
unordered_map<string, string> fileToFilePath;
unordered_map<string, string> downloadedFiles;
unordered_map<string, unordered_map<string, set<string>>> filesSeeded;

void error(const char *msg)
{
    cout << msg << endl;
    exit(1);
}

bool pathExists(const string &s)
{
    struct stat buffer;
    return (stat(s.c_str(), &buffer) == 0);
}

int createUser(vector<string> command)
{
    if (loginCredentails.find(command[1]) != loginCredentails.end())
    {
        return -1;
    }
    else
    {
        loginCredentails.insert(make_pair(command[1], command[2]));
    }
    return 0;
}

int canLogin(vector<string> userInput, string clientUID)
{

    string userID = userInput[1];
    string password = userInput[2];

    if (loginCredentails.find(userID) == loginCredentails.end())
    {
        return -1;
    }

    if (loginCredentails[userID] != password)
    {
        return -1;
    }

    if (isLoggedIn.find(userID) != isLoggedIn.end())
    {
        if (!isLoggedIn[userID])
        {
            isLoggedIn[userID] = true;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        isLoggedIn.insert(make_pair(userID, true));
    }

    return 0;
}

vector<string> stringParser(string address, string delimiter)
{

    vector<string> list;
    string s = string(address);
    size_t pos = 0;
    while ((pos = s.find(delimiter)) != string::npos)
    {
        list.push_back(s.substr(0, pos));
        s.erase(0, pos + delimiter.size());
    }
    list.push_back(s);
    return list;
}
void ifstream_r(int sockFD)
{
    char sync[4];
    read(sockFD, sync, 4);
}

void uploadFile(vector<string> command, int clientSocketFD, string clientUID)
{

    string groupID = command[2];
    string response = "\033[0;93m--------------------------\033[0m";
    write(clientSocketFD, &response[0], response.size());
    ifstream_r(clientSocketFD);

    if (groupMemberMap.find(groupID) != groupMemberMap.end())
    {
        if (groupMemberMap[groupID].find(clientUID) != groupMemberMap[groupID].end())
        {
            struct stat downPath;
            const string s = command[1];
            auto response = stat(s.c_str(), &downPath);

            if (response == 0)
            {
                int buffer = 524288;
                char fileDetails[buffer] = {0};
                string response = "\033[0;93m---Upload Started---\033[0m";
                write(clientSocketFD, &response[0], response.size());

                string sFileDetails = string(fileDetails);

                if (read(clientSocketFD, fileDetails, buffer))
                {
                    if (string(fileDetails) == "error")
                        return;

                    vector<string> uploadedFileDetails = stringParser(string(fileDetails), "~~");
                    //fdet = [filepath, peer address, file size, file hash, piecewise hash]

                    string combinedHash = "";

                    int i = 4;
                    while (i < uploadedFileDetails.size())
                    {
                        combinedHash += uploadedFileDetails[i];
                        if (i == uploadedFileDetails.size() - 1)
                        {
                            break;
                        }
                        else
                        {
                            combinedHash += "~~";
                        }
                        i++;
                    }

                    vector<string> fullPath = stringParser(string(uploadedFileDetails[0]), "/");
                    string filename = fullPath[fullPath.size() - 1];
                    piecewiseHash[filename] = combinedHash;
                    fullHash[filename] = uploadedFileDetails[3];

                    // cout << fullHash[filename] << endl;

                    if (filesSeeded[groupID].find(filename) == filesSeeded[groupID].end())
                    {
                        filesSeeded[groupID].insert({filename, {clientUID}});
                    }
                    else
                    {
                        filesSeeded[groupID][filename].insert(clientUID);
                    }
                    fileSize[filename] = uploadedFileDetails[2];

                    string response = "\033[0;92m---Done---\033[0m";
                    write(clientSocketFD, &response[0], response.size());
                }
            }
            else
            {
                string reply = "fileError:";
                write(clientSocketFD, &reply[0], reply.size());
            }
        }
        else
        {
            string reply = "restricted:";
            write(clientSocketFD, &reply[0], reply.size());
        }
    }
    else
    {
        string reply = "groupError:";
        write(clientSocketFD, &reply[0], reply.size());
    }
}

void downloadFile(vector<string> command, int clientSocketFD, string clientUID)
{
    string groupID = command[1];
    string fileName = command[2];
    string path = command[3];
    string response = "\033[0;93m--------------------------\033[0m";
    write(clientSocketFD, &response[0], response.size());
    ifstream_r(clientSocketFD);

    if (groupMemberMap.find(groupID) != groupMemberMap.end())
    {
        if (groupMemberMap[groupID].find(clientUID) != groupMemberMap[groupID].end())
        {
            struct stat downPath;
            const string s = path;
            auto response = stat(s.c_str(), &downPath);

            if (response == 0)
            {
                int buffer = 524288;
                char fileDetails[buffer] = {0};
                string response = "\033[0;93m---Download Started---\033[0m";
                write(clientSocketFD, &response[0], response.size());

                if (read(clientSocketFD, fileDetails, buffer))
                {
                    string sFileDetails = string(fileDetails);
                    vector<string> uploadedFileDetails = stringParser(sFileDetails, "~~");

                    string seederList = "";

                    if (filesSeeded[groupID].find(uploadedFileDetails[0]) != filesSeeded[groupID].end())
                    {
                        set<string> seeds = filesSeeded[groupID][uploadedFileDetails[0]];

                        for (string seed : seeds)
                        {
                            if (isLoggedIn[seed])
                            {
                                seederList += userToPortMap[seed] + "~~";
                            }
                        }

                        seederList += fileSize[uploadedFileDetails[0]];
                        write(clientSocketFD, &seederList[0], seederList.size());

                        ifstream_r(clientSocketFD);

                        string fileDet = uploadedFileDetails[0];
                        string pieceHash = piecewiseHash[fileDet];
                        write(clientSocketFD, &pieceHash[0], pieceHash.length());

                        ifstream_r(clientSocketFD);
                        string finalHash = fullHash[fileDet];
                        write(clientSocketFD, &finalHash[0], finalHash.length());

                        filesSeeded[groupID][fileName].insert(clientUID);
                    }
                    else
                    {
                        string reply = "---File Not Found---";
                        write(clientSocketFD, &reply[0], reply.size());
                    }
                }
            }
            else
            {
                string reply = "nosuchpath:";
                write(clientSocketFD, &reply[0], reply.size());
                return;
            }
        }
        else
        {
            string reply = "restricted:";
            write(clientSocketFD, &reply[0], reply.size());
        }
    }
    else
    {
        string reply = "groupError:";
        write(clientSocketFD, &reply[0], reply.size());
    }
}

int create_group(vector<string> command, int client_socket, string clientUID)
{
    //inpt - [create_group gid]
    string groupID = command[1];

    int i = 0;
    while (i < allGroups.size())
    {
        if (allGroups[i] == groupID)
        {
            return 0;
        }
        i++;
    }
    groupAdminMap.insert(make_pair(groupID, clientUID));
    allGroups.push_back(groupID);
    groupMemberMap[groupID].insert(clientUID);
    return 1;
}

void list_groups(vector<string> command, int clientSocketFD)
{

    string allgroups = "\033[0;93m---All Groups---\033[0m";
    write(clientSocketFD, &allgroups[0], allgroups.size());

    ifstream_r(clientSocketFD);

    if (allGroups.size() > 0)
    {
        string reply = "";
        for (string group : allGroups)
        {
            reply += group + "~~";
        }
        write(clientSocketFD, &reply[0], reply.size());
    }
    else
    {
        string response = "No groups found~~";
        write(clientSocketFD, &response[0], response.size());
        return;
    }
}

void join_group(vector<string> command, int clientSocketFD, string clientUID)
{
    //inpt - [join_group gid]
    string groupID = command[1];

    if (groupAdminMap.find(groupID) != groupAdminMap.end())
    {
        if (groupMemberMap[groupID].find(clientUID) != groupMemberMap[groupID].end())
        {
            string response = "\033[0;91m---Already in the group---\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
        else
        {
            grpPendngRequests[groupID].insert(clientUID);
            string response = "\033[0;92m---Sent join request---\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
    }
    else
    {
        string response = "\033[0;91m---Group ID does not exist---\033[0m";
        write(clientSocketFD, &response[0], response.size());
    }
}

void list_requests(vector<string> command, int clientSocketFD, string clientUID)
{

    string response = "\033[0;93m--------------------------\033[0m";
    write(clientSocketFD, &response[0], response.size());

    ifstream_r(clientSocketFD);

    string groupID = command[1];

    cout << "-------------" << endl;
    // cout << clientUID << endl;
    cout << clientUID << endl;
    cout << "-------------" << endl;

    if (groupAdminMap.find(groupID) == groupAdminMap.end())
    {
        string reply = "err1";
        write(clientSocketFD, &reply[0], reply.size());
    }

    else if (groupAdminMap[groupID] != clientUID)
    {
        string reply = "err1";
        write(clientSocketFD, &reply[0], reply.size());
    }

    else if (grpPendngRequests[groupID].size() != 0)
    {
        string reply = "";
        for (auto i = grpPendngRequests[groupID].begin(); i != grpPendngRequests[groupID].end(); i++)
        {
            reply += string(*i) + "~~";
        }
        write(clientSocketFD, &reply[0], reply.length());
    }

    else
    {
        string reply = "err2";
        write(clientSocketFD, &reply[0], reply.size());
    }
}

void accept_request(vector<string> command, int clientSocketFD, string clientUID)
{
    string response = "\033[0;93m--------------------------\033[0m";
    write(clientSocketFD, &response[0], response.size());

    ifstream_r(clientSocketFD);

    string groupID = command[1];
    string userID = command[2];

    if (groupAdminMap.find(groupID) != groupAdminMap.end())
    {
        if (groupAdminMap.find(groupID)->second == clientUID)
        {
            grpPendngRequests[groupID].erase(userID);
            groupMemberMap[groupID].insert(userID);
            string response = "\033[0;92m---Request accepted---\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
        else
        {
            string response = "\033[0;91m---Not authorized---\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
    }
    else
    {
        string response = "\033[0;91m---No such group exists---\033[0m";
        write(clientSocketFD, &response[0], response.size());
    }
}

void leave_group(vector<string> command, int clientSocketFD, string clientUID)
{

    string response = "\033[0;93m--------------------------\033[0m";
    write(clientSocketFD, &response[0], response.size());

    ifstream_r(clientSocketFD);

    string groupID = command[1];

    if (groupAdminMap.find(groupID) == groupAdminMap.end())
    {
        string response = "\033[0;91m---Group does not exist---\033[0m";
        write(clientSocketFD, &response[0], response.size());
    }
    else if (groupMemberMap[groupID].find(clientUID) == groupMemberMap[groupID].end())
    {
        string response = "\033[0;91m---Not a member of the group---\033[0m";
        write(clientSocketFD, &response[0], response.size());
    }
    else if (groupMemberMap[groupID].find(clientUID) != groupMemberMap[groupID].end())
    {
        if (groupAdminMap[groupID] == clientUID)
        {
            groupAdminMap.erase(groupID);
            groupMemberMap.erase(groupID);
            allGroups.erase(remove(allGroups.begin(), allGroups.end(), groupID), allGroups.end());
            // allGroups.erase(groupID);
            string response = "\033[0;91m---Group Deleted (as you were owner)---\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
        else
        {
            groupMemberMap[groupID].erase(clientUID);
            string response = "\033[0;92m---Group left---\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
    }
}

void list_files(vector<string> command, int clientSocketFD, string clientUID)
{

    string response = "\033[0;93m--------------------------\033[0m";
    write(clientSocketFD, &response[0], response.size());

    ifstream_r(clientSocketFD);

    string groupID = command[1];

    if (groupAdminMap.find(groupID) != groupAdminMap.end())
    {
        if (filesSeeded[groupID].size() == 0)
        {
            string response = "\033[0;91m---No files to share\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
        else if (groupMemberMap[groupID].find(clientUID) == groupMemberMap[groupID].end())
        {
            string response = "\033[0;91m---Not a member of the group\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
        else
        {
            string reply = "";
            for (auto seed : filesSeeded[groupID])
            {
                reply += seed.first + "~~";
            }
            int effectiveLength = reply.size() - 2;
            reply = reply.substr(0, effectiveLength);
            write(clientSocketFD, &reply[0], reply.length());
        }
    }
    else
    {
        string response = "\033[0;91m----Invalid Group ID\033[0m";
        write(clientSocketFD, &response[0], response.size());
    }
}

void stop_share(vector<string> command, int clientSocketFD, string clientUID)
{
    string groupID = command[1];
    string fileName = command[2];

    if (groupAdminMap.find(groupID) != groupAdminMap.end())
    {
        if (filesSeeded[groupID].find(fileName) != filesSeeded[groupID].end())
        {
            filesSeeded[groupID][fileName].erase(clientUID);
            if (filesSeeded[groupID][fileName].size() == 0)
            {
                filesSeeded[groupID].erase(fileName);
            }
            string response = "\033[0;92m---Stopped Seeding---\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
        else
        {
            string response = "\033[0;91m---File not present---\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }
    }
    else
    {
        string response = "\033[0;91m---Invalid Group ID\033[0m";
        write(clientSocketFD, &response[0], response.size());
    }
}

void processRequest(vector<string> userCommand, int clientSocketFD, string clientUID, string clientGID)
{
    string command = userCommand[0];
    // cout << "***************" << endl;
    // for (auto c : userCommand)
    // {
    //     cout << c << endl;
    // }
    // cout << "***************" << endl;

    if (command == "create_user")
    {
        if (userCommand.size() != 3)
        {
            write(clientSocketFD, "Invalid argument count", 22);
        }
        else
        {
            auto response = createUser(userCommand);

            if (response < 0)
            {
                write(clientSocketFD, "User exists", 11);
            }
            else
            {
                write(clientSocketFD, "Account created", 15);
            }

            // if (loginCredentails.find(userCommand[1]) != loginCredentails.end())
            // {
            //     write(clientSocketFD, "User exists", 11);
            // }
            // else
            // {
            //     loginCredentails.insert({userCommand[1], userCommand[2]});
            //     write(clientSocketFD, "Account created", 15);
            // }
        }
    }

    else if (command == "login")
    {
        // cout << "***************" << endl;
        // for (auto c : userCommand)
        // {
        //     cout << c << endl;
        // }
        // cout << "***************" << endl;
        if (userCommand.size() != 3)
        {
            write(clientSocketFD, "Invalid argument count", 22);
        }
        else
        {
            int response = canLogin(userCommand, clientUID);
            // cout << response << endl;
            if (response == 0)
            {
                write(clientSocketFD, "Login Successful", 16);
                int buffer = 96;
                char buf[buffer];
                read(clientSocketFD, buf, buffer);
                clientUID = userCommand[1];
                userToPortMap[clientUID] = string(buf);
            }
            else if (response > 0)
            {
                write(clientSocketFD, "You already have one active session", 35);
            }
            else if (response < 0)
            {
                write(clientSocketFD, "Username/password incorrect", 28);
            }
        }
    }

    else if (command == "logout")
    {
        isLoggedIn[clientUID] = false;
        write(clientSocketFD, "Logout Successful", 17);
    }

    else if (command == "create_group")
    {
        if (userCommand.size() != 2)
        {
            write(clientSocketFD, "Invalid argument count", 22);
            return;
        }
        else
        {
            int response = create_group(userCommand, clientSocketFD, clientUID);
            if (response >= 0)
            {
                clientGID = userCommand[1];
                write(clientSocketFD, "Group created", 13);
            }
            else
            {
                write(clientSocketFD, "Group exists", 12);
            }
        }
    }

    else if (command == "list_groups")
    {
        if (userCommand.size() != 1)
        {
            write(clientSocketFD, "Invalid argument count", 22);
            return;
        }
        else
        {
            list_groups(userCommand, clientSocketFD);
        }
    }

    else if (command == "join_group")
    {
        if (userCommand.size() != 2)
        {
            string reply = "---Invalid Input---";
            write(clientSocketFD, &reply[0], reply.size());
            return;
        }
        else
        {
            join_group(userCommand, clientSocketFD, clientUID);
        }
    }

    else if (command == "list_requests")
    {

        cout << "***************" << endl;
        for (auto c : userCommand)
        {
            cout << c << endl;
        }
        cout << "***************" << endl;
        if (userCommand.size() != 2)
        {
            string reply = "---Invalid Input---";
            write(clientSocketFD, &reply[0], reply.size());
            return;
        }
        else
        {
            list_requests(userCommand, clientSocketFD, clientUID);
        }
        cout << "***************" << endl;
        for (auto c : userCommand)
        {
            cout << c << endl;
        }
        cout << "***************" << endl;
    }

    else
    {
        string reply = "---Invalid Input---";
        write(clientSocketFD, &reply[0], reply.size());
        return;
    }
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

void handleClientRequest(int clientSocketFD)
{

    string clientUID = "";
    string clientGID = "";

    while (1)
    {
        char userInput[1024] = {0};

        int response1 = read(clientSocketFD, userInput, 1024);
        if (response1 <= 0)
        {
            isLoggedIn[clientUID] = false;
            close(clientSocketFD);
            break;
        }

        string s;
        string input = string(userInput);

        vector<string> userCommand = sstream(input);

        // cout << "***************" << endl;
        // for (auto c : command)
        // {
        //     cout << c << endl;
        // }
        // cout << "***************" << endl;

        // processRequest(command, clientSockFD, clientUID, clientGID);

        string command = userCommand[0];
        // cout << "***************" << endl;
        // for (auto c : userCommand)
        // {
        //     cout << c << endl;
        // }
        // cout << "***************" << endl;

        if (command == "create_user")
        {
            if (userCommand.size() != 3)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                auto response = createUser(userCommand);

                if (response < 0)
                {
                    string response = "\033[0;91m---User exists---\033[0m";
                    write(clientSocketFD, &response[0], response.size());
                }
                else
                {
                    string response = "\033[0;92m---Account Created---\033[0m";
                    write(clientSocketFD, &response[0], response.size());
                }

                // if (loginCredentails.find(userCommand[1]) != loginCredentails.end())
                // {
                //     write(clientSocketFD, "User exists", 11);
                // }
                // else
                // {
                //     loginCredentails.insert({userCommand[1], userCommand[2]});
                //     write(clientSocketFD, "Account created", 15);
                // }
            }
        }

        else if (command == "login")
        {
            // cout << "***************" << endl;
            // for (auto c : userCommand)
            // {
            //     cout << c << endl;
            // }
            // cout << "***************" << endl;
            if (userCommand.size() != 3)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                int response = canLogin(userCommand, clientUID);
                // cout << response << endl;
                if (response == 0)
                {
                    string reply = "---Login Successful---";
                    write(clientSocketFD, &reply[0], reply.size());
                    int buffer = 96;
                    char buf[buffer];
                    read(clientSocketFD, buf, buffer);
                    clientUID = userCommand[1];
                    userToPortMap[clientUID] = string(buf);
                }
                else if (response > 0)
                {
                    string response = "\033[0;91m---Session already active---\033[0m";
                    write(clientSocketFD, &response[0], response.size());
                }
                else if (response < 0)
                {
                    string response = "\033[0;91m---Credentials are not corect---\033[0m";
                    write(clientSocketFD, &response[0], response.size());
                }
            }
        }

        else if (command == "logout")
        {
            isLoggedIn[clientUID] = false;
            string response = "\033[0;92m---Logout Successful---\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }

        else if (command == "create_group")
        {
            if (userCommand.size() != 2)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                int response = create_group(userCommand, clientSocketFD, clientUID);
                if (response > 0)
                {
                    clientGID = userCommand[1];
                    string response = "\033[0;92m---New group created (you are owner)---\033[0m";
                    write(clientSocketFD, &response[0], response.size());
                }
                else
                {
                    string response = "\033[0;91m---Group already exists---\033[0m";
                    write(clientSocketFD, &response[0], response.size());
                }
            }
        }

        else if (command == "list_groups")
        {
            if (userCommand.size() != 1)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                list_groups(userCommand, clientSocketFD);
            }
        }

        else if (command == "join_group")
        {
            if (userCommand.size() != 2)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                join_group(userCommand, clientSocketFD, clientUID);
            }
        }

        else if (command == "list_requests")
        {

            // cout << "***************" << endl;
            // for (auto c : userCommand)
            // {
            //     cout << c << endl;
            // }
            // cout << "***************" << endl;
            if (userCommand.size() != 2)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                list_requests(userCommand, clientSocketFD, clientUID);
            }
        }

        else if (command == "requests")
        {

            // cout << "***************" << endl;
            // for (auto c : userCommand)
            // {
            //     cout << c << endl;
            // }
            // cout << "***************" << endl;
            if (userCommand.size() != 3)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                list_requests(userCommand, clientSocketFD, clientUID);
            }
        }

        else if (command == "accept_request")
        {
            if (userCommand.size() != 3)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                accept_request(userCommand, clientSocketFD, clientUID);
            }
        }

        else if (command == "upload_file")
        {
            if (userCommand.size() != 3)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                uploadFile(userCommand, clientSocketFD, clientUID);
            }
        }

        else if (command == "list_files")
        {
            if (userCommand.size() != 2)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                list_files(userCommand, clientSocketFD, clientUID);
            }
        }
        else if (command == "stop_share")
        {
            if (userCommand.size() != 3)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                stop_share(userCommand, clientSocketFD, clientUID);
            }
        }
        else if (command == "show_downloads")
        {
            string response = "\033[0;93m---Loading...\033[0m";
            write(clientSocketFD, &response[0], response.size());
        }

        else if (command == "download_file")
        {
            if (userCommand.size() != 4)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                downloadFile(userCommand, clientSocketFD, clientUID);
            }
        }

        else if (command == "leave_group")
        {
            if (userCommand.size() != 2)
            {
                string reply = "---Invalid Input---";
                write(clientSocketFD, &reply[0], reply.size());
            }
            else
            {
                leave_group(userCommand, clientSocketFD, clientUID);
            }
            cout << "----------------------" << endl;
            cout << "After leave group" << endl;
            cout << "----------------------" << endl;
        }

        else
        {
            string reply = "---Invalid Input---";
            write(clientSocketFD, &reply[0], reply.size());
        }
    }
    close(clientSocketFD);
}

void *check_input(void *arg)
{
    while (true)
    {
        string inputline;
        getline(cin, inputline);
        if (inputline == "quit")
        {
            exit(0);
        }
    }
}

void setTrackers(string trackerInfoFile, string trackerNum)
{

    ifstream readFile(trackerInfoFile);
    string str = "";

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

    if (trackerNum == "1")
    {
        curTrackerIP = tracker1IP;
        curTrackerPort = tracker1Port;
    }
    else
    {
        curTrackerIP = tracker2IP;
        curTrackerPort = tracker2Port;
    }

    // cout << tracker1IP << ":" << tracker1Port << endl;
    // cout << tracker2IP << ":" << tracker2Port << endl;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        error("Input Format: <tracker info file name> and <tracker_number>");
    }

    string trackerInfoFile = argv[1];
    string trackerNum = argv[2];
    setTrackers(trackerInfoFile, trackerNum);

    cout << "\033[1;93m==================Initializing==================\033[0m" << endl;

    // int tracker_socket;
    // struct sockaddr_in address;
    // int opt = 1;
    // int addrlen = sizeof(address);
    // pthread_t exitDetectionThreadId;

    int sockfd, newsockfd, portno;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("Error Opening socket!");

    bzero((char *)&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(curTrackerPort);

    inet_pton(AF_INET, curTrackerIP.c_str(), &server_addr.sin_addr);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error("Binding failed!");

    if (listen(sockfd, 3) < 0)
    {
        error("listen failed");
    }

    vector<thread> threadVector;

    pthread_t exitDetectionThreadId;
    if (pthread_create(&exitDetectionThreadId, NULL, check_input, NULL) == -1)
    {
        error("p thread");
    }

    while (true)
    {
        client_len = sizeof(client_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
        if (newsockfd < 0)
        {
            error("Couldn't accept a client");
        }
        threadVector.push_back(thread(handleClientRequest, newsockfd));
    }
    for (int i = 0; i < threadVector.size(); i++)
    {
        threadVector[i].join();
    }
    return 0;
}
