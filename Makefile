CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

TARGET = myshell

SRC = src/main.cpp \
      src/parser/tokenizer.cpp \
      src/parser/parser.cpp \
      src/builtins/builtins.cpp \
      src/executer/executer.cpp
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
