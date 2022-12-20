#include "BoundedBuffer.h"

using namespace std;

BoundedBuffer::BoundedBuffer(int _cap) : cap(_cap)
{
    // modify as needed
}

BoundedBuffer::~BoundedBuffer()
{
    // modify as needed
}

void BoundedBuffer::push(char *msg, int size)
{
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    vector<char> request;
    for (int i = 0; i < size; i++)
    {
        request.push_back(msg[i]);
    }
    // 2. Wait until there is room in the queue (i.e., queue length is less than cap)
    unique_lock<mutex> lk(mx);
    overflow.wait(lk, [&, this]()
                  { return this->q.size() < (long unsigned int)this->cap; });
    // 3. Then push the vector at the end of the queue
    q.push(request);
    lk.unlock();
    // 4. Wake up threads that were waiting for push
    underflow.notify_one();
}

int BoundedBuffer::pop(char *msg, int size)
{
    // 1. Wait until the queue has at least 1 item
    unique_lock<mutex> lk(mx);
    underflow.wait(lk, [&, this]()
                   { return this->q.size() > 0; });
    // 2. Pop the front  item of the queue. The popped item is a vector<char>
    vector<char> front = q.front();
    q.pop();
    lk.unlock();
    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    assert(front.size() <= (long unsigned int)size);

    char *buf = new char[front.size()];
    for (long unsigned int i = 0; i < front.size(); i++)
    {
        buf[i] = front.at(i);
    }
    memcpy(msg, buf, front.size());
    delete[] buf;
    // 4. Wake up threads that were waiting for pop
    overflow.notify_one();
    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    return front.size();
}

size_t BoundedBuffer::size()
{
    return q.size();
}