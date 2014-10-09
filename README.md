PCSCNFC
=======

This programm will get NFC UID via PC/SC and output it to the stdout/socket

#### ToDo:
* Find type of the card
* Work with many devices, not only first one
* Check response for random UID (passports and other stuff)
* Alter the output structure for stdout or socket
* Daemon mode
* Configuration file


#### Protocol of unix-socket 
PCSCNFC can send only 2 types of messages: found new card and the card was removed. Each message is duplicated the entire card number (UID). 

**Message Structure:** 
* First byte: the message size in bytes. Minimum is 2 bytes, and maximum is 255 bytes 
* Second byte: message type. 0x17 - new card. 0xfa - card removed. 
* Subsequent bytes: a unique card number.

**Example of message:** *0x08 0x17 0x34 0xc1 0x05 0x71 0xc4 0x15 0x36* 
* 0x08 - message will consist of 8 bytes .
* 0x17 - new card .
* Sequence 0x34 0xc1 0x05 0x71 0xc4 0x15 0x36 - unique card number (UID).
