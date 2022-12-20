#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFORequestChannel.h"

mutex fileLock;

// ecgno to use for datamsgs
#define ECGNO 1

using namespace std;

FIFORequestChannel *createNewChan(FIFORequestChannel *cont_chan)
{
    // send new channel request
    MESSAGE_TYPE new_chan = NEWCHANNEL_MSG;
    cont_chan->cwrite(&new_chan, sizeof(MESSAGE_TYPE));

    char name[30];                          // variable to hold name
    cont_chan->cread(name, sizeof(string)); // reading the name from server and holding into variable

    FIFORequestChannel *chan2 = new FIFORequestChannel(name, FIFORequestChannel::CLIENT_SIDE); // dynamically creating new channel as to keep it out of this context
    return chan2;
}

void patient_thread_function(int patient_no, BoundedBuffer *request_buffer, int num_requests)
{
    // functionality of the patient threads
    for (int i = 0; i < num_requests; i++)
    {
        char buf[MAX_MESSAGE];
        datamsg x(patient_no, 0.004 * i, ECGNO);
        memcpy(buf, &x, sizeof(datamsg));
        request_buffer->push(buf, sizeof(datamsg));
    }
}

void file_thread_function(BoundedBuffer *request_buffer, int64_t filesize, char *buf_req, int len, int m)
{
    // functionality of the file thread

    // looping through file and gathering information
    for (int i = 0; i < filesize / m; i++)
    {
        // create file message instance
        filemsg *file_req = (filemsg *)buf_req;

        // change file message variables accordingly
        file_req->offset = m * i;

        // finding length needing to be read
        file_req->length = m;

        // sending file request to the buffer
        request_buffer->push(buf_req, len);
    }
    if (filesize % m != 0) //  checking to see if division does not go in evenly
    {
        int rem = filesize % m; // setting rem to the bits left to make sure to get the last part of the file

        // create file message instance
        filemsg *file_req = (filemsg *)buf_req;

        // change file message variables accordingly
        file_req->offset = filesize - rem;

        // finding length needing to be read
        file_req->length = rem;

        // sending request to the buffer
        request_buffer->push(buf_req, len);
    }
    delete[] buf_req;
}

