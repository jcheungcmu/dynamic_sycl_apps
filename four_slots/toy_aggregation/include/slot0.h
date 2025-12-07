#ifndef SLOT0_H
#define SLOT0_H

#include <sycl/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>

#include "types.h"
#include "common_pipes.h"
#include "common_const.h"

using namespace sycl;

extern "C" {
  event slot0_agg(queue &q);
  void slot0_user_control_handler(queue &q);
  event slot0_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_dest_addr);
  void slot0_credits_handler(queue &q, uint8_t src_addr);  
}


#endif 