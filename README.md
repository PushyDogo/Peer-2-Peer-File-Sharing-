
## AOS Assignment 3

- To run the code you must have g++ installed.
- Then run the following commands to compile and execute tracker and client

#### TRACKER
 - ` g++ tracker.cpp -o tracker -pthread ` 
 - ` ./tracker tracker_info.txt 1 `

#### Client
 - `g++ client.cpp -o client -lcrypto -pthread ` 
 - `./client 127.0.0.1:2701 tracker_info.txt`

