#include <vector>   // vector, push_back, at
#include <string>   // string
#include <iostream> // cin, getline
#include <fstream>  // ofstream
#include <unistd.h> // getopt, exit, EXIT_FAILURE
#include <assert.h> // assert
#include <thread>   // thread, join
#include <sstream>  // stringstream

#include "BoundedBuffer.h" // BoundedBuffer class

#define MAX_MSG_LEN 256

using namespace std;

/************** Helper Function Declarations **************/

void parse_column_names(vector<string> &_colnames, const string &_opt_input);
void write_to_file(const string &_filename, const string &_text, bool _first_input = false);

/************** Thread Function Definitions **************/

// "primary thread will be a UI data entry point"
void ui_thread_function(BoundedBuffer *bb)
{
    // TODO: implement UI Thread Function

    // string to hold the inputs given
    string input = "";

    // grabbing the first input from user
    cout << "enter data>";
    getline(cin, input);

    // creating buffer to push information into
    char *message = new char[MAX_MSG_LEN];
    while (input != "Exit")
    {

        // copying information into buffer
        strcpy(message, input.c_str());

        // pushing information onto queue
        bb->push(message, input.size() + 1);

        // receiveing more information from user
        cout << "enter data>";
        getline(cin, input);
    }
    delete[] message;
}

// "second thread will be the data processing thread"
// "will open, write to, and close a csv file"
void data_thread_function(BoundedBuffer *bb, string filename, const vector<string> &colnames)
{
    // TODO: implement Data Thread Function
    // (NOTE: use "write_to_file" function to write to file)

    // storing size of columns
    int columns = colnames.size();

    // writing the column names to the file
    for (int i = 0; i < columns; i++)
    {
        string text = i == columns - 1 ? "\n" : ", ";
        write_to_file(filename, colnames.at(i) + text, i == 0);
    }

    //  creating a buffer to receive messages with
    char *message = new char[MAX_MSG_LEN];

    // counter to keep track of when to newline
    int counter = 0;

    while (true)
    {
        // popping off a message from the queue
        bb->pop(message, MAX_MSG_LEN);

        // extracting infromation from buffer
        string msg = message;

        // checking whether to break out or not
        if (msg == "Exit")
        {
            break;
        }
        // figuring out whether we are at the last column or not
        string suffix = counter == columns - 1 ? "\n" : ", ";

        // writing to the file
        write_to_file(filename, msg + suffix, false);

        // incrementing counter
        counter++;
        if (counter == columns)
        {
            counter = 0;
        }
    }
    delete[] message;
}

/************** Main Function **************/

int main(int argc, char *argv[])
{

    // variables to be used throughout the program
    vector<string> colnames;                  // column names
    string fname;                             // filename
    BoundedBuffer *bb = new BoundedBuffer(3); // BoundedBuffer with cap of 3

    // read flags from input
    int opt;
    while ((opt = getopt(argc, argv, "c:f:")) != -1)
    {
        switch (opt)
        {
        case 'c': // parse col names into vector "colnames"
            parse_column_names(colnames, optarg);
            break;
        case 'f':
            fname = optarg;
            break;
        default: // invalid input, based on https://linux.die.net/man/3/getopt
            fprintf(stderr, "Usage: %s [-c colnames] [-f filename]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // TODO: instantiate ui and data threads
    thread uit(ui_thread_function, bb);
    thread datat(data_thread_function, bb, fname, colnames);
    // TODO: join ui_thread
    uit.join();
    // TODO: "Once the user has entered 'Exit', the main thread will
    // "send a signal through the message queue to stop the data thread"
    string q = "Exit";
    char *quit = new char[MAX_MSG_LEN];
    strcpy(quit, q.c_str());
    bb->push(quit, q.size() + 1);
    delete[] quit;
    // TODO: join data thread
    datat.join();
    // CLEANUP: delete members on heap
    delete bb;
}

/************** Helper Function Definitions **************/

// function to parse column names into vector
// input: _colnames (vector of column name strings), _opt_input(input from optarg for -c)
void parse_column_names(vector<string> &_colnames, const string &_opt_input)
{
    stringstream sstream(_opt_input);
    string tmp;
    while (sstream >> tmp)
    {
        _colnames.push_back(tmp);
    }
}

// function to append "text" to end of file
// input: filename (name of file), text (text to add to file), first_input (whether or not this is the first input of the file)
void write_to_file(const string &_filename, const string &_text, bool _first_input)
{
    // based on https://stackoverflow.com/questions/26084885/appending-to-a-file-with-ofstream
    // open file to either append or clear file
    ofstream ofile;
    if (_first_input)
        ofile.open(_filename);
    else
        ofile.open(_filename, ofstream::app);
    if (!ofile.is_open())
    {
        perror("ofstream open");
        exit(-1);
    }

    // sleep for a random period up to 5 seconds
    usleep(rand() % 5000);

    // add data to csv
    ofile << _text;

    // close file
    ofile.close();
}