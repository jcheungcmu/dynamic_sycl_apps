#include "slot1.h"

extern "C" {
  event slot1_agg(queue &q) {

    return q.submit([&](handler &h) {

      h.single_task<class agg_task> ([=]() { 

        packet_data write_data_packet;
        packet_data read_data_packet;

        write_data_packet.dest_addr = 0; // will be overwritten
        write_data_packet.src_addr = 0; // will be overwritten

        key_value local_table;

        uint8_t isFirstKey = 1;

        while(1) {

          read_data_packet = read_data_user_pipe::read();

          if (read_data_packet.user == raw_token_user_id) {

            if (isFirstKey == 1) {
              local_table.key = read_data_packet.payload.key;
              local_table.value = read_data_packet.payload.value;
              isFirstKey = 0;
            } 
            else { 
              if (read_data_packet.payload.key == local_table.key) {
                local_table.value += read_data_packet.payload.value;
              }
              else { // forward the key
                write_data_packet.user = read_data_packet.user; 
                write_data_packet.payload = read_data_packet.payload;
                write_data_user_pipe::write(write_data_packet);
              }
            } 


          }
          else if (read_data_packet.user == result_token_user_id || read_data_packet.user == end_of_stream_token_user_id) { 
            // forward the result or end of stream token
            write_data_packet.user = read_data_packet.user; 
            write_data_packet.payload = read_data_packet.payload;
            write_data_user_pipe::write(write_data_packet);
          }
          else if (read_data_packet.user == flush_token_user_id) {

            if (isFirstKey == 0) {
              // there is an aggregated result to send
              write_data_packet.user = result_token_user_id; // tag as result 
              write_data_packet.payload = local_table;
              write_data_user_pipe::write(write_data_packet);
            }

            // forward the flush token 
            write_data_packet.user = read_data_packet.user;
            write_data_packet.payload = read_data_packet.payload; // payload is don't care
            write_data_user_pipe::write(write_data_packet); // forward the flush token
            break;
          }
        }
        
      });

    });
  }

  event slot1_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_dest_addr) {

    return q.submit([&](handler &h) {

      h.single_task<class write_data_translation_task> ([=]() { 

        packet_data write_data_packet;
        packet_data read_data_packet;

        bool read_data_valid = false;
        uint8_t dest_addr = init_dest_addr; // always initially set to head addr
        uint8_t src_addr = init_src_addr;

        packet_ctrl update_dest_packet;
        bool update_dest_valid = false; 

        packet_ctrl write_ctrl_packet;

        while(1) {

          read_data_packet = write_data_user_pipe::read(read_data_valid);
          update_dest_packet = update_dest_pipe::read(update_dest_valid);

          // race on dest_addr doesn't matter
          if (update_dest_valid) { 
            dest_addr = update_dest_packet.payload;

            write_ctrl_packet.dest_addr = update_dest_packet.src_addr;
            write_ctrl_packet.user = ack_tail_update_user_id; 
            write_ctrl_packet.src_addr = src_addr; 
            write_ctrl_packet.payload = 0; // don't care

            write_ack_tail_update_pipe::write(write_ctrl_packet); // ack tail update
          }

          if (read_data_valid) {
            write_data_packet.user = read_data_packet.user;
            write_data_packet.payload = read_data_packet.payload;
            write_data_packet.src_addr = src_addr;
            write_data_packet.dest_addr = dest_addr;
            
            write_data_with_translated_addr_pipe::write(write_data_packet);

            if (write_data_packet.user == flush_token_user_id) {
              break;
            }
          }
          
        }

        
      });

    });
  }

  void slot1_user_control_handler(queue &q) {
    q.submit([&](handler &h) {

      h.single_task<class write_ctrl_handler> ([=]() { 

        packet_ctrl ack_tail_update_packet;

        bool ack_tail_update_packet_valid = false;

        packet_ctrl write_ctrl_packet;

        uint8_t read_stop;
        bool stop_valid = false;

        while(1) {

          read_stop = stop_write_ctrl_user_pipe::read(stop_valid);
          
          if (stop_valid) { 
            break;
          }

          ack_tail_update_packet = write_ack_tail_update_pipe::read(ack_tail_update_packet_valid);

          if (ack_tail_update_packet_valid == true) {

            write_ctrl_packet = ack_tail_update_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);
          }

        }

      });
    });

    q.submit([&](handler &h) {

      h.single_task<class read_ctrl_handler> ([=]() { 
        packet_ctrl read_ctrl_packet;
        bool read_ctrl_valid = false;

        uint8_t read_stop;
        bool stop_valid = false;

        while(1) {

          read_stop = stop_read_ctrl_user_pipe::read(stop_valid);

          if (stop_valid) { 
            break;
          }    

          read_ctrl_packet = read_ctrl_user_pipe::read(read_ctrl_valid);

          if (read_ctrl_valid) {

            if (read_ctrl_packet.user == update_dest_user_id) { 
              update_dest_pipe::write(read_ctrl_packet); // forward to addr translation
            }

          }

        }
      });
    });

  }

  void slot1_credits_handler(queue &q, uint8_t src_addr) {

    q.submit([&](handler &h) {

      h.single_task<class write_data_credits_handler> ([=]() { 

        packet_data write_data_packet;
        uint8_t total_credits = max_credits; // init to max credits 
        
        uint8_t new_credit;

        bool read_data_valid = false;

        bool credit_valid = false;
        while(1) {

          if (total_credits > 0) {

            write_data_packet = write_data_with_translated_addr_pipe::read(read_data_valid);

            if (read_data_valid) {

              write_data_iopipe::write(write_data_packet); 

              // flush token 
              if (write_data_packet.user == flush_token_user_id) {
                stop_read_ctrl_pipe::write(1);
                stop_write_ctrl_pipe::write(1);
                stop_read_data_pipe::write(1);
                stop_read_ctrl_user_pipe::write(1);
                stop_write_ctrl_user_pipe::write(1);
                break;
              }
              
              total_credits--;

            }

          } 

          if (total_credits < max_credits) { 
            // read new credit
            new_credit = credit_pipe::read(credit_valid);

            if (credit_valid) {
              total_credits += 1;
            }
          }


        }

      });

    });

    q.submit([&](handler &h) {

      h.single_task<class read_data_credits_handler> ([=]() { 

        bool read_data_valid = false;

        packet_data read_data_packet;
        packet_ctrl write_ctrl_packet;

        uint8_t read_stop;
        bool stop_valid = false;

        while(1) {

          read_stop = stop_read_data_pipe::read(stop_valid);

          if (stop_valid) { 
            break;
          }

          read_data_packet = read_data_iopipe::read(read_data_valid); // block until data comes in
          
          if (read_data_valid) {

            if (read_data_packet.user != flush_token_user_id) { // ack non flush tokens
              write_ctrl_packet.dest_addr = read_data_packet.src_addr;
              write_ctrl_packet.user = ack_data_user_id; // reserved for ack
              write_ctrl_packet.src_addr = src_addr; 
              write_ctrl_packet.payload = 0; // don't care

              write_ack_data_pipe::write(write_ctrl_packet); // ack
            }

            read_data_user_pipe::write(read_data_packet); // block until successful write
          }

        }

      });

    });

    q.submit([&](handler &h) {

      h.single_task<class read_ctrl_credits_handler> ([=]() { 

        packet_ctrl read_ctrl_packet;
        bool read_ctrl_valid = false;

        uint8_t read_stop;
        bool stop_valid = false;

        while(1) {

          read_stop = stop_read_ctrl_pipe::read(stop_valid);

          if (stop_valid) { 
            break;
          }    

          read_ctrl_packet = read_ctrl_iopipe::read(read_ctrl_valid);

          if (read_ctrl_valid) {

            if (read_ctrl_packet.user == ack_data_user_id) { // credit ack, no need to forward to user kernel
              credit_pipe::write(1); // block until successful write
            }
            else {
              read_ctrl_user_pipe::write(read_ctrl_packet); // forward to user kernel
            }

          }

        }

      });

    });

    q.submit([&](handler &h) {

      h.single_task<class write_ctrl_credits_handler> ([=]() { 

        packet_ctrl ack_data_packet;
        bool ack_data_packet_valid;

        packet_ctrl user_packet;
        bool user_packet_valid;

        packet_ctrl write_ctrl_packet;
        bool send_packet = false;

        uint8_t read_stop;
        bool stop_valid = false;



        while(1) {

          read_stop = stop_write_ctrl_pipe::read(stop_valid);
          
          if (stop_valid) { 
            break;
          }

          user_packet = write_ctrl_user_pipe::read(user_packet_valid);

          if (user_packet_valid) {
            write_ctrl_packet = user_packet;
            send_packet = true;
          }

          // lowest priority to data acks
          else if (user_packet_valid == false) {

            ack_data_packet = write_ack_data_pipe::read(ack_data_packet_valid);

            if (ack_data_packet_valid) {
              write_ctrl_packet = ack_data_packet;
              send_packet = true;
            }
          }

          if (send_packet) {
            write_ctrl_iopipe::write(write_ctrl_packet);
            send_packet = false;
          }
        }

      });

    });
    
  }

}
