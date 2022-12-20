/*
	Original author of the starter code
	Tanzir Ahmed
	Department of Computer Science & Engineering
	Texas A&M University
	Date: 2/8/20

	Please include your Name, UIN, and the date below
	Name: Joshua Downey
	UIN: 728004847
	Date: 09/06/2022
*/
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;

FIFORequestChannel *createNewChan(FIFORequestChannel *cont_chan)
{
	// send new channel request
	MESSAGE_TYPE new_chan = NEWCHANNEL_MSG;
	cont_chan->cwrite(&new_chan, sizeof(MESSAGE_TYPE));

	char name[30];							// variable to hold name
	cont_chan->cread(name, sizeof(string)); // reading the name from server and holding into variable

	FIFORequestChannel *chan2 = new FIFORequestChannel(name, FIFORequestChannel::CLIENT_SIDE); // dynamically creating new channel as to keep it out of this context
	return chan2;
}

double requestDataMsg(int p, double t, int e, FIFORequestChannel *chan)
{
	// example data point request
	char buf[MAX_MESSAGE]; // 256

	datamsg x(p, t, e); /// change from hard coding to real values
	memcpy(buf, &x, sizeof(datamsg));
	chan->cwrite(buf, sizeof(datamsg)); // question

	double reply;
	chan->cread(&reply, sizeof(double)); // answer
	return reply;
}

void requestDataMsg1000(int p, FIFORequestChannel *chan)
{
	ofstream csvFile("./received/x1.csv"); // create file to write to
	for (int i = 0; i < 1000; i++)
	{						  // iterate over first 1000 values
		double t = 0.004 * i; // determine current time value
		// request values of both ecgs
		double ecg1 = requestDataMsg(p, t, 1, chan);
		double ecg2 = requestDataMsg(p, t, 2, chan);
		csvFile << t << "," << ecg1 << "," << ecg2 << "\n"; // write to file
	}
	csvFile.close(); // close file
}

void fileRequest(int m, string filename, FIFORequestChannel *chan)
{
	filemsg fm(0, 0); // setting up inital file message

	int len = sizeof(filemsg) + (filename.size() + 1); // creating size of buffer
	char *buf_req = new char[len];					   // creating buffer for request

	memcpy(buf_req, &fm, sizeof(filemsg));				 // copying our file message into buffer
	strcpy(buf_req + sizeof(filemsg), filename.c_str()); // copying our filname into buffer
	chan->cwrite(buf_req, len);							 // I want the file length;

	int64_t filesize = 0;					 // variable to hold file size from cread
	chan->cread(&filesize, sizeof(int64_t)); // creading the filesize from the server

	char *buf_resp = new char[m]; // buffer for the responses from the server

	int rem = 0;
	if (filesize % m != 0) //  checking to see if division does not go in evenly
	{
		rem = 1; // setting rem to 1 to make sure to get the last bit in our for loop
	}

	// setting up ofstream for file writing and creation
	string filePath = "received/" + filename;
	FILE *filecopy = fopen(filePath.c_str(), "wb");

	// looping through file and gathering information
	for (int i = 0; i < filesize / m + rem; i++)
	{
		// create file message instance
		filemsg *file_req = (filemsg *)buf_req;

		// change file message variables accordingly
		file_req->offset = m * i;

		// finding length needing to be read
		int length = filesize - file_req->offset < m ? filesize - file_req->offset : m;
		file_req->length = length;

		// send request
		chan->cwrite(buf_req, len);

		// receive the response
		chan->cread(buf_resp, length);

		// writing into file
		fwrite(buf_resp, 1, length, filecopy);
	}
	
	fclose(filecopy); //closing file

	// deleting buffers
	delete[] buf_req;
	delete[] buf_resp;
}

int main(int argc, char *argv[])
{
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m = MAX_MESSAGE;
	bool c = false;
	string filename = "";
	vector<FIFORequestChannel *> channels;
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1)
	{
		switch (opt)
		{
		case 'p':
			p = atoi(optarg);
			break;
		case 't':
			t = atof(optarg);
			break;
		case 'e':
			e = atoi(optarg);
			break;
		case 'f':
			filename = optarg;
			break;
		case 'm':
			m = atoi(optarg);
			break;
		case 'c':
			c = true;
		}
	}
	// give arguments for the server
	// server needs './server' , '-m', '<val for -m arg>', 'NULL'
	// fork
	// In the child, run execvp using server arguments
	pid_t pid;
	pid = fork();
	if (pid == -1)
	{
		cout << "error couldn't start child process" << endl;
		return 0;
	}
	else if (pid == 0)
	{
		char *args[] = {const_cast<char *>("./server"), const_cast<char *>("-m"), const_cast<char *>(to_string(m).c_str()), nullptr};
		execvp(args[0], args);
	}

	// creating control channel
	FIFORequestChannel *cont_chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(cont_chan);

	// checking to see if we had a new channel argument
	if (c)
	{
		channels.push_back(createNewChan(cont_chan)); // calling the create channel function and storing it then pushing onto the vector
	}

	FIFORequestChannel chanM = *channels.back(); // creating variable to hold the channel which will be used for communication

	// checking for whether or not the user wants a datamsg or 1000 datamsgs
	if (p > 0 && t > 0 && e > 0)
	{
		double reply = requestDataMsg(p, t, e, &chanM);
		string r = "";
		if(e == 2){
			r = "\r";
		}
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << r << endl;
	}
	else if (p > 0)
	{
		requestDataMsg1000(p, &chanM);
	}

	// requesting file if there is a filename present to request
	if (!filename.empty())
	{
		fileRequest(m, filename, &chanM);
	}

	// closing the channels
	MESSAGE_TYPE q = QUIT_MSG;
	for (int i = channels.size() - 1; i >= 0; i--) // looping from back to front as to make sure the control channel is last to be closed
	{
		channels.at(i)->cwrite(&q, sizeof(MESSAGE_TYPE)); // sending quit messages to servers
		delete channels.at(i);							  // deleting the dynamically allocated channels
	}

	// client waits for the server to exit
	waitpid(pid, 0, 0);
	cout << "Client terminated" << endl;
}