//
// Created by Phil Romig on 11/13/18.
//

#include "packetstats.h"

// ****************************************************************************
// * pk_processor()
// *  Most/all of the work done by the program will be done here (or at least it
// *  it will originate here). The function will be called once for every
// *  packet in the savefile.
// ****************************************************************************
void pk_processor(u_char *user, const struct pcap_pkthdr *pkthdr, const u_char *packet) {

    resultsC* results = (resultsC*)user;
    results->incrementTotalPacketCount();
    DEBUG << "Processing packet #" << results->packetCount() << ENDL;
    char s[256]; memset(s,0,256); memcpy(s,ctime(&(pkthdr->ts.tv_sec)),strlen(ctime(&(pkthdr->ts.tv_sec)))-1);
    TRACE << "\tPacket timestamp is " << s;
    TRACE << "\tPacket capture length is " << pkthdr->caplen ;
    TRACE << "\tPacket physical length is " << pkthdr->len ;

    // ***********************************************************************
    // * Process the link layer header
    // *  Hint -> use the ether_header structure defined in
    // ***********************************************************************

	struct ether_header *ether_hdr = (struct ether_header*)(packet); //can asssume all packets start with a flavor of eithernet frame
	uint16_t et = ntohs(ether_hdr->ether_type); //extract the type
	u_int64_t dm = 0;
	u_int64_t sm = 0;
	memcpy(&dm, &(ether_hdr->ether_dhost), 6);
	memcpy(&sm, &(ether_hdr->ether_shost), 6);
	results->newDstMac(dm);
	results->newSrcMac(sm);
    // *******************************************************************
    // * If it's an ethernet packet, extract the src/dst address and  
    // * find the ethertype value to see what the next layer is.
	// * 
	// * If it's not an ethernet packet, count is as "other" and your done
	// * with this packet.
    	// *******************************************************************
	if(et < 1536){ //bytes 13 and 14 contain an ether_length
		//frame 802.3 style, increment that count and don't process packet anymore
		results->newOtherLink(pkthdr->len);
		return;
	}
	else{
		//is Ethernet II frame, get source and destinatino fields
		//MAC address is 6 bytes, but there is no data type stores 6 bytes, so use
		// unsigned long (8 bytes) to store the address and pad the rest with 2 more bytes
		results->newEthernet(pkthdr->len); //creat new ethernet with the length
		
		//check the type code
		if(et == 0x0806){ //type code for ARP
			results->newARP(pkthdr->len);
		}
		else if(et == 0x0800){//type code for IPv4
			results->newIPv4(pkthdr->len);
		}
		else if(et == 0x86DD){ //type code for IPv6
			results->newIPv6(pkthdr->len);
		}
		else{
			//add to OtherNetwork category and dont do any more with the packet
			results->newOtherNetwork(pkthdr->len);
			return;
		}
	}

    // ***********************************************************************
    // * Process the network layer
    // ***********************************************************************
    	// *******************************************************************
	// *  Use ether_type to decide what the next layer is.
	uint8_t proto = -1; //will hold the number for the transport layer protocol
	unsigned int header_length = -1; //will hold the header length
	if(et == 0x0806 || et == 0x86DD){ //type code for ARP and IPv6
		// If it's ARP or IPv6 count it and you are done with this packet.
		//dont do anyting, we alreayd recorded it above
		return;
	}
	else if(et == 0x800){
		//If it's IPv4 extract the src and dst addresses and find the
		//protocol field to see what the next layer is.
		struct ip *ippkt = (struct ip*)(packet + 14);
		results->newDstIPv4(ippkt->ip_dst.s_addr);
		results->newSrcIPv4(ippkt->ip_src.s_addr);
		proto = ippkt->ip_p;
		header_length = ippkt->ip_hl;
	}  
	else{
		//If it's not ARP, IPv4 or IPv6 count it as otherNetwork.
		results->newOtherNetwork(pkthdr->len);
		return;
	}

    // ***********************************************************************
    // * Process the transport layer header
    // ***********************************************************************

    	// *******************************************************************
	// * If the packet is an IPv4 packet, then use the Protcol field
	// * to find out what the next layer is.
	// * 
	// *
	// * If it's UDP or TCP, decode the transport hearder to extract
	// * the src/dst ports and TCP flags if needed.
	// *
	if(proto == IPPROTO_ICMP){
		// * If it's ICMP, count it and you are done with this packet.
		results->newICMP(pkthdr->len);
		return;
	}
	else if(proto == IPPROTO_TCP){
		//if its tcp, count it then decode source and dst ports, and get TCP flags
		results->newTCP(pkthdr->len);
		struct tcphdr *tcp_hdr = (struct tcphdr*)(packet + (4 * header_length) + 14);
		uint16_t srcport = ntohs(tcp_hdr->th_sport);
		uint16_t dstport = ntohs(tcp_hdr->th_dport);
		results->newDstTCP(dstport);
		results->newSrcTCP(srcport);
		std::bitset<9> flags(tcp_hdr->th_flags); //create bitset of flag bits to test (goes right to left)
		if(flags.test(1)){
			results->incrementSynCount();
		}
		if(flags.test(0)){
			results->incrementFinCount();
		}
	}
	else if(proto == IPPROTO_UDP){
		//if its UDP, get the src and dst addresses
		results->newUDP(pkthdr->len);
		struct udphdr *udp_hdr = (struct udphdr*)(packet + (4 * header_length) + 14);
		uint16_t srcport = ntohs(udp_hdr->uh_sport);
		uint16_t dstport = ntohs(udp_hdr->uh_dport);
		results->newDstUDP(dstport);
		results->newSrcUDP(srcport);
	}
	else{
		// * If it's not ICMP, UDP or TCP, count it as otherTransport
		results->newOtherTransport(pkthdr->len);
		return;
	}


    return;
}
