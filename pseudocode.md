
# Pseduocode for the Client and Server

#### By Matthew Getgen

---
## server.c

**send_acknowledgement():**
- make ACK packet with ACK num from SEQ num;
- send packet to client;

**wait_for_acknowledgement(i):**
- if tried less than 8 times:
    - wait to receive packet;
    - if haven't receive acknowledgement within 2 seconds:
        - resend data;
        - wait_for_acknowledgement(i+1);
    - else:
        - if incorrect response:
            - resend data;
            - wait_for_acknowledgement(i+1);
        - else:
            - return;

**send_finale_packet():**
- make FIN packet with same SEQ num as last;
- send_data();
- wait_for_acknowledgement(1);

**send_error_packet():**
- make ERR packet with ERR num;
- send_data();
- wait_for_acknowledgement(1);

**send_file():**
- if you can open file:
    - while not at EOF, read from file:
        - if buffer is not full:
            - write data into buffer;
        - else:
            - send data;
            - wait_for_acknowledgement(1);
    - at end of while; (at this point it has written to buff without sending it)
    - send buff;
    - wait_for_acknowledgement(1);
- else:
    - send ERR to client saying "file not found!" (err 2);
    - wait_for_acknowledgement(1);
    - return -1;
- close file;
- return 0;

**handle_connection():**
- wait to recv data (basically infinitely);
- when received:
    - send_acknowledgement();
    - if packet received is not SEQ or is not SEQ 1:
        - send_error_packet();
        - return;
    - else:
        - send_file();
        - return;

**main():**
- open socket;
- handle_connection();

---
## client.c

**send_acknowledgement():**
- make ACK packet with ACK num from SEQ num;
- send packet to server;

**wait_for_acknowledgement(i):**
- if tried less than 8 times:
    - wait to receive packet;
    - if haven't receive acknowledgement within 2 seconds:
        - resend data;
        - wait_for_acknowledgement(i+1);
    - else:
        - if incorrect response:
            - resend data;
            - wait_for_acknowledgement(i+1);
        - else:
            - return;

**handle_connection():**
- pack_packet();
- send packet and file request;
- wait_for_acknowledgement(1);
- open/make local file
- while packet received is not fin packet and wait for less than 8 times:
    - receive data;
    - if packet is SEQ packet:
        - if seq_num == next:
            - read data;
            - write data into local file;
        - send_acknowledgement;
    - else:
        - close file;
        - if packet is ERR packet:
            - print error and return;
        - else if packet is FIN packet:
            - print finished statement and return;
        - send_acknowledgement;
    - return;

**main():**
- open socket;
- handle_connection();

