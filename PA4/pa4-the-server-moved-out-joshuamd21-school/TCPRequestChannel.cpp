#include "TCPRequestChannel.h"

using namespace std;

TCPRequestChannel::TCPRequestChannel(const std::string _ip_address, const std::string _port_no)
{

    if (_ip_address == "")
    {
        struct sockaddr_in server;

        // create socket
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            cout << "error creating socket" << endl;
        }

        // provide necessary information for sockaddr_in
        bzero((char *)&server, sizeof(server));
        // Add IPV4 address family to the server address struct
        server.sin_family = AF_INET;
        // convert short from host byte order to network byte order
        server.sin_port = htons(atoi(_port_no.c_str()));
        // ipv4 address, use current ipv4 address (INADDR_ANY)
        server.sin_addr.s_addr = INADDR_ANY;

        // bind the server to a port
        if (bind(sockfd, (const struct sockaddr *)&server, sizeof(server)) == -1)
        {
            cout << "error binding server to port" << endl;
        }

        // start listening to the port and queuing up messages
        listen(sockfd, 5);
    }
    else
    {
        struct sockaddr_in server_info;
        hostent *server;

        // get the server host name
        server = gethostbyname(_ip_address.c_str());

        // provide necessary information for sockaddr_in
        bzero((char *)&server_info, sizeof(server_info));
        bcopy((char *)server->h_addr, (char *)&server_info.sin_addr.s_addr, server->h_length);
        // Add IPV4 address family to the server address struct
        server_info.sin_family = AF_INET;
        // convert short from host byte order to network byte order
        server_info.sin_port = htons(atoi(_port_no.c_str()));

        // creating socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);

        // connecting to server
        if (connect(sockfd, (const struct sockaddr *)&server_info, sizeof(server_info)) == -1)
        {
            cout << "error has occured connecting to server" << endl;
        }
    }
}

TCPRequestChannel::TCPRequestChannel(int _sockfd)
{
    this->sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel()
{
    close(sockfd);
}

int TCPRequestChannel::accept_conn()
{
    struct sockaddr_in storage;
    socklen_t clilen = sizeof(storage);
    return accept(sockfd, (struct sockaddr *)&storage, &clilen);
}

int TCPRequestChannel::cread(void *msgbuf, int msgsize)
{
    return read(sockfd, msgbuf, msgsize);
}

int TCPRequestChannel::cwrite(void *msgbuf, int msgsize)
{
    return write(sockfd, msgbuf, msgsize);
}
