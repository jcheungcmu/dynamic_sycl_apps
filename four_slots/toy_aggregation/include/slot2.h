#ifndef SLOT2_H
#define SLOT2_H

#include <sycl/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>

#include "types.h"
#include "common_pipes.h"
#include "common_const.h"

using namespace sycl;

extern "C" {
  event slot2_agg(queue &q);
  void slot2_user_control_handler(queue &q);
  event slot2_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_dest_addr);
  void slot2_credits_handler(queue &q, uint8_t src_addr);  
}


#endif 