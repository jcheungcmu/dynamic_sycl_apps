#ifndef CONST_H
#define CONST_H

const uint8_t max_credits = 1;
const uint8_t max_slots = 4;

// packet user IDs
const uint8_t end_of_stream_token_user_id = 0; // data 
const uint8_t flush_token_user_id = 1; // data 
const uint8_t result_token_user_id = 2; // data 
const uint8_t raw_token_user_id = 3; // data 
const uint8_t update_dest_user_id = 4; // ctrl
const uint8_t ack_data_user_id = 5; // ctrl
const uint8_t ack_tail_update_user_id = 6; // ctrl

// PR module IDs 
// const uint8_t module_id_agg = 0;
const uint8_t stop_pr_kernels_module_id = 255;
#endif
