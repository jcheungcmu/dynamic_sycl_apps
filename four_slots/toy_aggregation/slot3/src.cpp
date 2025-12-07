#include "slot3.h"

extern "C" {
  event slot3_src(queue &q, buffer<key_value>& input_buf, size_t num_items) {

    return q.submit([&](handler &h) {
      // Create an accessor with read permission.
      accessor input(input_buf, h, read_only);

      h.single_task<class src_task> ([=]() { 

        packet_data write_data_packet;
        write_data_packet.dest_addr = 0; // will be overwritten
        write_data_packet.src_addr = 0; // will be overwritten 

        key_value local_table;
        key_value data; 

        uint8_t pr_handler_ack; 
        bool pr_handler_ack_valid;

        size_t i;

        for (i = 0; i < num_items; i++) {

          data = input[i];

          if (i == 0) { 
            local_table.key = data.key;
            local_table.value = data.value;
          }
          else if (data.key == local_table.key) {
            local_table.value += data.value;
          }
          else {
            write_data_packet.user = raw_token_user_id; 
            write_data_packet.payload = data; 
            write_data_user_pipe::write(write_data_packet);
          }

        }
        
        if (i == num_items) {
          local_table_pipe::write(local_table); // send local table to results handler

          // send end of stream token first to close PR handler 
          write_data_packet.user = end_of_stream_token_user_id; // end of stream token
          write_data_packet.payload = {0,0}; // payload is don't care
          write_data_user_pipe::write(write_data_packet);

          // wait for OK from PR handler then clean up everything
          while(1) {
            pr_handler_ack = pr_handler_to_src_pipe::read(pr_handler_ack_valid); 

            if (pr_handler_ack_valid) {
              write_data_packet.user = flush_token_user_id; // flush token
              write_data_packet.payload = {0,0}; // payload is don't care
              write_data_user_pipe::write(write_data_packet);
              break;
            }
          }

        }

        // clean up with flush token 

      });

    });
  }
  

  event slot3_results_handler (queue &q, buffer<key_value>& result_buf, buffer<uint8_t>& count_buf) {

    return q.submit([&](handler &h) {

      accessor result(result_buf, h, write_only, no_init);
      accessor count_acc(count_buf, h, write_only, no_init);
      
      h.single_task<class results_handler_task> ([=]() { 

        packet_data read_data_packet;

        key_value data; 
        
        uint8_t count = 0;
        key_value read_local_table;
        
        read_local_table = local_table_pipe::read();
        result[0] = read_local_table;
        count++;

        while(1) {

            read_data_packet = results_pipe::read();

            if (read_data_packet.user == result_token_user_id) { // results
              data = read_data_packet.payload;

              result[count] = data;
              count++;
            }
            else if (read_data_packet.user == flush_token_user_id) { // done
              count_acc[0] = count;
              stop_read_ctrl_pipe::write(1);
              stop_write_ctrl_pipe::write(1);
              stop_read_data_pipe::write(1);

              stop_read_ctrl_user_pipe::write(1);
              stop_write_ctrl_user_pipe::write(1);
              break;
            }

        }
        
      });

    });
  }

  event slot3_overflow_handler (queue &q, buffer<key_value>& overflow_buf, buffer<size_t>& count_buf) { 
    return q.submit([&](handler &h) {

      accessor overflow(overflow_buf, h, write_only, no_init);
      accessor count_acc(count_buf, h, write_only, no_init);

      h.single_task<class overflow_handler> ([=]() { 

        packet_data read_data_packet;
        size_t count = 0; 

        pr_request_packet request;

        while(1) { 
          read_data_packet = overflow_pipe::read();

          if (read_data_packet.user == raw_token_user_id) { 
            overflow[count] = read_data_packet.payload;
            count++;

            // notify PR handler
            request.module_id = read_data_packet.payload.key; 
            request.slot_addr = 0; // unused, cannot request specific slot in this design

            overflow_to_pr_request_filter_pipe::write(request);
          }
          else if (read_data_packet.user == end_of_stream_token_user_id) { 
            request.module_id = stop_pr_kernels_module_id; 
            request.slot_addr = 0; // unused, cannot request specific slot in this design
    
            overflow_to_pr_request_filter_pipe::write(request);
          }
          else if (read_data_packet.user == flush_token_user_id) { 
            count_acc[0] = count;
            break;
          }
        }
        
      });

    });
  }

  event slot3_fork (queue &q) { 
    return q.submit([&](handler &h) {

      h.single_task<class fork_task> ([=]() { 

        packet_data read_data_packet;

        while(1) { 
          read_data_packet = read_data_user_pipe::read();

          if (read_data_packet.user == raw_token_user_id) { 
            overflow_pipe::write(read_data_packet);
          }
          else if (read_data_packet.user == result_token_user_id) { 
            results_pipe::write(read_data_packet);
          }
          else if (read_data_packet.user == end_of_stream_token_user_id) { 
            overflow_pipe::write(read_data_packet);
          }
          else if (read_data_packet.user == flush_token_user_id) { 
            results_pipe::write(read_data_packet);
            overflow_pipe::write(read_data_packet);
            break;
          }
        }

      });

    });
  }

  void slot3_user_control_handler(queue &q) {
    q.submit([&](handler &h) {

      h.single_task<class write_ctrl_handler> ([=]() { 

        packet_ctrl update_dest_packet;
        bool update_dest_packet_valid = false;

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

          // mux ack packets and update dest packets, give priority to update dest packets
          update_dest_packet = pr_handler_to_write_ctrl_pipe::read(update_dest_packet_valid);

          if (update_dest_packet_valid == true) {
            write_ctrl_packet = update_dest_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);
          }
          else if (update_dest_packet_valid == false) { 

            ack_tail_update_packet = write_ack_tail_update_pipe::read(ack_tail_update_packet_valid);

            if (ack_tail_update_packet_valid == true) {

              write_ctrl_packet = ack_tail_update_packet;
              write_ctrl_user_pipe::write(write_ctrl_packet);

            }
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
            else if (read_ctrl_packet.user == ack_tail_update_user_id) {
              read_ctrl_to_pr_handler_pipe::write(read_ctrl_packet); // forward to pr handler
            }

          }

        }
      });
    });

  }


  event slot3_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_dest_addr) {

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

  void slot3_credits_handler(queue &q, uint8_t src_addr) {

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
                // stop_read_ctrl_pipe::write(1);
                // stop_write_ctrl_pipe::write(1);
                // stop_read_data_pipe::write(1);
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


  event pr_handler(queue &q, uint8_t src_addr, uint8_t init_tail_addr) { 
    return q.submit([&](handler &h) {

      h.single_task<class pr_handler_task> ([=]() { 

        pr_request_packet request;
        pr_request_packet response;

        uint8_t tail_addr = init_tail_addr;

        packet_ctrl write_ctrl_packet;
        packet_ctrl read_ctrl_packet;

        while(1) { 

          request = pr_request_filter_to_pr_handler_pipe::read(); 

          if (request.module_id == stop_pr_kernels_module_id) {
              // tell src to send flush token
              pr_handler_to_src_pipe::write(1);

              // clean up pr_request_kernel
              pr_request_pipe::write(request);
              break;
          }
          else { // valid request
              pr_request_pipe::write(request);

              // block until response
              response = pr_ack_pipe::read(); 
              if (response.module_id == request.module_id) { // pr request was successful    
                write_ctrl_packet.dest_addr = tail_addr; // dest is previous tail addr
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.user = update_dest_user_id; // reserved for update dest
                write_ctrl_packet.payload = response.slot_addr; // new module is at this address

                pr_handler_to_write_ctrl_pipe::write(write_ctrl_packet); // send update dest packet

                // wait for ack that tail has been updated, system is now stable
                read_ctrl_packet = read_ctrl_to_pr_handler_pipe::read();

                if (read_ctrl_packet.src_addr == tail_addr) { // ack should always come from previous tail
                  pr_handler_to_pr_request_filter_pipe::write(1); // ack to pr_request_filter, ready to take new request
                }

                tail_addr = response.slot_addr; // update tail addr
              }
              else if (response.module_id != request.module_id) {
                pr_handler_to_pr_request_filter_pipe::write(1); // ack to pr_request_filter, ready to take new request
              }

          }
  
        }

      });

    });
  }

  event pr_request_kernel(queue &q, buffer<pr_request_packet>& a_buf) {

    return q.submit([&](handler &h) {
      accessor a(a_buf, h, write_only, no_init);

      h.single_task<class pr_request> ([=]() { 

        pr_request_packet read_request;
        read_request = pr_request_pipe::read();

        a[0] = read_request; // Store the initial request in the first element of the buffer
      });

    });
  }

  event pr_ack_kernel(queue &q, pr_request_packet response) {

    return q.submit([&](handler &h) {

      h.single_task<class pr_ack> ([=]() { 

        pr_ack_pipe::write(response);
      });

    });
  }

  event pr_request_filter(queue &q) { // telemetry on the overflow rate

  return q.submit([&](handler &h) {

    h.single_task<class pr_request_filter> ([=]() { 

      pr_request_packet request;
      bool request_in_progress = false;

      bool request_valid = false;

      uint8_t request_done;
      bool request_done_valid = false;
      
      while(1) { 
        
        request = overflow_to_pr_request_filter_pipe::read(request_valid);
        request_done = pr_handler_to_pr_request_filter_pipe::read(request_done_valid);

        if (request_done_valid) {
          request_in_progress = false;
        }

        if (request_valid) {
          if (request.module_id == stop_pr_kernels_module_id) {
            pr_request_filter_to_pr_handler_pipe::write(request);
            break;
          }
          else {
            if (request_in_progress == false) {
              pr_request_filter_to_pr_handler_pipe::write(request);
              request_in_progress = true;
            }

            // else ignore the request
          }
        }
        
      }
    });

  });
}


}
