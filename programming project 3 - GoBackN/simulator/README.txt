Nathan Lambert
No special directions to compile and run, just type 'make' and run the executable with the appropriate arguments.

Corruption is detected by comparing the checksum from the packet that was received to a checksum that is calculated
all of the data in the packet itself. The checksum is calculated by adding the ack number, seq number, and the 
ASCII values of all the elements in the message together.

There would be some cases where corruption would not be detected. For example, if the acknum is corrupted to one
more than its actual value while the seq num is corrupted to one less than its actual value.  
