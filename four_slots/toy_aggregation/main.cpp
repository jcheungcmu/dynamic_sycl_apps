#include <sycl/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>
#include "types.h"
#include "common_const.h"

#include <dlfcn.h>
#include <algorithm>
#include <vector>

using namespace sycl;
using namespace std;

// Create an exception handler for asynchronous SYCL exceptions
static auto exception_handler = [](sycl::exception_list e_list) {
  for (std::exception_ptr const &e : e_list) {
    try {
      std::rethrow_exception(e);
    }
    catch (std::exception const &e) {
#if _DEBUG
      std::cout << "Failure" << std::endl;
#endif
      std::terminate();
    }
  }
};

int main() {

  auto platforms = sycl::platform::get_platforms();

  cout << "Getting platforms\n";

  for (auto platform : sycl::platform::get_platforms())
  {
      std::cout << "\n\n\n\nPlatform: "
                << platform.get_info<sycl::info::platform::name>()
                << std::endl;

      for (auto device : platform.get_devices())
      {
          std::cout << "\n\n\n\n\t****************Device: "
                    << device.get_info<sycl::info::device::name>()
                    << std::endl;
      }
  }

  // dynamic loading flow for splitting kernels across homogeneous fpgas

  auto slot0_lib = dlopen("/home/jcheung2/multi_fpga/four_slots/aggv2_dev/slot0/helper.so", RTLD_NOW);
  auto slot0_agg = (event (*)(queue&))dlsym(slot0_lib, "slot0_agg");
  auto slot0_credits_handler = (void (*)(queue&, uint8_t))dlsym(slot0_lib, "slot0_credits_handler");
  auto slot0_addr_translation = (event (*)(queue&, uint8_t, uint8_t))dlsym(slot0_lib, "slot0_addr_translation");
  auto slot0_user_control_handler = (void (*)(queue&))dlsym(slot0_lib, "slot0_user_control_handler");

  auto slot1_lib = dlopen("/home/jcheung2/multi_fpga/four_slots/aggv2_dev/slot1/helper.so", RTLD_NOW);
  auto slot1_agg = (event (*)(queue&))dlsym(slot1_lib, "slot1_agg");
  auto slot1_credits_handler = (void (*)(queue&, uint8_t))dlsym(slot1_lib, "slot1_credits_handler");
  auto slot1_addr_translation = (event (*)(queue&, uint8_t, uint8_t))dlsym(slot1_lib, "slot1_addr_translation");
  auto slot1_user_control_handler = (void (*)(queue&))dlsym(slot1_lib, "slot1_user_control_handler");

  auto slot2_lib = dlopen("/home/jcheung2/multi_fpga/four_slots/aggv2_dev/slot2/helper.so", RTLD_NOW);
  auto slot2_agg = (event (*)(queue&))dlsym(slot2_lib, "slot2_agg");
  auto slot2_credits_handler = (void (*)(queue&, uint8_t))dlsym(slot2_lib, "slot2_credits_handler");
  auto slot2_addr_translation = (event (*)(queue&, uint8_t, uint8_t))dlsym(slot2_lib, "slot2_addr_translation");
  auto slot2_user_control_handler = (void (*)(queue&))dlsym(slot2_lib, "slot2_user_control_handler");

  auto slot3_lib = dlopen("/home/jcheung2/multi_fpga/four_slots/aggv2_dev/slot3/src.so", RTLD_NOW);
  auto slot3_src     =  (event (*)(queue&, buffer<key_value>&, size_t))dlsym(slot3_lib, "slot3_src");
  auto slot3_user_control_handler = (void (*)(queue&))dlsym(slot3_lib, "slot3_user_control_handler");
  auto slot3_results_handler     =  (event (*)(queue&, buffer<key_value>&, buffer<uint8_t>&))dlsym(slot3_lib, "slot3_results_handler");
  auto slot3_overflow_handler     =  (event (*)(queue&, buffer<key_value>&, buffer<size_t>&))dlsym(slot3_lib, "slot3_overflow_handler");
  auto slot3_fork     =  (event (*)(queue&))dlsym(slot3_lib, "slot3_fork");
  auto slot3_addr_translation = (event (*)(queue&, uint8_t, uint8_t))dlsym(slot3_lib, "slot3_addr_translation");
  auto slot3_credits_handler = (void (*)(queue&, uint8_t))dlsym(slot3_lib, "slot3_credits_handler");
  auto slot3_pr_handler = (event (*)(queue&, uint8_t, uint8_t))dlsym(slot3_lib, "pr_handler");
  auto slot3_pr_request_kernel = (event (*)(queue&, buffer<pr_request_packet>&))dlsym(slot3_lib, "pr_request_kernel");
  auto slot3_pr_ack_kernel = (event (*)(queue&, pr_request_packet))dlsym(slot3_lib, "pr_ack_kernel");
  auto slot3_pr_request_filter = (event (*)(queue&))dlsym(slot3_lib, "pr_request_filter");

  size_t N = 3000000;

  uint8_t addr_s0 = 0;
  uint8_t addr_s1 = 1; 
  uint8_t addr_s2 = 2;
  uint8_t addr_s3 = 3;


  std::vector<key_value> src_mem(N);
  std::vector<key_value> results_mem(4);
  std::vector<uint8_t> results_count_mem(1);

  std::vector<key_value> overflow_mem(N);
  std::vector<size_t> overflow_count_mem(1);
  std::vector<pr_request_packet> pr_request(1);

  results_count_mem[0] = 0;
  overflow_count_mem[0] = 0;
  pr_request[0] = {0,0};

  // key value of stop_pr_kernels_module_id (255) is not allowed
  for (size_t i = 0; i < N; i++) {
    key_value kv;
    kv.value = 1;

    if (i == 0) {
      kv.key = 0;
    }
    else if (i < 1000000) {
      kv.key = 1;
    }
    else if (i < 2000000) { 
      kv.key = 2;
    }
    else {
      kv.key = 3;
    }
    // kv.key = i%4;

    if (i < 4) {
      results_mem[i] = {0,0};
    }

    src_mem[i] = kv;
    overflow_mem[i] = {0,0};
  }


  buffer<key_value> buf_src_mem(&src_mem[0], N);
  buffer<key_value> buf_results_mem(&results_mem[0], 4);
  buffer<uint8_t> buf_results_count_mem(&results_count_mem[0], 1);

  buffer<key_value> buf_overflow_mem(&overflow_mem[0], N);
  buffer<size_t> buf_overflow_count_mem(&overflow_count_mem[0], 1);

  buffer<pr_request_packet> buf_pr_request(&pr_request[0], 1);
  

  // static test
  // try {

  //   // slots appear as two different fpga devices
  //   cout << "CREATING q0\n";
  //   queue q0(platforms[1].get_devices()[0], exception_handler);
  //   cout << "CREATING q1\n";
  //   queue q1(platforms[1].get_devices()[1], exception_handler);
  //   cout << "CREATING q2\n";
  //   queue q2(platforms[1].get_devices()[2], exception_handler);
  //   cout << "CREATING q3\n";
  //   queue q3(platforms[1].get_devices()[3], exception_handler);

  //   // setup ioshims and credit handlers on all slots
  //   slot0_credits_handler(q0, addr_s0);
  //   slot1_credits_handler(q1, addr_s1);
  //   slot2_credits_handler(q2, addr_s2);
  //   slot3_credits_handler(q3, addr_s3);

  //   // setup address translation on all slots
  //   // 3 -> 0 -> 1 -> 2 -> 3 
  //   slot3_addr_translation(q3, addr_s3, addr_s0);
  //   slot0_addr_translation(q0, addr_s0, addr_s1);
  //   slot1_addr_translation(q1, addr_s1, addr_s2);
  //   slot2_addr_translation(q2, addr_s2, addr_s3);

  //   // setup user control handlers on all slots
  //   slot0_user_control_handler(q0);
  //   slot1_user_control_handler(q1);
  //   slot2_user_control_handler(q2);
  //   slot3_user_control_handler(q3);

  //   // set up aggregation kernels on slots 0,1,2
  //   slot2_agg(q2);
  //   slot1_agg(q1);
  //   slot0_agg(q0);

    // // slot3 specific stuff
    // slot3_fork(q3);

    // slot3_pr_request_filter(q3);
    // slot3_pr_handler(q3, addr_s3, addr_s3);
    // slot3_pr_request_kernel(q3, buf_pr_request);

    // slot3_results_handler(q3, buf_results_mem, buf_results_count_mem);
    // slot3_overflow_handler(q3, buf_overflow_mem, buf_overflow_count_mem);
    // slot3_src(q3, buf_src_mem, N);

  // } catch (std::exception const &e) {
  //   cout << "An exception is caught while computing on device.\n";
  //   terminate();
  // }




  // dynamic version

  try {

    // slots appear as two different fpga devices
    cout << "CREATING q0\n";
    queue q0(platforms[1].get_devices()[0], exception_handler);
    cout << "CREATING q1\n";
    queue q1(platforms[1].get_devices()[1], exception_handler);
    cout << "CREATING q2\n";
    queue q2(platforms[1].get_devices()[2], exception_handler);
    cout << "CREATING q3\n";
    queue q3(platforms[1].get_devices()[3], exception_handler);

    // initialize the program to use only 1 slot 
    slot3_credits_handler(q3, addr_s3);
    slot3_addr_translation(q3, addr_s3, addr_s3);
    slot3_user_control_handler(q3);

    slot3_fork(q3);

    // set up pr stuff
    slot3_pr_request_filter(q3);
    slot3_pr_handler(q3, addr_s3, addr_s3); // init tail to self
    auto ev_pr_req = slot3_pr_request_kernel(q3, buf_pr_request);

    slot3_results_handler(q3, buf_results_mem, buf_results_count_mem);
    slot3_overflow_handler(q3, buf_overflow_mem, buf_overflow_count_mem);
    slot3_src(q3, buf_src_mem, N);

    int slots_used = 1; 

    event slot2_closed;
    while (1) {

      cout << "Waiting for PR request...\n";
      ev_pr_req.wait();

      uint8_t request_id;

      {
        host_accessor<pr_request_packet> host_pr_request(buf_pr_request);
        request_id = host_pr_request[0].module_id;
      }

      if (request_id == stop_pr_kernels_module_id) {
        cout << "Stop received from PR handler\n";
        break;
      }
      else {
        cout << "PR request received.\n";
        cout << "Request ID: " << (int)request_id << "\n";

        pr_request_packet response;


        if (slots_used == 1 && request_id == 1) { 
          response.module_id = request_id;
          response.slot_addr = addr_s0;

          slot0_credits_handler(q0, addr_s0);
          slot0_addr_translation(q0, addr_s0, addr_s3); // new slots always send to head
          slot0_agg(q0);
          slot0_user_control_handler(q0);

          slots_used++;

        }

        else if (slots_used == 2 && request_id == 2) { 
          response.module_id = request_id;
          response.slot_addr = addr_s1; 

          slot1_credits_handler(q1, addr_s1);
          slot1_addr_translation(q1, addr_s1, addr_s3); // new slots always send to head
          slot1_agg(q1);
          slot1_user_control_handler(q1);

          slots_used++;

        }

        else if (slots_used == 3 && request_id == 3) { 
          response.module_id = request_id;
          response.slot_addr = addr_s2; 

          slot2_credits_handler(q2, addr_s2);
          slot2_closed = slot2_addr_translation(q2, addr_s2, addr_s3); // new slots always send to head
          slot2_agg(q2);
          slot2_user_control_handler(q2);

          slots_used++;

        }
        else {
          response.module_id = stop_pr_kernels_module_id; // indicate failure
          response.slot_addr = 4;
        }


        // Reset PR request monitor
        ev_pr_req = slot3_pr_request_kernel(q3, buf_pr_request);
        cout << "PR request monitor reset.\n";
        // Send PR ack
        slot3_pr_ack_kernel(q3, response);
        cout << "Responded to request for ID=" << (int)request_id << " with response ID=" << (int)response.module_id << "\n";
      }
      
    }

    slot2_closed.wait();

  } catch (std::exception const &e) {
    cout << "An exception is caught while computing on device.\n";
    terminate();
  }

  host_accessor<uint8_t> host_results_count_mem(buf_results_count_mem);
  host_accessor<size_t> host_overflow_count_mem(buf_overflow_count_mem);

  cout << "Results count: " << (int)host_results_count_mem[0] << "\n";
  cout << "Overflow count: " << host_overflow_count_mem[0] << "\n";

  host_accessor<key_value> host_results_mem(buf_results_mem);
  host_accessor<key_value> host_overflow_mem(buf_overflow_mem);

  for (uint8_t i = 0; i < (int)host_results_count_mem[0]; i++) {
    cout << "results_mem[" << (int)i << "] = {"
         << (int)host_results_mem[i].key << ", "
         << (int)host_results_mem[i].value << "}\n";
  }

  std::vector<uint8_t> overflow_keys;
  std::vector<size_t> overflow_values; 

  for (size_t i = 0; i < host_overflow_count_mem[0]; i++) {

    auto it = std::find(overflow_keys.begin(), overflow_keys.end(), host_overflow_mem[i].key);
    // first search key vector if key exists
    if (it != overflow_keys.end()) {
      auto index = std::distance(overflow_keys.begin(), it);
      overflow_values[index] += host_overflow_mem[i].value;
    }
    else {
      overflow_keys.push_back(host_overflow_mem[i].key);
      overflow_values.push_back(host_overflow_mem[i].value);
    }
  }

  for (size_t i = 0; i < overflow_keys.size(); i++) {
    cout << "overflow[" << (int)overflow_keys[i] << "] = " << overflow_values[i] << "\n";
  }

  // for (size_t i = 0; i < host_overflow_count_mem[0]; i++) {
  //   cout << "overflow_mem[" << i << "] = {"
  //        << (int)host_overflow_mem[i].key << ", "
  //        << (int)host_overflow_mem[i].value << "}\n";
  // }

  return 0;


}

