#include "includes.h"

// ***************************************************************************
// * ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose
// *
// * These are the functions you need to fill in.
// ***************************************************************************


// ***************************************************************************
// * Because of the way the simulator works you will likey need global variables
// * You can define those here.
// ***************************************************************************
const int BUFFER_SIZE = 10; //serves as window size (number of unacked packets)
const float ALPHA = 0.125; //alpha used in calculated estimated_rtt
const float BETA = 0.25; //beta used in calulated dev_rtt

int current_seq_a;
int current_ack_a;
float timer_start_a; //keeps track of the time a's timer is started

int current_seq_b; //shouldn't have to update (unless doing extra credit)
int current_ack_b;

float estimated_rtt; //averaged round trip time
float sample_rtt; //measured round trip time for a packet
float dev_rtt; //rtt deviation
float timeout_interval;

std::vector<struct pkt> packet_buffer; //holds unacked packets. conditions in place to ensure size <= 10

// ***************************************************************************
// * The following routine will be called once (only) before any other
// * entity A routines are called. You can use it to do any initialization
// ***************************************************************************
void A_init() {
    current_ack_a = 0;
    current_seq_a = 0;

    timeout_interval = 3.0;
    dev_rtt = 0; //deviation starts at 0
}

// ***************************************************************************
// * The following rouytine will be called once (only) before any other
// * entity B routines are called. You can use it to do any initialization
// ***************************************************************************
void B_init() {
    current_ack_b = 0;
    current_seq_b = 0;
}


// ***************************************************************************
// * Called from layer 5, passed the data to be sent to other side 
// ***************************************************************************
int A_output(struct msg message) {
    std::cout << "Layer 4 on side A has recieved a message from the application that should be sent to side B: "
              << message << std::endl;

    if(packet_buffer.size() >= BUFFER_SIZE){
        return 0; //buffer is full so refuse the message
    }

    //incriment a's seq number
    current_seq_a++;

    //create a new packet that contains the message  that is to be sent
    int checksum = create_checksum(current_seq_a, current_ack_a, message.data);
    struct pkt my_pkt = *(make_pkt(current_seq_a, current_ack_a, checksum, &message));
    
    //add packet to buffer because it hasn't been acked yet
    packet_buffer.push_back(my_pkt);

    //send packet to the network for delivery to the other side
    simulation->tolayer3(A, my_pkt);

    if(packet_buffer.size() == 1){ //only start timer if there is 1 unacked packet. timer should be for oldest unacked packet
        simulation->starttimer(A, timeout_interval); //start the timer for the packet. increment should be slightly longer than averate_rtt
        timer_start_a = simulation->getSimulatorClock();
    }
    
    return (1); /* Return a 0 to refuse the message */
}


// ***************************************************************************
// * Called from layer 3, when a packet arrives for layer 4 on side A
// ***************************************************************************
void A_input(struct pkt packet) {
    std::cout << "Layer 4 on side A has recieved a packet sent over the network from side B:" << packet << std::endl;

    //check for corruption
    if(is_corrupted(&packet)){
        std::cout << "ACK FROM B WAS CORRUPTED" << std::endl;
        return;
    }

    //do a cumulative ack
    std::vector<struct pkt> still_unacked;
    for(struct pkt p: packet_buffer){
        if(p.seqnum > packet.acknum){ //not cumulatively acked
            still_unacked.push_back(p);
        }
        if(p.seqnum == packet.acknum){ //got ack for packet that we're expecting, so stop the timer
            simulation->stoptimer(A);
            sample_rtt = simulation->getSimulatorClock() - timer_start_a; //calculated new sample_rtt
            update_estimated_rtt(sample_rtt);
        }
    }
    packet_buffer.clear();
    packet_buffer = still_unacked; //update unacked packet list
    if(packet_buffer.size() != 0){
        std::sort(packet_buffer.begin(), packet_buffer.end(), compare_func);
        //start a new timer for the other packets
        simulation->starttimer(A, timeout_interval);
    }
    
}


