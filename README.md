<h1 align="center">EPS - Endpoint Services</h1>

## Description
This is a simple client-server that uses WebSockets over SSL connection. 
It was created to test some concepts and libraries, so it should not be used as an implementation reference for serious work.

## Dependencies
- Boost.asio
- [Crow](https://crowcpp.org/master)

## How To Build (Linux)
- Install Boost development libraries in your system
    - Download the library from [HERE](https://boostorg.jfrog.io/artifactory/main/release/1.84.0/source/boost_1_84_0.tar.bz2) or go to their page [HERE](https://www.boost.org/users/download/)
    - Example:
```shell
tar xjf boost_1_84_0.tar.bz2
cd boost_1_84_0
./bootstrap.sh --prefix=/usr/local
sudo ./b2 link=static install 
```
- Use CMake to configure the project from the root tree

## How to Run
- Copy the certificates **server.crt** and **server.key** to the same directories where the executables for the client (**eps-client**) and the server (**eps-server**) are located.
- From the command line, run the server: **./eps-server**
- From the command line, run the client: **./eps-client**

### The client will show the following Menu:
```
[MENU] Client (v0.1.0) connected to localhost:8008
   1. Check the server version
   2. Get updates
   3. Push settings changes

Enter your choice (0 to disconnect and quit):
```

### Updating the server version
By default, the server starts with a version greater than the clients (v1.0.5), however it could 
be upgraded to version v0.1.6 which will be broadcast the message to the clients.
```
[MENU] Server (v0.1.5) port: 8008
   1. Update to version 0.1.6 and notify clients

Enter your choice (0 to disconnect and quit): 1
```
