TARGET = /home/jcheung2/ofs_fourslot/oneapi-asp/n6001:ofs_n6001_iopipes

MHZ = 200MHz

EARLY = -fsycl-link=early 

SEED = 0

FLAGS = -fsycl -fPIC -fintelfpga -Xstarget=$(TARGET) -shared -Xshardware -Xsclock=$(MHZ) -Xsseed=$(SEED)

COMMON = -I ./include

s0:
	icpx $(FLAGS) $(COMMON) slot0/helper.cpp -o slot0/helper.so

s1:
	icpx $(FLAGS) $(COMMON) slot1/helper.cpp -o slot1/helper.so

s2:
	icpx $(FLAGS) $(COMMON) slot2/helper.cpp -o slot2/helper.so

s3:
	icpx $(FLAGS) $(COMMON) slot3/src.cpp    -o slot3/src.so

s0_early:
	icpx $(FLAGS) $(EARLY) $(COMMON) slot0/helper.cpp -o slot0/helper_early.so

s1_early:
	icpx $(FLAGS) $(EARLY) $(COMMON) slot1/helper.cpp -o slot1/helper_early.so

s2_early:
	icpx $(FLAGS) $(EARLY) $(COMMON) slot2/helper.cpp -o slot2/helper_early.so

s3_early:
	icpx $(FLAGS) $(EARLY) $(COMMON) slot3/src.cpp		-o slot3/src_early.so

all_early:
	make s0_early
	make s1_early
	make s2_early
	make s3_early


