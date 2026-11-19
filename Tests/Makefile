# Offline tests. Works with g++ or clang++ on Linux (the Mac build is in
# scripts/build_vst_mac.sh). Everything is compiled as C++98 to prove it builds
# on old toolchains.
#   make run        program store + DSP test suite, writes demo WAVs to ./renders
#   make vst-run    builds the plugin as a shared library and loads it in a mini VST host
#   make asan       both, under AddressSanitizer and UndefinedBehaviorSanitizer

CXX      ?= c++
CXXFLAGS ?= -O2 -std=c++98 -Wall -Wextra -pedantic -Wno-long-long
DSP_SRC   = ../Source/DSP/Sdx330.cpp ../Source/DSP/Factory.cpp
HEADERS   = ../Source/DSP/*.h ../Source/VST2/*.h

dsp_test: $(DSP_SRC) dsp_test.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(DSP_SRC) dsp_test.cpp -o $@ $(LDFLAGS)

TBSDX330.so: $(DSP_SRC) ../Source/VST2/SdxVST.cpp ../Source/VST2/Programs.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -fPIC -fvisibility=hidden -shared ../Source/VST2/SdxVST.cpp ../Source/VST2/Programs.cpp $(DSP_SRC) -o $@ $(LDFLAGS)

vst_host_test: vst_host_test.cpp $(DSP_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) vst_host_test.cpp $(DSP_SRC) -ldl -o $@ $(LDFLAGS)

program_test: program_test.cpp ../Source/VST2/Programs.cpp ../Source/DSP/Factory.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) program_test.cpp ../Source/VST2/Programs.cpp ../Source/DSP/Factory.cpp -o $@ $(LDFLAGS)

run: dsp_test program_test
	./program_test
	mkdir -p renders
	./dsp_test renders

vst-run: TBSDX330.so vst_host_test
	./vst_host_test ./TBSDX330.so

asan:
	$(MAKE) clean
	$(MAKE) run vst-run CXXFLAGS="-O1 -g -std=c++98 -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address,undefined"
	$(MAKE) clean

clean:
	rm -rf dsp_test program_test vst_host_test TBSDX330.so renders

.PHONY: run vst-run asan clean
