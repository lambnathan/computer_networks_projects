
// ***********************************************************
// * Any additional include files should be added here.
// ***********************************************************

// ***********************************************************
// * Any functions you want to add should be included here.
// ***********************************************************
struct pkt *make_pkt(int seq,int ack, int checksum, struct msg *message);

int create_checksum(int seq, int ack, char* message);
bool is_corrupted(struct pkt *packet);
void update_estimated_rtt(float sample_rtt);
bool compare_func(struct pkt p1, struct pkt p2);