void worker_thread_function(BoundedBuffer *request_buffer, BoundedBuffer *response_buffer, FIFORequestChannel *w_chan)
{
    // make sure to include quitmsg abiliy that is how these workers will stop
    // functionality of the worker threads
    char buf[MAX_MESSAGE];
    while (true)
    {
        // creating buffer to receive request from request_buffer
        request_buffer->pop(buf, MAX_MESSAGE);

        // casting the buf as MESSAGE_TYPE since we know that it will contain a MESSAGE_TYPE
        MESSAGE_TYPE *type = (MESSAGE_TYPE *)buf;
        if (*type == QUIT_MSG)
        {
            MESSAGE_TYPE q = QUIT_MSG;
            w_chan->cwrite(&q, sizeof(MESSAGE_TYPE)); // sending quit messages to server
            delete w_chan;
            break;
        }
        else if (*type == DATA_MSG)
        {
            datamsg *request = (datamsg *)buf;
            int patient_no = request->person;

            // requesting datamsg from the server
            w_chan->cwrite(buf, sizeof(datamsg)); // question

            // checking for reply from server
            double reply;
            w_chan->cread(&reply, sizeof(double)); // answer

            // creating struct to hold our prepended patient number and then also our data received from the server
            response r(patient_no, reply);

            // creating buffer for sending struct over
            char *buf_resp = new char[sizeof(response)];
            //  copying struct information into the buffer
            memcpy(buf_resp, &r, sizeof(response));

            // pushing information onto the buffer
            response_buffer->push(buf_resp, sizeof(response));
            delete[] buf_resp;
        }
        else if (*type == FILE_MSG)
        {
            // cast the buffer as a filemsg since the type was found to be filemsg
            filemsg *request = (filemsg *)buf;

            // find the filename and create the filepath from the filename
            string filename = buf + sizeof(filemsg);
            string filepath = "received/" + filename;

            // sending message through to server
            w_chan->cwrite(buf, sizeof(filemsg) + filename.size() + 1);

            // creating buffer for the response from the server
            char *buf_resp = new char[request->length];

            // receiving data back from the server
            w_chan->cread(buf_resp, request->length);

            // open the file for writing
            int fd = open(filepath.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

            // setting position of file writing
            lseek(fd, request->offset, SEEK_SET);

            // writing to file the message byte
            if (write(fd, buf_resp, request->length) == -1)
            {
                cout << "eror writing to file" << endl;
                return;
            }

            // closing our file
            close(fd);

            // deleting our buffer
            delete[] buf_resp;
        }
    }
}

void histogram_thread_function(BoundedBuffer *response_buffer, HistogramCollection *hc)
{
    // functionality of the histogram threads
    // creating buffer to receive onformation from response_buffer
    char buf[MAX_MESSAGE];
    while (true)
    {
        // receive information from response_buffer
        response_buffer->pop(buf, sizeof(response));

        // casting our buffer now as a struct to obtain the information
        struct response *r = (response *)buf;
        if (r->patient < 0)
        {
            break;
        }
        // updating the histogram accordingly
        hc->update(r->patient, r->reply);
    }
}

int main(int argc, char *argv[])
{
    int n = 1000;        // default number of requests per "patient"
    int p = 10;          // number of patients [1,15]
    int w = 100;         // default number of worker threads
    int h = 20;          // default number of histogram threads
    int b = 20;          // default capacity of the request buffer (should be changed)
    int m = MAX_MESSAGE; // default capacity of the message buffer
    string f = "";       // name of file to be transferred

    // read arguments
    int opt;
    while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            n = atoi(optarg);
            break;
        case 'p':
            p = atoi(optarg);
            break;
        case 'w':
            w = atoi(optarg);
            break;
        case 'h':
            h = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 'm':
            m = atoi(optarg);
            break;
        case 'f':
            f = optarg;
            break;
        }
    }

    // fork and exec the server
    int pid = fork();
    if (pid == 0)
    {
        execl("./server", "./server", "-m", (char *)to_string(m).c_str(), nullptr);
    }

    // initialize overhead (including the control channel)
    FIFORequestChannel *chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
    HistogramCollection hc;

    // making histograms and adding to collection
    for (int i = 0; i < p; i++)
    {
        Histogram *h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
    thread *file;
    if (!f.empty())
    {
        p = 0;
        h = 0;
        filemsg fm(0, 0);                           // setting up inital file message
        int len = sizeof(filemsg) + (f.size() + 1); // creating size of buffer
        char *buf_req = new char[len];              // creating buffer for request

        memcpy(buf_req, &fm, sizeof(filemsg));        // copying our file message into buffer
        strcpy(buf_req + sizeof(filemsg), f.c_str()); // copying our filname into buffer
        chan->cwrite(buf_req, len);                   // I want the file length

        int64_t filesize = 0;                    // variable to hold file size from cread
        chan->cread(&filesize, sizeof(int64_t)); // creading the filesize from the server

        file = new thread(file_thread_function, &request_buffer, filesize, buf_req, len, m);
    }
    // record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* create all threads here */
    // patient threads
    vector<thread *> patients;
    for (int i = 0; i < p; i++)
    {
        thread *pa = new thread(patient_thread_function, i + 1, &request_buffer, n);
        patients.push_back(pa);
    }
    // worker threads
    vector<thread *> workers;
    for (int i = 0; i < w; i++)
    {
        FIFORequestChannel *work_chan = createNewChan(chan);
        thread *wo = new thread(worker_thread_function, &request_buffer, &response_buffer, work_chan);
        workers.push_back(wo);
    }
    // histogram threads
    vector<thread *> histograms;
    for (int i = 0; i < h; i++)
    {
        thread *hi = new thread(histogram_thread_function, &response_buffer, &hc);
        histograms.push_back(hi);
    }

    /* join all threads here */
    // patient threads
    for (int i = 0; i < p; i++)
    {
        patients.at(i)->join();
        delete patients.at(i);
    }
    // checking if we are doing file transfer
    if (!f.empty())
    {
        file->join();
        delete file;
    }
    // add quit msgs here to tell workers they are done in for loop
    MESSAGE_TYPE q = QUIT_MSG;
    char *quit = new char[sizeof(MESSAGE_TYPE)];
    memcpy(quit, &q, sizeof(MESSAGE_TYPE));
    for (int i = 0; i < w; i++)
    {
        request_buffer.push(quit, sizeof(MESSAGE_TYPE));
    }
    delete[] quit;
    // worker threads
    for (int i = 0; i < w; i++)
    {
        workers.at(i)->join();
        delete workers.at(i);
    }

    // sending our own quit msgs to the response buffer in order to stop the histgram threads
    response r(-1, -1);
    char *quit_h = new char[sizeof(response)];
    memcpy(quit_h, &r, sizeof(response));
    for (int i = 0; i < h; i++)
    {
        response_buffer.push(quit_h, sizeof(response));
    }
    delete[] quit_h;
    // histogram threads
    for (int i = 0; i < h; i++)
    {
        histograms.at(i)->join();
        delete histograms.at(i);
    }
    // record end time
    gettimeofday(&end, 0);

    // print the results
    if (f == "")
    {
        hc.print();
    }
    int secs = ((1e6 * end.tv_sec - 1e6 * start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int)1e6);
    int usecs = (int)((1e6 * end.tv_sec - 1e6 * start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int)1e6);
    std::cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    // quit and close control channel
    // MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite((char *)&q, sizeof(MESSAGE_TYPE));
    std::cout << "All Done!" << endl;
    delete chan;

    // wait for server to exit
    wait(nullptr);
}
