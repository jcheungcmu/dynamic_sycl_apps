#ifndef SLOT1_H
#define SLOT1_H

#include <sycl/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>

#include "types.h"
#include "common_pipes.h"
#include "common_const.h"

using namespace sycl;

extern "C" {
  event slot1_agg(queue &q);
  void slot1_user_control_handler(queue &q);
  event slot1_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_dest_addr);
  void slot1_credits_handler(queue &q, uint8_t src_addr);  
}


#endif 