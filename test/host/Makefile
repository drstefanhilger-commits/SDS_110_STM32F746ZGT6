# SDS_110 – Host-Tests (Linux/WSL, g++), Beschreibung: doc/Host_Tests.md
#
#   make -C test/host            # alle Tests bauen
#   make -C test/host check      # Prüfungen (t_*) ausführen, Exit-Code != 0 bei Fehler
#   make -C test/host measure    # Messungen (m_*) ausführen, Tabellen mit doc/Host_Tests.md vergleichen
#   make -C test/host longrun    # Langzeitmessung (~5 min Signal, einige Minuten Rechenzeit)
#   make -C test/host clean
#   make -C test/host gegenprobe TEST=t_scaling CODE=/pfad/zu/altem/stand   # ein Test gegen älteren Code
#
# Die Tests übersetzen die portablen Module aus Core/SDS_110 gegen Stubs für CMSIS-DSP,
# CMSIS-RTOS2 und HAL (shim*/). FFT über kiss_fft (test/host/kiss) statt arm_rfft_fast_f32.
# STM32F746ZGT6-Port: ohne SDRAM-Selbsttest (kein SDRAM) und ohne tools/features (Merkmalsversion hier).

PROJ  := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
HERE  := $(PROJ)/test/host
# Stand des getesteten Codes (Standard: dieses Repo; für Gegenproben z. B. ein git worktree)
CODE  ?= $(PROJ)
S     := $(CODE)/Core/SDS_110
BUILD ?= $(if $(filter $(PROJ),$(CODE)),$(PROJ)/build/test_host,$(PROJ)/build/test_host_gegenprobe)
CXX   ?= g++
CC    ?= gcc

CXXFLAGS := -std=gnu++17 -O2 -Wall
DEPFLAGS := -MMD -MP                 # Header-Abhängigkeiten der Objekte (.d neben .o)
INC_CHAIN := -I$(HERE)/shim -I$(HERE)/common -I$(S) -I$(HERE)/kiss

# Portable Module der Signalkette (für t_bearing_broadband, t_unit_report und alle m_*)
MOD_SRCS := $(S)/Harness/Signal_Simulator.cpp \
            $(S)/Sensor_Unit_112/Microphone_Array_114.cpp \
            $(S)/Sensor_Unit_112/Pre_Processor_118.cpp \
            $(S)/Sensor_Unit_112/Frame_Assembler.cpp \
            $(S)/Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.cpp \
            $(S)/Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.cpp \
            $(S)/Processing_Module_120/Machine_Learning_Module_124/HBD.cpp \
            $(S)/Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.cpp \
            $(S)/Processing_Module_120/Localisation_Module_128/Localisation_Module_128.cpp \
            $(S)/Processing_Module_120/Output_Interface_130/Output_Interface_130.cpp
SHIM_SRCS := $(HERE)/shim/arm_rfft.cpp $(HERE)/shim/sds_stubs.cpp
MOD_OBJS := $(patsubst $(CODE)/%.cpp,$(BUILD)/obj/code/%.o,$(MOD_SRCS)) \
            $(patsubst $(HERE)/%.cpp,$(BUILD)/obj/test/%.o,$(SHIM_SRCS)) $(BUILD)/obj/kiss_fft.o

CHECKS   := t_scaling t_frame_assembler t_unit_report t_timebase t_logger t_bearing_broadband t_ml124 t_hopclock t_gcc_direct t_sound_speed t_azimuth t_feedback t_usb_commands t_local_position t_sds_data t_bearing_f0 t_hop_gap t_sim_flyby
MEASURES := m_overview m_hbd_diag m_bearing_drone m_selection m_distance m_confidence m_bearing_f0
ALL      := $(CHECKS) $(MEASURES) m_longrun

all: $(addprefix $(BUILD)/,$(ALL))

$(BUILD)/obj/code/%.o: $(CODE)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INC_CHAIN) -c $< -o $@
$(BUILD)/obj/test/%.o: $(HERE)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INC_CHAIN) -c $< -o $@
$(BUILD)/obj/kiss_fft.o: $(HERE)/kiss/kiss_fft.c
	@mkdir -p $(dir $@)
	$(CC) -O2 $(DEPFLAGS) -I$(HERE)/kiss -c $< -o $@
-include $(MOD_OBJS:.o=.d)

# Tests auf der ganzen Signalkette
CHAIN_TESTS := t_bearing_broadband t_gcc_direct t_sound_speed t_azimuth t_feedback t_bearing_f0 t_sim_flyby $(MEASURES) m_longrun
$(addprefix $(BUILD)/,$(CHAIN_TESTS)): $(BUILD)/%: $(HERE)/%.cpp $(MOD_OBJS)
	$(CXX) $(CXXFLAGS) $(INC_CHAIN) $< $(MOD_OBJS) -o $@