// ***************************************************************************
// * Called from layer 5, passed the data to be sent to other side
// ***************************************************************************
int B_output(struct msg message) {
    std::cout << "Layer 4 on side B has recieved a message from the application that should be sent to side A: "
              << message << std::endl;

    return (1); /* Return a 0 to refuse the message */
}


// ***************************************************************************
// // called from layer 3, when a packet arrives for layer 4 on side B 
// ***************************************************************************
void B_input(struct pkt packet) {
    std::cout << "Layer 4 on side B has recieved a packet from layer 3 sent over the network from side A:" << packet
              << std::endl;

    //check if the packet got corrupted. if so, return and don't send ack
    if(is_corrupted(&packet)){
        std::cout << "PACKET FROM A WAS CORRUPTED" << std::endl;
        return;
    }
    if(packet.seqnum != current_ack_b + 1){ //got an out-of-order packet, discard it
        std::cout << "GOT AN OUT OF ORDER PACKET FROM A" << std::endl;
        return;
    }

    //takes data out of packet, copies it to a message
    struct msg message;
    for(int i = 0; i < 20; i++){
        message.data[i] = packet.payload[i];
    }

    //forward message to the application layer
    simulation->tolayer5(B, message);
    
    //create packet and send ack back to A
    current_ack_b++; //increment b ack number
    int checksum = create_checksum(current_seq_b, current_ack_b, message.data);
    struct pkt ack = *(make_pkt(current_seq_b, current_ack_b, checksum, &message));
    simulation->tolayer3(B, ack);
}


// ***************************************************************************
// * Called when A's timer goes off 
// ***************************************************************************
void A_timerinterrupt() {
    std::cout << "Side A's timer has gone off." << std::endl;
    //first update the timeout interval
    timeout_interval *= 2;

    //if there is a timeout, we have to resend all of the unacked packets
    std::sort(packet_buffer.begin(), packet_buffer.end(), compare_func); //sort unacked packets by sequence number
    for(int i = 0; i < packet_buffer.size(); i++){
        simulation->tolayer3(A, packet_buffer[i]);
        if(i == 0){
            simulation->starttimer(A, timeout_interval); //restart the simulator clock for first packet resent
            timer_start_a = simulation->getSimulatorClock();
        }
    }
}

// ***************************************************************************
// * Called when B's timer goes off 
// ***************************************************************************
void B_timerinterrupt() {
    std::cout << "Side B's timer has gone off." << std::endl;
}

//simple checksum function
//adds sequence number, ack number, and the ascii int value of each character of the message
int create_checksum(int seq, int ack, char* message){
    int sum = seq + ack;
    for(int i = 0; i < 20; i++){
        sum += message[i];
    }
    return sum;
}

//check if given packet was corrupted
bool is_corrupted(pkt* packet){
    return packet->checksum != create_checksum(packet->seqnum, packet->acknum, packet->payload);
}

//helper function to create packet
struct pkt* make_pkt(int seq, int ack, int checksum, struct msg* message){
    struct pkt* my_packet = new pkt;
    my_packet->acknum = ack;
    my_packet->checksum = checksum;
    my_packet->seqnum = seq;
    for(int i = 0; i < 20; i++){
        my_packet->payload[i] = message->data[i];
    }
    return my_packet;
}

//formula for updating the estimated round trip time and the timeout interval
void update_estimated_rtt(float sample_rtt){
    estimated_rtt = ((1-ALPHA) * estimated_rtt) + (ALPHA * sample_rtt);
    float diff = sample_rtt - estimated_rtt;
    if(diff < 0){
        diff *= -1;
    }
    dev_rtt = (1 - BETA) * dev_rtt + BETA * diff;
    timeout_interval = estimated_rtt + 4 * dev_rtt;
}

//function to use as the comparison function when sorting 
//a vector or packets with the algorithm library
bool compare_func(struct pkt p1, struct pkt p2) { return (p1.seqnum < p2.seqnum); }
