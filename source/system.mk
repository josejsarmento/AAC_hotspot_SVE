## Makefile variables ## 

# C compiler #
HOST_CC := g++
HOST_CC_FLAGS := -g

# ARM compiler #
GEM5_CC := aarch64-linux-g++
GEM5_CC_FLAGS := -g
#GEM5_CC_FLAGS += -I/home/aac2021/ccross_aarch64/aarch64-linux/lib/

ifeq ($(SVE),1)
	GEM5_CC_FLAGS += -D SVE
endif

ifeq ($(NOUNROLL),1)
	GEM5_CC_FLAGS += -D NOUNROLLING
endif

# Benchmark flags #
ifeq ($(TIME),1)
	HOST_CC_FLAGS += -D TIME
	GEM5_CC_FLAGS += -D TIME
endif

ifeq ($(VERBOSE),1)
	HOST_CC_FLAGS += -D VERBOSE
	GEM5_CC_FLAGS += -D VERBOSE
endif
ifeq ($(OUTPUT),1)
	HOST_CC_FLAGS += -D OUTPUT
	GEM5_CC_FLAGS += -D OUTPUT
endif

# Optimization flags #
HOST_CC_FLAGS += -O2
GEM5_CC_FLAGS += -static -O2

# Scalable Vector Extension instruction set support #
ifeq ($(SVE),1)
	GEM5_CC_FLAGS += -march=armv8-a+sve
	GEM5_CC_FLAGS += -fno-tree-vectorize
endif

# Directories #
DATA_DIR = ../data
GEM5_DIR = $(HOME)/gem5

# CPU specs #
## CPU selection
ifndef CPU
CPU := ex5_LITTLE
#CPU := ex5_big
endif
## Vector length: n x 128-bits (128-bits to 2048 bits)
ifndef VL 
VL := 8
endif

# Benchmark specs #
## Number of iterations
ifndef ITER
I := 1
endif
## Matrix size
ifndef S
S := 64
endif


# Gem5 debug #
ifeq ($(D),1)
DEBUG := --debug-flag=Exec
endif

# Makefile debug option #
print-%: ; @echo $*=$($*)


## old stuff ##
# run_host: hotspot_host
# 	@echo "======================================================================"
# 	@echo "WARNING: Running with 1000 iterations... "
# 	@echo "you may change the number of iterations and the problem size"
# 	@echo "======================================================================"
# 	./hotspot_host 1024 1024 1000 1 $(DATA_DIR)/temp_1024 $(DATA_DIR)/power_1024 output_host.out

# run_gem5: hotspot_gem5
# 	@echo "======================================================================"
# 	@echo "WARNING: Running the smallest problem size (64) with 2 iterations... "
# 	@echo "you should change the number of iterations and the problem size"
# 	@echo "for the final results"
# 	@echo "======================================================================"
# 	$(GEM5_DIR)/build/ARM/gem5.opt $(GEM5_DIR)/configs/example/se.py --cpu-type=ex5_LITTLE --caches --svevl=1 -c \
# 		hotspot_gem5 -o '64 64 2 1 $(DATA_DIR)/temp_64 $(DATA_DIR)/power_64 output_gem5.out'
