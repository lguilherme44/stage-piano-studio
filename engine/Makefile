CXX = clang++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -Iinclude -MMD -MP
LDFLAGS = -framework CoreAudio -framework CoreMIDI -framework AudioToolbox -framework CoreFoundation

SRC = src/AudioEngine.cpp \
      src/MidiEngine.cpp \
      src/Equalizer.cpp \
      src/Compressor.cpp \
      src/Chorus.cpp \
      src/Delay.cpp \
      src/Reverb.cpp \
      src/TimbrePreset.cpp \
      src/Sampler.cpp \
      src/StagePianoHost.cpp \
      src/main.cpp

OBJ = $(SRC:.cpp=.o)
DEPS = $(OBJ:.o=.d)
TARGET = bin/stage_piano_engine

all: $(TARGET)

bin:
	mkdir -p bin

$(TARGET): bin $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LDFLAGS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -rf src/*.o src/*.d bin/

.PHONY: all clean
