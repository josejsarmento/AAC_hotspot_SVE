include ./system.mk


compile_all: hotspot_gem5 hotspot_host

compile_host: hotspot_host

compile_gem5: hotspot_gem5

run_host: hotspot_host
	./hotspot_host $(S) $(S) $(I) 1 $(DATA_DIR)/temp_$(S) $(DATA_DIR)/power_$(S) output_host.out

run_gem5: hotspot_gem5
	$(GEM5_DIR)/build/ARM/gem5.opt $(GEM5_DIR)/configs/example/se.py --cpu-type=$(CPU) --caches --svevl=$(VL) $(DEBUG) -c \
		hotspot_gem5 -o '$(S) $(S) $(I) 1 $(DATA_DIR)/temp_$(S) $(DATA_DIR)/power_$(S) output_gem5.out'

hotspot_host: hotspot.cpp
	$(HOST_CC) $(HOST_CC_FLAGS) $< -o $@

hotspot_gem5: hotspot.cpp
	$(GEM5_CC) $(GEM5_CC_FLAGS) $< -o $@

compare:
	diff output_gem5.out files_output/output_gem5_$(S).out > $(S).diff

objdump:
	aarch64-linux-objdump -d hotspot_gem5 > objdump.txt

clean:
	rm -f hotspot_host hotspot_gem5 output_*.out *.diff objdump.txt temp.out

.PHONY: run_host run_gem5 clean compare objdump
