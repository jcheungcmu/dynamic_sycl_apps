#ifndef PIPES_H
#define PIPES_H

#include <sycl/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>
#include "types.h"

using namespace sycl;

using write_data_iopipe = ext::intel::kernel_writeable_io_pipe<pipe_id_0, packet_data, 0>;
using read_data_iopipe = ext::intel::kernel_readable_io_pipe<pipe_id_1, packet_data, 0>;

using write_ctrl_iopipe = ext::intel::kernel_writeable_io_pipe<pipe_id_2, packet_ctrl, 0>;
using read_ctrl_iopipe = ext::intel::kernel_readable_io_pipe<pipe_id_3, packet_ctrl, 0>;

// using write_data_shim_pipe = ext::intel::pipe<class ShimPipeWriteData, packet_data, 0>; 
// using read_data_shim_pipe = ext::intel::pipe<class ShimPipeReadData, packet_data, 0>;

// using write_ctrl_shim_pipe = ext::intel::pipe<class ShimPipeWriteCtrl, packet_ctrl, 0>; 
// using read_ctrl_shim_pipe = ext::intel::pipe<class ShimPipeReadCtrl, packet_ctrl, 0>;

using write_data_user_pipe = ext::intel::pipe<class UserPipeWriteData, packet_data, 0>;
using read_data_user_pipe = ext::intel::pipe<class UserPipeReadData, packet_data, 0>;

using write_ctrl_user_pipe = ext::intel::pipe<class UserPipeWriteCtrl, packet_ctrl, 0>;
using read_ctrl_user_pipe = ext::intel::pipe<class UserPipeReadCtrl, packet_ctrl, 0>;

using write_data_with_translated_addr_pipe = ext::intel::pipe<class UserPipeWriteDataWithTranslatedAddr, packet_data, 0>;
// using read_data_addr_translation_pipe = ext::intel::pipe<class UserPipeReadDataAddrTranslation, packet_data, 0>;

using credit_pipe = ext::intel::pipe<class CreditPipe, uint8_t, 0>;
using update_dest_pipe = ext::intel::pipe<class UpdateDestPipe, packet_ctrl, 0>;


// stops credit handler
using stop_read_data_pipe = ext::intel::pipe<class StopReadDataPipe, uint8_t, 0>;
using stop_write_ctrl_pipe = ext::intel::pipe<class StopWriteCtrlPipe, uint8_t, 0>;
using stop_read_ctrl_pipe = ext::intel::pipe<class StopReadCtrlPipe, uint8_t, 0>;

// stops user control handler
using stop_write_ctrl_user_pipe = ext::intel::pipe<class StopWriteCtrlUserPipe, uint8_t, 0>;
using stop_read_ctrl_user_pipe = ext::intel::pipe<class StopReadCtrlUserPipe, uint8_t, 0>;

using write_ack_data_pipe = ext::intel::pipe<class WriteAckDataPipe, packet_ctrl, 0>;
using write_ack_tail_update_pipe = ext::intel::pipe<class WriteAckTailUpdatePipe, packet_ctrl, 0>;

// extern "C" {
//   event iopipe_shim(queue &q);
//   event credits_handler(queue &q, uint8_t src_addr);
// }

#endif