# t_ml124: Signalkette + Merkmalsversion dieses Code-Stands (gleiche Berechnung wie tools/features)
# (gleiche Berechnung wie tools/features im Repo SDS_110: SHA-256 über Config, 114, 118, Frame_Assembler, 122)
FEATURE_SRCS := $(S)/SDS_110_Config.hpp \
                $(S)/Sensor_Unit_112/Microphone_Array_114.hpp $(S)/Sensor_Unit_112/Microphone_Array_114.cpp \
                $(S)/Sensor_Unit_112/Pre_Processor_118.hpp $(S)/Sensor_Unit_112/Pre_Processor_118.cpp \
                $(S)/Sensor_Unit_112/Frame_Assembler.hpp $(S)/Sensor_Unit_112/Frame_Assembler.cpp \
                $(S)/Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp \
                $(S)/Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.cpp
FEATURE_VERSION := $(shell cat $(FEATURE_SRCS) | grep -v DspOptimize.hpp | sha256sum | cut -c1-16)
$(BUILD)/t_ml124: $(HERE)/t_ml124.cpp $(MOD_OBJS)
	$(CXX) $(CXXFLAGS) $(INC_CHAIN) -DSDS_FEATURE_VERSION='"$(FEATURE_VERSION)"' $< $(MOD_OBJS) -o $@

# Einzelmodule
$(BUILD)/t_unit_report: $(HERE)/t_unit_report.cpp $(S)/Processing_Module_120/Output_Interface_130/Output_Interface_130.cpp \
                        $(S)/Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.cpp \
                        $(BUILD)/obj/test/shim/arm_rfft.o $(BUILD)/obj/test/shim/sds_stubs.o $(BUILD)/obj/kiss_fft.o
	$(CXX) $(CXXFLAGS) $(INC_CHAIN) $^ -o $@
$(BUILD)/t_scaling: $(HERE)/t_scaling.cpp $(S)/Sensor_Unit_112/Microphone_Array_114.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(HERE)/shim -I$(S) $^ -o $@
$(BUILD)/t_frame_assembler: $(HERE)/t_frame_assembler.cpp $(S)/Sensor_Unit_112/Frame_Assembler.cpp $(S)/Sensor_Unit_112/Microphone_Array_114.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(HERE)/shim -I$(S) $^ -o $@
$(BUILD)/t_timebase: $(HERE)/t_timebase.cpp $(S)/Infrastructure/Utils/TimeBase.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(S) $< -o $@
$(BUILD)/t_usb_commands: $(HERE)/t_usb_commands.cpp $(S)/Infrastructure/Utils/CommandAssembler.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(S) $< -o $@
$(BUILD)/t_local_position: $(HERE)/t_local_position.cpp $(S)/Infrastructure/Utils/LocalPosition.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(S) $< -o $@
$(BUILD)/t_sds_data: $(HERE)/t_sds_data.cpp $(S)/Infrastructure/Model/SDS_Data.cpp $(S)/Infrastructure/Model/SDS_Data.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(HERE)/shim -I$(S) $(HERE)/t_sds_data.cpp $(S)/Infrastructure/Model/SDS_Data.cpp -o $@
$(BUILD)/t_hop_gap: $(HERE)/t_hop_gap.cpp $(S)/Sensor_Unit_112/Microphone_Array_114.cpp $(S)/Sensor_Unit_112/Frame_Assembler.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(HERE)/shim -I$(S) $^ -o $@
$(BUILD)/t_hopclock: $(HERE)/t_hopclock.cpp $(S)/Infrastructure/Utils/HopClock.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(S) $< -o $@
$(BUILD)/t_logger: $(HERE)/t_logger.cpp $(S)/Infrastructure/Utils/Logger.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -pthread -I$(HERE)/shim_irq -I$(HERE)/shim -I$(S) $^ -o $@

check: $(addprefix $(BUILD)/,$(CHECKS))
	@fail=0; for t in $(CHECKS); do echo "=== $$t"; $(BUILD)/$$t || { echo "*** $$t FEHLGESCHLAGEN"; fail=1; }; done; \
	 if [ $$fail = 0 ]; then echo "=== alle Prüfungen bestanden"; else echo "=== Prüfungen fehlgeschlagen"; exit 1; fi

measure: $(addprefix $(BUILD)/,$(MEASURES))
	@for t in $(MEASURES); do echo "=== $$t"; $(BUILD)/$$t; done

longrun: $(BUILD)/m_longrun
	$(BUILD)/m_longrun -1

# Gegenprobe: einen Test gegen einen anderen Code-Stand (CODE=...) bauen und ausführen.
# Erwartung vor der jeweiligen Korrektur: Test schlägt fehl (make meldet dann einen Fehler).
gegenprobe:
	@test -n "$(TEST)" || { echo "Aufruf: make gegenprobe TEST=t_name CODE=/pfad"; exit 2; }
	@$(MAKE) --no-print-directory $(BUILD)/$(TEST)
	$(BUILD)/$(TEST)

clean:
	rm -rf $(BUILD)

.PHONY: all check measure longrun gegenprobe clean
