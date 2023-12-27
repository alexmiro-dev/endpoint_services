<h1 align="center">EPS - Endpoint Services</h1>

## Description
This is a simple client-server that uses WebSockets over SSL connection.

## Dependencies
- Boost.asio
- [Crow](https://crowcpp.org/master)

## How To Build (Linux)
- Install Boost development libraries in your system
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