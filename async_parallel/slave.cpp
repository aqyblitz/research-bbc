// All necessary imports.
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/socket.h>

using namespace std;

// TODO
// - we're sending the data back to all the sockets for relaxation...including the one we got it from. Nah.

void setup();
void teardown();
void emscriptem_main(int32_t& s, int32_t& k, int32_t& r_s, bool& sent, bool& c_recv, int32_t& c, bool& loop);

int main(int argc, char *argv[])
{
    setup();

    int32_t s=0;
    int32_t r_s=0;
    int32_t c;
    int32_t k;
    bool sent=false;
    bool c_recv=false;
    bool loop=true;
    // Initialize constants
    while(loop)
    {
        emscriptem_main(s,k,r_s,sent,c_recv,c,loop); // pass in variables for constants by reference
    }

    teardown();

    return 0;
}

void setup()
{
    // Setup socket stuff.
    // Initialize arrays.
}

void teardown()
{
    // Close connections.
    // Delete stuff allocated with new.
}

void build_fd_table()
{
    // Build the fd table. To be called after EVERY select.
}

void relax()
{
    // Relax the data
}

void emscriptem_main(int32_t& s, int32_t& k, int32_t& r_s, bool& sent, bool& c_recv, int32_t& c, bool& loop)   // add constants to signature
{
    int n;

    if(s==0) // Initialized
    {
        if(!sent)
        {
            // [SEND] id
            sent=true;
        }
        if(r_s==0)
        {
            // n= read(); //information
            if(n==4*sizeof(int32_t))// [TODO] this is where we'd combine things into a struct
            {
                // store constants
                r_s=1;
            }
        }
        if(r_s==1)
        {
            //n=read(); //data block
            if(n==0) // Change 0 to send_size!
            {
                // store data block
                s=1;
                c_recv=true;
                return;
            }
        }
    }

    if(s==1)	// Command mode
    {
        if(c_recv) // Receiving command
        {
            // n=read(); // command
            if(n==2*sizeof(int32_t)) // what I believe is the size of the command
            {
                // the stuff's read into an array, so grab:
                // c
                // k
            }
            c_recv=false;
        }
        else // Acting on command
        {
            if(c==0)
            {
                // send row
                c_recv=true;
            }
            if(c==1)
            {
                // n=read(); // broadcast row
                if(n==0) // vertex_total*sizeof(int32_t)
                {
                    relax();
                    // send ack
                }
            }
        }
    }
}
